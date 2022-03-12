#include "sounds_internal.h"

/* Sound effect, general plan.
 */

#define SOUND_KEY_level  1
#define SOUND_KEY_bend   2
#define SOUND_KEY_mrange 3
 
struct sound_effect {
  int master;
  int pitch;
  double mrate;
  struct sound_event {
    int time;
    int key;
    double value;
  } *eventv;
  int eventc,eventa;
};

struct sound_slider {
  int key;
  double va,vz;
  int p,c;
};

static void sound_effect_cleanup(struct sound_effect *effect) {
  if (effect->eventv) free(effect->eventv);
}

// Always returns a valid index (possibly c). First at this time, if any present.
static int sound_effect_search_events(const struct sound_effect *effect,int time) {
  int lo=0,hi=effect->eventc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (time<effect->eventv[ck].time) hi=ck;
    else if (time>effect->eventv[ck].time) lo=ck+1;
    else {
      while ((ck>lo)&&(effect->eventv[ck-1].time==time)) ck--;
      return ck;
    }
  }
  return lo;
}

static int sound_effect_add_event(struct sound_effect *effect,int time,int key,double value) {

  if (time<0) return -1;
  switch (key) {
    case SOUND_KEY_level: if ((value<0.0)||(value>99.0)) return -1; break;
    case SOUND_KEY_bend: break;
    case SOUND_KEY_mrange: if (value<0.0) return -1; break;
    default: return -1;
  }

  if (effect->eventc>=effect->eventa) {
    int na=effect->eventa+8;
    if (na>INT_MAX/sizeof(struct sound_event)) return -1;
    void *nv=realloc(effect->eventv,sizeof(struct sound_event)*na);
    if (!nv) return -1;
    effect->eventv=nv;
    effect->eventa=na;
  }
  
  int p=sound_effect_search_events(effect,time+1);
  struct sound_event *event=effect->eventv+p;
  memmove(event+1,event,sizeof(struct sound_event)*(effect->eventc-p));
  effect->eventc++;
  
  event->time=time;
  event->key=key;
  event->value=value;
  
  return 0;
}

static int sound_effect_get_duration(const struct sound_effect *effect) {
  if (effect->eventc<1) return 1;
  return effect->eventv[effect->eventc-1].time+1;
}

static void sound_effect_ms_to_frames(struct sound_effect *effect,int rate) {
  struct sound_event *event=effect->eventv;
  int i=effect->eventc;
  for (;i-->0;event++) {
    event->time=(event->time*rate)/1000;
    if (event->time<0) event->time=0;
  }
}

static int sound_effect_read_header(struct sound_effect *effect,const char *src,int srcc) {
  struct decoder decoder={.src=src,.srcc=srcc};
  
  decode_text_whitespace(&decoder,1);
  if (decode_text_int(&effect->master,&decoder)<0) return -1;
  decode_text_whitespace(&decoder,1);
  if (decode_text_int(&effect->pitch,&decoder)<0) return -1;
  decode_text_whitespace(&decoder,1);
  if (decode_text_float(&effect->mrate,&decoder)<0) return -1;
  
  if ((effect->master<0)||(effect->master>99)) {
    fprintf(stderr,"master must be in 0..99, found %d\n",effect->master);
    return -1;
  }
  if ((effect->pitch<1)||(effect->pitch>11025)) {
    fprintf(stderr,"pitch must be in 1..11025, found %d\n",effect->pitch);
    return -1;
  }
  if ((effect->mrate<0.0)||(effect->mrate>100.0)) {
    fprintf(stderr,"mrate must be in 0..100 (fractional), found %f\n",effect->mrate);
    return -1;
  }
  
  return decoder.srcp;
}

static int sound_key_eval(const char *src,int srcc) {
  #define _(tag) if ((srcc==sizeof(#tag)-1)&&!memcmp(src,#tag,srcc)) return SOUND_KEY_##tag;
  _(level)
  _(bend)
  _(mrange)
  #undef _
  return -1;
}

