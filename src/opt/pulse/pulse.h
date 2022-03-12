#ifndef PULSE_H
#define PULSE_H

#include <stdint.h>

struct pulse;

void pulse_del(struct pulse *pulse);

struct pulse *pulse_new(
  int rate,int chanc,
  void (*cb)(int16_t *dst,int dstc,struct pulse *pulse),
  void *userdata,
  const char *appname
);

int pulse_get_rate(const struct pulse *pulse);
int pulse_get_chanc(const struct pulse *pulse);
void *pulse_get_userdata(const struct pulse *pulse);
int pulse_get_status(const struct pulse *pulse);
int pulse_lock(struct pulse *pulse);
int pulse_unlock(struct pulse *pulse);

#endif
