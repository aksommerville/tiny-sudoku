#include "ae_internal.h"
#include "tool/common/poller.h"
#include "tool/common/fs.h"
#include "tool/common/ossmidi/ossmidi.h"

#if BC_USE_pulse
  #include "opt/pulse/pulse.h"
#endif
#if BC_USE_inotify
  #include "opt/inotify/inotify.h"
#endif
#if BC_USE_alsamidi
  #include "opt/alsamidi/alsamidi.h"
#endif

/* Delete.
 */
 
void audioedit_del(struct audioedit *ae) {
  if (!ae) return;
  
  // Kill driver first to ensure the callback doesn't fire mid-deletion
  #if BC_USE_pulse
    pulse_del(ae->pulse);
  #endif
  
  poller_del(ae->poller);
  ossmidi_del(ae->ossmidi);
  if (ae->notev) free(ae->notev);
  if (ae->wavev[0]) free(ae->wavev[0]);
  if (ae->wavev[1]) free(ae->wavev[1]);
  if (ae->wavev[2]) free(ae->wavev[2]);
  if (ae->wavev[3]) free(ae->wavev[3]);
  
  #if BC_USE_inotify
    inotify_del(ae->inotify);
  #endif
  #if BC_USE_alsamidi
    alsamidi_del(ae->alsamidi);
  #endif
  
  free(ae);
}

/* Audio driver lock management.
 * We are careful to allow redundant calls.
 * Lock whenever you need to, and the main update loop unlocks as the cycle completes.
 */
 
int audioedit_lock(struct audioedit *ae) {
  if (ae->locked) return 0;
  #if BC_USE_pulse
    if (pulse_lock(ae->pulse)<0) return -1;
  #endif
  ae->locked=1;
  return 0;
}

int audioedit_unlock(struct audioedit *ae) {
  if (!ae->locked) return 0;
  #if BC_USE_pulse
    pulse_unlock(ae->pulse);
  #endif
  ae->locked=0;
  return 0;
}

/* PCM callback.
 */
 
#if BC_USE_pulse
static void audioedit_cb_pcm(int16_t *v,int c,struct pulse *pulse) {
  struct audioedit *ae=pulse_get_userdata(pulse);
  for (;c-->0;v++) *v=bbd_update(&ae->bbd);
}
#endif

/* MIDI callbacks.
 */
 
static int audioedit_cb_midi_connect(struct ossmidi *ossmidi,struct ossmidi_device *device) {
  struct audioedit *ae=ossmidi_get_userdata(ossmidi);
  const char *name=ossmidi_device_get_name(device);
  const char *path=ossmidi_device_get_path(device);
  fprintf(stderr,"%s: Connected MIDI device '%s'.\n",path,name);
  return 0;
}
 
static int audioedit_cb_midi_disconnect(struct ossmidi *ossmidi,struct ossmidi_device *device) {
  struct audioedit *ae=ossmidi_get_userdata(ossmidi);
  const char *name=ossmidi_device_get_name(device);
  const char *path=ossmidi_device_get_path(device);
  fprintf(stderr,"%s: Disconnected MIDI device '%s'.\n",path,name);
  return 0;
}
 
static int audioedit_cb_midi_event(struct ossmidi *ossmidi,struct ossmidi_device *device,const struct bb_midi_event *event) {
  struct audioedit *ae=ossmidi_get_userdata(ossmidi);
  return audioedit_event(ae,event);
}

#if BC_USE_alsamidi
static int audioedit_cb_alsamidi_readable(int fd,void *userdata) {
  struct audioedit *ae=userdata;
  uint8_t buf[256];
  int bufc=alsamidi_update(buf,sizeof(buf),ae->alsamidi);
  int bufp=0;
  while (bufp<bufc) {
    struct bb_midi_event event={0};
    int err=bb_midi_stream_decode(&event,&ae->alsamidi_stream,buf+bufp,bufc-bufp);
    if (err<1) break;
    bufp+=err;
    if (audioedit_event(ae,&event)<0) return -1;
  }
  return 0;
}
#endif

/* Inotify callback.
 */
 
#if BC_USE_inotify
static int audioedit_cb_inotify(const char *path,const char *base,int wd,void *userdata) {
  struct audioedit *ae=userdata;
  //fprintf(stderr,"%s %s\n",__func__,path);
  
  // MIDI device?
  if (!memcmp(path,AE_MIDI_DEVICE_DIR,sizeof(AE_MIDI_DEVICE_DIR)-1)) {
    if (!memcmp(base,"midi",4)) {
      if (ossmidi_examine_file(0,ae->ossmidi,path,base)>0) return 0;
    }
  }
  
  // Data file?
  if (!memcmp(path,AE_DATA_DIR,sizeof(AE_DATA_DIR)-1)) {
    const char *sfx=path_suffix(base);
    if (sfx) {
      if (!strcmp(sfx,"mid")) {
        // MIDI file. I guess not interesting to us.
        return 0;
      }
      if (!strcmp(sfx,"wave")) {
        if (audioedit_load_wave(ae,path)<0) return -1;
        return 0;
      }
      if (!strcmp(sfx,"sound")) {
        if (audioedit_load_sound(ae,path)<0) return -1;
        return 0;
      }
    }
  }
  
  return 0;
}

static int audioedit_cb_inotify_readable(int fd,void *userdata) {
  struct audioedit *ae=userdata;
  return inotify_read(ae->inotify);
}
#endif

/* Configure.
 * This does not bring subsystems online.
 */
 
