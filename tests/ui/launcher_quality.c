/* Launcher API and Screen Settings share persistence, including quality exclusions. */
#include "video_settings.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define make_directory(p) _mkdir(p)
#else
#include <sys/stat.h>
#define make_directory(p) mkdir((p), 0755)   /* POSIX */
#endif

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
        CHECK(video_setting_get(&s, aa) == 4 && video_setting_get(&s, smooth) == 1);
        CHECK(video_setting_get(&s, api) == backend);
    }
    video_setting_set(&s,aa,4);
    video_settings_init(&loaded,1,argv[1]);
    CHECK(video_setting_get(&loaded,aa)==4);
    make_directory("build");   /* the preset path below is relative to the test working dir */
    CHECK(video_settings_preset_export(&s,"build/edge-aa.moonpreset","Edge",0));
    video_setting_set(&s,aa,1);
    CHECK(video_settings_preset_import(&s,"build/edge-aa.moonpreset",0));
    CHECK(s.current.antialiasing==4);
    remove("build/edge-aa.moonpreset");
    /* Personal files share the existing launcher/game draft and store. */
    char preset_path[1200]; snprintf(preset_path,sizeof preset_path,"%s.moonpreset",argv[1]);
    video_setting_set(&s, aa, 3);
    int host_width=s.current.video.width, host_api=s.current.renderer;
    CHECK(video_settings_preset_export(&s,preset_path,"Touring",0));
    CHECK(!s.store.modified && !strcmp(s.store.preset,"Touring"));
    video_setting_set(&s,preset,0);
    CHECK(!s.current.preset_tools && !s.personal_preset[0]);
    pc_video_options previous=s.current;
    s.draft=s.current;
    CHECK(video_settings_preset_import(&s,preset_path,1));
    CHECK(!memcmp(&previous,&s.current,sizeof previous));
    CHECK(s.draft.antialiasing==3 && s.draft.video.width==host_width && s.draft.renderer==host_api);
    CHECK(!strcmp(s.store.preset,"Classic")); /* Cancel requires no rollback. */
    CHECK(video_settings_preset_import(&s,preset_path,0));
    CHECK(s.current.antialiasing==3 && !s.store.modified);
    video_settings_init(&loaded,1,argv[1]);
    CHECK(!strcmp(loaded.store.preset,"Touring") && !loaded.store.modified);
    CHECK(video_setting_get(&loaded,preset)==2);
    video_setting_set(&s,smooth,!s.current.texture_smoothing);
    CHECK(s.store.modified && !strcmp(s.store.preset,"Touring"));
    remove(preset_path);
    int transfer=setting("Save Transfer"); CHECK(transfer>=0);
    video_setting_set(&s,preset,0); CHECK(!s.current.save_tools);
    video_setting_set(&s,transfer,1); CHECK(s.current.save_tools && s.store.modified);
    video_settings_init(&loaded,1,argv[1]); CHECK(loaded.current.save_tools);
    video_setting_set(&s,preset,1); CHECK(s.current.save_tools);
    int filtering = setting("Texture Filtering");
    CHECK(filtering >= 0);
    video_setting_set(&s, filtering, 4);
    video_settings_init(&loaded, 1, argv[1]);
    CHECK(video_setting_get(&loaded, filtering) == 4);
    video_setting_set(&s, preset, 0);
    CHECK(video_setting_get(&s, filtering) == 0);
    video_setting_set(&s, preset, 1);
    CHECK(video_setting_get(&s, filtering) == 3);
    int shadows = setting("Car Shadows");
    CHECK(shadows >= 0);
    video_setting_set(&s, shadows, 0);
    video_settings_init(&loaded, 1, argv[1]);
    CHECK(video_setting_get(&loaded, shadows) == 0);
    video_setting_set(&s, preset, 1);
    CHECK(video_setting_get(&s, shadows) == 1);
    video_setting_set(&s, preset, 0);
    CHECK(video_setting_get(&s, shadows) == 0);
    int materials = setting("Material Shading");
    CHECK(materials >= 0);
    video_setting_set(&s, materials, 1);
    video_settings_init(&loaded, 1, argv[1]);
    CHECK(video_setting_get(&loaded, materials) == 1);
    video_setting_set(&s, preset, 0);
    CHECK(video_setting_get(&s, materials) == 0);
    video_setting_set(&s, preset, 1);
    CHECK(video_setting_get(&s, materials) == 1);
    int reflections = setting("Reflection Quality");
    CHECK(reflections >= 0);
    video_setting_set(&s, reflections, 2);
    video_settings_init(&loaded, 1, argv[1]);
    CHECK(video_setting_get(&loaded, reflections) == 2);
    video_setting_set(&s, preset, 0);
    CHECK(video_setting_get(&s, reflections) == 0);
    video_setting_set(&s, preset, 1);
    CHECK(video_setting_get(&s, reflections) == 1);
    int maps = setting("Shadow Maps");
    CHECK(maps >= 0);
    video_setting_set(&s, maps, 3);
    video_settings_init(&loaded, 1, argv[1]);
    CHECK(video_setting_get(&loaded, maps) == 3);
    video_setting_set(&s, preset, 0);
    CHECK(video_setting_get(&s, maps) == 0);
    video_setting_set(&s, preset, 1);
    CHECK(video_setting_get(&s, maps) == 2);
    int lighting = setting("Lighting");
    CHECK(lighting >= 0);
    video_setting_set(&s, lighting, 1);
    video_settings_init(&loaded, 1, argv[1]);
    CHECK(video_setting_get(&loaded, lighting) == 1);
    video_setting_set(&s, preset, 0);
    CHECK(video_setting_get(&s, lighting) == 0);
    video_setting_set(&s, preset, 1);
    CHECK(video_setting_get(&s, lighting) == 1);
    const char *solar_labels[]={"Daylight","Solar Time","Daylight Cycle","HDR","Sun Rays","Ambient Occlusion"};
    const int chosen[]={1,9,2,1,2,1};
    const int classic[]={0,5,0,0,0,0};
    const int enhanced[]={1,5,1,1,2,1};
    for(int i=0;i<6;i++) {
        int id=setting(solar_labels[i]);CHECK(id>=0);
        video_setting_set(&s,id,chosen[i]);
        video_settings_init(&loaded,1,argv[1]);
        CHECK(video_setting_get(&loaded,id)==chosen[i]);
        video_setting_set(&s,preset,0);CHECK(video_setting_get(&s,id)==classic[i]);
        video_setting_set(&s,preset,1);CHECK(video_setting_get(&s,id)==enhanced[i]);
    }
    CHECK(moon_options_set(&s.store,"audio.custom_music","1"));
    video_setting_set(&s,preset,0);
    CHECK(!moon_options_int(&s.store,"audio.custom_music",1,0,1) && !s.store.modified);
    remove(argv[1]);
    puts("Launcher quality shares the game store, exclusions and presets on both APIs");
    return 0;
}
