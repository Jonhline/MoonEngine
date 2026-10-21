#ifndef MOON_PHOTO_PAUSE_H
#define MOON_PHOTO_PAUSE_H
#include "photo_mode.h"
#include "strobe_player.h"
typedef struct photo_pause photo_pause;
photo_pause *photo_pause_open(gt4_vol *, gt4_photo_mode *, const float apertures[17],
                              const int32_t shutters[19]);
void photo_pause_close(photo_pause *);
/* 003C5AB8: authored photograph rectangle in the 640x480 editor. */
void photo_pause_viewport(int portrait, int wide, float box[4]);
void photo_pause_draw(photo_pause *, double dt, int wide);
void photo_pause_help(photo_pause *, int tool);
void photo_pause_focus(photo_pause *);
int photo_pause_value(photo_pause *, const char *, strb_value *);
#endif
