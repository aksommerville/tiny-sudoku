#include "imgcvt.h"
#include "tool/common/fs.h"

/* Generate C text for a structured image.
 */
 
static int imgcvt_output_c(
  struct encoder *dst,
  struct imgcvt_context *ctx,
  const uint16_t *src,
  int w,int h,
  int transparent,
  int stride,
  int pixelsize
) {
  int tiny=tool_context_is_tiny(&ctx->hdr);
  dst->c=0;

  if (encode_fmt(dst,"#include <stdint.h>\n#include \"render.h\"\n")<0) return -1;
  const char *qualifier="";
  if (tiny) {
    if (encode_fmt(dst,"#include <avr/pgmspace.h>\n")<0) return -1;
    qualifier="PROGMEM";
  }

  if (encode_fmt(dst,"const uint16_t %.*s_STORAGE[] %s={\n",ctx->hdr.namec,ctx->hdr.name,qualifier)<0) return -1;
  int i=w*h;
  for (;i-->0;src++) {
    if (encode_fmt(dst,"%d,",*src)<0) return -1;
    if (!(i%16)) encode_fmt(dst,"\n");
  }
  if (encode_fmt(dst,"};\n")<0) return -1;
  
  if (encode_fmt(dst,"const struct render_image %.*s={\n",ctx->hdr.namec,ctx->hdr.name)<0) return -1;
  if (encode_fmt(dst,"  .v=(void*)%.*s_STORAGE,\n",ctx->hdr.namec,ctx->hdr.name)<0) return -1;
  if (encode_fmt(dst,"  .w=%d,\n  .h=%d,\n  .stride=%d,\n",w,h,stride)<0) return -1;
  if (encode_fmt(dst,"  .pixelsize=%d,\n",pixelsize)<0) return -1;
  if (encode_fmt(dst,"  .colorkey=%d,\n",transparent)<0) return -1;
  // Not set: (fgcolor,bgcolor)
  if (encode_fmt(dst,"};\n")<0) return -1;
  
  return 0;
}
 
static int imgcvt_output_c8(
  struct encoder *dst,
  struct imgcvt_context *ctx,
  const uint8_t *src,
  int w,int h,
  int transparent,
  int stride,
  int pixelsize
) {
  int tiny=tool_context_is_tiny(&ctx->hdr);
  dst->c=0;

  if (encode_fmt(dst,"#include <stdint.h>\n#include \"render.h\"\n")<0) return -1;
  const char *qualifier="";
  if (tiny) {
    if (encode_fmt(dst,"#include <avr/pgmspace.h>\n")<0) return -1;
    qualifier="PROGMEM";
  }

  if (encode_fmt(dst,"const uint8_t %.*s_STORAGE[] %s={\n",ctx->hdr.namec,ctx->hdr.name,qualifier)<0) return -1;
  int i=(stride>>3)*h;
  int brk=0;
  for (;i-->0;src++) {
    if (encode_fmt(dst,"%d,",*src)<0) return -1;
    if (!(i%16)) encode_fmt(dst,"\n");
  }
  if (encode_fmt(dst,"};\n")<0) return -1;
  
  if (encode_fmt(dst,"const struct render_image %.*s={\n",ctx->hdr.namec,ctx->hdr.name)<0) return -1;
  if (encode_fmt(dst,"  .v=(void*)%.*s_STORAGE,\n",ctx->hdr.namec,ctx->hdr.name)<0) return -1;
  if (encode_fmt(dst,"  .w=%d,\n  .h=%d,\n  .stride=%d,\n",w,h,stride)<0) return -1;
  if (encode_fmt(dst,"  .pixelsize=%d,\n",pixelsize)<0) return -1;
  if (encode_fmt(dst,"  .colorkey=%d,\n",transparent)<0) return -1;
  // Not set: (fgcolor,bgcolor)
  if (encode_fmt(dst,"};\n")<0) return -1;
  
  return 0;
}

/* Check whether an RGBA PNG object has any transparent pixels.
 */
 
static int imgcvt_has_transparency(const struct png_image *image) {
  const uint8_t *v=image->pixels;
  v+=3;
  int i=image->w*image->h;
  for (;i-->0;v+=4) if (!((*v)&0x80)) return 1;
  return 0;
}

static int imgcvt_is_bw(const struct png_image *image) {
  const uint8_t *v=image->pixels;
  int i=image->w*image->h;
  for (;i-->0;v+=4) {
    if (v[3]!=0xff) return 0;
    if ((v[0]!=v[1])||(v[1]!=v[2])) return 0;
    if ((v[0]!=0x00)&&(v[0]!=0xff)) return 0;
  }
  return 1;
}

/* 1-bit from 32-bit.
 * Caller should verify the input is indeed black-and-white; we only examine the first bit of each input pixel.
 * Both strides are in bytes.
 */
 
