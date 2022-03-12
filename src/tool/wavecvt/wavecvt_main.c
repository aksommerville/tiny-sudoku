/* wavecvt_main.c
 * Generates single-period waves from a list of commands.
 *
 * The initial buffer is silent. You must begin with one of these overwriting commands:
 *   sine
 *   square
 *   saw
 *   noise
 *
 * Followed by any number of modifier commands:
 *   harmonics COEFFICIENTS...
 *   fm RATE RANGE # Considering our extremely low resolution, RATE should probly always be 1
 *   normalize [LEVEL=1]
 *   gain GAIN [CLIP]
 *   mavg SIZE # SIZE in samples
 */

#include "tool/common/tool_context.h"
#include "tool/common/serial.h"
#include "tool/common/fs.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#define WAVE_LENGTH 512

/* Overwrite with a trivial wave.
 */
 
static int wavecvt_cmd_sine(double *wave,const char *arg,int argc) {
  if (argc) {
    fprintf(stderr,"WARNING: 'sine' command takes no parameters.\n");
  }
  double p=0.0,d=(M_PI*2.0)/WAVE_LENGTH;
  int i=WAVE_LENGTH;
  for (;i-->0;wave++,p+=d) *wave=sin(p);
  return 0;
}
 
static int wavecvt_cmd_square(double *wave,const char *arg,int argc) {
  if (argc) {
    fprintf(stderr,"WARNING: 'square' command takes no parameters.\n");
  }
  int midp=WAVE_LENGTH>>1;
  int i=0;
  for (;i<midp;i++,wave++) *wave=1.0;
  for (;i<WAVE_LENGTH;i++,wave++) *wave=-1.0;
  return 0;
}
 
static int wavecvt_cmd_saw(double *wave,const char *arg,int argc) {
  if (argc) {
    fprintf(stderr,"WARNING: 'saw' command takes no parameters.\n");
  }
  double p=1.0,d=(-2.0)/WAVE_LENGTH;
  int i=WAVE_LENGTH;
  for (;i-->0;wave++,p+=d) *wave=p;
  return 0;
}
 
static int wavecvt_cmd_noise(double *wave,const char *arg,int argc) {
  if (argc) {
    fprintf(stderr,"WARNING: 'noise' command takes no parameters.\n");
  }
  int i=WAVE_LENGTH;
  for (;i-->0;wave++) *wave=(rand()&0xffff)/32768.0-1.0;
  return 0;
}

/* Expand harmonics of the current wave.
 */
 
static void wavecvt_harmonics_1(double *dst,const double *src,int step,double level) {
  int i=WAVE_LENGTH;
  int srcp=0;
  for (;i-->0;dst++) {
    (*dst)+=src[srcp]*level;
    srcp+=step;
    if (srcp>=WAVE_LENGTH) srcp-=WAVE_LENGTH;
  }
}
 
static int wavecvt_cmd_harmonics(double *wave,const char *arg,int argc) {

  // Read the coefficients.
  const int COEF_LIMIT=16; // Must be <=WAVE_LENGTH, beyond that whatever.
  double coefv[COEF_LIMIT];
  int coefc=0;
  int argp=0;
  while (argp<argc) {
    if ((unsigned char)arg[argp]<=0x20) { argp++; continue; }
    const char *token=arg+argp;
    int tokenc=0;
    while ((argp<argc)&&((unsigned char)arg[argp]>0x20)) { argp++; tokenc++; }
    if (coefc>=COEF_LIMIT) {
      fprintf(stderr,"WARNING: Too many coefficients (limit %d). Ignoring '%.*s' and beyond.\n",COEF_LIMIT,tokenc,token);
      break;
    }
    if (sr_float_eval(coefv+coefc,token,tokenc)<0) {
      fprintf(stderr,"Failed to evaluate '%.*s' as float.\n",tokenc,token);
      return -1;
    }
    coefc++;
  }
  
  // Synthesize into a temp buffer, then copy it back.
  double tmp[WAVE_LENGTH]={0};
  while (coefc-->0) wavecvt_harmonics_1(tmp,wave,coefc+1,coefv[coefc]);
  memcpy(wave,tmp,sizeof(tmp));
  return 0;
}

/* Perform a single-period harmonic frequency modulation.
 */
 