static int audioedit_configure(struct audioedit *ae,int argc,char **argv) {

  // Defaults.
  ae->rate=22050;
  ae->chanc=1;
  
  if (argc>=1) ae->exename=argv[0];
  if (!ae->exename||!ae->exename[0]) ae->exename="audioedit";
  
  int argp=1;
  while (argp<argc) {
    const char *arg=argv[argp++];
    //TODO command line
    fprintf(stderr,"%s:ERROR: Unexpected argument '%s'\n",ae->exename,arg);
  }
  
  return 0;
}

/* New.
 */
 
struct audioedit *audioedit_new(int argc,char **argv) {
  struct audioedit *ae=calloc(1,sizeof(struct audioedit));
  if (!ae) return 0;
  
  ae->scanfs=1;
  ae->autoplay_sounds=1;
  
  // Mimic songcvt's default wave assignments. Only the low 2 bits matter.
  ae->pidv[0]=ae->pidv[4]=ae->pidv[ 8]=ae->pidv[12]=0;
  ae->pidv[1]=ae->pidv[5]=ae->pidv[ 9]=ae->pidv[13]=1;
  ae->pidv[2]=ae->pidv[6]=ae->pidv[10]=ae->pidv[14]=2;
  ae->pidv[3]=ae->pidv[7]=ae->pidv[11]=ae->pidv[15]=3;
  
  if (!(ae->poller=poller_new())) {
    audioedit_del(ae);
    return 0;
  }
  
  // Over-engineered MIDI input...
  struct ossmidi_delegate ossmidi_delegate={
    .userdata=ae,
    .cb_connect=audioedit_cb_midi_connect,
    .cb_disconnect=audioedit_cb_midi_disconnect,
    .cb_event=audioedit_cb_midi_event,
  };
  /*XXX disabling pull-midi-in -- i'm going to go promiscuous and let qtractor talk to the devices *
  if (!(ae->ossmidi=ossmidi_new(AE_MIDI_DEVICE_DIR,ae->poller,&ossmidi_delegate))) {
    audioedit_del(ae);
    return 0;
  }
  /**/
  
  // Read command line...
  if (audioedit_configure(ae,argc,argv)<0) {
    audioedit_del(ae);
    return 0;
  }
  
  // Optional promiscuous MIDI input...
  #if BC_USE_alsamidi
    if (!(ae->alsamidi=alsamidi_new())) {
      fprintf(stderr,"%s: Failed to open promiscuous MIDI receiver.\n",ae->exename);
    } else {
      fprintf(stderr,"%s: Acquired promiscuous MIDI receiver.\n",ae->exename);
      int fd=alsamidi_get_fd(ae->alsamidi);
      if (fd>=0) {
        struct poller_file file={
          .fd=fd,
          .userdata=ae,
          .cb_readable=audioedit_cb_alsamidi_readable,
        };
        poller_add_file(ae->poller,&file);
      }
    }
  #endif
  
  // PCM output...
  #if BC_USE_pulse
    if (!(ae->pulse=pulse_new(ae->rate,ae->chanc,audioedit_cb_pcm,ae,"audioedit"))) {
      fprintf(stderr,"%s:ERROR: Failed to initialize PulseAudio (%d,%d).\n",ae->exename,ae->rate,ae->chanc);
      audioedit_del(ae);
      return 0;
    }
    ae->rate=pulse_get_rate(ae->pulse);
    ae->chanc=pulse_get_chanc(ae->pulse);
  #else
    fprintf(stderr,"%s:WARNING: No audio driver.\n",ae->exename);
  #endif
  
  // File detection...
  #if BC_USE_inotify
    if (!(ae->inotify=inotify_new(audioedit_cb_inotify,ae))) {
      fprintf(stderr,"%s:ERROR: Failed to initialize inotify.\n",ae->exename);
      audioedit_del(ae);
      return 0;
    }
    inotify_watch(ae->inotify,AE_MIDI_DEVICE_DIR);
    inotify_watch(ae->inotify,AE_DATA_DIR);
    struct poller_file infile={
      .fd=inotify_get_fd(ae->inotify),
      .userdata=ae,
      .cb_readable=audioedit_cb_inotify_readable,
    };
    poller_add_file(ae->poller,&infile);
  #else
    fprintf(stderr,"%s:WARNING: inotify disabled.\n",ae->exename);
  #endif
  
  bbd_init(&ae->bbd,ae->rate);
  
  return ae;
}

/* Update.
 */
 
int audioedit_update(struct audioedit *ae) {

  if (ae->scanfs) {
    ae->scanfs=0;
    ae->autoplay_sounds=0;
    #if BC_USE_inotify
      if (inotify_scan(ae->inotify)<0) {
        audioedit_unlock(ae);
        return -1;
      }
      audioedit_unlock(ae);
    #endif
    ae->autoplay_sounds=1;
  }

  if (poller_update(ae->poller,AE_POLL_TIMEOUT_MS)<0) {
    audioedit_unlock(ae);
    return -1;
  }
  
  audioedit_unlock(ae);
  return 0;
}

/* Add note.
 */
 
int audioedit_add_note(struct audioedit *ae,uint8_t chid,uint8_t noteid,uint16_t pd,uint8_t wave) {
  if (ae->notec>=ae->notea) {
    int na=ae->notea+8;
    if (na>INT_MAX/sizeof(struct ae_note)) return -1;
    void *nv=realloc(ae->notev,sizeof(struct ae_note)*na);
    if (!nv) return -1;
    ae->notev=nv;
    ae->notea=na;
  }
  struct ae_note *note=ae->notev+ae->notec++;
  note->chid=chid;
  note->noteid=noteid;
  note->pd=pd;
  note->wave=wave;
  return 0;
}
