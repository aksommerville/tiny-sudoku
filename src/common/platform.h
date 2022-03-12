/* platform.h
 * Common API that we can use on Native and Tiny targets alike.
 */
 
#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>

#define BC_PLATFORM_tiny      1
#define BC_PLATFORM_linux     2
#define BC_PLATFORM_macos     3
#define BC_PLATFORM_mswin     4

#ifndef BC_PLATFORM
  #define BC_PLATFORM BC_PLATFORM_tiny
#endif

#if BC_PLATFORM==BC_PLATFORM_tiny
  #define bc_log(fmt,...)
#else
  #include <stdio.h>
  #define bc_log(fmt,...) fprintf(stderr,fmt"\n",##__VA_ARGS__)
#endif

#ifdef __cplusplus
  extern "C" {
#endif

/* Implemented by shared main unit.
 **********************************************************************/
 
void setup();
void loop();
int16_t audio_next();

/* Implemented by platform-specific unit.
 *********************************************************************/

/* Nonzero on success.
 * Should be the first thing you do in setup().
 */
uint8_t platform_init();

/* Returns bitfields of the current input state.
 * Call at the beginning of loop().
 */
uint8_t platform_update();

#define BUTTON_UP      0x01
#define BUTTON_DOWN    0x02
#define BUTTON_LEFT    0x04
#define BUTTON_RIGHT   0x08
#define BUTTON_A       0x10
#define BUTTON_B       0x20
#define BUTTON_RSVD1   0x40
#define BUTTON_EXTRA   0x80 /* indicates more data is available (TODO how to retrieve?) */

/* Deliver a framebuffer, typically the last thing you do in loop().
 * (fb) is 96x64 16-bit pixels.
 */
void platform_send_framebuffer(const void *fb);

uint32_t millis();
uint32_t micros();

/* Implemented generically.
 *******************************************************************/
 
extern const uint8_t tiny_ctab8[768];

#ifdef __cplusplus
  }
#endif

#endif
