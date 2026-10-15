#include "screen_setting.h"
#include "video_gl.h"
#include "license_image.h"
#include <string.h>
void screen_setting_draw(gt4_screen_setting *setting, const int options[4])
{
    /* Original: func_0033F2E8 +0x458..0x49C: correct the scene before
       display overlays. func_003EFD08 skips the identity transform. */
    if (!(options[0] || options[1] || options[2] || options[3]))
        return;
    if (!setting->valid || memcmp(setting->options, options, sizeof setting->options)) {
        float parameters[6];
        gt4_license_image_options(options, parameters);
        gt4_license_image_lut(parameters, setting->lut);
        gt4_license_image_mix(parameters[5], setting->mix);
        memcpy(setting->options, options, sizeof setting->options);
        setting->valid = 1;
    }
    setting->capture = video_grab(setting->capture, 0, 0, 640, 448);
    video_license_image(setting->capture, 0, 0, 640, 448, setting->lut, setting->mix);
}
void screen_setting_close(gt4_screen_setting *setting)
{
    plat_texture_destroy(setting->capture);
    memset(setting, 0, sizeof *setting);
}
