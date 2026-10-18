/* Launcher API and Screen Settings share persistence, including quality exclusions. */
#include "video_settings.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "launcher quality line %d: %s\n", __LINE__, #x);                       \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static int setting(const char *label)
{
    for (int i = 0; i < video_setting_count(); i++)
        if (!strcmp(video_setting(i)->label, label))
            return i;
    return -1;
}

int main(int argc, char **argv)
{
    CHECK(argc >= 2);
    video_settings s, loaded;
    int aa = setting("Anti-Aliasing"), scale = setting("3D Resolution"),
        smooth = setting("Texture Smoothing"), api = setting("Graphics API"),
        fsr = setting("Upscaling"), preset = setting("PC Preset");
    CHECK(aa >= 0 && scale >= 0 && smooth >= 0 && api >= 0 && fsr >= 0 && preset >= 0);
    CHECK(!strcmp(video_setting(aa)->group, "Quality"));
    for (int backend = 0; backend < 2; backend++) {
        video_settings_init(&s, 1, argv[1]);
        video_setting_set(&s, api, backend);
        video_setting_set(&s, fsr, 3);
        video_setting_set(&s, aa, 3);
        CHECK(s.current.video.upscale == 0 && s.current.render_scale == 2);
        video_setting_set(&s, smooth, 1);
        video_settings_init(&loaded, 1, argv[1]);
        CHECK(video_setting_get(&loaded, api) == backend);
        CHECK(video_setting_get(&loaded, aa) == (argc > 2 ? 0 : 3));
        CHECK(video_setting_get(&loaded, smooth) == 1);
        CHECK(loaded.store.modified);
        video_setting_set(&s, scale, 0);
        CHECK(s.current.antialiasing == 0 && s.current.video.upscale == 0);
        video_settings_init(&loaded, 1, argv[1]);
        CHECK(video_setting_get(&loaded, scale) == 0);
        video_setting_set(&s, aa, 2);
        video_setting_set(&s, fsr, 2);
        CHECK(s.current.antialiasing == 0 && s.current.render_scale == 2);
        video_setting_set(&s, preset, 0);
        CHECK(video_setting_get(&s, aa) == 0 && video_setting_get(&s, scale) == 2);
        CHECK(video_setting_get(&s, smooth) == 0);
        video_setting_set(&s, preset, 1);
        CHECK(video_setting_get(&s, aa) == 2 && video_setting_get(&s, smooth) == 1);
        CHECK(video_setting_get(&s, api) == backend);
    }
    remove(argv[1]);
    puts("Launcher quality shares the game store, exclusions and presets on both APIs");
    return 0;
}
