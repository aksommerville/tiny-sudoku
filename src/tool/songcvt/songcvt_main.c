#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include "tool/common/tool_context.h"
#include "tool/common/bb_midi.h"
#include "common/bbd.h"

/* We ask the MIDI file reader for an output rate so many times greater than BBD's fixed tick rate.
 * This smooths out rounding errors in tempo.
 * Composers are still strongly advised to keep the MIDI files synced to 1/48 s.
 * Divide it away before encoding delays.
 */
#define TEMPO_OVERSAMPLE 256

/* Converter context (not overriding tool_context).
 */
 
struct songcvt_ctx {
  struct bb_midi_file_reader *reader;
  struct encoder dst;
  const char *srcpath;
  int delay; // frames
  int time; // Total elapsed time in frames, for delays actually emitted.
  int lastdelayp; // Position in (dst) just after the last non-delay event.
  int lastdelaytime; // Time at lastdelayp
  
  // At Note On, we emit an incomplete event and record it here for later completion.
  struct note {
    int dstp;
    int time; // Note On time in frames
    uint8_t chid;
    uint8_t noteid;
    int level; // Estimate of level at runtime based on velocity.
  } *notev;
  int notec,notea;
  
  // Map of MIDI channels to wave IDs (0..15=>0..3)
  uint8_t waveidv[16];
};

static void songcvt_ctx_cleanup(struct songcvt_ctx *ctx) {
  bb_midi_file_reader_del(ctx->reader);
  if (ctx->notev) free(ctx->notev);
}

/* Acquire the MIDI file reader.
 */
 
static int songcvt_ctx_begin(struct songcvt_ctx *ctx,const void *src,int srcc) {
  struct bb_midi_file *file=bb_midi_file_new(src,srcc);
  if (!file) {
    fprintf(stderr,"%s: Failed to decode MIDI file.\n",ctx->srcpath);
    return -1;
  }
  int rate=BBD_SONG_TEMPO*TEMPO_OVERSAMPLE;
  struct bb_midi_file_reader *reader=bb_midi_file_reader_new(file,rate);
  bb_midi_file_del(file);
  if (!reader) return -1;
  reader->repeat=0;
  ctx->reader=reader;
  ctx->dst.c=0;
  return 0;
}

/* Add a note to the registry.
 * Caller must add its placeholder to the output immediately after this.
 */
 
static int songcvt_add_note(struct songcvt_ctx *ctx,uint8_t chid,uint8_t noteid,uint8_t velocity) {
  if (ctx->notec>=ctx->notea) {
    int na=ctx->notea+16;
    if (na>INT_MAX/sizeof(struct note)) return -1;
    void *nv=realloc(ctx->notev,sizeof(struct note)*na);
    if (!nv) return -1;
    ctx->notev=nv;
    ctx->notea=na;
  }
  struct note *note=ctx->notev+ctx->notec++;
  note->dstp=ctx->dst.c;
  note->time=ctx->time;
  note->chid=chid;
  note->noteid=noteid;
  note->level=0x0100+((velocity&0x7f)<<6); // from bbd.c:bbd_note() but 7-bit rather than 6
  return 0;
}

/* Emit any pending delay.
 * Do this before emitting any event.
 */
 
static int songcvt_flush_delay(struct songcvt_ctx *ctx) {
  int tickc=ctx->delay/TEMPO_OVERSAMPLE;
  while (tickc>=0x7f) {
    if (encode_intbe(&ctx->dst,0x7f,1)<0) return -1;
    tickc-=0x7f;
    ctx->delay-=0x7f*TEMPO_OVERSAMPLE;
    ctx->time+=0x7f*TEMPO_OVERSAMPLE;
  }
  if (tickc>0) {
    if (encode_intbe(&ctx->dst,tickc,1)<0) return -1;
    ctx->delay-=tickc*TEMPO_OVERSAMPLE;
    ctx->time+=tickc*TEMPO_OVERSAMPLE;
  }
  if (ctx->delay<0) return -1; // shouldn't be possible
  return 0;
}

/* Note On.
 */
 
