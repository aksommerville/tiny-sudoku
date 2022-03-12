#include "platform.h"
#include "game.h"
#include "render.h"
#include "data.h"
#include "bbd.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

extern struct bbd bbd;

struct game game={0};

/* Reset.
 */
 
void game_reset() {
  memset(&game,0,sizeof(struct game));
  game.state=GAME_STATE_PLAY;
  game.selzone=SELZONE_FIELD;
  game.fselx=4;
  game.fsely=4;
  sudoku_generate(game.field);
  game.startms=millis();
}

/* Get time since start, in [h,m,s]
 */
 
static uint16_t game_get_time(uint8_t *dst,uint32_t startms) {
  uint32_t endms=millis();
  uint32_t ms=endms-startms;
  uint32_t s=ms/1000;
  uint32_t m=s/60;
  uint32_t h=m/60;
  dst[0]=h;
  dst[1]=m%60;
  dst[2]=s%60;
  return ms%1000;
}

/* Draw one cell.
 */
 
static void game_draw_tile_opaque(
  void *dst,int dststride,uint8_t tileid
) {
  int16_t *src=tiles.v;
  src+=(tileid>>4)*tiles.stride*TILESIZE+(tileid&0x0f)*TILESIZE;
  render_blit_16_16_opaque_unchecked(
    dst,1,dststride-TILESIZE,
    src,tiles.stride,
    TILESIZE,TILESIZE
  );
}
 
static void game_draw_tile_colorkey(
  void *dst,int dststride,uint8_t tileid
) {
  int16_t *src=tiles.v;
  src+=(tileid>>4)*tiles.stride*TILESIZE+(tileid&0x0f)*TILESIZE;
  render_blit_16_16_colorkey_unchecked(
    dst,1,dststride-TILESIZE,
    src,tiles.stride,
    TILESIZE,TILESIZE
  );
}
 
static void game_draw_cell(
  void *dst,int dststride,
  uint8_t col,uint8_t row,
  uint16_t cell
) {
  uint8_t digit=cell&FIELD_CELL_LABEL;
  uint8_t bgtileid=0x60;
  if (game.state==GAME_STATE_DONE) {
    bgtileid+=4;
  } else if ((col==game.fselx)&&(row==game.fsely)) {
    if (game.selzone==SELZONE_FIELD) {
      if (game.renderseq&0x10) bgtileid+=2;
      else bgtileid+=4;
    } else {
      bgtileid+=6;
    }
  } else if (cell&FIELD_CELL_ERROR) {
    bgtileid+=10;
  } else if (cell&FIELD_CELL_PROVIDED) {
    bgtileid+=8;
  }
  if (col%3==2) bgtileid+=0x01;
  if (row%3==2) bgtileid+=0x10;
  game_draw_tile_opaque(dst,dststride,bgtileid);
  
  if ((digit>=1)&&(digit<=9)) {
    game_draw_tile_colorkey(dst,dststride,0x80+digit);
  }
}

/* Draw.
 */
 
