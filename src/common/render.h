/* render.h
 * Software rendering to the Tiny's 16-bit bgr565be framebuffer.
 */
 
#ifndef RENDER_H
#define RENDER_H

#include <stdint.h>

#define RENDER_XFORM_XREV 0x01
#define RENDER_XFORM_YREV 0x02
#define RENDER_XFORM_SWAP 0x04

struct render_image {
  void *v;
  int16_t w,h;
  int16_t stride; // In pixels. For sub-byte pixels, this must pad to a byte boundary.
  uint8_t pixelsize; // 1,2,4,8,16
  uint8_t colorkey; // Nonzero for natural zeroes to be transparent.
  uint16_t fgcolor,bgcolor; // For (pixelsize==1) only.
};

/* Friendly "checked" blitter.
 * (w,h) refer to (src), if SWAP is in play.
 * Regardless of (xform), the top-left pixel of output is at (dstx,dsty).
 * Only 16-bit images can be used for (dst).
 *****************************************************************/

void render_blit(
  struct render_image *dst,int16_t dstx,int16_t dsty,
  const struct render_image *src,int16_t srcx,int16_t srcy,
  int16_t w,int16_t h,
  uint8_t xform
);

/* Primitive "unchecked" blitters.
 * Caller is responsible for bounds checking and applying any axiswise transform.
 ********************************************************************/

void render_blit_16_16_opaque_unchecked(
  uint16_t *dst, // First output pixel. You apply the transform.
  int16_t dstdmin, // Advancement of (dst) for (x+1) in (src), typically 1.
  int16_t dstdmaj, // Advancement of (dst) for (y+1) in (src), typically stride-w.
  const uint16_t *src, // First input pixel, top-left corner.
  uint16_t srcstride, // Pixels row-to-row in (src), typically its width.
  int16_t w,int16_t h // How many to copy.
);

void render_blit_16_16_colorkey_unchecked(
  uint16_t *dst, // First output pixel. You apply the transform.
  int16_t dstdmin, // Advancement of (dst) for (x+1) in (src), typically 1.
  int16_t dstdmaj, // Advancement of (dst) for (y+1) in (src), typically stride-w.
  const uint16_t *src, // First input pixel, top-left corner.
  uint16_t srcstride, // Pixels row-to-row in (src), typically its width.
  int16_t w,int16_t h // How many to copy.
);

void render_blit_16_1_replace_unchecked(
  uint16_t *dst, // First ouput pixel. You apply the transform.
  int16_t dstdmin, // Advancement of (dst) for (x+1) in (src), typically 1.
  int16_t dstdmaj, // Advancement of (dst) for (y+1) in (src), typically stride-w.
  const uint8_t *src, // Byte containing first input pixel, top-left corner.
  uint8_t mask0, // Single bit indicating offset to first pixel (0x80>>(x&7))
  uint16_t srcstride, // Pixels row-to-row in (src), typically its width.
  int16_t w,int16_t h, // How many to copy.
  uint16_t bgcolor, // Background (zero) color or zero for none.
  uint16_t fgcolor // Foreground (one) color or zero for none.
);

#endif