static int songcvt_note_on(struct songcvt_ctx *ctx,const struct bb_midi_event *event) {
  if (songcvt_flush_delay(ctx)<0) return -1;
  if (songcvt_add_note(ctx,event->chid,event->a,event->b)<0) return -1;
  uint8_t tmp[]={
    0x80|event->a, // 0x80=eventid, 0x7f=noteid
    (ctx->waveidv[event->chid]<<6)|(event->b>>1), // waveid and velocity (7 bits down to 6)
    0, // ttl, will fill in later
  };
  if (encode_raw(&ctx->dst,tmp,sizeof(tmp))<0) return -1;
  ctx->lastdelayp=ctx->dst.c;
  ctx->lastdelaytime=ctx->time;
  return 0;
}

/* Note Off.
 */
 
static int songcvt_note_off(struct songcvt_ctx *ctx,const struct bb_midi_event *event) {
  // Effective current time is elapsed plus pending delay.
  // We don't flush the delay because we have no technical need to.
  int now=ctx->time+ctx->delay;
  // There can be multiple instances of a Note in flight at once. (because our Note On doesn't check)
  // That's outside MIDI's spec, and doesn't really make sense.
  // If it happens, a single Note Off stops all instances of that note.
  int i=ctx->notec;
  struct note *note=ctx->notev+i-1;
  for (;i-->0;note--) {
    if ((note->chid==event->chid)&&(note->noteid==event->a)) {
    
      int ttlframes=now-note->time;
      int ttlticks=(ttlframes+TEMPO_OVERSAMPLE/2)/TEMPO_OVERSAMPLE;
      
      // Extend duration based on an estimate of the release length.
      // When playing live (eg from qtractor), release *begins* at Note Off, but once encoded it *ends* there.
      // Luckily, BBD is not complicated, we can calculate the exact release length if we can assume 22050 Hz output.
      // Or at a minimum, we do the same thing here as in ae_event.c:audioedit_stop_voice().
      // So it should come out the same as you heard.
      int add22k=note->level>>1;
      int addframec=(add22k*BBD_SONG_TEMPO*TEMPO_OVERSAMPLE)/22050;
      int addtickc=(addframec+TEMPO_OVERSAMPLE/2)/TEMPO_OVERSAMPLE;
      if (addtickc>0) {
        ttlticks+=addtickc;
      }
      
      if (ttlticks>0xff) {
        fprintf(stderr,
          "%s:WARNING: Long note 0x%02x on channel 0x%x, reducing duration from %d ticks to 255.\n",
          ctx->srcpath,note->noteid,note->chid,ttlticks
        );
        ttlticks=0xff;
      }
      ctx->dst.v[note->dstp+2]=ttlticks;
      
      if (now>ctx->lastdelaytime) ctx->lastdelaytime=now;
      
      ctx->notec--;
      memmove(note,note+1,sizeof(struct note)*(ctx->notec-i));
    }
  }
  return 0;
}

/* Receive event.
 */
 
static int songcvt_event(struct songcvt_ctx *ctx,const struct bb_midi_event *event) {
  //fprintf(stderr,"%s: %02x %02x %02x %02x\n",ctx->srcpath,event->opcode,event->chid,event->a,event->b);
  switch (event->opcode) {
    case BB_MIDI_OPCODE_NOTE_ON: return songcvt_note_on(ctx,event);
    case BB_MIDI_OPCODE_NOTE_OFF: return songcvt_note_off(ctx,event);
    case BB_MIDI_OPCODE_PROGRAM: ctx->waveidv[event->chid&0x0f]=event->a&3; return 0;
    // bb_midi_file_reader handles tempo for us
    // Anything else we might need? Volume?
  }
  return 0;
}

/* Receive delay.
 */
 
static int songcvt_delay(struct songcvt_ctx *ctx,int framec) {
  ctx->delay+=framec;
  return 0;
}

/* Pop an event from the reader and emit whatever is ready.
 * Returns >0 if still in progress, 0 if finished, <0 for real errors.
 */
 
static int songcvt_update(struct songcvt_ctx *ctx) {
  struct bb_midi_event event={0};
  int framec=bb_midi_file_reader_update(&event,ctx->reader);
  if (framec<0) {
    //TODO bb_midi_file_reader doesn't distinguish encoding errors from end-of-song. Can we?
    return 0;
  }
  if (framec>0) {
    if (bb_midi_file_reader_advance(ctx->reader,framec)<0) return -1;
    if (songcvt_delay(ctx,framec)<0) return -1;
    return 1;
  }
  if (songcvt_event(ctx,&event)<0) return -1;
  return 1;
}

