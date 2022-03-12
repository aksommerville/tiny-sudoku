#include "tool_context.h"
#include "fs.h"
#include "decoder.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

/* Cleanup.
 */

void tool_context_cleanup(struct tool_context *ctx) {
  if (ctx->src) free(ctx->src);
  encoder_cleanup(&ctx->dst);
  memset(ctx,0,sizeof(struct tool_context));
}

/* Configure.
 */

int tool_context_configure(
  struct tool_context *ctx,
  int argc,char **argv,
  int (*cb_option)(struct tool_context *ctx,const char *k,int kc,const char *v,int vc)
) {
  int argp=1;
  while (argp<argc) {
  
    // Pop argument and split key/value.
    const char *arg=argv[argp++];
    const char *k=0,*v=0;
    int kc=0,vc=0;
    if (arg[0]!='-') {
      v=arg;
    } else if (arg[1]!='-') {
      k=arg+1;
      kc=1;
      v=arg+2;
    } else {
      k=arg+2;
      while (*k=='-') k++;
      while (k[kc]&&(k[kc]!='=')) kc++;
      if (k[kc]=='=') v=k+kc+1;
    }
    if (v) while (v[vc]) vc++;
    
    // Allow caller to override.
    if (cb_option) {
      int err=cb_option(ctx,k,kc,v,vc);
      if (err<0) return -1;
      if (err>0) continue;
    }
    
    // Naked argument is srcpath.
    if (kc==0) {
      if (ctx->srcpath) {
        fprintf(stderr,"%s: Multiple input paths.\n",argv[0]);
        return -1;
      }
      ctx->srcpath=arg;
      continue;
    }
    
    // Named options...
    
    if (kc==1) switch (k[0]) {
      case 'o': {
          if (ctx->dstpath) {
            fprintf(stderr,"%s: Multiple output paths.\n",argv[0]);
            return -1;
          }
          ctx->dstpath=arg+2;
        } continue;
    }
    
    if ((kc==4)&&!memcmp(k,"help",4)) {
      fprintf(stderr,"Usage: %s -oOUTPUT INPUT [--platform=linux|macos|mswin|tiny]\n",argv[0]);
      return -1;
    }
    
    if ((kc==8)&&!memcmp(k,"platform",8)) {
      ctx->platform=v;
      continue;
    }
      
    fprintf(stderr,"%s: Unexpected argument '%s'.\n",argv[0],arg);
    return -1;
  }
  return 0;
}

/* Acquire input.
 */

int tool_context_acquire_input(struct tool_context *ctx) {

  if (!ctx->srcpath) {
    fprintf(stderr,"Input path required.\n");
    return -1;
  }

  if (ctx->src) free(ctx->src);
  ctx->src=0;
  if ((ctx->srcc=file_read(&ctx->src,ctx->srcpath))<0) {
    ctx->srcc=0;
    fprintf(stderr,"%s: Failed to read file.\n",ctx->srcpath);
    return -1;
  }

  if (!ctx->namec) {
    ctx->name=ctx->srcpath;
    const char *src=ctx->srcpath;
    int ok=1;
    for (;*src;src++) {
      if (*src=='/') {
        ctx->name=src+1;
        ctx->namec=0;
        ok=1;
      } else if (*src=='.') {
        ok=0;
      } else if (ok) {
        ctx->namec++;
      }
    }
    ok=0;
    if ((ctx->namec>0)&&(ctx->namec<=32)) {
      if ((ctx->name[0]<'0')||(ctx->name[0]>'9')) {
        ok=1;
        int i=ctx->namec;
        while (i-->0) {
          char ch=ctx->name[i];
          if ((ch>='a')&&(ch<='z')) continue;
          if ((ch>='A')&&(ch<='Z')) continue;
          if ((ch>='0')&&(ch<='9')) continue;
          if (ch=='_') continue;
          ok=0;
          break;
        }
      }
    }
    if (!ok) {
      fprintf(stderr,"%s: Unable to determine object name. Input path must begin with 1..32-character C identifier.\n",ctx->srcpath);
      return -1;
    }
  }

  return 0;
}

/* Flush output.
 */
 
int tool_context_flush_output(struct tool_context *ctx) {
  if (!ctx->dstpath) {
    fprintf(stderr,"Output path required.\n");
    return -1;
  }
  if (file_write(ctx->dstpath,ctx->dst.v,ctx->dst.c)<0) {
    fprintf(stderr,"%s: Failed to write file.\n",ctx->dstpath);
    return -1;
  }
  return 0;
}

/* Encode C text.
 */
 
int tool_context_encode_text(struct tool_context *ctx,const void *_src,int srcc,int type) {

  const char *tname;
  switch (type) {
    case -8: tname="int8_t"; break;
    case -16: tname="int16_t"; break;
    case -32: tname="int32_t"; break;
    case 8: tname="uint8_t"; break;
    case 16: tname="uint16_t"; break;
    case 32: tname="uint32_t"; break;
    default: return -1;
  }
  int wordsize=(type<0)?-type:type;
  wordsize>>=3;

  int tiny=tool_context_is_tiny(ctx);
  ctx->dst.c=0;
  if (encode_fmt(&ctx->dst,"#include <stdint.h>\n")<0) return -1;
  const char *qualifier="";
  if (tiny) {
    if (encode_fmt(&ctx->dst,"#include <avr/pgmspace.h>\n")<0) return -1;
    qualifier="PROGMEM";
  }
  if (encode_fmt(&ctx->dst,"const %s %.*s[] %s={\n",tname,ctx->namec,ctx->name,qualifier)<0) return -1;
  const uint8_t *src8=_src;
  const uint16_t *src16=_src;
  const uint32_t *src32=_src;
  int i=srcc;
  for (;i-->0;) {
    int v;
    switch (type) {
      case 8: v=*src8++; break;
      case 16: v=*src16++; break;
      case 32: v=*src32++; break;
      case -8: v=(int8_t)*src8++; break;
      case -16: v=(int16_t)*src16++; break;
      case -32: v=(int32_t)*src32++; break;
    }
    if (encode_fmt(&ctx->dst,"%d,",v)<0) return -1;
    if (!(i%16)) encode_fmt(&ctx->dst,"\n");
  }
  if (encode_fmt(&ctx->dst,"};\n")<0) return -1;
  if (encode_fmt(&ctx->dst,"const uint32_t %.*s_len %s=%d;\n",ctx->namec,ctx->name,qualifier,srcc)<0) return -1;
  return 0;
}

/* Platform check.
 */
 
int tool_context_is_tiny(const struct tool_context *ctx) {
  if (!ctx->platform) return 0;
  return !strcmp(ctx->platform,"tiny");
}
