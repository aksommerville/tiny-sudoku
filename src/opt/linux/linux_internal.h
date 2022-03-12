#ifndef LINUX_INTERNAL_H
#define LINUX_INTERNAL_H

#include "common/platform.h"
#include <stdio.h>
#include <stdlib.h>

#if BC_USE_pulse
  #include "opt/pulse/pulse.h"
  extern struct pulse *pulse;
#endif

#if BC_USE_x11
  #include "opt/x11/x11.h"
  extern struct x11 *x11;
#endif

#if BC_USE_evdev
  #include "opt/evdev/evdev.h"
  extern struct evdev *evdev;
#endif

#endif
