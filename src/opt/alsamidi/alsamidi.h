/* alsamidi.h
 * Promiscuous MIDI receiver, using ALSA.
 * Use this to feed audioedit from an external sequencer.
 */
 
#ifndef ALSAMIDI_H
#define ALSAMIDI_H

struct alsamidi;

void alsamidi_del(struct alsamidi *alsamidi);
struct alsamidi *alsamidi_new();

int alsamidi_get_fd(struct alsamidi *alsamidi);

int alsamidi_update(void *dst,int dsta,struct alsamidi *alsamidi);

#endif