void game_draw(struct render_image *dst) {
  memset(dst->v,0x00,dst->w*dst->h*2);
  
  const int fieldw=9*TILESIZE;
  const int fieldh=9*TILESIZE;
  int fieldx=1;
  int fieldy=1;
  int16_t *dstrow=dst->v;
  dstrow+=fieldy*dst->stride+fieldx;
  const int majorstride=dst->stride*TILESIZE;
  const int minorstride=TILESIZE;
  int row=0;
  const uint16_t *fieldp=game.field;
  for (;row<9;row++,dstrow+=majorstride) {
    int16_t *dstp=dstrow;
    int col=0;
    for (;col<9;col++,dstp+=minorstride,fieldp++) {
      game_draw_cell(dstp,dst->stride,col,row,*fieldp);
    }
  }
  
  // For symmetry's sake, copy the bottom row to just above the top, then right to left.
  memcpy(
    ((uint16_t*)dst->v)+dst->stride*(fieldy-1)+fieldx,
    ((uint16_t*)dst->v)+dst->stride*(fieldy+fieldh-1)+fieldx,
    fieldw*2
  );
  render_blit_16_16_opaque_unchecked(
    ((uint16_t*)dst->v)+dst->stride*(fieldy-1)+fieldx-1,
    0,dst->stride,
    ((uint16_t*)dst->v)+dst->stride*(fieldy-1)+fieldx+fieldw-1,
    dst->stride,
    1,fieldh+1
  );
  
  // Palette.
  if (game.state==GAME_STATE_PLAY) {
    int palw=3*TILESIZE;
    int palh=4*TILESIZE;
    int palx=70;
    int paly=30;
    dstrow=((uint16_t*)dst->v)+paly*dst->stride+palx;
    int8_t digit=-2;
    int8_t ddigit=(game.selzone==SELZONE_PALETTE)?1:0;
    for (row=0;row<4;row++,dstrow+=majorstride) {
      int16_t *dstp=dstrow;
      int col=0;
      for (;col<3;col++,dstp+=minorstride,digit+=ddigit) {
        uint8_t bgtileid=0x60;
        if ((game.selzone==SELZONE_PALETTE)&&(col==game.pselx)&&(row==game.psely)) {
          if (game.renderseq&0x10) bgtileid+=2;
          else bgtileid+=4;
        }
        game_draw_tile_opaque(dstp,dst->stride,bgtileid);
        if (digit>=1) {
          game_draw_tile_colorkey(dstp,dst->stride,0x80+digit);
        }
      }
    }
    memcpy(
      ((uint16_t*)dst->v)+dst->stride*(paly-1)+palx,
      ((uint16_t*)dst->v)+dst->stride*(paly+palh-1)+palx,
      palw*2
    );
    render_blit_16_16_opaque_unchecked(
      ((uint16_t*)dst->v)+dst->stride*(paly-1)+palx-1,
      0,dst->stride,
      ((uint16_t*)dst->v)+dst->stride*(paly-1)+palx+palw-1,
      dst->stride,
      1,palh+1
    );
  }
  
  // Clock.
  {
    uint8_t showcolon=1;
    if (game.state==GAME_STATE_PLAY) showcolon=(game_get_time(game.time,game.startms)<500);
    const uint8_t *src=digits4x7.v;
    uint16_t *dstp=((uint16_t*)dst->v)+dst->stride*1+66;
    uint16_t color=0xffff;
    #define DIGIT(n) { \
      render_blit_16_1_replace_unchecked( \
        dstp,1,dst->stride-4, \
        src+(((n)*4)>>3), \
        0x80>>(((n)*4)&7), \
        digits4x7.stride, \
        4,7,0,color \
      ); \
      dstp+=5; \
    }
    #define COLON { \
      if (showcolon) render_blit_16_1_replace_unchecked( \
        dstp,1,dst->stride-1, \
        src+(40>>3), \
        0x80>>(40&7), \
        digits4x7.stride, \
        1,7,0,color \
      ); \
      dstp+=2; \
    }
    DIGIT(game.time[0]%10)
    COLON
    DIGIT(game.time[1]/10)
    DIGIT(game.time[1]%10)
    COLON
    DIGIT(game.time[2]/10)
    DIGIT(game.time[2]%10)
    #undef DIGIT
    #undef COLON
  }
  
  game.renderseq++;
}

/* Enter DONE state.
 */
 
static void game_finish() {
  game_get_time(game.time,game.startms);
  game.state=GAME_STATE_DONE;
}

/* Check one cell for errors (ie an exposed neighbor shows the same value).
 */
 
static uint8_t game_is_error(uint8_t col,uint8_t row,uint8_t digit) {
  uint8_t i=0; for (;i<9;i++) {
    if (i!=col) {
      uint16_t neighbor=game.field[row*9+i];
      if ((neighbor&15)==digit) {
        game.field[row*9+i]|=FIELD_CELL_ERROR;
        return 1;
      }
    }
    if (i!=row) {
      uint16_t neighbor=game.field[i*9+col];
      if ((neighbor&15)==digit) {
        game.field[i*9+col]|=FIELD_CELL_ERROR;
        return 1;
      }
    }
  }
  uint8_t cola=(col/3)*3,rowa=(row/3)*3;
  uint8_t colz=cola+2,rowz=rowa+2;
  uint8_t qrow=rowa; for (;qrow<=rowz;qrow++) {
    uint8_t qcol=cola; for (;qcol<=colz;qcol++) {
      if ((qrow==row)&&(qcol==col)) continue;
      uint16_t neighbor=game.field[qrow*9+qcol];
      if ((neighbor&15)==digit) {
        game.field[qrow*9+qcol]|=FIELD_CELL_ERROR;
        return 1;
      }
    }
  }
  return 0;
}

