#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "tool/common/tool_context.h"

/* Convert text to binary.
 * The text format is one line per row, two characters per column.
 * For now the only blocks we recognize are:
 *   "  " => 0x00
 *   "T " => 0x01
 *   "R " => 0x02
 * Output is an 8-byte header:
 *   u8 width
 *   u8 height
 *   [6] reserved, zero
 * Followed by cells, LRTB.
 * TODO header for imageid and songid
 */
 
static int map_convert(struct encoder *dst,struct tool_context *ctx) {
  struct decoder src={.src=ctx->src,.srcc=ctx->srcc};
  
  // Prepare the 8-byte header. We'll update it as we go.
  dst->c=0;
  if (encode_null(dst,8)<0) return -1;
  dst->v[2]=3; // songid
  dst->v[3]=2; // imageid
  
  const char *line=0;
  int linec=0,lineno=0;
  while ((linec=decode_line(&line,&src))>0) {
    lineno++;
    // No comments or any other nonstandard stuff.
    int linep=0,colc=0;
    for (;linep<=linec-2;linep+=2,colc++) {
      char a=line[linep],b=line[linep+1];
      uint8_t v;
      
           if ((a==' ')&&(b==' ')) v=0x00;
      else if ((a=='T')&&(b==' ')) v=0x01;
      else if ((a=='R')&&(b==' ')) v=0x02;
      
      else {
        fprintf(stderr,"%s:%d: Unexpected block '%c%c' around column %d.\n",ctx->srcpath,lineno,linep);
        return -1;
      }
      
      //XXX TEMP randomly insert animated grass
      if (!v&&!(rand()%6)) v=0xc0;
      
      if (encode_intbe(dst,v,1)<0) return -1;
    }
    if (!colc) {
      fprintf(stderr,"%s:%d: Empty lines forbidden.\n",ctx->srcpath,lineno);
      return -1;
    }
    if (colc>255) {
      fprintf(stderr,"%s:%d: Row too long (%d, limit 255)\n",ctx->srcpath,lineno,colc);
      return -1;
    }
    if (!dst->v[0]) { // width not established; this is the first row
      dst->v[0]=colc;
    } else if (dst->v[0]!=colc) {
      fprintf(stderr,"%s:%d: Expected %d columns, found %d.\n",ctx->srcpath,lineno,dst->v[0],colc);
      return -1;
    }
    if ((uint8_t)dst->v[1]==0xff) {
      fprintf(stderr,"%s:%d: Limit 255 rows.\n",ctx->srcpath,lineno);
      return -1;
    }
    dst->v[1]++;
  }
  return 0;
}

/* Main.
 */
 
int main(int argc,char **argv) {
  struct tool_context ctx={0};
  if (tool_context_configure(&ctx,argc,argv,0)<0) return 1;
  if (tool_context_acquire_input(&ctx)<0) return 1;
  struct encoder dst={0};
  if (map_convert(&dst,&ctx)<0) {
    fprintf(stderr,"%s: Failed to convert map.\n",ctx.srcpath);
    return 1;
  }
  if (tool_context_encode_text(&ctx,dst.v,dst.c,8)<0) return 1;
  if (tool_context_flush_output(&ctx)<0) return 1;
  return 0;
}
