#include "render.h"
#include "platform.h"

/* Bounds check helper.
 */
 
struct overbox {
  int16_t l,r,t,b;
  int16_t overl,overr,overt,overb;
};

static inline uint8_t overbox_check(
  struct overbox *box,
  int16_t bl,int16_t bt,int16_t br,int16_t bb
) {
  if ((box->overl=bl-box->l)<0) box->overl=0;
  if ((box->overt=bt-box->t)<0) box->overt=0;
  if ((box->overr=box->r-br)<0) box->overr=0;
  if ((box->overb=box->b-bb)<0) box->overb=0;
  if (box->overl||box->overt||box->overr||box->overb) return 1;
  return 0;
}

/* Friendly blit.
 */

void render_blit(
  struct render_image *dst,int16_t dstx,int16_t dsty,
  const struct render_image *src,int16_t srcx,int16_t srcy,
  int16_t w,int16_t h,
  uint8_t xform
) {
  // We only blit into 16-bit images.
  if (dst->pixelsize!=16) return;

  // Clip thoroughly. It's a real pain due to xform...
  struct overbox srcbox={.l=srcx,.r=srcx+w,.t=srcy,.b=srcy+h};
  struct overbox dstbox={.l=dstx,.t=dsty};
  if (xform&RENDER_XFORM_SWAP) {
    dstbox.r=dstx+h;
    dstbox.b=dsty+w;
  } else {
    dstbox.r=dstx+w;
    dstbox.b=dsty+h;
  }
  uint8_t dstover=overbox_check(&dstbox,0,0,dst->w,dst->h);
  uint8_t srcover=overbox_check(&srcbox,0,0,src->w,src->h);
  if (dstover) { // Increase (srcbox) overages where (dstbox) exceeds them, mindful of (xform).
    if (xform&RENDER_XFORM_SWAP) {
      if (xform&RENDER_XFORM_XREV) {
        if (dstbox.overb>srcbox.overl) { srcbox.overl=dstbox.overb; srcover=1; }
        if (dstbox.overt>srcbox.overr) { srcbox.overr=dstbox.overt; srcover=1; }
      } else {
        if (dstbox.overt>srcbox.overl) { srcbox.overl=dstbox.overt; srcover=1; }
        if (dstbox.overb>srcbox.overr) { srcbox.overr=dstbox.overb; srcover=1; }
      }
      if (xform&RENDER_XFORM_YREV) {
        if (dstbox.overr>srcbox.overt) { srcbox.overt=dstbox.overr; srcover=1; }
        if (dstbox.overl>srcbox.overb) { srcbox.overb=dstbox.overl; srcover=1; }
      } else {
        if (dstbox.overl>srcbox.overt) { srcbox.overt=dstbox.overl; srcover=1; }
        if (dstbox.overr>srcbox.overb) { srcbox.overb=dstbox.overr; srcover=1; }
      }
    } else {
      if (xform&RENDER_XFORM_XREV) {
        if (dstbox.overr>srcbox.overl) { srcbox.overl=dstbox.overr; srcover=1; }
        if (dstbox.overl>srcbox.overr) { srcbox.overr=dstbox.overl; srcover=1; }
      } else {
        if (dstbox.overl>srcbox.overl) { srcbox.overl=dstbox.overl; srcover=1; }
        if (dstbox.overr>srcbox.overr) { srcbox.overr=dstbox.overr; srcover=1; }
      }
      if (xform&RENDER_XFORM_YREV) {
        if (dstbox.overb>srcbox.overt) { srcbox.overt=dstbox.overb; srcover=1; }
        if (dstbox.overt>srcbox.overb) { srcbox.overb=dstbox.overt; srcover=1; }
      } else {
        if (dstbox.overt>srcbox.overt) { srcbox.overt=dstbox.overt; srcover=1; }
        if (dstbox.overb>srcbox.overb) { srcbox.overb=dstbox.overb; srcover=1; }
      }
    }
  }
  if (srcover) {
    // Apply (srcbox) overages to original parameters.
    srcx+=srcbox.overl;
    srcy+=srcbox.overt;
    w-=srcbox.overl+srcbox.overr;
    h-=srcbox.overt+srcbox.overb;
    if (xform&RENDER_XFORM_SWAP) {
      if (xform&RENDER_XFORM_XREV) dsty+=srcbox.overr;
      else dsty+=srcbox.overl;
      if (xform&RENDER_XFORM_YREV) dstx+=srcbox.overb;
      else dstx+=srcbox.overt;
    } else {
      if (xform&RENDER_XFORM_XREV) dstx+=srcbox.overr;
      else dstx+=srcbox.overl;
      if (xform&RENDER_XFORM_YREV) dsty+=srcbox.overb;
      else dsty+=srcbox.overt;
    }
  }
  if ((w<1)||(h<1)) return;
  
  // Determine output geometry based on (xform).
  uint16_t *dstp=dst->v;
  dstp+=dst->stride*dsty+dstx;
  int16_t dstdmin,dstdmaj;
  if (xform&RENDER_XFORM_SWAP) {
    if (xform&RENDER_XFORM_XREV) {
      dstp+=dst->stride*(w-1);
      dstdmin=-dst->stride;
    } else {
      dstdmin=dst->stride;
    }
    if (xform&RENDER_XFORM_YREV) {
      dstp+=h-1;
      dstdmaj=-1;
    } else {
      dstdmaj=1;
    }
  } else {
    if (xform&RENDER_XFORM_XREV) {
      dstp+=w-1;
      dstdmin=-1;
    } else {
      dstdmin=1;
    }
    if (xform&RENDER_XFORM_YREV) {
      dstp+=dst->stride*(h-1);
      dstdmaj=-dst->stride;
    } else {
      dstdmaj=dst->stride;
    }
  }
  dstdmaj-=dstdmin*w;
  
  // Select an appropriate unchecked blitter based on (src) pixelsize and colorkey.
  switch (src->pixelsize) {
    case 1: {
        uint8_t mask0=0x80>>(srcx&7);
        uint8_t *srcp=src->v;
        srcp+=src->stride*srcy+(srcx>>3);
        render_blit_16_1_replace_unchecked(
          dstp,dstdmin,dstdmaj,srcp,mask0,src->stride,w,h,src->bgcolor,src->fgcolor
        );
      } break;
    case 2: /* TODO */ break;
    case 4: /* TODO */ break;
    case 8: /* TODO */ break;
    case 16: {
        uint16_t *srcp=src->v;
        srcp+=src->stride*srcy+srcx;
        if (src->colorkey) {
          render_blit_16_16_colorkey_unchecked(
            dstp,dstdmin,dstdmaj,srcp,src->stride,w,h
          );
        } else {
          render_blit_16_16_opaque_unchecked(
            dstp,dstdmin,dstdmaj,srcp,src->stride,w,h
          );
        }
      } break;
  }
}