static void imgcvt_y1_from_rgba(
  uint8_t *dst,int dststride,
  const uint8_t *src,int srcstride,
  int w,int h
) {
  memset(dst,0,dststride*h);
  for (;h-->0;dst+=dststride,src+=srcstride) {
    uint8_t *dstp=dst;
    const uint8_t *srcp=src;
    uint8_t mask=0x80;
    int xi=w;
    for (;xi-->0;srcp+=4) {
      if (srcp[0]&0x80) (*dstp)|=mask;
      if (!(mask>>=1)) {
        mask=0x80;
        dstp++;
      }
    }
  }
}

/* Convert file in memory.
 */
 
static int imgcvt(struct imgcvt_context *ctx) {

  // Decode PNG, then force RGBA for intermediate processing.
  struct png_image *inpng=png_decode(ctx->hdr.src,ctx->hdr.srcc);
  if (!inpng) {
    fprintf(stderr,"%s: Failed to decode file as PNG.\n",ctx->hdr.srcpath);
    return -1;
  }
  struct png_image png={0};
  if (png_image_convert(&png,8,6,inpng)<0) {
    fprintf(stderr,"%s: Failed to convert to RGBA.\n",ctx->hdr.srcpath);
    png_image_del(inpng);
    png_image_cleanup(&png);
  }
  png_image_del(inpng);
  
  // Check for transparency.
  // If an image has no transparent pixels, we assume it will be used opaquely and permit natural zeroes.
  // Otherwise, natural zero is transparent, and we nudge black a little off.
  int transparent=imgcvt_has_transparency(&png);
  
  // If we're emitting C and the input contains only black and white, make 1-bit pixels.
  const char *sfx=path_suffix(ctx->hdr.dstpath);
  if (!transparent&&!strcmp(sfx,"c")&&imgcvt_is_bw(&png)) {
    int dststride=(png.w+7)>>3;
    uint8_t *dst=malloc(dststride*png.h);
    if (!dst) return -1;
    imgcvt_y1_from_rgba(dst,dststride,png.pixels,png.stride,png.w,png.h);
    if (imgcvt_output_c8(&ctx->hdr.dst,ctx,dst,png.w,png.h,0,dststride<<3,1)<0) return -1;
    png_image_cleanup(&png);
    free(dst);
    return 0;
  }
  
  // Convert to 16-bit big-endian BGR565.
  // Conceivably we might someday want different formats for different platforms or something. Patch that in here.
  if ((png.w>0x01000000)||(png.h>0x01000000)) return -1; // -Walloc-size-larger-than, I wish there were a better way around it :(
  uint16_t *bgr565=malloc(png.w*png.h*2);
  if (!bgr565) return -1;
  uint16_t *dst=bgr565;
  const uint8_t *src=png.pixels;
  int i=png.w*png.h;
  for (;i-->0;dst++,src+=4) {
    *dst=
      (src[2]&0x00f8)|
      (src[1]>>5)|((src[1]<<11)&0xe000)|
      ((src[0]<<5)&0x1f00);
    if (transparent) {
      if (!(src[3]&0x80)) {
        *dst=0;
      } else if (!*dst) {
        *dst=0x2000; // darkest green
      }
    }
  }
  
  // Pick an output method based on dstpath.
  if (!strcmp(sfx,"c")) {
    if (imgcvt_output_c(&ctx->hdr.dst,ctx,bgr565,png.w,png.h,transparent,png.w,16)<0) return -1;
  } else if (!strcmp(sfx,"tsv")) {
    ctx->hdr.dst.v=(char*)bgr565;
    ctx->hdr.dst.c=png.w*png.h*2;
    bgr565=0;
  } else {
    fprintf(stderr,"%s: Unable to determine output format from path. Must end '.c' or '.tsv'\n",ctx->hdr.dstpath);
    return -1;
  }
  
  png_image_cleanup(&png);
  if (bgr565) free(bgr565);
  return 0;
}

/* Extra command-line options.
 */
 
static int cb_option(struct tool_context *astool,const char *k,int kc,const char *v,int vc) {
  struct imgcvt_context *ctx=(struct imgcvt_context*)astool;
  return 0;
}

/* Main.
 */
 
int main(int argc,char **argv) {
  struct imgcvt_context ctx={0};
  struct tool_context *astool=(struct tool_context*)&ctx;
  if (tool_context_configure(astool,argc,argv,cb_option)<0) return 1;
  if (tool_context_acquire_input(astool)<0) return 1;
  if (imgcvt(&ctx)<0) {
    fprintf(stderr,"%s: Failed to convert image.\n",ctx.hdr.srcpath);
    return 1;
  }
  if (tool_context_flush_output(astool)<0) return 1;
  return 0;
}