/* Examine.
 * Check for errors and completion.
 * Called after a change.
 */
 
static void game_examine() {
  uint8_t complete=1;
  uint16_t *p=game.field;
  uint8_t row=0; for (;row<9;row++) {
    uint8_t col=0; for (;col<9;col++,p++) {
      uint8_t digit=(*p)&FIELD_CELL_LABEL;
      if (!digit) {
        complete=0;
        (*p)&=~FIELD_CELL_ERROR;
        continue;
      }
      if (digit!=(((*p)>>4)&15)) {
        //fprintf(stderr,"INCORRECT at %d,%d shown=%d real=%d\n",col,row,digit,((*p)>>4)&15);
        complete=0;
      }
      if (game_is_error(col,row,digit)) {
        (*p)|=FIELD_CELL_ERROR;
      } else {
        (*p)&=~FIELD_CELL_ERROR;
      }
    }
  }
  if (complete) {
    game_finish();
  }
}

/* Move selection.
 */
 
static void game_move_selection(int8_t dx,int8_t dy) {
  if (game.state!=GAME_STATE_PLAY) return;
  bbd_pcm(&bbd,move,move_len);
  switch (game.selzone) {
    case SELZONE_FIELD: {
        game.fselx+=dx; if (game.fselx<0) game.fselx=8; else if (game.fselx>=9) game.fselx=0;
        game.fsely+=dy; if (game.fsely<0) game.fsely=8; else if (game.fsely>=9) game.fsely=0;
      } break;
    case SELZONE_PALETTE: {
        game.pselx+=dx; if (game.pselx<0) game.pselx=2; else if (game.pselx>=3) game.pselx=0;
        game.psely+=dy; if (game.psely<0) game.psely=3; else if (game.psely>=4) game.psely=0;
      } break;
    default: return;
  }
}

/* Select whatever's highlighted.
 */
 
static void game_activate() {
  switch (game.state) {
    case GAME_STATE_INIT: game_reset(); return;
    case GAME_STATE_PLAY: switch (game.selzone) {
        case SELZONE_FIELD: {
            uint16_t v=game.field[game.fsely*9+game.fselx];
            if (v&FIELD_CELL_PROVIDED) {
              bbd_pcm(&bbd,error,error_len);
            } else {
              bbd_pcm(&bbd,palette,palette_len);
              uint8_t digit=v&FIELD_CELL_LABEL;
              game.selzone=SELZONE_PALETTE;
              if ((digit>=1)&&(digit<=9)) {
                game.pselx=(digit-1)%3;
                game.psely=(digit-1)/3+1;
              } else {
                game.pselx=1;
                game.psely=0;
              }
            }
          } break;
        case SELZONE_PALETTE: {
            uint8_t v=(game.psely-1)*3+game.pselx+1;
            if (v>9) v=0;
            uint16_t *dst=game.field+game.fsely*9+game.fselx;
            (*dst)=((*dst)&~FIELD_CELL_LABEL)|v;
            game.selzone=SELZONE_FIELD;
            game_examine();
            if (game.field[game.fsely*9+game.fselx]&FIELD_CELL_ERROR) {
              bbd_pcm(&bbd,error,error_len);
            } else {
              bbd_pcm(&bbd,placeok,placeok_len);
            }
          } break;
      } break;
    case GAME_STATE_DONE: default: game.state=GAME_STATE_INIT; return;
  }
}

/* Cancel.
 */
 
static void game_cancel() {
  switch (game.selzone) {
    case SELZONE_PALETTE: {
        bbd_pcm(&bbd,cancel,cancel_len);
        game.selzone=SELZONE_FIELD;
      } break;
  }
}

/* Update game.
 */
 
void game_update(uint8_t input) {
  if (input!=game.pvinput) {
    switch (input&(BUTTON_LEFT|BUTTON_RIGHT)&~game.pvinput) {
      case BUTTON_LEFT: game_move_selection(-1,0); break;
      case BUTTON_RIGHT: game_move_selection(1,0); break;
    }
    switch (input&(BUTTON_UP|BUTTON_DOWN)&~game.pvinput) {
      case BUTTON_UP: game_move_selection(0,-1); break;
      case BUTTON_DOWN: game_move_selection(0,1); break;
    }
    if (input&BUTTON_A) {
      game_activate();
    }
    if (input&BUTTON_B) {
      game_cancel();
    }
    game.pvinput=input;
  }
}