static int sound_effect_read_event(struct sound_effect *effect,const char *src,int srcc) {

  int time,key;
  double value;
  const char *token;
  int srcp=0,tokenc;
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
    if (src[srcp]=='#') {
      while (srcp<srcc) if (src[srcp++]==0x0a) break;
      continue;
    }
    break;
  }
  
  token=src+srcp;
  tokenc=0;
  while ((srcp<srcc)&&(src[srcp]!=':')) { srcp++; tokenc++; }
  if (sr_int_eval(&time,token,tokenc)<2) {
    fprintf(stderr,"Failed to evaluate '%.*s' as event time\n",tokenc,token);
    return -1;
  }
  if ((srcp>=srcc)||(src[srcp++]!=':')) {
    fprintf(stderr,"Expected ':' after event time\n");
    return -1;
  }
  
  token=src+srcp;
  tokenc=0;
  while ((srcp<srcc)&&(src[srcp]!='=')) { srcp++; tokenc++; }
  if ((key=sound_key_eval(token,tokenc))<0) {
    fprintf(stderr,"Failed to evaluate '%.*s' as event key (level,bend,mrange)\n",tokenc,token);
    return -1;
  }
  if ((srcp>=srcc)||(src[srcp++]!='=')) {
    fprintf(stderr,"Expected '=' after event key\n");
    return -1;
  }
  
  token=src+srcp;
  tokenc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)&&(src[srcp]!='#')) { srcp++; tokenc++; }
  if (sr_float_eval(&value,token,tokenc)<0) {
    fprintf(stderr,"Failed to evaluate '%.*s' as event value\n",tokenc,token);
    return -1;
  }
  
  if (sound_effect_add_event(effect,time,key,value)<0) return -1;
  
  return srcp;
}

/* Advance a slider and return the current value.
 */
 
static double sound_slider_update(
  const struct sound_effect *effect,
  struct sound_slider *slider,
  int t
) {

  // Rebuild ramp if we ran off the end.
  // (c<0) means we finished the last leg, so no point searching.
  if ((slider->p>=slider->c)&&(slider->c>=0)) {
    double va=slider->vz;
    int ta=t;
    int eventp=sound_effect_search_events(effect,t+1);
    int prep=eventp-1;
    for (;prep>=0;prep--) {
      if (effect->eventv[prep].key==slider->key) {
        va=effect->eventv[prep].value;
        ta=effect->eventv[prep].time;
        break;
      }
    }
    double vz=va;
    int tz=-1;
    for (;eventp<effect->eventc;eventp++) {
      if (effect->eventv[eventp].key==slider->key) {
        vz=effect->eventv[eventp].value;
        tz=effect->eventv[eventp].time;
        break;
      }
    }
    slider->p=0;
    slider->va=va;
    slider->vz=vz;
    if (tz<0) slider->c=-1;
    else slider->c=tz-ta+1;
  }
  
  // If we're at one of the limits, return it (eg after the last event).
  if (slider->p>=slider->c) return slider->vz;
  int p=slider->p++;
  if (p<=0) return slider->va;
  
  // Linear interpolation. We don't do curves.
  double zweight=(double)p/(double)slider->c;
  double aweight=1.0-zweight;
  return slider->va*aweight+slider->vz*zweight;
}

/* Synthesize.
 */
 
