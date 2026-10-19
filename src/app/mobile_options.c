#include "mobile_options.h"
#include <stdio.h>
#include <string.h>
void moon_mobile_options_load(const moon_options *store, moon_mobile_options *out, int *menus)
{
    int enhanced = strcmp(store->preset, "Classic") != 0;
    *menus = moon_options_int(store, "controls.touch_menu", enhanced, 0, 1);
    *out = (moon_mobile_options){moon_options_int(store, "controls.touch_race", enhanced, 0, 1),
                                 moon_options_int(store, "controls.touch_size", 1, 0, 2),
                                 moon_options_int(store, "controls.touch_opacity", 2, 0, 4),
                                 moon_options_int(store, "controls.touch_layout", 0, 0, 1),
                                 moon_options_int(store, "controls.touch_steering", 0, 0, 1),
                                 moon_options_int(store, "graphics.android_render_scale", 0, 0, 3)};
}
void moon_mobile_options_save(moon_options *store, const moon_mobile_options *v, int menus)
{
    const char *keys[] = {"controls.touch_menu",   "controls.touch_race",
                          "controls.touch_size",   "controls.touch_opacity",
                          "controls.touch_layout", "controls.touch_steering",
                          "graphics.android_render_scale"};
    int values[] = {menus, v->enabled, v->size, v->opacity, v->layout, v->steering,
                    v->render_scale};
    for (int i = 0; i < 7; i++) {
        char text[16];
        snprintf(text, sizeof text, "%d", values[i]);
        moon_options_set(store, keys[i], text);
    }
}
void moon_mobile_options_preset(moon_options *store, int enhanced)
{
    moon_mobile_options v = {enhanced != 0, 1, 2, 0, 0, 0};
    moon_mobile_options_save(store, &v, enhanced != 0);
}
