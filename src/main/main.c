#include "platform.h"
#include "bbd.h"
#include "render.h"
#include "game.h"
#include "data.h"
#include <string.h>

#if BC_PLATFORM==BC_PLATFORM_tiny
  #include <avr/pgmspace.h>
#else
  #define PROGMEM
#endif

/* Globals.
 ****************************************************/

static uint16_t fb[96*64];
struct bbd bbd={0};
static uint8_t pvinput=0;

static struct render_image fbimg={
  .v=fb,
  .w=96,
  .h=64,
  .stride=96,
  .pixelsize=16,
};

/* Main.
 **********************************************************/

int16_t audio_next() {
  return bbd_update(&bbd);
}

static void redraw() {
  switch (game.state) {
    case GAME_STATE_INIT: render_blit(&fbimg,0,0,&splash,0,0,96,64,0); break;
    case GAME_STATE_PLAY: game_draw(&fbimg); break;
    case GAME_STATE_DONE: game_draw(&fbimg); break;
    default: game.state=GAME_STATE_INIT;
  }
}

void loop() {
  uint8_t input=platform_update();
  if (input!=pvinput) {
    pvinput=input;
  }
  
  game_update(input);
  
  redraw();
  platform_send_framebuffer(fb);
}

void setup() {
  bbd_init(&bbd,22050);
  platform_init();
}
