/* game.h
 * Global state of the game in progress.
 * It's a singleton, but invalid during the initial menu.
 */
 
#ifndef GAME_H
#define GAME_H

#include <stdint.h>

// 7*9==63, just small enough to fit on the Tiny.
#define TILESIZE 7

#define SELZONE_FIELD 0
#define SELZONE_PALETTE 1

#define FIELD_CELL_LABEL     0x000f /* mask for displayed digit; 0..9 only */
#define FIELD_CELL_VALUE     0x00f0 /* mask for true value; 1..9 only */
#define FIELD_CELL_VISIBLE   0x0100
#define FIELD_CELL_PROVIDED  0x0200
#define FIELD_CELL_ERROR     0x0400

#define GAME_STATE_INIT 0
#define GAME_STATE_PLAY 1
#define GAME_STATE_DONE 2

struct render_image;

extern struct game {
  uint8_t state;
  uint32_t startms;
  uint8_t time[3]; // [h,m,s], only valid in DONE state
  uint8_t selzone;
  int8_t fselx,fsely; // 0..8, selection in field
  int8_t pselx,psely; // 0..2, selection in palette
  uint8_t renderseq; // counts render frames for animation. overflows frequently
  uint8_t pvinput;
  uint16_t field[81]; // 0x000f=value(1..9), 0x0010=visible, 0x0020=provided, 0x0040=error
} game;

void game_reset();

void game_draw(struct render_image *dst);

void game_update(uint8_t input);

void sudoku_generate(uint16_t *v);

#endif
