/* data.h
 * Structured access to embedded data.
 * This is where named data objects acquire an ID for simpler reference.
 */
 
#ifndef DATA_H
#define DATA_H

#include <stdint.h>

struct render_image;

// Sound effects.
extern const int16_t move[];
extern const uint32_t move_len;
extern const int16_t palette[];
extern const uint32_t palette_len;
extern const int16_t placeok[];
extern const uint32_t placeok_len;
extern const int16_t error[];
extern const uint32_t error_len;
extern const int16_t cancel[];
extern const uint32_t cancel_len;

// Images.
extern struct render_image tiles;
extern struct render_image splash;
extern struct render_image digits4x7;

// Waves.
extern const int16_t wave0[];
extern const int16_t wave1[];
extern const int16_t wave2[];
extern const int16_t wave3[];

uint32_t data_sound_by_id(const int16_t **dst,uint8_t id);
struct render_image *data_image_by_id(uint8_t id);
uint32_t data_song_by_id(const uint8_t **dst,uint8_t id);

#endif