static int wavecvt_cmd_fm(double *wave,const char *arg,int argc) {

  // Read parameters.
  int argp=0;
  while ((argp<argc)&&((unsigned char)arg[argp]<=0x20)) argp++;
  int rate=1;
  if (argp<argc) {
    const char *token=arg+argp;
    int tokenc=0;
    while ((argp<argc)&&((unsigned char)arg[argp]>0x20)) { argp++; tokenc++; }
    if (sr_int_eval(&rate,token,tokenc)<2) {
      fprintf(stderr,"Failed to evaluate '%.*s' as integer for FM rate.\n",tokenc,token);
      return -1;
    }
    while ((argp<argc)&&((unsigned char)arg[argp]<=0x20)) argp++;
  }
  double range=1.0;
  if (argp<argc) {
    const char *token=arg+argp;
    int tokenc=0;
    while ((argp<argc)&&((unsigned char)arg[argp]>0x20)) { argp++; tokenc++; }
    if (sr_float_eval(&range,token,tokenc)<0) {
      fprintf(stderr,"Failed to evaluate '%.*s' as float for FM range.\n",tokenc,token);
      return -1;
    }
    while ((argp<argc)&&((unsigned char)arg[argp]<=0x20)) argp++;
  }
  
  // Synthesize.
  double tmp[WAVE_LENGTH];
  double carp=0.0; // 0..WAVE_LENGTH
  double modp=0.0; // 0..M_PI*2
  double modrate=(rate*M_PI*2.0)/WAVE_LENGTH;
  int i=WAVE_LENGTH;
  double *dst=tmp;
  for (;i-->0;dst++) {
  
    int icarp=lround(carp);
    while (icarp>=WAVE_LENGTH) { icarp-=WAVE_LENGTH; carp-=WAVE_LENGTH; }
    while (icarp<0) { icarp+=WAVE_LENGTH; carp+=WAVE_LENGTH; }
    *dst=wave[icarp];
    
    double mod=sin(modp)*range;
    modp+=modrate;
    if (modp>=M_PI) modp-=M_PI*2.0;
    double cardp=1.0+mod;
    carp+=cardp;
  }

  memcpy(wave,tmp,sizeof(tmp));
  return 0;
}

/* Normalize.
 */
 
static int wavecvt_cmd_normalize(double *wave,const char *arg,int argc) {

  double target=1.0;
  if (argc>0) {
    if (sr_float_eval(&target,arg,argc)<0) {
      fprintf(stderr,"Failed to evaluate '%.*s' as float for normalize target.\n",argc,arg);
      return -1;
    }
  }

  double hi=wave[0],lo=wave[0];
  int i=WAVE_LENGTH;
  double *v=wave;
  for (;i-->0;v++) {
    if (*v>hi) hi=*v;
    else if (*v<lo) lo=*v;
  }
  if (lo<0.0) lo=-lo;
  if (lo>hi) hi=lo;
  double scale=target/hi;
  for (i=WAVE_LENGTH,v=wave;i-->0;v++) (*v)*=scale;
  return 0;
}

/* Gain and clip (probly not useful?)
 */
 
static int wavecvt_cmd_gain(double *wave,const char *arg,int argc) {

  // Read parameters.
  int argp=0;
  while ((argp<argc)&&((unsigned char)arg[argp]<=0x20)) argp++;
  double gain=1.0;
  if (argp<argc) {
    const char *token=arg+argp;
    int tokenc=0;
    while ((argp<argc)&&((unsigned char)arg[argp]>0x20)) { argp++; tokenc++; }
    if (sr_float_eval(&gain,token,tokenc)<0) {
      fprintf(stderr,"Failed to evaluate '%.*s' as float for gain.\n",tokenc,token);
      return -1;
    }
    while ((argp<argc)&&((unsigned char)arg[argp]<=0x20)) argp++;
  }
  double clip=999.0;
  if (argp<argc) {
    const char *token=arg+argp;
    int tokenc=0;
    while ((argp<argc)&&((unsigned char)arg[argp]>0x20)) { argp++; tokenc++; }
    if (sr_float_eval(&clip,token,tokenc)<0) {
      fprintf(stderr,"Failed to evaluate '%.*s' as float for clip.\n",tokenc,token);
      return -1;
    }
    while ((argp<argc)&&((unsigned char)arg[argp]<=0x20)) argp++;
  }
  
  double nclip=-clip;
  int i=WAVE_LENGTH;
  for (;i-->0;wave++) {
    double v=(*wave)*gain;
    if (v<nclip) *wave=nclip;
    else if (v>clip) *wave=clip;
    else *wave=v;
  }
  return 0;
}

/* Moving average, cheap low-pass.
 */
 
static int wavecvt_cmd_mavg(double *wave,const char *src,int srcc) {
  int size;
  if (sr_int_eval(&size,src,srcc)<0) {
    fprintf(stderr,"Failed to evaluate mavg size.\n");
    return -1;
  }
  if ((size<1)||(size>=WAVE_LENGTH)) {
    fprintf(stderr,"Unreasonable mavg size %d, must be in 1..%d.\n",size,WAVE_LENGTH-1);
    return -1;
  }
  
  // Set up a ring buffer. Populate with the wave's tail, to get the right initial average.
  double buf[WAVE_LENGTH]={0};
  int bufp=0;
  double avg=0.0;
  const double *tail=wave+WAVE_LENGTH-size;
  int i=size;
  for (;i-->0;tail++) {
    double adj=(*tail)/size;
    buf[bufp++]=adj;
    avg+=adj;
  }
  
  // With the initial state set, now run it start to finish like a regular moving average.
  // (because it *is* a regular moving average).
  for (i=WAVE_LENGTH;i-->0;wave++) {
    double adj=(*wave)/size;
    avg+=adj;
    avg-=buf[bufp];
    *wave=avg;
    buf[bufp]=adj;
    bufp++;
    if (bufp>=size) bufp=0;
  }
  return 0;
}

