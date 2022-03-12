/* bbd.h
 * Another substitute for BBA, since BBC didn't pan out.
 * This time, balls-to-the-wall simple:
 *  - Square waves only.
 *  - Fixed pitch per note.
 *  - Notes have predetermined TTL.
 *  - Notes have the same, basically no-op, envelope.
 */

#ifndef BBD_H
#define BBD_H

#include <stdint.h>

struct bbd;

/* Basic public interface.
 **********************************************************************/
 
void bbd_init(struct bbd *bbd,uint16_t rate);
int16_t bbd_update(struct bbd *bbd);
void bbd_pcm(struct bbd *bbd,const int16_t *v,uint16_t c);
struct bbd_voice *bbd_note(struct bbd *bbd,uint8_t noteid,uint8_t velocity_and_wave,uint8_t ttl);
void bbd_play_song(struct bbd *bbd,const void *src,uint16_t srcc);
void bbd_silence(struct bbd *bbd);

/* Song Format
 * Leading byte with high bit unset is a delay (fixed rate 48 ticks/s).
 * Otherwise it's a note, 3 bytes: [0x80|noteid,velocity,ttl]
 *   u8 0x80|noteid
 *   u8 0xc0=wave
 *      0x3f=velocity
 *   u8 ttl
 */

/* Details.
 *********************************************************************/
 
#define BBD_SONG_TEMPO 48
#define BBD_VOICE_LIMIT 8
#define BBD_PCM_LIMIT 8

struct bbd {
  uint16_t rate;
  uint16_t framespertick;
  
  const uint8_t *song;
  uint16_t songc;
  uint16_t songp;
  uint16_t songdelay;
  uint8_t songrepeat;
  
  struct bbd_voice {
    uint16_t p;
    uint16_t pd;
    int16_t level;
    uint32_t ttl;
    uint8_t wave; // 0,1,2,3
  } voicev[BBD_VOICE_LIMIT];
  uint8_t voicec;
  
  struct bbd_pcm {
    const int16_t *v;
    uint16_t c;
  } pcmv[BBD_PCM_LIMIT];
  uint8_t pcmc;
  
  // If set, the wave must be a single period of 512 samples.
  // Unset, we produce a square wave.
  const int16_t *wavev[4];
};

#endif
