/* evdev.h
 * Interface to Linux evdev for joysticks.
 */

#ifndef EVDEV_H
#define EVDEV_H

#include <stdint.h>

struct evdev;

#define EVDEV_BTN_ALTEVENT BUTTON_RSVD1
#define EVDEV_ALTEVENT_QUIT 1
#define EVDEV_ALTEVENT_FULLSCREEN 2

void evdev_del(struct evdev *evdev);

struct evdev *evdev_new(
  int (*cb_button)(struct evdev *evdev,uint8_t btnid,int value),
  void *userdata
);

void *evdev_get_userdata(const struct evdev *evdev);

int evdev_update(struct evdev *evdev);

#endif
