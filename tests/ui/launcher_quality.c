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
    CHECK(setting("Part Preview Fades") == -1); /* Original GT Auto behaviour, no preference. */
    int aa = setting("Anti-Aliasing"), scale = setting("3D Resolution"),
        smooth = setting("Texture Smoothing"), api = setting("Graphics API"),
        fsr = setting("Upscaling"), preset = setting("PC Preset");
    int fov = setting("Dynamic FOV"), fov_deg = setting("FOV Amount"),
        cam_lag = setting("Camera Dynamics"), shake = setting("Camera Shake"),
        wind = setting("Wind Audio"), roar = setting("Road Roar Boost"),
        blur = setting("Radial Blur"), blur_amount = setting("Blur Strength"),
        vignette = setting("Vignette"), discord = setting("Discord Rich Presence");
    CHECK(aa >= 0 && scale >= 0 && smooth >= 0 && api >= 0 && fsr >= 0 && preset >= 0);
    CHECK(fov >= 0 && fov_deg >= 0 && cam_lag >= 0 && shake >= 0 && wind >= 0 && roar >= 0 &&
          blur >= 0 && blur_amount >= 0 && vignette >= 0 && discord >= 0);
    CHECK(!strcmp(video_setting(aa)->group, "Quality"));
    /* Both settings pages consume this one ordered list; a row added to one
       page only must fail here. */
    static const char *const tweak_order[] = {
        "Sense of Speed", "Dynamic FOV", "FOV Amount", "Camera Dynamics", "Camera Shake",
        "Wind Audio", "Road Roar Boost", "Radial Blur", "Blur Strength", "Vignette",
        "Discord Rich Presence", "Skip Intro", "Assist Line", "Phantom Opponents",
        "Field Size"};
    int tweak_group = video_setting_tweak_group();
    CHECK(tweak_group >= 0 && tweak_group < video_setting_count());
    CHECK(!strcmp(video_setting(tweak_group)->label, "Sense of Speed"));
    CHECK(video_settings_tweak_count() == (int)(sizeof tweak_order / sizeof *tweak_order));
    int listed = 0;
    for (int i = 0; i < video_setting_count(); i++) {
        if (strcmp(video_setting(i)->group, "Tweaks"))
            continue;
        CHECK(listed < video_settings_tweak_count());
        CHECK(i == tweak_group + listed);
        CHECK(!strcmp(video_setting(i)->label, tweak_order[listed]));
        listed++;
    }
    CHECK(listed == video_settings_tweak_count());
    for (int i = 0; i < video_settings_tweak_count(); i++) {
        int id = tweak_group + i;
        CHECK(video_setting_tweak_is_child(id) == (i >= 1 && i <= 9));
        CHECK(!strcmp(video_settings_tweak(i)->label, tweak_order[i]));
    }
    int lamps=setting("Realistic Car Lamps"),night=setting("Night Sky");CHECK(lamps>=0 && night>=0);
    for (int backend = 0; backend < 2; backend++) {
        video_settings_init(&s, 1, argv[1]);
        video_setting_set(&s, tweak_group, 0);
        CHECK(video_setting_get(&s, tweak_group) == 0);
        video_setting_set(&s, tweak_group, 1);
        CHECK(video_setting_get(&s, tweak_group) == 1);
        video_setting_set(&s, api, backend);
        video_setting_set(&s, fsr, 3);
        video_setting_set(&s, aa, 3);
        CHECK(s.current.video.upscale == 0 && s.current.render_scale == 2);
        video_setting_set(&s, smooth, 1);
        video_setting_set(&s, lamps, 1);video_setting_set(&s,night,1);
        video_settings_init(&loaded, 1, argv[1]);
        CHECK(video_setting_get(&loaded, api) == backend);
        CHECK(video_setting_get(&loaded, aa) == (argc > 2 ? 0 : 3));
        CHECK(video_setting_get(&loaded, smooth) == 1);
        CHECK(video_setting_get(&loaded,lamps)==1 && video_setting_get(&loaded,night)==1);
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
        CHECK(video_setting_get(&s,lamps)==0 && video_setting_get(&s,night)==0);
        CHECK(!s.current.speed_fov && !s.current.speed_cam_lag && !s.current.speed_shake &&
              !s.current.speed_wind && !s.current.speed_road_roar && !s.current.speed_blur &&
              !s.current.speed_vignette && s.current.speed_fov_degrees == 0 &&
              s.current.speed_blur_amount == 0);
        video_setting_set(&s, preset, 1);
        CHECK(video_setting_get(&s, aa) == 4 && video_setting_get(&s, smooth) == 1);
        CHECK(video_setting_get(&s,lamps)==1);
        CHECK(s.current.speed_fov && s.current.speed_cam_lag && s.current.speed_shake &&
              s.current.speed_wind && s.current.speed_road_roar && s.current.speed_blur &&
              s.current.speed_vignette && s.current.speed_fov_degrees == 8 &&
              s.current.speed_blur_amount == 35);
        CHECK(video_setting_get(&s, api) == backend);
    }
    video_setting_set(&s,aa,4);
    video_settings_init(&loaded,1,argv[1]);
    CHECK(video_setting_get(&loaded,aa)==4);
    int phantom = setting("Phantom Opponents"), field = setting("Field Size");
    CHECK(phantom >= 0 && field >= 0);
    video_setting_set(&s, phantom, 1);
    video_setting_set(&s, field, 2); /* MOON_CONDUCTOR_CARS + 2 */
    video_settings_init(&loaded,1,argv[1]);
    CHECK(video_setting_get(&loaded, phantom) == 1);
    CHECK(video_setting_get(&loaded, field) == 2);
    CHECK(video_settings_field_size_warning(20) == 0);
    CHECK(video_settings_field_size_warning(21) == 1);
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
    CHECK(video_setting_get(&loaded,preset)==3); /* a personal preset reads as Modified */
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
    int reflections = setting("Reflection Quality");
    CHECK(reflections >= 0);
    video_setting_set(&s, reflections, 2);
    video_settings_init(&loaded, 1, argv[1]);
    CHECK(video_setting_get(&loaded, reflections) == 2);
    video_setting_set(&s, preset, 0);
    CHECK(video_setting_get(&s, reflections) == 0);
    video_setting_set(&s, preset, 1);
    CHECK(video_setting_get(&s, reflections) == 1);
    int lighting = setting("Realistic Lighting");
    CHECK(lighting >= 0);
    video_setting_set(&s, lighting, 1);
    video_settings_init(&loaded, 1, argv[1]);
    CHECK(video_setting_get(&loaded, lighting) == 1);
    video_setting_set(&s, preset, 0);
    CHECK(video_setting_get(&s, lighting) == 0);
    video_setting_set(&s, preset, 1);
    CHECK(video_setting_get(&s, lighting) == 1);
    int sky=setting("Realistic Skybox");CHECK(sky>=0);
    video_setting_set(&s,sky,1);
    CHECK(s.current.daylight && s.current.night_sky && s.current.solar_hour==24 && s.current.solar_cycle==1);
    video_settings_init(&loaded,1,argv[1]);
    CHECK(loaded.current.daylight && loaded.current.night_sky && loaded.current.solar_hour==24);
    video_setting_set(&s,sky,0);CHECK(!s.current.daylight && !s.current.night_sky);
    video_setting_set(&s,lighting,1);
    CHECK(s.current.hdr && s.current.material_shading && s.current.shadow_maps==2 && s.current.car_shadows && s.current.ambient_occlusion && !s.current.sun_rays);
    video_setting_set(&s,lighting,0);
    CHECK(!s.current.hdr && !s.current.shadow_maps && !s.current.car_shadows && !s.current.ambient_occlusion);
    const char *hidden[]={"Night Sky","Solar Time","Sun / Moon Cycle","HDR","Sun Rays","Ambient Occlusion","Material Shading","Car Shadows","Shadow Maps"};
    for(unsigned i=0;i<sizeof hidden/sizeof *hidden;i++){int id=setting(hidden[i]);CHECK(id>=0 && !strcmp(video_setting(id)->group,"Legacy"));}
    /* Performance: Enhanced with the Fast shadow filter, kept across a reload */
    int shadow_filter=setting("Shadow Filter");
    CHECK(shadow_filter>=0 && !strcmp(video_setting(shadow_filter)->group,"Legacy"));
    video_setting_set(&s,preset,2);
    CHECK(!strcmp(s.store.preset,"Performance") && !s.store.modified && video_setting_get(&s,preset)==2);
    CHECK(s.current.shadow_filter && s.current.ambient_occlusion && s.current.antialiasing==4 && s.current.shadow_maps==2 && s.current.hdr && s.current.desktop);
    video_settings_init(&loaded,1,argv[1]);
    CHECK(video_setting_get(&loaded,preset)==2 && loaded.current.shadow_filter && loaded.current.ambient_occlusion);
    video_setting_set(&s,shadow_filter,0);
    CHECK(video_setting_get(&s,preset)==3 && !s.current.shadow_filter);
    video_setting_set(&s,preset,1);
    CHECK(!s.current.shadow_filter && s.current.ambient_occlusion && !strcmp(s.store.preset,"Enhanced"));
    CHECK(moon_options_set(&s.store,"audio.custom_music","1"));
    video_setting_set(&s,preset,0);
    CHECK(!moon_options_int(&s.store,"audio.custom_music",1,0,1) && !s.store.modified);
    remove(argv[1]);
    puts("Launcher quality shares the game store, exclusions and presets on both APIs");
    return 0;
}