static void sound_effect_synthesize(int16_t *v,int c,struct sound_effect *effect,int mainrate) {

  if (0) {
    fprintf(stderr,
      "%s master=%d pitch=%d mrate=%f c=%d...\n",
      __func__,effect->master,effect->pitch,effect->mrate,c
    );
    const struct sound_event *event=effect->eventv;
    int i=effect->eventc;
    for (;i-->0;event++) {
      fprintf(stderr,"  %6d %d=%f\n",event->time,event->key,event->value);
    }
  }

  struct sound_slider slider_level={.key=SOUND_KEY_level,.va=0.0,.vz=0.0,.p=0,.c=0};
  struct sound_slider slider_bend={.key=SOUND_KEY_bend,.va=0.0,.vz=0.0,.p=0,.c=0};
  struct sound_slider slider_mrange={.key=SOUND_KEY_mrange,.va=0.0,.vz=0.0,.p=0,.c=0};
  int t=0;
  double carp=0.0; // Carrier position.
  double modp=0.0; // Modulator position.
  double carpd=(M_PI*2.0*effect->pitch)/mainrate; // Pre-bend carrier rate, normalized to 2pi.
  
  for (;c-->0;v++,t++) {
  
    // OK this is very inefficient, but we're a compile-time tool so whatever.
    double level=sound_slider_update(effect,&slider_level,t);
    double bend=sound_slider_update(effect,&slider_bend,t);
    double mrange=sound_slider_update(effect,&slider_mrange,t);
    level=(level*effect->master*3.2); // level and master both in 0..99; product in about 10k
    
    double sample=sin(carp)*level;
    if (sample<=-32768.0) *v=-32768;
    else if (sample>=32767.0) *v=32767;
    else *v=(int16_t)sample;
    
    double mod=sin(modp)*mrange;
    double rate=carpd*pow(2.0,bend/1200.0);
    modp+=rate*effect->mrate;
    if (modp>M_PI) modp-=M_PI*2.0;
    else if (modp<-M_PI) modp+=M_PI*2.0;
    
    carp+=rate+mod*rate;
    if (carp>M_PI) carp-=M_PI*2.0;
    else if (carp<-M_PI) carp+=M_PI*2.0;
  }
}

/* Generate one sound effect and add it to the output.
 * Input starts with three numbers:
 *   MASTER integer 0..99
 *   PITCH integer 1..11k
 *   MRATE float 0..
 * Followed by events:
 *   TIME:KEY=VALUE
 *   level=0..99
 *   bend=cents
 *   mrange=float
 * Times are written in milliseconds, and we convert to frames.
 */
 
static int generate_sound(void *dstpp,const char *src,int srcc) {

  struct sound_effect effect={0};
  int srcp=sound_effect_read_header(&effect,src,srcc);
  if (srcp<0) return -1;
  
  while (srcp<srcc) {
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
    int err=sound_effect_read_event(&effect,src+srcp,srcc-srcp);
    if (err<1) return -1;
    srcp+=err;
  }
  
  const int rate=22050;
  sound_effect_ms_to_frames(&effect,rate);
  int duration=sound_effect_get_duration(&effect);
  if (duration>0xffff) {
    fprintf(stderr,"Sound too long, limit=65535 samples\n");
    return -1;
  }
  
  int16_t *dst=calloc(2,duration);
  if (!dst) return -1;
  sound_effect_synthesize(dst,duration,&effect,rate);
  *(void**)dstpp=dst;
  return duration;
}

/* From sounds description text to C text.
 */
 
static int convert(struct sounds_context *ctx) {

  void *bin=0;
  int binc=0;
  
  if ((binc=generate_sound(&bin,ctx->hdr.src,ctx->hdr.srcc))<0) {
    fprintf(stderr,"%s: Failed to generate sound effect.\n",ctx->hdr.srcpath);
    return -1;
  }
  
  if (!bin) return -1;
  if (tool_context_encode_text(&ctx->hdr,bin,binc,-16)<0) return -1;
  free(bin);
  return 0;
}

/* Arguments.
 */
 
static int cb_arg(struct tool_context *astool,const char *k,int kc,const char *v,int vc) {
  struct sounds_context *ctx=(struct sounds_context*)astool;
  return 0;
}

/* Main.
 */
 
int main(int argc,char **argv) {
  struct sounds_context ctx={0};
  struct tool_context *astool=(struct tool_context*)&ctx;
  if (tool_context_configure(astool,argc,argv,cb_arg)<0) return 1;
  if (tool_context_acquire_input(astool)<0) return 1;
  if (convert(&ctx)<0) return 1;
  if (tool_context_flush_output(astool)<0) return 1;
  return 0;
}
