#include "data.h"
#include "render.h"

/* Get resource by id.
 */
 
uint32_t data_sound_by_id(const int16_t **dst,uint8_t id) {
  switch (id) {
    #define _(id,name) case id: *dst=name; return name##_len;
    _(1,move)
    _(2,palette)
    _(3,placeok)
    _(4,error)
    _(5,cancel)
    #undef _
  }
  return 0;
}

struct render_image *data_image_by_id(uint8_t id) {
  switch (id) {
    #define _(id,name) case id: return &name;
    _(1,tiles)
    _(2,splash)
    _(3,digits4x7)
    #undef _
  }
  return 0;
}

uint32_t data_song_by_id(const uint8_t **dst,uint8_t id) {
  switch (id) {
    #define _(id,name) case id: *dst=name; return name##_len;
    #undef _
  }
  return 0;
}
