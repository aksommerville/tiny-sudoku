/* x11.h
 * Video provider for Linux.
 * This only uses Xlib, so should be highly compatible.
 */

#ifndef X11_H
#define X11_H

#include <stdint.h>

struct x11;

void x11_del(struct x11 *x11);

struct x11 *x11_new(
  const char *title,
  int fbw,int fbh,
  int fullscreen,
  int (*cb_button)(struct x11 *x11,uint8_t btnid,int value),
  int (*cb_close)(struct x11 *x11),
  void *userdata
);

void *x11_get_userdata(const struct x11 *x11);

int x11_set_fullscreen(struct x11 *x11,int state);

int x11_update(struct x11 *x11);
int x11_swap(struct x11 *x11,const void *fb);
void x11_inhibit_screensaver(struct x11 *x11);

#endif