/* Process one command (one line of input).
 */
 
static int wavecvt_command(double *wave,const char *src,int srcc) {

  // Strip comment and edge space, get out if empty.
  int i=0;
  for (;i<srcc;i++) if (src[i]=='#') srcc=i;
  while (srcc&&((unsigned char)src[srcc-1]<=0x20)) srcc--;
  while (srcc&&((unsigned char)src[0]<=0x20)) { srcc--; src++; }
  if (!srcc) return 0;
  
  // First word is the command.
  int srcp=0;
  const char *cmd=src;
  int cmdc=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; cmdc++; }
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  
  if ((cmdc==4)&&!memcmp(cmd,"sine",4)) return wavecvt_cmd_sine(wave,src+srcp,srcc-srcp);
  if ((cmdc==6)&&!memcmp(cmd,"square",6)) return wavecvt_cmd_square(wave,src+srcp,srcc-srcp);
  if ((cmdc==3)&&!memcmp(cmd,"saw",3)) return wavecvt_cmd_saw(wave,src+srcp,srcc-srcp);
  if ((cmdc==5)&&!memcmp(cmd,"noise",5)) return wavecvt_cmd_noise(wave,src+srcp,srcc-srcp);
  if ((cmdc==9)&&!memcmp(cmd,"harmonics",9)) return wavecvt_cmd_harmonics(wave,src+srcp,srcc-srcp);
  if ((cmdc==2)&&!memcmp(cmd,"fm",2)) return wavecvt_cmd_fm(wave,src+srcp,srcc-srcp);
  if ((cmdc==9)&&!memcmp(cmd,"normalize",9)) return wavecvt_cmd_normalize(wave,src+srcp,srcc-srcp);
  if ((cmdc==4)&&!memcmp(cmd,"gain",4)) return wavecvt_cmd_gain(wave,src+srcp,srcc-srcp);
  if ((cmdc==4)&&!memcmp(cmd,"mavg",4)) return wavecvt_cmd_mavg(wave,src+srcp,srcc-srcp);
  
  fprintf(stderr,"Unknown command '%.*s' (sine,square,saw,noise,harmonics,fm,normalize,gain)\n",cmdc,cmd);
  return -1;
}

/* Convert.
 */
 
static int wavecvt(double *wave,struct tool_context *ctx) {
  struct decoder src={.src=ctx->src,.srcc=ctx->srcc};
  const char *line;
  int linec,lineno=0;
  while ((linec=decode_line(&line,&src))>0) {
    lineno++;
    if (wavecvt_command(wave,line,linec)<0) {
      fprintf(stderr,"%s:%d: Failed to execute command.\n",ctx->srcpath,lineno);
      return -1;
    }
  }
  return 0;
}

/* Quantize final production.
 */
 
static void wavecvt_quantize(int16_t *dst,const double *src) {
  int i=WAVE_LENGTH;
  for (;i-->0;dst++,src++) {
    double v=(*src)*32700.0;
    if (v<-32768.0) *dst=-32768;
    else if (v>32767.0) *dst=32767;
    else *dst=(int16_t)v;
  }
}

/* Choose output format based on path.
 */
 
static char wavecvt_choose_output_format(struct tool_context *ctx) {
  const char *sfx=path_suffix(ctx->dstpath);
  if (!strcmp(sfx,"bin")) return 'b';
  return 'c';
}

/* Main.
 */
 
int main(int argc,char **argv) {
  srand(0); // Force rand() to behave somewhat predictably.
  struct tool_context ctx={0};
  double wave[WAVE_LENGTH]={0};
  if (tool_context_configure(&ctx,argc,argv,0)<0) return 1;
  if (tool_context_acquire_input(&ctx)<0) return 1;
  
  if (wavecvt(wave,&ctx)<0) {
    fprintf(stderr,"%s: Conversion failed.\n",ctx.srcpath);
    return 1;
  }
  int16_t qwave[WAVE_LENGTH];
  wavecvt_quantize(qwave,wave);
  
  switch (wavecvt_choose_output_format(&ctx)) {
    case 'b': {
        ctx.dst.c=0;
        if (encode_raw(&ctx.dst,qwave,sizeof(qwave))<0) return 1;
      } break;
    case 'c': {
        if (tool_context_encode_text(&ctx,qwave,WAVE_LENGTH,-16)<0) return 1;
      } break;
    default: return 1;
  }
  
  if (tool_context_flush_output(&ctx)<0) return 1;
  return 0;
}