/* After conversion has completed, one last chance to flush caches, validate output, etc.
 */
 
static int songcvt_finish(struct songcvt_ctx *ctx) {
  if (songcvt_flush_delay(ctx)<0) return -1;
  
  const int warn_about_trimming=0;
  
  // Trim leading delay.
  int leadc=0,leadtickc=0;
  while ((leadc<ctx->dst.c)&&!(ctx->dst.v[leadc]&0x80)) {
    leadtickc+=ctx->dst.v[leadc];
    leadc++;
  }
  if (leadc) {
    int sec=(leadtickc+BBD_SONG_TEMPO/2)/BBD_SONG_TEMPO;
    int framec=leadtickc*TEMPO_OVERSAMPLE;
    if (warn_about_trimming) fprintf(stderr,
      "%s:WARNING: Removing %d bytes (%d ticks, ~%d s) of leading delay.\n",
      ctx->srcpath,leadc,leadtickc,sec
    );
    ctx->dst.c-=leadc;
    memmove(ctx->dst.v,ctx->dst.v+leadc,ctx->dst.c);
    ctx->time-=framec;
    ctx->lastdelayp-=leadc;
    ctx->lastdelaytime-=framec;
    struct note *note=ctx->notev;
    int i=ctx->notec;
    for (;i-->0;note++) note->time-=framec;
  }
  
  // Trim trailing delay if excessive.
  // Logic Pro X does this and I can't figure out why. (I did file a bug with Apple years ago, guess they ignored it)
  int trailframec=ctx->time-ctx->lastdelaytime;
  int trailtickc=trailframec/TEMPO_OVERSAMPLE; // might not match encoded ticks exactly, not a big deal
  if (trailtickc>BBD_SONG_TEMPO*4) { // Arbitrary choice: 4 seconds trailing silence is too much.
    int trailc=ctx->dst.c-ctx->lastdelayp;
    int sec=(trailtickc+BBD_SONG_TEMPO/2)/BBD_SONG_TEMPO;
    if (warn_about_trimming) fprintf(stderr,
      "%s:WARNING: Removing %d bytes (%d ticks, ~%d s) of trailing delay.\n",
      ctx->srcpath,trailc,trailtickc,sec
    );
    ctx->dst.c-=trailc;
    ctx->time-=trailframec;
  }
  
  // Any notes still pending, terminate them and issue a warning.
  struct note *note=ctx->notev;
  int i=ctx->notec;
  for (;i-->0;note++) {
    fprintf(stderr,
      "%s:WARNING: Note 0x%02x on channel 0x%x was never released.\n",
      ctx->srcpath,note->noteid,note->chid
    );
    int ttl=(ctx->time-note->time)/TEMPO_OVERSAMPLE;
    if (ttl>0xff) ttl=0xff;
    ctx->dst.v[note->dstp+2]=ttl;
  }
  
  return 0;
}

/* Convert song in memory.
 */
 
static int songcvt(struct tool_context *tctx) {
  struct songcvt_ctx ctx={
    .srcpath=tctx->srcpath,
    .waveidv={0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3},
  };
  
  if (songcvt_ctx_begin(&ctx,tctx->src,tctx->srcc)<0) return -1;
  
  while (1) {
    int err=songcvt_update(&ctx);
    if (err<0) {
      fprintf(stderr,"%s: Error during conversion.\n",ctx.srcpath);
      songcvt_ctx_cleanup(&ctx);
      return -1;
    }
    if (!err) break;
  }
  
  if (songcvt_finish(&ctx)<0) {
    fprintf(stderr,"%s: Error finishing conversion.\n",ctx.srcpath);
    songcvt_ctx_cleanup(&ctx);
    return -1;
  }

  if (tool_context_encode_text(tctx,ctx.dst.v,ctx.dst.c,8)<0) {
    songcvt_ctx_cleanup(&ctx);
    return -1;
  }
  
  songcvt_ctx_cleanup(&ctx);
  return 0;
}

/* Main.
 */
 
int main(int argc,char **argv) {
  struct tool_context ctx={0};
  if (tool_context_configure(&ctx,argc,argv,0)<0) return 1;
  if (tool_context_acquire_input(&ctx)<0) return 1;
  if (songcvt(&ctx)<0) return 1;
  if (tool_context_flush_output(&ctx)<0) return 1;
  return 0;
}
