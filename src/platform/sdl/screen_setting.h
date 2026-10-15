#ifndef OPENGT4_SCREEN_SETTING_H
#define OPENGT4_SCREEN_SETTING_H
#include "platform/platform.h"
typedef struct {
    plat_texture *capture;
    int valid, options[4];
    uint8_t lut[256][3], mix[3][3];
} gt4_screen_setting;
void screen_setting_draw(gt4_screen_setting *setting, const int options[4]);
void screen_setting_close(gt4_screen_setting *setting);
#endif
