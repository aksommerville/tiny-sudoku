#include "ae_internal.h"
#include "tool/common/bb_midi.h"
#include "tool/common/fs.h"

/* Special hacky handling for qtractor's metronome.
 * See notes under BB_MIDI_OPCODE_NOTE_ON.
 */
 
static int16_t audioedit_metr0[5000];
static int16_t audioedit_metr1[5000];
static int audioedit_metr0_ok=0;
static int audioedit_metr1_ok=0;

static void audioedit_metronome_generate(int16_t *v,int c,int which) {
  double level,curve;
  switch (which) {
    case 0: level=0.800; curve=0.999; break;
    case 1: level=0.600; curve=0.995; break;
  }
  for (;c-->0;v++,level*=curve) {
    double sample=((rand()&0xffff)-32768)/32768.0;
    sample*=level;
    *v=sample*32768.0;
  }
}
 
static int audioedit_metronome(struct audioedit *ae,int which) {
  switch (which&1) {
    case 0: {
        if (!audioedit_metr0_ok) {
          audioedit_metronome_generate(audioedit_metr0,sizeof(audioedit_metr0)>>1,0);
          audioedit_metr0_ok=1;
        }
        bbd_pcm(&ae->bbd,audioedit_metr0,sizeof(audioedit_metr0)>>1);
      } break;
    case 1: {
        if (!audioedit_metr1_ok) {
          audioedit_metronome_generate(audioedit_metr1,sizeof(audioedit_metr1)>>1,1);
          audioedit_metr1_ok=1;
        }
        bbd_pcm(&ae->bbd,audioedit_metr1,sizeof(audioedit_metr1)>>1);
      } break;
  }
}

/* Release any voices in BBD that match this request.
 * BBD doesn't have a concept of releasing, why it's kind of hairy.
 */
 
static void audioedit_stop_voice(struct audioedit *ae,uint16_t pd,uint8_t wave) {
  struct bbd_voice *voice=ae->bbd.voicev;
  int i=ae->bbd.voicec;
  for (;i-->0;voice++) {
    if (voice->pd!=pd) continue;
    if (voice->wave!=wave) continue;
    int nttl=voice->level>>1;
    if (nttl<voice->ttl) voice->ttl=nttl;
  }
}

/* Receive MIDI event.
 */
 
int audioedit_event(struct audioedit *ae,const struct bb_midi_event *event) {
  if (audioedit_lock(ae)<0) return -1;
  switch (event->opcode) {
  
    case BB_MIDI_OPCODE_NOTE_ON: {
        //fprintf(stderr,"NOTE ON %d %02x %02x\n",event->chid,event->a,event->b);
        
        // 0x4c/0x60 and 0x4d/0x40 on channel 9 are qtractor's metronome.
        // I estimate low risk of conflict handling these special: 
        // Channel 9 is supposed to be reserved for drums, which we don't do.
        if (event->chid==0x09) {
          if ((event->a==0x4c)&&(event->b==0x60)) {
            return audioedit_metronome(ae,0);
          } else if ((event->a==0x4d)&&(event->b==0x40)) {
            return audioedit_metronome(ae,1);
          }
        }
        
        uint8_t wavvel=(ae->pidv[event->chid]<<6)|(event->b>>1); // 2 bits of wave and 6 bits of velocity
        struct bbd_voice *voice=bbd_note(&ae->bbd,event->a,wavvel,0xff);
        if (voice) {
          if (audioedit_add_note(ae,event->chid,event->a,voice->pd,voice->wave)<0) return -1;
        }
      } break;
      
    case BB_MIDI_OPCODE_NOTE_OFF: {
        int i=ae->notec;
        struct ae_note *note=ae->notev+i-1;
        for (;i-->0;note--) {
          if ((note->chid==event->chid)&&(note->noteid==event->a)) {
            audioedit_stop_voice(ae,note->pd,note->wave);
            ae->notec--;
            memmove(note,note+1,sizeof(struct ae_note)*(ae->notec-i));
          }
        }
      } break;
      
    case BB_MIDI_OPCODE_PROGRAM: {
        if (event->chid<16) {
          ae->pidv[event->chid]=event->a;
        }
      } break;
      
    case BB_MIDI_OPCODE_SYSTEM_RESET: {
        bbd_silence(&ae->bbd);
      } break;
      
    case BB_MIDI_OPCODE_CONTROL: switch (event->a) {
        case BB_MIDI_CONTROL_SOUND_OFF:
        case BB_MIDI_CONTROL_NOTES_OFF: {
            bbd_silence(&ae->bbd);
          } break;
      } break;
      
  }
  return 0;
}

/* Read, compile, and install a wave definition.
 * I made a bad decision, putting the wave-compile logic only in the 'wavecvt' tool.
 * Because of that, and laziness, we invoke the wavecvt executable rather than compiling in this process.
 */
 
int audioedit_load_wave(struct audioedit *ae,const char *path) {
  //fprintf(stderr,"%s %s\n",__func__,path);
  
  // Get wave id from the basename, or abort if unknown. Could surely be smarter here.
  int waveid=-1;
  const char *base=path_basename(path);
       if (!strcmp(base,"wave0.wave")) waveid=0;
  else if (!strcmp(base,"wave1.wave")) waveid=1;
  else if (!strcmp(base,"wave2.wave")) waveid=2;
  else if (!strcmp(base,"wave3.wave")) waveid=3;
  if ((waveid<0)||(waveid>3)) {
    fprintf(stderr,"%s:WARNING: Ignoring wave file because it's not one of the known files.\n",path);
    return 0;
  }
  
  // Call wavecvt to compile it.
  const char *binpath="mid/audioedit.bin"; // important that it end ".bin"
  char cmd[256];
  int cmdc=snprintf(cmd,sizeof(cmd),"out/tool/wavecvt -o%s %s",binpath,path);
  if ((cmdc<1)||(cmdc>=sizeof(cmd))) return 0;
  if (system(cmd)) {
    fprintf(stderr,"%s: Failed to convert via command: %s\n",path,cmd);
    return 0;
  }
  
  // Read the output file. It must be exactly 1024 bytes long.
  void *bin=0;
  int binc=file_read(&bin,binpath);
  if (binc!=1024) {
    fprintf(stderr,"%s: wavecvt produced %d-byte file, expected 1024. Command: %s\n",path,binc,cmd);
    if (bin) free(bin);
    return 0;
  }
  
  // Retain in ae and install in bbd.
  if (ae->wavev[waveid]) free(ae->wavev[waveid]);
  ae->wavev[waveid]=bin;
  if (audioedit_lock(ae)<0) return -1;
  ae->bbd.wavev[waveid]=bin;
  
  fprintf(stderr,"%s: Replaced wave %d\n",path,waveid);
  
  return 0;
}

/* Read, compile, and play a sound effect.
 */
 
int audioedit_load_sound(struct audioedit *ae,const char *path) {
  fprintf(stderr,"TODO %s %s\n",__func__,path);
  if (ae->autoplay_sounds) {
    //...play it
  }
  return 0;
}
