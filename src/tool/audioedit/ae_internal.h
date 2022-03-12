#ifndef AE_INTERNAL_H
#define AE_INTERNAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include "common/bbd.h"
#include "tool/common/bb_midi.h"

struct pulse;
struct inotify;
struct poller;
struct ossmidi;
struct alsamidi;

#define AE_MIDI_DEVICE_DIR "/dev"
#define AE_DATA_DIR "src/data/embed"

#define AE_POLL_TIMEOUT_MS 1000

struct audioedit {
  struct pulse *pulse;
  struct inotify *inotify;
  struct poller *poller;
  struct ossmidi *ossmidi;
  struct alsamidi *alsamidi;
  const char *exename;
  int rate,chanc;
  int scanfs; // nonzero to scan for new files on the next update (initially nonzero)
  struct bbd bbd;
  int locked;
  int autoplay_sounds;
  struct bb_midi_stream alsamidi_stream;
  
  uint8_t pidv[16];
  struct ae_note {
    uint8_t chid; // id in midi
    uint8_t noteid; // id in midi
    uint16_t pd; // id in bbd
    uint8_t wave; // id in bbd
  } *notev;
  int notec,notea;
  
  void *wavev[4];
};

struct audioedit *audioedit_new(int argc,char **argv);
void audioedit_del(struct audioedit *ae);

// Sleeps for a finite period if no activity; call as fast as you like.
int audioedit_update(struct audioedit *ae);

int audioedit_event(struct audioedit *ae,const struct bb_midi_event *event);

int audioedit_lock(struct audioedit *ae);
int audioedit_unlock(struct audioedit *ae);

int audioedit_add_note(struct audioedit *ae,uint8_t chid,uint8_t noteid,uint16_t pd,uint8_t wave);

int audioedit_load_wave(struct audioedit *ae,const char *path);
int audioedit_load_sound(struct audioedit *ae,const char *path);

#endif