/* Unchecked blit: 16-to-16 opaque.
 */

void render_blit_16_16_opaque_unchecked(
  uint16_t *dst,int16_t dstdmin,int16_t dstdmaj,
  const uint16_t *src,uint16_t srcstride,
  int16_t w,int16_t h
) {
  for (;h-->0;src+=srcstride,dst+=dstdmaj) {
    const uint16_t *srcp=src;
    uint16_t i=w;
    for (;i-->0;srcp++,dst+=dstdmin) {
      *dst=*srcp;
    }
  }
}

/* Unchecked blit: 16-to-16 with colorkey.
 */

void render_blit_16_16_colorkey_unchecked(
  uint16_t *dst,int16_t dstdmin,int16_t dstdmaj,
  const uint16_t *src,uint16_t srcstride,
  int16_t w,int16_t h
) {
  for (;h-->0;src+=srcstride,dst+=dstdmaj) {
    const uint16_t *srcp=src;
    uint16_t i=w;
    for (;i-->0;srcp++,dst+=dstdmin) {
      if (*srcp) *dst=*srcp;
    }
  }
}

/* Unchecked blit: 1-to-16.
 */

void render_blit_16_1_replace_unchecked(
  uint16_t *dst,int16_t dstdmin,int16_t dstdmaj,
  const uint8_t *src,uint8_t mask0,uint16_t srcstride,
  int16_t w,int16_t h,
  uint16_t bgcolor,uint16_t fgcolor
) {
  if (srcstride&7) return;
  srcstride>>=3;
  for (;h-->0;src+=srcstride,dst+=dstdmaj) {
    const uint8_t *srcp=src;
    uint8_t mask=mask0;
    uint16_t i=w;
    for (;i-->0;dst+=dstdmin) {
      if ((*srcp)&mask) {
        if (fgcolor) *dst=fgcolor;
      } else {
        if (bgcolor) *dst=bgcolor;
      }
      if (mask==1) {
        srcp++;
        mask=0x80;
      } else {
        mask>>=1;
      }
    }
  }
}
