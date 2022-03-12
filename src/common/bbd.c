#include "bbd.h"

/* Note rates.
 * If you initialize multiple contexts, this will get messed up.
 */
 
static uint16_t bbd_ratev[128]={
  0x0018,0x001a,0x001b,0x001d,0x001f,0x0020,0x0022,0x0024,0x0027,0x0029,0x002b,0x002e,0x0031,0x0033,0x0037,0x003a,
  0x003d,0x0041,0x0045,0x0049,0x004d,0x0052,0x0057,0x005c,0x0061,0x0067,0x006d,0x0074,0x007a,0x0082,0x0089,0x0092,
  0x009a,0x00a3,0x00ad,0x00b7,0x00c2,0x00ce,0x00da,0x00e7,0x00f5,0x0103,0x0113,0x0123,0x0135,0x0147,0x015a,0x016f,
  0x0185,0x019c,0x01b4,0x01ce,0x01ea,0x0207,0x0226,0x0247,0x0269,0x028e,0x02b5,0x02de,0x030a,0x0338,0x0369,0x039d,
  0x03d4,0x040e,0x044c,0x048d,0x04d2,0x051c,0x056a,0x05bc,0x0613,0x0670,0x06d2,0x0739,0x07a7,0x081c,0x0897,0x091a,
  0x09a5,0x0a37,0x0ad3,0x0b78,0x0c26,0x0cdf,0x0da3,0x0e73,0x0f4f,0x1038,0x112f,0x1234,0x1349,0x146f,0x15a6,0x16f0,
  0x184d,0x19bf,0x1b47,0x1ce6,0x1e9e,0x2070,0x225d,0x2469,0x2693,0x28de,0x2b4c,0x2ddf,0x3099,0x337d,0x368d,0x39cb,
  0x3d3b,0x40df,0x44bb,0x48d1,0x4d26,0x51bc,0x5698,0x5bbe,0x6133,0x66fb,0x6d1a,0x7397,0x7a77,0x81bf,0x8976,0x91a2,
};

static uint16_t bbd_ratev_rate=22050;

/* Init.
 */
 
void bbd_init(struct bbd *bbd,uint16_t rate) {
  if (rate<BBD_SONG_TEMPO) rate=BBD_SONG_TEMPO;
  bbd->rate=rate;
  bbd->framespertick=rate/BBD_SONG_TEMPO;
  bbd->songrepeat=1;
  
  if (rate!=bbd_ratev_rate) {
    uint16_t *v=bbd_ratev;
    uint8_t i=128;
    for (;i-->0;v++) {
      *v=(((uint32_t)(*v))*rate)/bbd_ratev_rate;
    }
    bbd_ratev_rate=rate;
  }
}

/* Silence.
 */

void bbd_silence(struct bbd *bbd) {
  bbd->voicec=0;
}

/* Begin PCM playback.
 */
 
void bbd_pcm(struct bbd *bbd,const int16_t *v,uint16_t c) {
  if (!v||!c) return;
  
  struct bbd_pcm *pcm;
  while (bbd->pcmc&&!bbd->pcmv[bbd->pcmc-1].c) bbd->pcmc--;
  if (bbd->pcmc<BBD_PCM_LIMIT) pcm=bbd->pcmv+bbd->pcmc++;
  else {
    struct bbd_pcm *q=bbd->pcmv;
    uint8_t i=bbd->pcmc;
    for (;i-->0;q++) {
      if (!q->c) {
        pcm=q;
        break;
      } else if (!pcm||(q->c<pcm->c)) {
        pcm=q;
      }
    }
  }
  
  pcm->v=v;
  pcm->c=c;
}

/* Begin note.
 */
 
struct bbd_voice *bbd_note(struct bbd *bbd,uint8_t noteid,uint8_t velocity,uint8_t ttl) {
  
  struct bbd_voice *voice=0;
  while (bbd->voicec&&!bbd->voicev[bbd->voicec-1].ttl) bbd->voicec--;
  if (bbd->voicec<BBD_VOICE_LIMIT) voice=bbd->voicev+bbd->voicec++;
  else {
    struct bbd_voice *q=bbd->voicev;
    uint8_t i=bbd->voicec;
    for (;i-->0;q++) {
      if (!q->ttl) {
        voice=q;
        break;
      } else if (!voice||(q->ttl<voice->ttl)) {
        voice=q;
      }
    }
  }
  
  voice->p=0;
  voice->pd=bbd_ratev[noteid&0x7f];
  voice->level=0x0100+((velocity&0x3f)<<7);
  voice->wave=velocity>>6;
  voice->ttl=(ttl+1)*bbd->framespertick;
  return voice;
}

/* Update song.
 */
 
static void bbd_update_song(struct bbd *bbd) {
  while (bbd->song) {
  
    if (bbd->songdelay) {
      bbd->songdelay--;
      return;
    }
    
    if (bbd->songp>=bbd->songc) {
      if (bbd->songrepeat) {
        bbd->songp=0;
      } else {
        bbd_play_song(bbd,0,0);
      }
      return;
    }
    
    uint8_t lead=bbd->song[bbd->songp++];
    
    if (!(lead&0x80)) {
      bbd->songdelay=lead*bbd->framespertick;
      return;
    }
    
    if (bbd->songp>bbd->songc-2) {
      bbd_play_song(bbd,0,0);
      return;
    }
    
    uint8_t velocity=bbd->song[bbd->songp++];
    uint8_t ttl=bbd->song[bbd->songp++];
    bbd_note(bbd,lead&0x7f,velocity,ttl);
  }
}

/* Update.
 */

int16_t bbd_update(struct bbd *bbd) {
  bbd_update_song(bbd);
  int32_t dst=0;
  
  struct bbd_voice *voice=bbd->voicev;
  uint8_t i=bbd->voicec;
  for (;i-->0;voice++) {
    if (!voice->ttl) continue;
    voice->ttl--;
    int32_t level=voice->level;
    uint32_t limit=voice->ttl|(voice->ttl<<1);
    if (level>limit) level=limit;
    
    // Wave provided by client, or square by default.
    const int16_t *wave=bbd->wavev[voice->wave];
    if (wave) dst+=(wave[voice->p>>7]*level)>>15;
    else if (voice->p&0x8000) dst-=level;
    else dst+=level;
    
    voice->p+=voice->pd;
  }
  
  struct bbd_pcm *pcm=bbd->pcmv;
  for (i=bbd->pcmc;i-->0;pcm++) {
    if (!pcm->c) continue;
    dst+=*pcm->v;
    pcm->v++;
    pcm->c--;
  }
  
  if (dst<-32768) return -32768;
  if (dst>32767) return 32767;
  return dst;
}

/* Change song.
 */

void bbd_play_song(struct bbd *bbd,const void *src,uint16_t srcc) {
  bbd->song=src;
  bbd->songp=0;
  bbd->songc=srcc;
  bbd->songdelay=0;
}
