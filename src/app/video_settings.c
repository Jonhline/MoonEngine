#include "moon_pref.h"
#include "options_presets.h"
#include <stddef.h>
#ifdef __ANDROID__
#include "mobile_options.h"
#endif
#include "platform/switch_file.h"
#include "video_settings.h"
#include "platform/renderer_backend.h"
#include "video_gl.h"
#include "render3d.h"
#include "discord_presence.h"
#ifdef __SWITCH__
#include "launcher.h"
#include "platform/switch_scene.h"
#endif
#include "plat_sdl.h"
#include "race_hud.h"
#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <direct.h>
#include <SDL_syswm.h>
#endif

static video_settings *settings_active;
static void vs_info_init(void);
static const int sizes[][2] = {{1280, 720},  {1280, 960},  {1600, 900},
                               {1920, 1080}, {2560, 1440}, {3840, 2160}};
/* Stored 0..23 keep their old hour mapping; 24 explicitly means Course. */
int video_settings_solar_hour(const pc_video_options *v)
{
    return v->solar_hour==24?-1:(v->solar_hour+7)%24;
}
/* The one sense-of-speed curve: stationary below MIN, full effect at MAX.
   Every speed-driven effect (FOV, blur, vignette, road roar) reads the same
   smoothed level, so tuning is this one edit. */
#define SPEED_FEEL_MIN_KMH 60.f
#define SPEED_FEEL_MAX_KMH 240.f
#define SPEED_FEEL_SMOOTH_SECONDS .25f
static volatile float speed_level;
static float speed_traced = -1.f;
static float speed_feel_target(float kmh)
{
    float f = (kmh - SPEED_FEEL_MIN_KMH) / (SPEED_FEEL_MAX_KMH - SPEED_FEEL_MIN_KMH);
    f = f < 0.f ? 0.f : f > 1.f ? 1.f : f;
    return f * f * (3.f - 2.f * f);
}
void video_settings_speed_frame(float kmh, double dt)
{
    float target = speed_feel_target(kmh);
    float a = dt > 0 ? (float)(dt / SPEED_FEEL_SMOOTH_SECONDS) : 1.f;
    a = a < 0.f ? 0.f : a > 1.f ? 1.f : a;
    speed_level += (target - speed_level) * a;
    const char *force = getenv("MOON_SPEED_FORCE_LEVEL");
    if (force) {
        float f = (float)atof(force);
        speed_level = f < 0.f ? 0.f : f > 1.f ? 1.f : f;
    }
    video_set_speed_level(speed_level);
    if (getenv("MOON_SPEED_TRACE") && fabsf(speed_level - speed_traced) > .02f) {
        const pc_video_options *v = settings_active ? &settings_active->current : NULL;
        float fov = 60.f, blur = 0.f, vignette = 0.f, roar = 1.f;
        if (v) {
            if (v->speed_fov) {
                float deg = (float)v->speed_fov_degrees;
                deg = deg < 0.f ? 0.f : deg > 30.f ? 30.f : deg;
                fov += deg * speed_level;
            }
            if (v->speed_blur)
                blur = (float)v->speed_blur_amount * speed_level;
            if (v->speed_vignette)
                vignette = speed_level * 100.f;
            if (v->speed_road_roar)
                roar = 1.f + .35f * speed_level;
        }
        fprintf(stderr, "speed: kmh=%.0f f=%.2f fov=%.1f blur=%.0f vignette=%.0f roar=%.2f\n",
                kmh, speed_level, fov, blur, vignette, roar);
        speed_traced = speed_level;
    }
}
float video_settings_speed_level(void)
{
    return speed_level;
}
static void quality(const pc_video_options *v)
{
    video_ui_quality(v->filter, v->text / 100.f);
#ifdef __ANDROID__
    video_set_scene_render_scale(v->mobile.render_scale);
#endif
#if !defined(__SWITCH__) && !defined(__ANDROID__)
    video_scene_quality(v->antialiasing, v->render_scale);
    r3d_texture_smoothing(v->texture_smoothing);
    r3d_texture_filtering(v->texture_filtering);
    r3d_car_shadows(v->car_shadows);
    r3d_car_lamps(v->car_lamps);
    r3d_night_sky(v->night_sky);
    r3d_material_shading(v->material_shading);
    r3d_reflection_quality(v->reflection_quality);
    r3d_shadow_map_quality(v->shadow_maps);
    r3d_shadow_filter(v->shadow_filter);
    r3d_modern_lighting(v->modern_lighting);
    r3d_daylight_options(v->daylight,video_settings_solar_hour(v),v->solar_cycle);
    video_hdr_options(v->hdr,v->sun_rays);
    video_ambient_occlusion(v->ambient_occlusion);
#endif
    video_set_speed_post(v->speed_blur, v->speed_blur_amount, v->speed_vignette);
    video_set_scene_scale(v->scene_scale);
}

static void apply_discord(video_settings *s)
{
    if (!s)
        return;
    discord_presence_configure(
        s->current.discord_presence,
        moon_options_get(&s->store, "integrations.discord_app_id", "1562846084085194752"));
}

void video_settings_init(video_settings *s, int persist, const char *config_file)
{
    memset(s, 0, sizeof *s);
    vs_info_init();
    settings_active = s;
#ifndef __SWITCH__
    s->tweaks_open = 1;
#endif
    s->persist = persist;
#ifdef __SWITCH__
    s->current.video = (plat_video_desc){1280, 720, 1, 1, 0};
    /* P: Switch always starts in 16:9, including older SD configurations. */
    s->current.aspect = 1;
    s->current.scene_resolution = MOON_SWITCH_PERFORMANCE;
    s->current.touch_menu = 1;
    moon_options_load(&s->store,NULL,NULL);
#else
    s->current.video = (plat_video_desc){1280, 960, 0, 1, 0};
#endif
    /* Classic starts with texture/glyph coverage from the disc. Optional
       PC sharpening remains available and existing preferences still load. */
    s->current.filter = 0;
    s->current.text = 0;
    s->current.render_scale = 2;
    s->current.scene_scale = 100;
    s->current.antialiasing = 4;
    s->current.texture_filtering = 3;
    s->current.car_shadows = 1;
    s->current.car_lamps = 1;
    s->current.night_sky=1;
    s->current.material_shading = 1;
    s->current.reflection_quality = 1;
    s->current.shadow_maps = 2;
    s->current.modern_lighting = 1;
    s->current.daylight=1;s->current.solar_hour=24;s->current.solar_cycle=1;s->current.hdr=1;s->current.sun_rays=0;s->current.ambient_occlusion=1;s->current.shadow_filter=0;
#ifndef __SWITCH__
    s->current.desktop = s->current.edge_pan = s->current.input_hints = s->current.main_menu = s->current.smooth_cursor = s->current.hover_feedback = 1;
    s->current.compact_cars = 1;
    s->current.graphics_swap = 1;
    s->current.controller_tiles = 1;
    s->current.preset_tools = 1;
    s->current.save_tools = 1;
    s->current.spatial_visibility = s->current.resident_reflections = 1;
#endif
#ifdef __ANDROID__
    s->current.renderer = 1;
    s->current.video = (plat_video_desc){1280, 720, 1, 1, 0};
    s->current.aspect = 1;
    s->current.desktop = s->current.edge_pan = 0;
    s->current.touch_menu = 1;
    s->current.mobile = (moon_mobile_options){1, 1, 2, 0, 0};
#endif
    if (!persist)
        return;
    if (config_file)
        snprintf(s->path, sizeof s->path, "%s", config_file);
    else {
        char *dir = moon_pref_path();
        if (!dir)
            return;
        snprintf(s->path, sizeof s->path, "%svideo.ini", dir);
        SDL_free(dir);
    }
#ifndef __SWITCH__
    char legacy[1024];
    snprintf(legacy, sizeof legacy, "%s", s->path);
    if (!config_file) {
        char *base = strrchr(s->path, 'v');
        if (base && !strcmp(base, "video.ini")) snprintf(base, sizeof s->path - (size_t)(base - s->path), "options.ini");
    }
    moon_options_load(&s->store, s->path, config_file ? NULL : legacy);
    s->current.desktop = moon_options_int(&s->store, "controls.desktop_menu", strcmp(s->store.preset, "Classic") != 0, 0, 1);
    s->current.edge_pan = moon_options_int(&s->store, "controls.gtmode_edge_pan", strcmp(s->store.preset,"Classic")!=0, 0, 1);
    s->current.input_hints = moon_options_int(&s->store, "interface.input_hints", strcmp(s->store.preset,"Classic")!=0, 0, 1);
    s->current.main_menu = moon_options_int(&s->store, "interface.main_menu_overview", strcmp(s->store.preset,"Classic")!=0, 0, 1);
    s->current.smooth_cursor = moon_options_int(&s->store, "interface.smooth_mouse_cursor", strcmp(s->store.preset,"Classic")!=0, 0, 1);
    s->current.hover_feedback = moon_options_int(&s->store, "interface.mouse_hover_feedback", strcmp(s->store.preset,"Classic")!=0, 0, 1);
    int enhanced = strcmp(s->store.preset, "Classic") != 0;
    s->current.speed_fov = moon_options_int(&s->store, "tweaks.speed_fov", 0, 0, 1);
    s->current.speed_fov_degrees =
        moon_options_int(&s->store, "tweaks.speed_fov_degrees", enhanced ? 8 : 0, 0, 30);
    s->current.speed_cam_lag = moon_options_int(&s->store, "tweaks.speed_cam_lag", 0, 0, 1);
    s->current.speed_shake = moon_options_int(&s->store, "tweaks.speed_shake", 0, 0, 1);
    s->current.speed_wind = moon_options_int(&s->store, "tweaks.speed_wind", 0, 0, 1);
    s->current.speed_road_roar = moon_options_int(&s->store, "tweaks.speed_road_roar", 0, 0, 1);
    s->current.speed_blur = moon_options_int(&s->store, "tweaks.speed_blur", 0, 0, 1);
    s->current.speed_blur_amount =
        moon_options_int(&s->store, "tweaks.speed_blur_amount", enhanced ? 35 : 0, 0, 100);
    s->current.speed_vignette = moon_options_int(&s->store, "tweaks.speed_vignette", 0, 0, 1);
    s->current.skip_intro = moon_options_int(&s->store, "tweaks.skip_intro", 0, 0, 1);
    s->current.assist_line = moon_options_int(&s->store, "tweaks.assist_line", 0, 0, 1);
    s->current.phantom_opponents = moon_options_int(&s->store, "race.phantoms", 0, 0, 1);
    {
        const char *raw = moon_options_get(&s->store, "race.field_size", "");
        int field_size = MOON_CONDUCTOR_CARS;
        if (raw && *raw) {
            char *end = NULL;
            long n = strtol(raw, &end, 10);
            if (!*raw || (end && *end) || n < MOON_CONDUCTOR_CARS || n > MOON_MAX_CARS)
                fprintf(stderr,
                        "race.field_size=%s refused: allowed %d..%d; using %d\n",
                        raw, MOON_CONDUCTOR_CARS, MOON_MAX_CARS, MOON_CONDUCTOR_CARS);
            else
                field_size = (int)n;
        }
        s->current.field_size = field_size;
    }
    s->current.discord_presence =
        moon_options_int(&s->store, "integrations.discord_presence", 0, 0, 1);
    s->current.preview_mode = !strcmp(moon_options_get(&s->store,"interface.preview_mode","before_after"),"benchmark");
    if (getenv("MOON_PREVIEW_MODE") && !strcmp(getenv("MOON_PREVIEW_MODE"), "benchmark"))
        s->current.preview_mode = 1;
    s->current.preset_tools = moon_options_int(&s->store,"interface.preset_tools",enhanced,0,1);
    s->current.save_tools = moon_options_int(&s->store,"interface.save_transfer_tools",enhanced,0,1);
#if !defined(__ANDROID__)
    s->current.antialiasing = moon_options_int(&s->store, "graphics.antialiasing", enhanced ? 4 : 0, 0, 4);
    s->current.render_scale = moon_options_int(&s->store, "graphics.render_scale", 2, 0, 2);
    {
        const char *raw = moon_options_get(&s->store, "graphics.scene_scale", "100");
        char *end = NULL;
        long pct = strtol(raw, &end, 10);
        if (end && *end == '%')
            end++;
        s->current.scene_scale = (!*raw || (end && *end) || pct < 25 || pct > 200) ? 100 : (int)pct;
    }
    s->current.texture_smoothing =
        moon_options_int(&s->store, "graphics.texture_smoothing", 0, 0, 1);
    s->current.texture_filtering = moon_options_int(&s->store, "graphics.texture_filtering", enhanced ? 3 : 0, 0, 4);
    s->current.car_shadows = moon_options_int(&s->store, "graphics.car_shadows", enhanced, 0, 1);
    s->current.material_shading = moon_options_int(
        &s->store, "graphics.material_shading", strcmp(s->store.preset, "Performance") ? enhanced : 0, 0, 1);
    s->current.reflection_quality = moon_options_int(&s->store, "graphics.reflection_quality", enhanced, 0, 2);
    s->current.shadow_maps = moon_options_int(&s->store, "graphics.shadow_maps", enhanced * 2, 0, 3);
    s->current.modern_lighting = moon_options_int(
        &s->store, "graphics.modern_lighting", strcmp(s->store.preset, "Performance") ? enhanced : 0, 0, 1);
    s->current.daylight=moon_options_int(&s->store,"graphics.daylight",enhanced,0,1);
    s->current.night_sky=moon_options_int(&s->store,"graphics.night_sky",enhanced,0,1);
    s->current.car_lamps=moon_options_int(&s->store,"graphics.car_lamps",strcmp(s->store.preset,"Classic")!=0,0,1);
    s->current.solar_hour=moon_options_int(&s->store,"graphics.solar_hour",enhanced?24:5,0,24);
    s->current.hdr=moon_options_int(&s->store,"graphics.hdr",enhanced,0,1);
    s->current.sun_rays=moon_options_int(&s->store,"graphics.sun_rays",enhanced*2,0,2);
    s->current.ambient_occlusion=moon_options_int(&s->store,"graphics.ambient_occlusion",enhanced,0,1);
    s->current.shadow_filter=moon_options_int(&s->store,"graphics.shadow_filter",!strcmp(s->store.preset,"Performance"),0,1);
    s->current.solar_cycle=moon_options_int(&s->store,"graphics.solar_cycle",enhanced,0,2);
    video_options_realistic_skybox(&s->current,s->current.daylight);
    video_options_realistic_lighting(&s->current,s->current.modern_lighting);
#endif
    s->current.compact_cars = moon_options_int(&s->store,"interface.compact_car_strip",enhanced,0,1);
    s->current.graphics_swap = moon_options_int(&s->store,"interface.graphics_swap_key",enhanced,0,1);
    s->current.controller_tiles = moon_options_int(&s->store,"interface.controller_tiles",enhanced,0,1);
    s->current.spatial_visibility = moon_options_int(&s->store, "graphics.spatial_visibility", enhanced, 0, 1);
    s->current.resident_reflections = moon_options_int(&s->store, "graphics.resident_reflections", enhanced, 0, 1);
#endif
#ifdef __SWITCH__
    char legacy_path[1024];
    snprintf(legacy_path,sizeof legacy_path,"%s",s->path);
    if (!moon_options_switch_load(&s->store,legacy_path)) return;
    snprintf(s->path,sizeof s->path,"%s",s->store.path);
    s->current.scene_resolution=moon_options_int(&s->store,MOON_SWITCH_SCENE_KEY,1,0,2);
    s->current.touch_menu = moon_options_int(&s->store, "controls.touch_menu",
                                             strcmp(s->store.preset, "Classic") != 0, 0, 1);
#endif
#ifdef __ANDROID__
    moon_mobile_options_load(&s->store, &s->current.mobile, &s->current.touch_menu);
    s->current.renderer = 1;
    s->current.desktop = s->current.edge_pan = 0;
    s->current.video.fullscreen = 1;
#endif
    FILE *f = fopen(s->path, "r");
    if (!f)
        return;
    char line[128], key[40];
    int value;
    while (fgets(line, sizeof line, f)) {
        char *setting = !strncmp(line, "graphics.", 9) ? line + 9 : line;
#ifndef __SWITCH__
        if (!strncmp(setting, "renderer=", 9)) {
            line[strcspn(line, "\r\n")] = 0;
            int backend = moon_renderer_parse(setting + 9);
            if (backend >= 0)
                s->current.renderer = backend;
        }
#endif
        if (sscanf(setting, "%39[^=]=%d", key, &value) == 2) {
            if (!strcmp(key, "width") && value >= 640 && value <= 7680)
                s->current.video.width = value;
            if (!strcmp(key, "height") && value >= 480 && value <= 4320)
                s->current.video.height = value;
            if (!strcmp(key, "fullscreen") && (value == 0 || value == 1))
                s->current.video.fullscreen = value;
            if (!strcmp(key, "vsync") && (value == 0 || value == 1))
                s->current.video.vsync = value;
            if (!strcmp(key, "low_latency") && value >= 0 && value <= 1)
                s->current.low_latency = value;
            if (!strcmp(key, "fps_limit") && value >= 0 && value <= 1000)
                s->current.fps_limit = value;
            if (!strcmp(key, "upscaling") && value >= 0 && value <= 4)
                s->current.video.upscale = value;
            if (!strcmp(key, "ui_filter") && value >= 0 && value <= 2)
                s->current.filter = value;
#ifndef __SWITCH__
            if (!strcmp(key, "aspect_ratio") && (value == 0 || value == 1))
                s->current.aspect = value;
#endif
            if (!strcmp(key, "text_sharpness") && value >= 0 && value <= 100)
                s->current.text = value;
        }
    }
    fclose(f);
#ifdef __ANDROID__
    s->current.renderer = 1;
    s->current.desktop = s->current.edge_pan = 0;
    s->current.video.fullscreen = 1;
#endif
}

static void save(video_settings *s)
{
    const pc_video_options *v = &s->current;
    const char *keys[] = {"graphics.width", "graphics.height", "graphics.fullscreen", "graphics.vsync", "graphics.upscaling", "graphics.ui_filter",
                         "graphics.text_sharpness", "graphics.aspect_ratio", "graphics.low_latency", "graphics.fps_limit", "controls.desktop_menu", "controls.gtmode_edge_pan", "interface.input_hints", "interface.main_menu_overview", "graphics.spatial_visibility", "graphics.resident_reflections", "interface.smooth_mouse_cursor", "interface.mouse_hover_feedback", "interface.compact_car_strip", "interface.graphics_swap_key", "interface.controller_tiles"};
    int values[] = {v->video.width, v->video.height, v->video.fullscreen, v->video.vsync,
                    v->video.upscale, v->filter, v->text, v->aspect, v->low_latency, v->fps_limit, v->desktop, v->edge_pan, v->input_hints, v->main_menu, v->spatial_visibility, v->resident_reflections, v->smooth_cursor, v->hover_feedback, v->compact_cars, v->graphics_swap, v->controller_tiles};
    for (int i = 0; i < (int)(sizeof values / sizeof values[0]); i++) {
        char value[32];
        snprintf(value, sizeof value, "%d", values[i]);
        moon_options_set(&s->store, keys[i], value);
    }
#if !defined(__SWITCH__) && !defined(__ANDROID__)
    const char *quality_keys[] = {"graphics.antialiasing", "graphics.render_scale",
                                  "graphics.texture_smoothing", "graphics.texture_filtering", "graphics.car_shadows", "graphics.material_shading", "graphics.reflection_quality", "graphics.shadow_maps", "graphics.modern_lighting", "graphics.daylight", "graphics.solar_hour", "graphics.solar_cycle","graphics.hdr","graphics.sun_rays","graphics.ambient_occlusion","graphics.shadow_filter"};
    moon_options_set(&s->store,"graphics.night_sky",v->night_sky?"1":"0");
    moon_options_set(&s->store,"graphics.car_lamps",v->car_lamps?"1":"0");
    const int quality_values[] = {v->antialiasing, v->render_scale, v->texture_smoothing, v->texture_filtering, v->car_shadows, v->material_shading, v->reflection_quality, v->shadow_maps, v->modern_lighting,v->daylight,v->solar_hour,v->solar_cycle,v->hdr,v->sun_rays,v->ambient_occlusion,v->shadow_filter};
    for (int i = 0; i < (int)(sizeof quality_values / sizeof quality_values[0]); i++) {
        char value[16];
        snprintf(value, sizeof value, "%d", quality_values[i]);
        moon_options_set(&s->store, quality_keys[i], value);
    }
#endif
    moon_options_set(&s->store,"interface.preview_mode",v->preview_mode ? "benchmark" : "before_after");
    {
        char scale[16];
        snprintf(scale, sizeof scale, "%d", v->scene_scale);
        moon_options_set(&s->store, "graphics.scene_scale", scale);
    }
    moon_options_set(&s->store,"interface.preset_tools",v->preset_tools ? "1" : "0");
    moon_options_set(&s->store,"interface.save_transfer_tools",v->save_tools ? "1" : "0");
    {
        const char *tweak_keys[] = {"tweaks.speed_fov", "tweaks.speed_fov_degrees",
                                    "tweaks.speed_cam_lag", "tweaks.speed_shake",
                                    "tweaks.speed_wind", "tweaks.speed_road_roar",
                                    "tweaks.speed_blur", "tweaks.speed_blur_amount",
                                    "tweaks.speed_vignette", "tweaks.skip_intro",
                                    "tweaks.assist_line", "race.phantoms", "race.field_size"};
        const int tweak_values[] = {v->speed_fov, v->speed_fov_degrees, v->speed_cam_lag,
                                    v->speed_shake, v->speed_wind, v->speed_road_roar,
                                    v->speed_blur, v->speed_blur_amount, v->speed_vignette,
                                    v->skip_intro, v->assist_line, v->phantom_opponents,
                                    v->field_size};
        for (unsigned i = 0; i < sizeof tweak_keys / sizeof *tweak_keys; i++) {
            char text_value[16];
            snprintf(text_value, sizeof text_value, "%d", tweak_values[i]);
            moon_options_set(&s->store, tweak_keys[i], text_value);
        }
        moon_options_set(&s->store, "integrations.discord_presence",
                         v->discord_presence ? "1" : "0");
    }
    moon_options_set(&s->store, "graphics.renderer", moon_renderer_name(v->renderer));
#ifdef __SWITCH__
    char scene_value[8];
    snprintf(scene_value,sizeof scene_value,"%d",v->scene_resolution);
    moon_options_set(&s->store,MOON_SWITCH_SCENE_KEY,scene_value);
    moon_options_set(&s->store, "controls.touch_menu", v->touch_menu ? "1" : "0");
#endif
#ifdef __ANDROID__
    moon_mobile_options_save(&s->store, &v->mobile, v->touch_menu);
#endif
    if (s->preset_draft == 0) moon_options_set(&s->store,"audio.custom_music","0");
    if (s->preset_draft >= 0) {
        moon_options_set(&s->store, "audio.jukebox_folders", s->preset_draft ? "1" : "0");
        moon_options_set(&s->store, "audio.jukebox_groups", s->preset_draft ? "1" : "0");
    }
    if (s->preset_draft >= 0) {
        snprintf(s->store.preset, sizeof s->store.preset, "%s", s->preset_draft == 2 ? "Performance" : s->preset_draft ? "Enhanced" : "Classic");
        s->store.modified = 0;
    }
    if (s->personal_preset[0]) {
        snprintf(s->store.preset, sizeof s->store.preset, "%s", s->personal_preset);
        s->store.modified = s->personal_modified;
    }
    if (s->persist && !moon_options_save(&s->store))
        snprintf(s->message, sizeof s->message, "Preferences applied; could not save settings.");
}

#include "video_presets.inc"

/* ---- the launcher's view of the same settings (P) ---- */
enum {
    VS_RENDERER,
    VS_FULLSCREEN,
    VS_RESOLUTION,
    VS_ASPECT,
    VS_VSYNC,
    VS_UPSCALE,
    VS_FILTER,
    VS_TEXT,
    VS_LOW_LATENCY,
    VS_SPATIAL,
    VS_REFLECTIONS,
    VS_DESKTOP,
    VS_EDGE_PAN,
    VS_HINTS,
    VS_MAIN_MENU,
    VS_SMOOTH_CURSOR,
    VS_HOVER,
    VS_COMPACT,
    VS_PRESET,
    VS_FPS,
    VS_AA,
    VS_SCALE,
    VS_TEXTURE_SMOOTH,
    VS_TEXTURE_FILTERING,
    VS_CAR_SHADOWS,
    VS_MATERIAL_SHADING,
    VS_REFLECTION_QUALITY,
    VS_SHADOW_MAPS,
    VS_MODERN_LIGHTING,
    VS_DAYLIGHT,VS_SOLAR_HOUR,VS_SOLAR_CYCLE,VS_HDR,VS_SUN_RAYS,
    VS_PRESET_TOOLS, VS_SAVE_TOOLS, VS_AMBIENT_OCCLUSION, VS_GRAPHICS_SWAP,
    VS_CUSTOM_MUSIC, VS_JUKEBOX_FOLDERS, VS_JUKEBOX_GROUPS, VS_CONTROLLER_TILES,
    VS_CAR_LAMPS, VS_NIGHT_SKY, VS_SHADOW_FILTER,
    VS_SPEED_GROUP,
    VS_SPEED_FOV, VS_SPEED_FOV_DEGREES, VS_SPEED_CAM_LAG, VS_SPEED_SHAKE, VS_SPEED_WIND,
    VS_SPEED_ROAD_ROAR, VS_SPEED_BLUR, VS_SPEED_BLUR_AMOUNT, VS_SPEED_VIGNETTE, VS_DISCORD,
    VS_SKIP_INTRO,
    VS_ASSIST_LINE,
    VS_PHANTOM_OPPONENTS,
    VS_FIELD_SIZE,
    VS_PREVIEW_MODE,
    VS_COUNT
};
/* PC Preset values: the built-in presets, then Modified (read only, never set) */
static const char *const vs_preset[] = {"Classic", "Enhanced", "Performance", "Toaster", "Modified"};
/* the menu's Modified item: Performance is a PC preset */
#if defined(__ANDROID__) || defined(__SWITCH__)
#define VS_MENU_MODIFIED 2
#else
#define VS_MENU_MODIFIED 4
#endif
static const char *const vs_preview_mode[] = {"Before & After", "Benchmark"};
static const char *const vs_fps[] = {"Off", "30 FPS", "60 FPS", "72 FPS", "120 FPS",
                                     "144 FPS", "165 FPS", "240 FPS", "Match Display"};
static const int vs_fps_values[] = {0, 30, 60, 72, 120, 144, 165, 240, 1000};
static const char *const vs_off_on[] = {"Off", "On"};
static const char *const vs_api[] = {"OpenGL", "Vulkan"};
static const char *const vs_mode[] = {"Windowed", "Full Screen"};
static const char *const vs_size[] = {"1280 x 720",  "1280 x 960",  "1600 x 900",
                                      "1920 x 1080", "2560 x 1440", "3840 x 2160"};
static const char *const vs_ratio[] = {"4:3", "16:9"};
static const char *const vs_fsr[] = {"Native", "FSR Ultra Quality", "FSR Quality", "FSR Balanced",
                                     "FSR Performance"};
static const char *const vs_filter[] = {"Smooth", "Sharp", "Original"};
static const char *const vs_aa[] = {"Original", "Off", "SSAA 150%", "SSAA 200%", "Edge AA"};
static const char *const vs_hours[]={"07:00","08:00","09:00","10:00","11:00","12:00","13:00","14:00","15:00","16:00","17:00","18:00","19:00","20:00","21:00","22:00","23:00","00:00","01:00","02:00","03:00","04:00","05:00","06:00","Course"};
static const char *const vs_rays[]={"Off","Screen Space","Volumetric"};
static const char *const vs_cycle[]={"Fixed","Real Time","60x"};
static const char *const vs_modern_lighting[] = {"Off", "On"};
static const char *const vs_shadow_filter[] = {"Soft", "Fast"};
static const char *const vs_shadow_maps[] = {"Off (Original)", "Low 512", "High 1024", "Ultra 2048"};
static const char *const vs_reflection_quality[] = {"Original 128", "High 256", "Ultra 512"};
static const char *const vs_material_shading[] = {"Original", "Per-Pixel"};
static const char *const vs_car_shadows[] = {"Original", "Projected"};
static const char *const vs_texture_filtering[] = {"Original", "Trilinear", "Anisotropic 4x", "Anisotropic 8x", "Anisotropic 16x"};
static const char *const vs_scale[] = {"35%", "50%", "65%", "75%", "100%", "125%"};
static const char *const vs_text[] = {"0%", "25%", "50%", "75%", "100%"};
static const char *const vs_controller_tiles[] = {"Original", "By Player"};
static const char *const vs_closed_open[] = {"Closed", "Open"};
static const char *const vs_fov_degrees[] = {"0°", "2°", "4°", "6°", "8°", "10°", "12°", "16°", "20°", "24°", "28°", "30°"};
static const int vs_fov_degrees_values[] = {0, 2, 4, 6, 8, 10, 12, 16, 20, 24, 28, 30};
static const char *const vs_blur_amount[] = {"0%", "10%", "20%", "30%", "35%", "40%", "50%", "60%", "75%", "100%"};
static const int vs_blur_amount_values[] = {0, 10, 20, 30, 35, 40, 50, 60, 75, 100};
#define VS_FIELD_CHOICES (MOON_MAX_CARS - MOON_CONDUCTOR_CARS + 1)
static char vs_field_size_names[VS_FIELD_CHOICES][8];
static const char *vs_field_size_name_ptrs[VS_FIELD_CHOICES];
static int vs_field_size_names_ready;
static video_setting_info vs_info[VS_COUNT];
static void field_size_names_init(void)
{
    if (vs_field_size_names_ready)
        return;
    for (int i = 0; i < VS_FIELD_CHOICES; i++) {
        snprintf(vs_field_size_names[i], sizeof vs_field_size_names[i], "%d",
                 MOON_CONDUCTOR_CARS + i);
        vs_field_size_name_ptrs[i] = vs_field_size_names[i];
    }
    vs_info[VS_FIELD_SIZE].count = VS_FIELD_CHOICES;
    vs_info[VS_FIELD_SIZE].names = vs_field_size_name_ptrs;
    vs_field_size_names_ready = 1;
}
static const video_setting_info vs_info_values[VS_COUNT] = {
    {"Video", "Graphics API", "The renderer. Applies the next time Moon Engine opens.", 2, vs_api},
    {"Video", "Window Mode", "A window, or the whole screen.", 2, vs_mode},
    {"Video", "Resolution", "The window's size.", 6, vs_size},
    {"Video", "Aspect Ratio", "The game's picture: the original 4:3 or widescreen.", 2, vs_ratio},
    {"Video", "VSync", "Wait for the display's refresh.", 2, vs_off_on},
    {"Video", "Upscaling", "Render below the window's size and sharpen it back (FSR).", 5, vs_fsr},
    {"Video", "UI Filter", "How the menus' pictures are scaled.", 3, vs_filter},
    {"Video", "Text Clarity", "Sharpen the menus' text.", 5, vs_text},
    {"Video", "Low Latency", "Shorter frame queue: quicker input, less margin.", 2, vs_off_on},
    {"Video", "Spatial Visibility", "Skip drawing what the camera cannot see.", 2, vs_off_on},
    {"Video", "GPU Reflections", "Course reflections kept on the graphics card.", 2, vs_off_on},
    {"Interface", "Desktop Input", "Mouse and keyboard in the menus.", 2, vs_off_on},
    {"Interface", "Map Edge Pan", "The GT Mode map scrolls at the screen's edge.", 2, vs_off_on},
    {"Interface", "Input Hints", "Show the buttons of the device in use.", 2, vs_off_on},
    {"Interface", "Main Menu", "The PC main menu overview.", 2, vs_off_on},
    {"Interface", "Smooth Cursor", "The menu cursor glides to the mouse.", 2, vs_off_on},
    {"Interface", "Hover Feedback", "Menu items react under the mouse.", 2, vs_off_on},
    {"Interface", "Compact Car Strip", "A tighter car list in the garage and dealers.", 2,
     vs_off_on},
    {"General", "PC Preset",
     "Classic: the PS2 game. Enhanced: the recommended improvements. Performance: Enhanced with the Fast shadow filter. Any change makes it Modified.",
     3, vs_preset},
    {"Video", "Frame Limit", "The most frames drawn a second.", 7, vs_fps},
    {"Quality", "Anti-Aliasing", "Edge AA smooths the final 3D image; SSAA renders extra pixels.", 5, vs_aa},
    {"Quality", "3D Resolution", "Scene resolution; menus and HUD keep their output resolution.", 3,
     vs_scale},
    {"Quality", "Texture Smoothing", "Smooth model surfaces that use point sampling.", 2,
     vs_off_on},
    {"Quality", "Texture Filtering", "Optional trilinear/anisotropic filtering. Original preserves authored sampling.", 5, vs_texture_filtering},

    {"Legacy", "Car Shadows", "Project the car geometry onto the local ground. Original keeps the authored shadow volume.", 2, vs_car_shadows},
    {"Legacy", "Material Shading", "Evaluate authored lighting and reflection normals per pixel. Original retains vertex sampling.", 2, vs_material_shading},
    {"Quality", "Reflection Quality", "Resolution of generated race/replay and studio reflections; higher levels cost GPU time.", 3, vs_reflection_quality},
    {"Legacy", "Shadow Maps", "Nearby car depth shadows on the course and car; higher levels cost GPU time.", 4, vs_shadow_maps},
    {"Quality", "Realistic Lighting", "Sun/sky illumination, material response, contact shading and HDR. Off restores original lighting.", 2, vs_modern_lighting},
    {"Quality","Realistic Skybox","Clouds, atmosphere, sun, moon and stars in one system. Off restores the disc sky.",2,vs_off_on},
    {"Legacy","Solar Time","Course uses the authored day/night flag. Explicit hours override it. Night Sky enables all 24 hours.",25,vs_hours},
    {"Legacy","Sun / Moon Cycle","Fixed, real-time or 60x sun/moon progression. Night Sky enables the full 24 hours.",3,vs_cycle},
    {"Legacy","HDR","Floating-point scene, filmic highlights and restrained bloom.",2,vs_off_on},
    {"Legacy","Sun Rays","Screen-space or shadow-mapped world scattering. Requires HDR and Daylight.",3,vs_rays},
    {"Interface","Preset Files","Show optional named-preset file actions. Off preserves the screen without them.",2,vs_off_on},
    {"Interface","Save Transfer","Show Export in the player profiles screen. Native imports remain available alongside PS2 imports.",2,vs_off_on},
    {"Legacy","Ambient Occlusion","Depth-based contact and cavity shading. Requires HDR and Daylight.",2,vs_off_on},
    {"Interface","Graphics Swap Key","F7 in races and replays swaps Classic and your graphics live.",2,vs_off_on},
    {"Sound","Custom Music","Your own songs from the Music folder. Off: the disc's music only.",2,vs_off_on},
    {"Sound","Jukebox Folders","Menu and Replay folders in the Jukebox, beside Race and Slideshow.",2,vs_off_on},
    {"Sound","Library Groups","The Jukebox Library lists your songs by their folder in Music.",2,vs_off_on},
    {"Interface","Controller Tiles","By Player: one button config tile per player. Original: a tile per device and port.",2,vs_controller_tiles},
    {"Quality","Realistic Car Lamps","Headlights illuminate nearby surfaces with occlusion; rear and brake lenses emit light. Off restores the PS2 effect.",2,vs_off_on},
    {"Legacy","Night Sky","Twilight, moon and stars. Night courses start at 22:00; requires Daylight.",2,vs_off_on},
    {"Legacy","Shadow Filter","Soft: sixteen samples for wide shadow edges. Fast: four samples, much less GPU time, slightly harder edges.",2,vs_shadow_filter},
    {"Tweaks","Sense of Speed","Open to show its individual switches. Collapsing hides them; it is not a master switch.",2,vs_closed_open},
    {"Tweaks","Dynamic FOV","Widen the chase lens with speed. Other cameras keep the original lens.",2,vs_off_on},
    {"Tweaks","FOV Amount","Degrees added to the chase lens when Dynamic FOV is on.",12,vs_fov_degrees},
    {"Tweaks","Camera Dynamics","A small acceleration-driven camera trail on top of the original lag.",2,vs_off_on},
    {"Tweaks","Camera Shake","Rotational shake from speed and acceleration; no physics change.",2,vs_off_on},
    {"Tweaks","Wind Audio","Synthesised wind in the race mix, driven by speed.",2,vs_off_on},
    {"Tweaks","Road Roar Boost","More of the existing road-noise source; no new sample.",2,vs_off_on},
    {"Tweaks","Radial Blur","A PC-only post pass around the screen centre. Off is the original image.",2,vs_off_on},
    {"Tweaks","Blur Strength","Strength of the radial blur when it is on.",10,vs_blur_amount},
    {"Tweaks","Vignette","A PC-only darkened frame edge. Off is the original image.",2,vs_off_on},
    {"Tweaks","Discord Rich Presence","Show the current course and car on Discord. The Application ID is fixed.",2,vs_off_on},
    {"Tweaks","Skip Intro","Skip the GT splash and the opening movie; boot straight to the main menu.",2,vs_off_on},
    {"Tweaks","Assist Line","Racing line with braking zones on every track and race.",2,vs_off_on},
    {"Tweaks","Phantom Opponents","Off is the original six-car race. On adds extra cars in a second conductor; they can win.",2,vs_off_on},
    {"Tweaks","Field Size","Total cars when Phantom Opponents is on, including the player's six-car conductor.",0,NULL},
    {"Interface","Preview Mode","Before & After compares two clips. Benchmark shows the live scene and current FPS.",2,vs_preview_mode},
};
/* The mutable table starts as the compiled one; the field-size row's names and
   count are filled from MOON_MAX_CARS by field_size_names_init(). */
static void vs_info_init(void)
{
    if (vs_info[VS_RENDERER].label)
        return;
    memcpy(vs_info, vs_info_values, sizeof vs_info);
    field_size_names_init();
}

/* A preset's values for the registered PC options; host size, mode and the API are kept. */
int video_settings_builtin_preset(const char *name)
{
    for (int i = 0; i < 4; i++)
        if (name && !strcmp(name, vs_preset[i])) return i;
    return -1;
}
void video_settings_preset_values(pc_video_options *v, int preset)
{
    const int enhanced = preset != 0;
    const int toaster = preset == 3;
    v->car_lamps=enhanced;v->night_sky=enhanced;
    v->preset_tools=v->save_tools=enhanced;
    v->antialiasing = toaster ? 0 : enhanced ? 4 : 0;
    v->render_scale = 2;
    v->texture_smoothing = enhanced;
    v->texture_filtering = enhanced ? 3 : 0;
    v->car_shadows = enhanced;
    v->material_shading = enhanced && preset != 2 && !toaster;
    v->reflection_quality = enhanced;
    v->shadow_maps = toaster ? 0 : enhanced * 2;
    v->modern_lighting = enhanced && preset != 2 && !toaster;
    v->scene_scale = toaster ? 65 : 100;
    v->daylight=enhanced;v->solar_hour=enhanced?24:5;v->solar_cycle=enhanced;v->hdr=enhanced;v->sun_rays=0;v->ambient_occlusion=enhanced;
    v->shadow_filter=preset==2; /* Performance: about 4 ms less GPU at 1080p on a GTX 1650 */
    v->filter = enhanced ? 0 : 2;
    v->text = 0;
    v->video.upscale = 0;
    v->desktop = v->edge_pan = v->input_hints = v->main_menu = enhanced;
    v->smooth_cursor = v->hover_feedback = v->compact_cars = v->graphics_swap = enhanced;
    v->controller_tiles = enhanced;
    v->spatial_visibility = v->resident_reflections = enhanced;
    v->speed_fov = v->speed_cam_lag = v->speed_shake = v->speed_wind = v->speed_road_roar =
        v->speed_blur = v->speed_vignette = enhanced;
    v->speed_fov_degrees = enhanced ? 8 : 0;
    v->speed_blur_amount = enhanced ? 35 : 0;
}
int video_setting_count(void)
{
    vs_info_init();
    return VS_COUNT;
}
int video_setting_is_field_size(int id)
{
    return id == VS_FIELD_SIZE;
}
/* The Tweaks rows, in order, are the single source both settings pages use. */
int video_settings_tweak_count(void)
{
    vs_info_init();
    return VS_FIELD_SIZE - VS_SPEED_GROUP + 1;
}
const video_setting_info *video_settings_tweak(int i)
{
    vs_info_init();
    return i >= 0 && i < video_settings_tweak_count() ? &vs_info[VS_SPEED_GROUP + i] : NULL;
}
int video_setting_tweak_group(void)
{
    return VS_SPEED_GROUP;
}
int video_setting_tweak_is_child(int id)
{
    return id > VS_SPEED_GROUP && id <= VS_SPEED_VIGNETTE;
}
const video_setting_info *video_setting(int i)
{
    vs_info_init();
    return i >= 0 && i < VS_COUNT ? &vs_info[i] : NULL;
}
int video_setting_get(const video_settings *s, int i)
{
    vs_info_init();
    const pc_video_options *v = &s->current;
    switch (i) {
    case VS_RENDERER: return v->renderer == MOON_RENDERER_VULKAN;
    case VS_FULLSCREEN: return v->video.fullscreen != 0;
    case VS_RESOLUTION:
        for (int k = 0; k < 6; k++)
            if (sizes[k][0] == v->video.width && sizes[k][1] == v->video.height)
                return k;
        return 0;
    case VS_ASPECT: return v->aspect != 0;
    case VS_VSYNC: return v->video.vsync != 0;
    case VS_UPSCALE: return v->video.upscale >= 0 && v->video.upscale < 5 ? v->video.upscale : 0;
    case VS_FILTER: return v->filter >= 0 && v->filter < 3 ? v->filter : 0;
    case VS_TEXT: return v->text / 25 < 5 ? v->text / 25 : 4;
    case VS_LOW_LATENCY: return v->low_latency != 0;
    case VS_SPATIAL: return v->spatial_visibility != 0;
    case VS_REFLECTIONS: return v->resident_reflections != 0;
    case VS_DESKTOP: return v->desktop != 0;
    case VS_EDGE_PAN: return v->edge_pan != 0;
    case VS_HINTS: return v->input_hints != 0;
    case VS_MAIN_MENU: return v->main_menu != 0;
    case VS_SMOOTH_CURSOR: return v->smooth_cursor != 0;
    case VS_HOVER: return v->hover_feedback != 0;
    case VS_COMPACT: return v->compact_cars != 0;
    case VS_GRAPHICS_SWAP: return v->graphics_swap != 0;
    case VS_NIGHT_SKY: return v->night_sky;
    case VS_CAR_LAMPS: return v->car_lamps;
    case VS_CONTROLLER_TILES: return v->controller_tiles != 0;
    case VS_AA:
        return v->antialiasing;
    case VS_SCALE:
        switch (v->scene_scale) {
        case 35: return 0;
        case 50: return 1;
        case 65: return 2;
        case 75: return 3;
        case 125: return 5;
        default: return 4;
        }
    case VS_TEXTURE_SMOOTH:
        return v->texture_smoothing;
    case VS_TEXTURE_FILTERING:
        return v->texture_filtering;
    case VS_CAR_SHADOWS:
        return v->car_shadows;
    case VS_MATERIAL_SHADING:
        return v->material_shading;
    case VS_REFLECTION_QUALITY:
        return v->reflection_quality;
    case VS_SHADOW_MAPS:
        return v->shadow_maps;
    case VS_HDR:return v->hdr;
    case VS_SUN_RAYS:return v->sun_rays;
    case VS_DAYLIGHT:return v->daylight;
    case VS_SOLAR_HOUR:return v->solar_hour;
    case VS_SOLAR_CYCLE:return v->solar_cycle;
    case VS_MODERN_LIGHTING:
        return v->modern_lighting;
    case VS_PRESET_TOOLS: return v->preset_tools;
    case VS_SPEED_GROUP:
#ifdef __SWITCH__
        return 1;
#else
        return s->tweaks_open;
#endif
    case VS_SPEED_FOV: return v->speed_fov;
    case VS_SPEED_FOV_DEGREES: {
        int n = 0;
        for (int k = 1; k < (int)(sizeof vs_fov_degrees_values / sizeof *vs_fov_degrees_values); k++)
            if (vs_fov_degrees_values[k] <= v->speed_fov_degrees) n = k;
        return n;
    }
    case VS_SPEED_CAM_LAG: return v->speed_cam_lag;
    case VS_SPEED_SHAKE: return v->speed_shake;
    case VS_SPEED_WIND: return v->speed_wind;
    case VS_SPEED_ROAD_ROAR: return v->speed_road_roar;
    case VS_SPEED_BLUR: return v->speed_blur;
    case VS_SPEED_BLUR_AMOUNT: {
        int n = 0;
        for (int k = 1; k < (int)(sizeof vs_blur_amount_values / sizeof *vs_blur_amount_values); k++)
            if (vs_blur_amount_values[k] <= v->speed_blur_amount) n = k;
        return n;
    }
    case VS_SPEED_VIGNETTE: return v->speed_vignette;
    case VS_SKIP_INTRO: return v->skip_intro;
    case VS_ASSIST_LINE: return v->assist_line;
    case VS_PREVIEW_MODE: return v->preview_mode != 0;
    case VS_PHANTOM_OPPONENTS: return v->phantom_opponents != 0;
    case VS_FIELD_SIZE: {
        int n = v->field_size - MOON_CONDUCTOR_CARS;
        if (n < 0)
            n = 0;
        if (n >= VS_FIELD_CHOICES)
            n = VS_FIELD_CHOICES - 1;
        return n;
    }
    case VS_DISCORD: return v->discord_presence;
    case VS_SAVE_TOOLS: return v->save_tools;
    case VS_AMBIENT_OCCLUSION:return v->ambient_occlusion;
    case VS_SHADOW_FILTER:return v->shadow_filter;
    /* P: the Jukebox's options live in the store; folders and groups follow the preset */
    case VS_CUSTOM_MUSIC: return moon_options_int(&s->store, "audio.custom_music", 0, 0, 1);
    case VS_JUKEBOX_FOLDERS:
    case VS_JUKEBOX_GROUPS:
        return moon_options_int(&s->store, i == VS_JUKEBOX_FOLDERS ? "audio.jukebox_folders" : "audio.jukebox_groups",
                                strcmp(s->store.preset, "Classic") != 0, 0, 1);
    case VS_PRESET: {
        int preset = video_settings_builtin_preset(s->store.preset);
        return s->store.modified || preset < 0 ? 3 : preset;
    }
    case VS_FPS:
        for (int k = 0; k < 9; k++)
            if (vs_fps_values[k] == v->fps_limit)
                return k;
        return 0;
    }
    return 0;
}
void video_setting_set(video_settings *s, int i, int value)
{
    vs_info_init();
    if (!s || i < 0 || i >= VS_COUNT)
        return;
    int n = vs_info[i].count;
    value = ((value % n) + n) % n;
    pc_video_options *v = &s->current;
    switch (i) {
    case VS_PRESET_TOOLS: v->preset_tools=value; break;
    case VS_SPEED_GROUP:
#ifndef __SWITCH__
        s->tweaks_open = value != 0;
#endif
        return; /* UI state only: never marks the preset modified or saves */
    case VS_SPEED_FOV: v->speed_fov=value!=0; break;
    case VS_SPEED_FOV_DEGREES: v->speed_fov_degrees=vs_fov_degrees_values[value>=0&&value<12?value:0]; break;
    case VS_SPEED_CAM_LAG: v->speed_cam_lag=value!=0; break;
    case VS_SPEED_SHAKE: v->speed_shake=value!=0; break;
    case VS_SPEED_WIND: v->speed_wind=value!=0; break;
    case VS_SPEED_ROAD_ROAR: v->speed_road_roar=value!=0; break;
    case VS_SPEED_BLUR: v->speed_blur=value!=0; break;
    case VS_SPEED_BLUR_AMOUNT: v->speed_blur_amount=vs_blur_amount_values[value>=0&&value<10?value:0]; break;
    case VS_SPEED_VIGNETTE: v->speed_vignette=value!=0; break;
    case VS_SKIP_INTRO: v->skip_intro=value!=0; break;
    case VS_ASSIST_LINE: v->assist_line=value!=0; break;
    case VS_PHANTOM_OPPONENTS: v->phantom_opponents=value!=0; break;
    case VS_FIELD_SIZE: v->field_size=MOON_CONDUCTOR_CARS + value; break;
    case VS_DISCORD: v->discord_presence=value!=0; break;
    case VS_SAVE_TOOLS: v->save_tools=value; break;
    case VS_CUSTOM_MUSIC: moon_options_set(&s->store, "audio.custom_music", value ? "1" : "0"); break;
    case VS_JUKEBOX_FOLDERS: moon_options_set(&s->store, "audio.jukebox_folders", value ? "1" : "0"); break;
    case VS_JUKEBOX_GROUPS: moon_options_set(&s->store, "audio.jukebox_groups", value ? "1" : "0"); break;
    case VS_RENDERER: v->renderer = value ? MOON_RENDERER_VULKAN : MOON_RENDERER_OPENGL; break;
    case VS_FULLSCREEN: v->video.fullscreen = value; break;
    case VS_RESOLUTION:
        v->video.width = sizes[value][0];
        v->video.height = sizes[value][1];
        break;
    case VS_ASPECT: v->aspect = value; break;
    case VS_VSYNC: v->video.vsync = value; break;
    case VS_UPSCALE:
        v->video.upscale = value;
        v->antialiasing = 0;
        v->render_scale = 2;
        break;
    case VS_FILTER: v->filter = value; break;
    case VS_TEXT: v->text = value * 25; break;
    case VS_LOW_LATENCY: v->low_latency = value; break;
    case VS_SPATIAL: v->spatial_visibility = value; break;
    case VS_REFLECTIONS: v->resident_reflections = value; break;
    case VS_DESKTOP: v->desktop = value; break;
    case VS_EDGE_PAN: v->edge_pan = value; break;
    case VS_HINTS: v->input_hints = value; break;
    case VS_MAIN_MENU: v->main_menu = value; break;
    case VS_SMOOTH_CURSOR: v->smooth_cursor = value; break;
    case VS_HOVER: v->hover_feedback = value; break;
    case VS_COMPACT: v->compact_cars = value; break;
    case VS_GRAPHICS_SWAP: v->graphics_swap = value; break;
    case VS_NIGHT_SKY:v->night_sky=value;if(value)v->daylight=1;break;
    case VS_CAR_LAMPS: v->car_lamps=value;break;
    case VS_CONTROLLER_TILES: v->controller_tiles = value; break;
    case VS_FPS: v->fps_limit = vs_fps_values[value]; break;
    case VS_AA:
        v->antialiasing = value;
        v->render_scale = 2;
        v->video.upscale = 0;
        break;
    case VS_SCALE:
        v->render_scale = value;
        v->antialiasing = 0;
        v->video.upscale = 0;
        break;
    case VS_TEXTURE_SMOOTH:
        v->texture_smoothing = value;
        break;
    case VS_TEXTURE_FILTERING:
        v->texture_filtering = value;
        break;
    case VS_CAR_SHADOWS:
        v->car_shadows = value;
        break;
    case VS_MATERIAL_SHADING:
        v->material_shading = value;
        break;
    case VS_REFLECTION_QUALITY:
        v->reflection_quality = value;
        break;
    case VS_SHADOW_MAPS:
        v->shadow_maps = value;
        break;
    case VS_HDR:v->hdr=value;break;
    case VS_SUN_RAYS:v->sun_rays=value;if(value==2)v->hdr=v->daylight=1;break;
    case VS_AMBIENT_OCCLUSION:v->ambient_occlusion=value;if(value)v->hdr=v->daylight=1;break;
    case VS_SHADOW_FILTER:v->shadow_filter=value;break;
    case VS_DAYLIGHT:video_options_realistic_skybox(v,value);break;
    case VS_SOLAR_HOUR:v->solar_hour=value;break;
    case VS_SOLAR_CYCLE:v->solar_cycle=value;break;
    case VS_MODERN_LIGHTING:
        video_options_realistic_lighting(v,value);
        break;
    case VS_PRESET: {
        /* as the Screen Settings page applies it: the registered PC options at the
           preset's values; host size, mode and the API are kept */
        s->personal_preset[0]=0;
        video_settings_preset_values(v, value);
        fprintf(stderr, "settings: preset=%s modern_lighting=%d material_shading=%d\n",
                vs_preset[value], v->modern_lighting, v->material_shading);
        s->draft = s->current;
        s->preset_draft = value;
        save(s);
        apply_discord(s);
        s->preset_draft = -1;
        return;
    }
    }
    s->draft = s->current;
    if (s->personal_preset[0]) s->personal_modified=1;
    s->preset_draft = -1; /* an individual change: the preset reads Modified */
    save(s);
    apply_discord(s);
}
/* Transient preview of any option set: graphics only, nothing is stored. */
void video_settings_preview_options(const pc_video_options *v)
{
    if (v)
        quality(v);
}
void video_settings_preview_graphics(const video_settings *s, int classic)
{
    if (!s) return;
    pc_video_options v = s->current;
    if (classic) {
        v.antialiasing = 0; v.render_scale = 2;
        v.texture_smoothing = v.texture_filtering = 0;
        v.car_shadows = v.material_shading = v.reflection_quality = 0;
        v.shadow_maps = v.modern_lighting = 0;
        v.night_sky = v.car_lamps = v.daylight = v.solar_cycle = v.hdr = v.sun_rays = v.ambient_occlusion = v.shadow_filter = 0;
        v.filter = 2; v.text = 0;
    }
    quality(&v);
}

void video_settings_apply_current(video_settings *s)
{
    settings_active = s;
    if (!s)
        return;
    pc_video_options *v = &s->current;
    video_performance_options(v->low_latency, v->fps_limit);
#ifndef __SWITCH__
    r3d_scene_options(v->spatial_visibility, v->resident_reflections);
#endif
    if (!video_set_vsync(v->video.vsync))
        v->video.vsync = 0;
    if (!video_set_upscale(v->video.upscale))
        v->video.upscale = 0;
    quality(v);
    apply_discord(s);
}

void video_settings_set_renderer(video_settings *s, int backend)
{
    if (!s)
        return;
    s->current.renderer = backend;
    save(s);
}

static void text(ui_widget *w, const char *value)
{
    if (w) {
        free(w->text);
        w->text = strdup(value);
    }
}
#ifndef __SWITCH__
/* PC-only width changes retain the authored right edge and frame corners. */
static void widen(ui_widget *w, float width)
{
    if(!w) return;
    float old=w->w,delta=width-old;
    for(int i=0;i<w->nkids;i++) {
        ui_widget *child=w->kids[i];
        if(child->x>old*.5f) child->x+=delta;
        else if(child->w>old*.5f) widen(child,child->w+delta);
    }
    w->w=width;
}
#endif
static void values(ui_project *ui, ui_widget *choice, const char *const *names, int count)
{
    ui_widget *box = ui_widget_child(ui_widget_child(choice, "popup"), "vbox");
    ui_widget *original = ui_option_item(choice, 0);
    if (!box || !original)
        return;
    ui_widget *sample = ui_widget_copy(ui, original);
    /* Keep the authored translucent background and other decorative faces. */
    for (int i = box->nkids - 1; i >= 0; i--)
        if (!strcmp(box->kids[i]->type, "Composite") || !strcmp(box->kids[i]->type, "Button"))
            ui_widget_detach(box->kids[i]);
    for (int i = 0; i < count; i++) {
        ui_widget *item = ui_widget_copy(ui, sample);
        item->name = names[i];
        item->y = i * 24.f;
        text(ui_widget_child(item, "label"), names[i]);
        ui_widget_append(box, item);
    }
}
static int tweak_row_index(int id)
{
    return 41 + id - VS_SPEED_GROUP;
}
static void read_draft(video_settings *s)
{
    pc_video_options *v = &s->draft;
    v->aspect = s->choices[0]->focus_index;
#if !defined(__SWITCH__) && !defined(__ANDROID__)
    v->preset_tools=s->choices[31]->focus_index;
    v->save_tools=s->choices[32]->focus_index;
    v->graphics_swap = s->choices[37]->focus_index;
    v->night_sky = s->choices[40]->focus_index;
    v->shadow_filter = s->choices[56]->focus_index;
    v->car_lamps = s->choices[39]->focus_index;
    v->controller_tiles = s->choices[38]->focus_index;
#endif
#ifdef __SWITCH__
    v->aspect = 1;
    s->choices[0]->focus_index = 1;
#endif
    v->video.fullscreen = s->choices[1]->focus_index;
    int size = s->choices[2]->focus_index;
    if (size >= 0 && size < (int)(sizeof sizes / sizeof sizes[0])) {
        v->video.width = sizes[size][0];
        v->video.height = sizes[size][1];
    }
    v->filter = s->choices[3]->focus_index;
    v->text = s->choices[4]->focus_index * 25;
    v->video.vsync = s->choices[5]->focus_index;
    v->video.upscale = s->choices[6]->focus_index;
#ifdef __SWITCH__
    v->scene_resolution = s->choices[7]->focus_index;
    v->touch_menu = s->choices[9]->focus_index;
#endif
#ifndef __SWITCH__
#ifndef __ANDROID__
    v->desktop = s->choices[7]->focus_index;
    v->edge_pan = s->choices[9]->focus_index;
    v->input_hints = s->choices[10]->focus_index;
    v->main_menu = s->choices[13]->focus_index;
    v->smooth_cursor = s->choices[14]->focus_index;
    v->hover_feedback = s->choices[15]->focus_index;
#endif
#ifndef __ANDROID__
    v->antialiasing = s->choices[17]->focus_index;
    {
        static const int scales[] = {35, 50, 65, 75, 100, 125};
        v->scene_scale = scales[s->choices[18]->focus_index >= 0 && s->choices[18]->focus_index < 6
                                    ? s->choices[18]->focus_index : 4];
    }
    v->texture_smoothing = s->choices[19]->focus_index;
    v->texture_filtering = s->choices[20]->focus_index;
    v->car_shadows = s->choices[21]->focus_index;
    v->material_shading = s->choices[22]->focus_index;
    v->reflection_quality = s->choices[23]->focus_index;
    v->shadow_maps = s->choices[24]->focus_index;
    v->modern_lighting = s->choices[25]->focus_index;
    v->daylight=s->choices[26]->focus_index;v->solar_hour=s->choices[27]->focus_index;v->solar_cycle=s->choices[28]->focus_index;v->hdr=s->choices[29]->focus_index;v->sun_rays=s->choices[30]->focus_index;v->ambient_occlusion=s->choices[33]->focus_index;
    v->speed_fov = s->choices[42]->focus_index;
    v->speed_fov_degrees = vs_fov_degrees_values[s->choices[43]->focus_index >= 0 && s->choices[43]->focus_index < 12 ? s->choices[43]->focus_index : 0];
    v->speed_cam_lag = s->choices[44]->focus_index;
    v->speed_shake = s->choices[45]->focus_index;
    v->speed_wind = s->choices[46]->focus_index;
    v->speed_road_roar = s->choices[47]->focus_index;
    v->speed_blur = s->choices[48]->focus_index;
    v->speed_blur_amount = vs_blur_amount_values[s->choices[49]->focus_index >= 0 && s->choices[49]->focus_index < 10 ? s->choices[49]->focus_index : 0];
    v->speed_vignette = s->choices[50]->focus_index;
    v->discord_presence = s->choices[51]->focus_index;
    v->skip_intro = s->choices[52]->focus_index;
    v->assist_line = s->choices[53]->focus_index;
    v->phantom_opponents = s->choices[tweak_row_index(VS_PHANTOM_OPPONENTS)]->focus_index;
    v->field_size = MOON_CONDUCTOR_CARS + s->choices[tweak_row_index(VS_FIELD_SIZE)]->focus_index;
#endif
    v->compact_cars = s->choices[16]->focus_index;
    v->spatial_visibility = s->choices[11]->focus_index;
    v->resident_reflections = s->choices[12]->focus_index;
#endif
#ifdef __ANDROID__
    v->touch_menu = s->choices[7]->focus_index;
    v->mobile = (moon_mobile_options){s->choices[9]->focus_index, s->choices[10]->focus_index,
                                      s->choices[13]->focus_index, s->choices[14]->focus_index,
                                      s->choices[15]->focus_index};
    v->mobile.render_scale = s->choices[17]->focus_index;
    v->desktop = v->edge_pan = 0;
    v->renderer = 1;
    v->video.fullscreen = 1;
#endif
}
static void tweaks_visibility(video_settings *s);
static void update_field_size_ui(video_settings *s);

static int idle(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv, ah_val *ret)
{
    (void)vm;
    (void)self;
    (void)data;
    (void)argc;
    (void)argv;
    *ret = ah_int(0);
    return 0;
}
static int changed(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv, ah_val *ret)
{
    (void)vm;
    (void)self;
    (void)argc;
    (void)argv;
    video_settings *s = data;
    read_draft(s);
    if (s->personal_preset[0]) s->personal_modified=1;
    update_field_size_ui(s);
#ifndef __SWITCH__
    ui_widget *sender = ah_host_data(self);
    if (sender == s->choices[41]) {
        s->tweaks_open = sender->focus_index;
        if (!s->tweaks_open)
            menu_native_focus(s->owner, s->choices[41]);
    }
    if (sender == s->choices[42] || sender == s->choices[48])
        tweaks_visibility(s);
#ifndef __ANDROID__
    if(sender==s->choices[26]) {
        int on=sender->focus_index;
        s->choices[40]->focus_index=on;
        s->choices[27]->focus_index=on?24:5;
        s->choices[28]->focus_index=on;
    }
    if(sender==s->choices[25]) {
        pc_video_options bundle=s->draft;
        video_options_realistic_lighting(&bundle,sender->focus_index);
        s->choices[21]->focus_index=bundle.car_shadows;
        s->choices[22]->focus_index=bundle.material_shading;
        s->choices[24]->focus_index=bundle.shadow_maps;
        s->choices[29]->focus_index=bundle.hdr;
        s->choices[30]->focus_index=bundle.sun_rays;
        s->choices[33]->focus_index=bundle.ambient_occlusion;
    }
    if(sender==s->choices[40] && sender->focus_index)s->choices[26]->focus_index=1;
    if((sender==s->choices[33] && sender->focus_index) || (sender==s->choices[30] && sender->focus_index==2)){
        s->choices[29]->focus_index=1;s->choices[26]->focus_index=1;
    }
#endif
    if (sender == s->choices[8] && s->choices[8]->focus_index < VS_MENU_MODIFIED) {
        int preset = s->choices[8]->focus_index, enhanced = preset != 0;
        s->personal_preset[0]=0; s->personal_modified=0;
        s->preset_draft = preset;
#ifndef __ANDROID__
        s->choices[31]->focus_index = enhanced;
        s->choices[32]->focus_index = enhanced;
        if (!enhanced)
            s->choices[34]->focus_index = 0; /* Classic: the disc's music only */
        s->choices[35]->focus_index = s->choices[36]->focus_index = enhanced;
        s->choices[39]->focus_index=enhanced;s->choices[40]->focus_index=enhanced;
        s->choices[37]->focus_index = s->choices[38]->focus_index = enhanced;
        s->choices[17]->focus_index = enhanced ? 4 : 0;
        s->choices[18]->focus_index = preset == 3 ? 2 : 4;
        s->choices[19]->focus_index = enhanced;
        s->choices[20]->focus_index = enhanced ? 3 : 0;
        s->choices[21]->focus_index = enhanced;
        s->choices[22]->focus_index = enhanced;
        s->choices[23]->focus_index = enhanced;
        s->choices[24]->focus_index = enhanced * 2;
        s->choices[25]->focus_index = enhanced;
        s->choices[26]->focus_index=enhanced;s->choices[27]->focus_index=enhanced?24:5;s->choices[28]->focus_index=enhanced;s->choices[29]->focus_index=enhanced;s->choices[30]->focus_index=0;s->choices[33]->focus_index=enhanced;
        s->choices[56]->focus_index = preset==2;
        s->choices[42]->focus_index=enhanced;s->choices[43]->focus_index=enhanced?4:0;
        s->choices[44]->focus_index=enhanced;s->choices[45]->focus_index=enhanced;
        s->choices[46]->focus_index=enhanced;s->choices[47]->focus_index=enhanced;
        s->choices[48]->focus_index=enhanced;s->choices[49]->focus_index=enhanced?4:0;
        s->choices[50]->focus_index=enhanced;
        s->choices[53]->focus_index=0; /* the assist line is never a preset default */
#endif
        s->choices[3]->focus_index = enhanced ? 0 : 2;
        s->choices[4]->focus_index = 0;
        s->choices[6]->focus_index = 0;
        s->choices[7]->focus_index = enhanced;
        s->choices[9]->focus_index = enhanced;
        s->choices[10]->focus_index = enhanced;
        s->choices[13]->focus_index = enhanced;
        s->choices[14]->focus_index = enhanced;
        s->choices[15]->focus_index = enhanced;
        s->choices[16]->focus_index = enhanced;
        s->choices[11]->focus_index = enhanced;
        s->choices[12]->focus_index = enhanced;
#ifdef __ANDROID__
        s->draft.input_hints = s->draft.main_menu = enhanced;
        s->draft.smooth_cursor = s->draft.hover_feedback = enhanced;
        s->choices[10]->focus_index = 1;
        s->choices[13]->focus_index = 2;
        s->choices[14]->focus_index = 0;
        s->choices[15]->focus_index = 0;
        s->choices[17]->focus_index = 0;
#endif
        read_draft(s);
    } else {
#ifndef __ANDROID__
        if (sender == s->choices[17]) {
            s->choices[18]->focus_index = video_setting_get(s, VS_SCALE);
            s->choices[6]->focus_index = 0;
        } else if (sender == s->choices[18] || sender == s->choices[6]) {
            s->choices[17]->focus_index = 0;
            if (sender == s->choices[18])
                s->choices[6]->focus_index = 0;
            else
                s->choices[18]->focus_index = video_setting_get(s, VS_SCALE);
        }
        read_draft(s);
#endif
        s->preset_draft = -1;
        s->choices[8]->focus_index = VS_MENU_MODIFIED;
        char caption[96];
        snprintf(caption,sizeof caption,"%s%s",s->personal_preset[0] ? s->personal_preset : "Modified",s->personal_preset[0] ? " (modified)" : "");
        text(ui_widget_child(ui_option_item(s->choices[8],VS_MENU_MODIFIED),"label"),caption);
        ui_set_tooltips(s->page, "Custom settings (modified preset). Apply with OK.");
    }
#endif
#ifdef __SWITCH__
    ui_widget *sender = ah_host_data(self);
    if (sender == s->choices[8] && s->choices[8]->focus_index < 2) {
        int enhanced = s->choices[8]->focus_index;
        s->preset_draft=enhanced;
        s->choices[3]->focus_index=enhanced?0:2;
        s->choices[4]->focus_index=0;
        s->choices[6]->focus_index=0;
        s->choices[7]->focus_index=enhanced?MOON_SWITCH_PERFORMANCE:MOON_SWITCH_CLASSIC;
        s->choices[9]->focus_index = enhanced;
        read_draft(s);
    } else {
        s->preset_draft=-1;
        s->choices[8]->focus_index=2;
    }
#endif
#ifdef _WIN32
    ui_widget *pane=ui_widget_child(s->page,"Pane");
    ui_widget *load_button=ui_widget_child(pane,"LoadPreset"), *save_button=ui_widget_child(pane,"SavePreset");
    int files = s->draft.preset_tools && s->active_tab == 5; /* Presets */
    if (load_button) load_button->visible=files;
    if (save_button) save_button->visible=files;
#endif
#ifndef __SWITCH__
    /* The in-game Tweaks switches are immediate: copy what the row changed
       from the draft into the live options and save it now, exactly as the
       launcher does. Display rows keep their apply-on-OK behaviour. */
    int tweak_row = 0;
    for (int i = 42; i <= 53; i++)
        if (ah_host_data(self) == s->choices[i]) {
            tweak_row = 1;
            break;
        }
    if (tweak_row) {
        s->current.speed_fov = s->draft.speed_fov;
        s->current.speed_fov_degrees = s->draft.speed_fov_degrees;
        s->current.speed_cam_lag = s->draft.speed_cam_lag;
        s->current.speed_shake = s->draft.speed_shake;
        s->current.speed_wind = s->draft.speed_wind;
        s->current.speed_road_roar = s->draft.speed_road_roar;
        s->current.speed_blur = s->draft.speed_blur;
        s->current.speed_blur_amount = s->draft.speed_blur_amount;
        s->current.speed_vignette = s->draft.speed_vignette;
        s->current.discord_presence = s->draft.discord_presence;
        s->current.skip_intro = s->draft.skip_intro;
        s->current.assist_line = s->draft.assist_line;
        video_set_speed_post(s->current.speed_blur, s->current.speed_blur_amount,
                             s->current.speed_vignette);
        apply_discord(s);
        save(s);
    }
#endif
    quality(&s->draft);
    tweaks_visibility(s);
    *ret = ah_int(2);
    return 0;
}
static const char *hints[] = {
    "Select the original 4:3 or widescreen 16:9 display ratio.",
    "Select windowed or full-screen display.",
    "Select the window resolution. Aspect Ratio controls the image proportions.",
    "Sharp preserves image edges. Smooth softens pixels. Original uses nearest sampling.",
    "Adjust text edge clarity independently of image filtering.",
    "Synchronize presentation with the monitor refresh rate.",
    "Native renders at full resolution. FSR 1 improves 3D performance; the UI stays at full resolution.",
    "Use mouse hover, left click, right click and wheel in menus. WASD also navigates. Classic keeps pad-style input.",
    "Apply Classic, Enhanced or Performance defaults to the registered PC options. Performance is Enhanced with the Fast shadow filter, for less GPU time. Individual edits mark the preset as modified.",
    "Move the GT Mode map near its edges with the mouse. Off keeps the original icon-based navigation.",
    "Show keyboard or controller prompts for the last device used. Off retains the original PS2 symbols.",
    "Reject off-screen spatial blocks before individual draw groups. Off uses the retained flat visibility path; original course lists remain active.",
    "Keep reflection views and composition on the GPU. Off uses the retained CPU remap. Both use the same six scene views.",
    "Show five main menu entries in the original vertical style. Use the wheel to scroll. Off restores the original layout.",
    "Add brief mouse position/angle easing and smooth cursor icon changes. Off follows the mouse immediately.",
    "Gently enlarge otherwise-static Options icons and labels. Existing game hover animations are preserved.",
    "Use smaller car thumbnails in garage and dealer lists. Off retains the original layout.",
    "Original retains the previous renderer. Off disables AA. Edge AA filters edges after HDR without changing UI. SSAA renders at 150% or 200% resolution; costs GPU time. Selecting AA resets FSR.",
    "Lower only the 3D resolution for performance. HUD and menus stay sharp. Selecting this disables SSAA and FSR.",
    "Smooth point-sampled model textures. Off keeps the authored texture sampling.",
    "Optional trilinear or anisotropic filtering preserves distant road detail. Original restores authored taps. Region-addressed textures retain their original rules.",
    "Projected uses the car and wheel geometry on the local ground. Original retains the authored shadow volume.",
    "Per-pixel highlights and reflections use authored material coefficients. Original keeps vertex lighting and reflection coordinates.",
    "Original uses 128-pixel generated maps. High uses 256; Ultra 512. Larger targets cost GPU time; authored environment images are unchanged.",
    "Depth shadows from nearby cars onto the course and car. Off restores the retained shadows; higher resolutions cost GPU time.",
    "Modern evaluates materials with GGX, sky fill and filmic highlights. Original retains the previous lighting. Course baked lighting remains authored.",
    "Realistic Skybox: clouds, atmosphere, sun, moon and stars. Off restores the authored sky.",
    "Choose solar time between 07:00 and 17:00.",
    "Advance daylight from simulation time; pause freezes it. No night.",
    "Floating-point scene, filmic resolve and bloom; UI remains SDR.",
    "Depth-occluded sun shafts; requires HDR and Daylight.",
    "Show Load/Save Preset file actions. Off hides the optional tools.",
    "Show Export on the player profiles screen. Off restores its previous footer.",
    "Screen-space contact and cavity shading. Requires HDR and Daylight.",
    "Your own songs from the Music folder in the Jukebox, races and menus. Off keeps the disc's music only.",
    "Add Menu and Replay folders to the Jukebox, beside Race and Slideshow.",
    "List your songs in the Jukebox Library by their folder in Music.",
    "F7 in races and replays swaps between Classic graphics and yours. Nothing is saved.",
    "By Player: one button config tile per player for any controller or wheel. Original: one tile per device.",
    "Realistic Car Lamps: headlights illuminate nearby surfaces with shadows; rear lenses brighten with the brake pedal. Off restores the original effect.",
    "Twilight, moon and stars, following the frame clock. Requires Daylight; Classic keeps the authored sky.",
    "Open Sense of Speed to show its individual switches. Collapsing hides them; it is not a master switch.",
    "Widen the chase camera's lens with speed. Views on, ahead or behind the car keep the game's own lens.",
    "Degrees added to the chase lens when Dynamic FOV is on.",
    "A small acceleration-driven trail layered on the game's own camera lag.",
    "Rotational noise shake, never more than 0.15 degrees; the car's motion is untouched.",
    "Synthesised wind from speed, mixed with the race's own sound.",
    "Boost the disc's existing road-noise source. Nothing new is sampled.",
    "A PC-only radial blur around the screen centre, after the scene resolve.",
    "How far the radial blur reaches when it is on.",
    "A PC-only darkened frame edge. Off leaves the original image.",
    "Show the current course and car on Discord. Off: nothing talks to Discord.",
    "Skip the GT splash and the opening movie; boot straight to the main menu.",
    "Racing line with braking zones on every track and race.",
    "Off is the original six-car race. On adds extra cars in a second conductor; they can win.",
    "Total cars when Phantom Opponents is on, including the player's six-car conductor.",
    "Soft blends sixteen shadow-map samples. Fast blends four over the same width: much less GPU time, slightly harder edges on the car body."};
static int focus(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv, ah_val *ret)
{
    (void)vm;
    (void)argc;
    (void)argv;
    video_settings *s = data;
    ui_widget *w = ah_host_data(self);
#ifdef __SWITCH__
    if (w == s->choices[9]) {
        ui_set_tooltips(
            s->page,
            "Tap to select and preview; tap the same item again to confirm. Drag lists or the GT Mode map.");
        *ret = ah_int(2);
        return 0;
    }
    if (w == s->choices[7]) {
        ui_set_tooltips(s->page,"Classic: PS2 render size. Balanced: sharper 3D. Performance: lower 3D resolution for 60 FPS; HUD stays sharp.");
        *ret=ah_int(2);return 0;
    }
    if (w == s->choices[8]) {
        ui_set_tooltips(s->page,"Classic selects PS2-sized 3D; Enhanced selects Performance. Individual edits mark the preset modified.");
        *ret=ah_int(2);return 0;
    }
#endif
#ifdef __ANDROID__
    if (w == s->choices[17]) {
        ui_set_tooltips(s->page, "Lower only the 3D scene resolution to save fill rate. Menus and "
                                 "HUD stay at full resolution.");
        *ret = ah_int(2);
        return 0;
    }
    const int mobile_rows[] = {7, 9, 10, 13, 14, 15};
    const char *mobile_hints[] = {
        "Touch Menus: tap a row to select it; drag the lists and the GT Mode map.",
        "Optional race buttons. External controllers take priority and hide the touch controls.",
        "Size of the on-screen race controls.",
        "Opacity of the on-screen race controls.",
        "Place steering on the left or mirror the layout.",
        "Analog drag steering or digital left/right steering. Vehicle physics is unchanged."};
    for (int i = 0; i < 6; i++)
        if (w == s->choices[mobile_rows[i]]) {
            ui_set_tooltips(s->page, mobile_hints[i]);
            *ret = ah_int(2);
            return 0;
        }
#endif
    for (int i = 0; i < (int)(sizeof s->choices / sizeof s->choices[0]); i++)
        if (s->choices[i] == w)
            ui_set_tooltips(s->page, hints[i]);
    *ret = ah_int(2);
    return 0;
}
#ifndef __SWITCH__
/* P: PC settings use separate pages with a shared, uncommitted draft.
   Stable choice indices belong to the store, not to their visible positions. */
#ifdef __ANDROID__
/* Six rows of 44 units fit between the tabs and the authored footer. The
   preset row joins Display so every tab has room for the thumb. */
enum { VIDEO_TAB_COUNT = 3 };
static const int tab_rows[VIDEO_TAB_COUNT][11] = {
    {8, 0, 1, 2, 3, 5, -1}, {4, 6, 17, 11, 12, 16, -1}, {7, 9, 10, 13, 14, 15, -1}};
#else
/* P: one page per Options tile, as the game's own Options: Screen (the display), then the
   PC Settings section's Graphics, Light & Shadow, Controls, Music and Presets. */
enum { VIDEO_TAB_COUNT = 7 };
static const int tab_rows[VIDEO_TAB_COUNT][16] = {
    {0, 1, 2, 5, 6, 3, 4, -1},
    {17, 18, 19, 20, 23, 12, 11, -1},
    /* UI row indices are separate from the launcher API's VS_* enum. */
    {25, 26, 39, 56, -1},
    {7, 10, 14, 15, 13, 9, 16, 37, 38, -1},
    {34, 35, 36, -1},
    {8, 31, 32, -1},
    {41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, -1}};
static const char *const page_tiles[VIDEO_TAB_COUNT] = {"", "PcGraphics", "PcLighting",
                                                        "PcControls", "PcMusic", "PcPresets",
                                                        "PcTweaks"};
static const char *const page_titles[VIDEO_TAB_COUNT] = {
    "Screen Settings", "Graphics", "Light & Shadow", "Controls", "Music", "Presets", "Tweaks"};
#endif
/* the Load/Save preset buttons' page; on the PC Tweaks is the last */
#ifdef __ANDROID__
enum { VIDEO_TAB_PRESETS = VIDEO_TAB_COUNT - 1 };
#else
enum { VIDEO_TAB_PRESETS = 5 };
#endif
#ifndef __ANDROID__
static float pc_pane_center; /* the authored pane's centre; each page is sized around it */
#endif
#ifdef __ANDROID__
static float widget_abs_y(const ui_widget *w)
{
    float y = 0;
    for (; w; w = w->parent)
        y += w->y;
    return y;
}
/* 44 engine units against the old 24/28/32 row boxes. The authored footer
   starts at 332 and the shared Android gesture-bar floor caps the bottom. */
static float android_row_pitch(const ui_widget *row, int count)
{
    const float top = 64.f, footer = 332.f;
    float safe = MOON_MOBILE_SAFE_FLOOR * video_view_height() - widget_abs_y(row->parent);
    float bottom = fminf(footer, safe);
    float pitch = count > 0 ? (bottom - top) / (float)count : 44.f;
    return fminf(44.f, pitch);
}
#endif
#ifndef __SWITCH__
#ifndef __ANDROID__
/* Positions every visible row of a tab in order, then sizes the pane to the
   rows that are actually shown: collapsing a group closes the gap instead of
   leaving empty slots and a pane taller than its content. */
/* In-game rows are 22 units, not the old 28/32: the Tweaks list is the longest
   page and must fit the 480-line view. The viewport shows this many rows; the
   open Sense of Speed group scrolls inside it instead of growing the pane. */
#define VIDEO_ROW_TOP 38
#define VIDEO_ROW_PITCH 22
#define VIDEO_TWEAKS_VIEW_ROWS 9
static void layout_tab_rows(video_settings *s, int tab)
{
    const int top = VIDEO_ROW_TOP, pitch = VIDEO_ROW_PITCH;
    int n = 0, shown = 0;
    while (n < 15 && tab_rows[tab][n] >= 0)
        n++;
    ui_widget *pane = ui_widget_child(s->page, "Pane");
    if (!pane)
        return;
    ui_widget *clip = ui_widget_child(pane, "clip"),
              *space = clip ? ui_widget_child(clip, "space") : NULL;
    for (int row = 0; row < n; row++) {
        ui_widget *w = s->rows[tab_rows[tab][row]];
        if (!w || !w->visible)
            continue;
        w->y = (float)(shown * pitch);
        shown++;
    }
    /* Keep the pane at the closed-size height on Tweaks: toggling the group
       must not move the title or the Cancel/OK buttons. */
    int fit = shown;
    if (tab == VIDEO_TAB_COUNT - 1)
        fit = VIDEO_TWEAKS_VIEW_ROWS;
    float y = (float)(top + fit * pitch + 12);
    ui_widget *load = ui_widget_child(pane, "LoadPreset"),
              *save_button = ui_widget_child(pane, "SavePreset");
    if (load && save_button) {
        load->y = save_button->y = y;
        if (tab == VIDEO_TAB_PRESETS)
            y += 36;
    }
    ui_widget *buttons = ui_widget_child(pane, "Buttons"), *bg = ui_widget_child(pane, "bg"),
              *body = ui_widget_child(pane, "PCBody");
    if (buttons) {
        buttons->y = y;
        pane->h = y + 38;
        pane->y = fmaxf(0, pc_pane_center - pane->h * .5f);
        if (bg)
            bg->h = pane->h;
        if (body)
            body->h = pane->h - 22;
        if (clip && space) {
            clip->y = (float)top;
            clip->w = 560;
            clip->h = (float)(fit * pitch + 6);
            space->h = fmaxf(clip->h, (float)(shown * pitch));
            /* The Tweaks list is the one page that scrolls inside its viewport;
               mark it on the left edge, out of the labels' way. */
            clip->scroll_indicator = tab == VIDEO_TAB_COUNT - 1;
        }
    }
}
/* Keep the focused row inside the Tweaks viewport: the pad's Up/Down move the
   focus through the same generic path as every page, so the viewport follows it
   instead of adding a second scroll mechanism. */
static int row_has_focus(const ui_widget *w)
{
    if (!w)
        return 0;
    if (w->focused)
        return 1;
    for (int i = 0; i < w->nkids; i++)
        if (row_has_focus(w->kids[i]))
            return 1;
    return 0;
}
void video_settings_scroll_tick(void)
{
    video_settings *s = settings_active;
    if (!s || !s->page)
        return;
    ui_widget *pane = ui_widget_child(s->page, "Pane");
    ui_widget *clip = pane ? ui_widget_child(pane, "clip") : NULL;
    ui_widget *space = clip ? ui_widget_child(clip, "space") : NULL;
    if (!clip || !space || space->h <= clip->h)
        return;
    float row_y = -1, row_h = 0;
    int focused = -1;
    for (int i = 0; i < (int)(sizeof s->rows / sizeof s->rows[0]); i++) {
        ui_widget *w = s->rows[i];
        if (!w || !w->visible || !row_has_focus(w))
            continue;
        row_y = w->y;
        row_h = w->h;
        focused = i;
        break;
    }
    if (row_y < 0)
        return;
    /* The wheel changes the viewport without moving focus; only a focus move
       (the pad) pulls the viewport after it. */
    if (focused == s->scroll_focus_row)
        return;
    s->scroll_focus_row = focused;
    float target = space->scroll_set ? space->scroll_ty : space->y;
    if (row_y + target < 0)
        target = -row_y;
    else if (row_y + row_h + target > clip->h)
        target = clip->h - row_y - row_h;
    target = fminf(0, fmaxf(clip->h - space->h, target));
    if (fabsf(target - (space->scroll_set ? space->scroll_ty : space->y)) < .5f)
        return;
    space->scroll_ty = target;
    if (!space->scroll_set)
        space->scroll_tx = space->x;
    space->scroll_set = 1;
}
#endif
#endif
static void update_field_size_ui(video_settings *s)
{
#ifndef __SWITCH__
    if (!s || !s->field_size_row)
        return;
    int toggle = tweak_row_index(VS_PHANTOM_OPPONENTS), field = tweak_row_index(VS_FIELD_SIZE);
    int on = s->choices[toggle] ? s->choices[toggle]->focus_index != 0 : s->current.phantom_opponents;
    int index = s->choices[field] ? s->choices[field]->focus_index : video_setting_get(s, VS_FIELD_SIZE);
    int value = MOON_CONDUCTOR_CARS + index;
    int warn = video_settings_field_size_warning(value);
    s->field_size_row->insensitive = !on;
    s->field_size_row->opacity = on ? 1.f : .4f;
    if (s->field_size_label) {
        if (warn) {
            const uint8_t red[] = {235, 54, 54, 255};
            s->field_size_label->has_runtime_color = 1;
            memcpy(s->field_size_label->runtime_color, red, 4);
            text(s->field_size_label, "Field Size - Experimental above 20 cars");
        } else {
            s->field_size_label->has_runtime_color = 0;
            text(s->field_size_label, "Field Size");
        }
        if (s->owner)
            ui_text_adjust_scale(menu_ui(s->owner), s->field_size_label);
    }
    for (int i = 0; i < 4; i++)
        if (s->field_size_warn[i])
            s->field_size_warn[i]->visible = warn;
#endif
}

static void tweaks_visibility(video_settings *s)
{
#ifndef __SWITCH__
    if (s->active_tab != VIDEO_TAB_COUNT - 1)
        return;
    int open = s->tweaks_open;
    for (int i = 42; i <= 50; i++)
        if (s->rows[i])
            s->rows[i]->visible = open;
    int fov = s->choices[42]->focus_index, blur = s->choices[48]->focus_index;
    s->rows[43]->insensitive = !fov;
    s->rows[43]->opacity = fov ? 1.f : .4f;
    s->rows[49]->insensitive = !blur;
    s->rows[49]->opacity = blur ? 1.f : .4f;
#ifndef __ANDROID__
    layout_tab_rows(s, s->active_tab);
#endif
    update_field_size_ui(s);
#endif
}

static void select_tab(video_settings *s, int tab)
{
    s->active_tab = tab;
    s->scroll_focus_row = -1;
    const int always = -1; /* Android: the preset row is in Display; PC: its own page */
    for (int i = 0; i < (int)(sizeof s->choices / sizeof s->choices[0]); i++)
        if (s->rows[i])
            s->rows[i]->visible = i == always;
    int count = 0;
    while (count < 15 && tab_rows[tab][count] >= 0)
        count++;
#ifdef __ANDROID__
    float pitch = android_row_pitch(s->rows[tab_rows[tab][0]], count);
#endif
    for (int row = 0; row < count; row++) {
        ui_widget *w = s->rows[tab_rows[tab][row]];
        w->visible = 1;
#ifdef __ANDROID__
        w->h = pitch;
        w->y = 64.f + row * pitch;
#endif
    }
#ifndef __ANDROID__
    ui_widget *pane = ui_widget_child(s->page, "Pane");
    ui_widget *title = ui_widget_child(pane, "Title");
    for (int i = 0; title && i < title->nkids; i++)
        if (title->kids[i]->text || title->kids[i]->text_key)
            text(title->kids[i], page_titles[tab]);
    layout_tab_rows(s, tab);
    tweaks_visibility(s);
    menu_native_focus(s->owner, s->choices[tab_rows[tab][0]]);
    return;
#endif
    for (int i = 0; i < VIDEO_TAB_COUNT; i++) {
        s->tabs[i]->active = i == tab;
        ui_widget *label = ui_widget_child(s->tabs[i], "label");
        if (label) {
            label->has_runtime_color = 1;
            const uint8_t selected[] = {130, 86, 12, 255};
            const uint8_t normal[] = {0, 0, 0, 255};
            memcpy(label->runtime_color, i == tab ? selected : normal, 4);
        }
    }
    menu_native_focus(s->owner, s->choices[tab_rows[tab][0]]);
}
static int tab_activate(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv, ah_val *ret)
{
    (void)vm;
    (void)argc;
    (void)argv;
    video_settings *s = data;
    for (int i = 0; i < VIDEO_TAB_COUNT; i++)
        if (ah_host_data(self) == s->tabs[i]) {
            select_tab(s, i);
            menu_play_effect(s->owner, "select");
        }
    *ret = ah_int(2);
    return 0;
}
static int tab_key(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv, ah_val *ret)
{
    (void)self;
    video_settings *s = data;
    ah_val key;
    *ret = ah_int(0);
    if (argc < 2 || !ah_get_member(vm, argv[1].u.o, "keysym", &key))
        return 0;
    if (key.u.i != 0xffc8 && key.u.i != 0xffd2)
        return 0;
    /* Leave open dropdown navigation with its original widget handler. */
    for (int i = 0; i < (int)(sizeof s->choices / sizeof s->choices[0]); i++)
        if (s->choices[i] && s->choices[i]->option_open)
            return 0;
    select_tab(s, (s->active_tab + (key.u.i == 0xffc8 ? VIDEO_TAB_COUNT - 1 : 1)) % VIDEO_TAB_COUNT);
    menu_play_effect(s->owner, "cursor");
    *ret = ah_int(2);
    return 0;
}
#endif
#if !defined(__SWITCH__) && !defined(__ANDROID__)
/* PC Tweaks: Up/Down step through the rows this page shows. The rows beyond
   the viewport are clipped out of the generic navigation collector, so the
   page moves focus itself and the shared clip/space follows it. Dropdowns
   keep their own Up/Down while open. */
static int tweak_key(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv,
                     ah_val *ret)
{
    (void)self;
    *ret = ah_int(0);
    video_settings *s = data;
    if (!s || s->active_tab != VIDEO_TAB_COUNT - 1 || argc < 2)
        return 0;
    ah_val key;
    if (!ah_get_member(vm, argv[1].u.o, "keysym", &key))
        return 0;
    int sym = (int)key.u.i;
    if (sym != 0xffd2 && sym != 0xffd4) /* Up, Down */
        return 0;
    for (int i = 0; i < (int)(sizeof s->choices / sizeof s->choices[0]); i++)
        if (s->choices[i] && s->choices[i]->option_open)
            return 0;
    ah_val widget;
    if (!ah_get_member(vm, argv[1].u.o, "widget", &widget))
        return 0;
    ui_widget *focused = ah_host_data(widget.u.o);
    int at = -1;
    for (int i = 0; i < (int)(sizeof s->rows / sizeof s->rows[0]); i++) {
        ui_widget *row = s->rows[i];
        if (!row || !row->visible)
            continue;
        for (ui_widget *p = focused; p; p = p->parent)
            if (p == row) {
                at = i;
                break;
            }
        if (at >= 0)
            break;
    }
    if (at < 0)
        return 0;
    int n = 0, pos = -1;
    const int *rows = tab_rows[VIDEO_TAB_COUNT - 1];
    while (n < 15 && rows[n] >= 0)
        n++;
    for (int i = 0; i < n; i++)
        if (rows[i] == at)
            pos = i;
    if (pos < 0)
        return 0;
    int step = sym == 0xffd2 ? -1 : 1;
    for (int i = pos + step; i >= 0 && i < n; i += step) {
        ui_widget *row = s->rows[rows[i]];
        if (!row || !row->visible || row->insensitive)
            continue;
        menu_native_focus(s->owner, s->choices[rows[i]]);
        menu_play_effect(s->owner, "cursor");
        *ret = ah_int(2);
        break;
    }
    return 0;
}
#endif
static int enter(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv, ah_val *ret)
{
    (void)vm;
    (void)self;
    (void)argc;
    (void)argv;
    video_settings *s = data;
#if !defined(__SWITCH__) && !defined(__ANDROID__)
    SDL_Window *window = plat_sdl_window();
    if (window)
        s->current.video.fullscreen = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
#endif
    s->draft = s->current;
    s->preset_draft = -1;
    snprintf(s->personal_preset, sizeof s->personal_preset, "%s",
             video_settings_builtin_preset(s->store.preset) < 0 ? s->store.preset : "");
    s->personal_modified=s->store.modified;
    s->message[0] = 0;
    s->choices[0]->focus_index = s->current.aspect;
    s->choices[1]->focus_index = s->current.video.fullscreen;
    int size = 6;
    for (int i = 0; i < (int)(sizeof sizes / sizeof sizes[0]); i++)
        if (sizes[i][0] == s->current.video.width && sizes[i][1] == s->current.video.height)
            size = i;
    ui_widget *custom = ui_option_item(s->choices[2], 6);
    custom->visible = size == 6;
    char resolution[64];
    snprintf(resolution, sizeof resolution, "%d x %d", s->current.video.width,
             s->current.video.height);
    text(ui_widget_child(custom, "label"), resolution);
    s->choices[2]->focus_index = size;
    s->choices[3]->focus_index = s->current.filter;
    s->choices[4]->focus_index = (s->current.text + 12) / 25;
    s->choices[5]->focus_index = s->current.video.vsync;
    s->choices[6]->focus_index = s->current.video.upscale;
#ifndef __SWITCH__
#ifndef __ANDROID__
    s->choices[31]->focus_index = s->current.preset_tools;
    s->choices[32]->focus_index = s->current.save_tools;
    s->choices[17]->focus_index = s->current.antialiasing;
    s->choices[18]->focus_index = video_setting_get(s, VS_SCALE);
    s->choices[19]->focus_index = s->current.texture_smoothing;
    s->choices[20]->focus_index = s->current.texture_filtering;
    s->choices[21]->focus_index = s->current.car_shadows;
    s->choices[22]->focus_index = s->current.material_shading;
    s->choices[23]->focus_index = s->current.reflection_quality;
    s->choices[24]->focus_index = s->current.shadow_maps;
    s->choices[25]->focus_index = s->current.modern_lighting;
    s->choices[26]->focus_index=s->current.daylight;s->choices[27]->focus_index=s->current.solar_hour;s->choices[28]->focus_index=s->current.solar_cycle;s->choices[29]->focus_index=s->current.hdr;s->choices[30]->focus_index=s->current.sun_rays;s->choices[33]->focus_index=s->current.ambient_occlusion;
    s->choices[41]->focus_index = s->tweaks_open;
    s->choices[42]->focus_index = s->current.speed_fov;
    {
        int n = 0;
        for (int k = 0; k < 12; k++)
            if (vs_fov_degrees_values[k] <= s->current.speed_fov_degrees) n = k;
        s->choices[43]->focus_index = n;
    }
    s->choices[44]->focus_index = s->current.speed_cam_lag;
    s->choices[45]->focus_index = s->current.speed_shake;
    s->choices[46]->focus_index = s->current.speed_wind;
    s->choices[47]->focus_index = s->current.speed_road_roar;
    s->choices[48]->focus_index = s->current.speed_blur;
    {
        int n = 0;
        for (int k = 0; k < 10; k++)
            if (vs_blur_amount_values[k] <= s->current.speed_blur_amount) n = k;
        s->choices[49]->focus_index = n;
    }
    s->choices[50]->focus_index = s->current.speed_vignette;
    s->choices[51]->focus_index = s->current.discord_presence;
    s->choices[52]->focus_index = s->current.skip_intro;
    s->choices[53]->focus_index = s->current.assist_line;
    s->choices[tweak_row_index(VS_PHANTOM_OPPONENTS)]->focus_index = s->current.phantom_opponents;
    s->choices[tweak_row_index(VS_FIELD_SIZE)]->focus_index = video_setting_get(s, VS_FIELD_SIZE);
#endif
    s->choices[7]->focus_index = s->current.desktop;
    s->choices[9]->focus_index = s->current.edge_pan;
    s->choices[10]->focus_index = s->current.input_hints;
    s->choices[13]->focus_index = s->current.main_menu;
    s->choices[14]->focus_index = s->current.smooth_cursor;
    s->choices[15]->focus_index = s->current.hover_feedback;
    s->choices[16]->focus_index = s->current.compact_cars;
    s->choices[11]->focus_index = s->current.spatial_visibility;
    s->choices[12]->focus_index = s->current.resident_reflections;
#ifndef __ANDROID__
    s->choices[34]->focus_index = video_setting_get(s, VS_CUSTOM_MUSIC);
    s->choices[35]->focus_index = video_setting_get(s, VS_JUKEBOX_FOLDERS);
    s->choices[36]->focus_index = video_setting_get(s, VS_JUKEBOX_GROUPS);
    s->choices[37]->focus_index = s->current.graphics_swap;
    s->choices[40]->focus_index=s->current.night_sky;
    s->choices[56]->focus_index=s->current.shadow_filter;
    s->choices[39]->focus_index = s->current.car_lamps;
    s->choices[38]->focus_index = s->current.controller_tiles;
#endif
    s->choices[8]->focus_index = s->personal_preset[0] || s->store.modified ? VS_MENU_MODIFIED : video_settings_builtin_preset(s->store.preset);
    char caption[96];
    snprintf(caption,sizeof caption,"%s%s",s->personal_preset[0] ? s->personal_preset : "Modified",s->personal_preset[0] && s->personal_modified ? " (modified)" : "");
    text(ui_widget_child(ui_option_item(s->choices[8],VS_MENU_MODIFIED),"label"),caption);
#endif
#ifdef __SWITCH__
    s->choices[7]->focus_index=s->current.scene_resolution;
    s->choices[9]->focus_index = s->current.touch_menu;
    s->choices[8]->focus_index=s->store.modified?2:strcmp(s->store.preset,"Classic")!=0;
#endif
#ifdef __ANDROID__
    s->choices[7]->focus_index = s->current.touch_menu;
    s->choices[9]->focus_index = s->current.mobile.enabled;
    s->choices[10]->focus_index = s->current.mobile.size;
    s->choices[13]->focus_index = s->current.mobile.opacity;
    s->choices[14]->focus_index = s->current.mobile.layout;
    s->choices[15]->focus_index = s->current.mobile.steering;
    s->choices[17]->focus_index = s->current.mobile.render_scale;
#endif
    quality(&s->current);
#if !defined(__SWITCH__) && !defined(__ANDROID__)
    /* the Options tile that opened the page; MOON_SETTINGS_PAGE for the tests */
    int page = 0;
    const char *tile = menu_root_focus_name(s->owner, s->option_root);
    for (int i = 1; i < VIDEO_TAB_COUNT; i++)
        if (tile && !strcmp(tile, page_tiles[i]))
            page = i;
    if (getenv("MOON_SETTINGS_PAGE"))
        page = abs(atoi(getenv("MOON_SETTINGS_PAGE"))) % VIDEO_TAB_COUNT;
    fprintf(stderr, "video settings: page %s\n", page_titles[page]);
    if (getenv("MOON_SETTINGS_TWEAKS_CLOSED")) {
        s->tweaks_open = 0;
        s->choices[41]->focus_index = 0;
    }
    select_tab(s, page);
    { /* tests: MOON_SETTINGS_ROW focuses one row (and the viewport follows it) */
        const char *row = getenv("MOON_SETTINGS_ROW");
        if (row) {
            int at = atoi(row);
            if (at >= 0 && at < (int)(sizeof s->choices / sizeof s->choices[0]) &&
                s->choices[at]) {
                menu_native_focus(s->owner, s->choices[at]);
            }
        }
    }
#elif !defined(__SWITCH__)
    select_tab(s, 0);
#else
    menu_native_focus(s->owner, s->choices[0]);
#endif
#ifdef _WIN32
    ui_widget *pane=ui_widget_child(s->page,"Pane");
    ui_widget *load_button=ui_widget_child(pane,"LoadPreset"), *save_button=ui_widget_child(pane,"SavePreset");
    int files = s->draft.preset_tools && s->active_tab == VIDEO_TAB_PRESETS;
    if (load_button) load_button->visible=files;
    if (save_button) save_button->visible=files;
#endif
#ifndef __SWITCH__
    ui_set_tooltips(s->page, hints[tab_rows[s->active_tab][0]]);
#else
    ui_set_tooltips(s->page, hints[0]);
#endif
    *ret = ah_int(0);
    return 0;
}
#ifdef _WIN32
static int preset_button_key(ah_vm *vm, ah_obj *self, void *data, int argc,
                              const ah_val *argv, ah_val *ret)
{
    video_settings *s=data; ah_val key;
    *ret=ah_int(0);
    if (argc<2 || !ah_get_member(vm,argv[1].u.o,"keysym",&key)) return 0;
    ui_widget *pane=ui_widget_child(s->page,"Pane");
    if (key.u.i==0xff51 || key.u.i==0xff53) {
        menu_native_focus(s->owner,ui_widget_child(pane,key.u.i==0xff51 ? "LoadPreset" : "SavePreset"));
    } else if (key.u.i==0xff52) {
        int row=0;
        while (row<14 && tab_rows[s->active_tab][row+1]>=0) row++;
        menu_native_focus(s->owner,s->choices[tab_rows[s->active_tab][row]]);
    } else if (key.u.i==0xff54) {
        ui_widget *buttons=ui_widget_child(pane,"Buttons");
        ui_widget *sender=ah_host_data(self);
        menu_native_focus(s->owner,ui_widget_child(buttons,!strcmp(sender->name,"LoadPreset") ? "Cancel" : "OK"));
    } else return 0;
    *ret=ah_int(2);
    return 0;
}
static int preset_file_action(ah_vm *vm, ah_obj *self, void *data, int argc,
                              const ah_val *argv, ah_val *ret)
{
    video_settings *s=data;
    int exporting=!strcmp(((ui_widget *)ah_host_data(self))->name,"SavePreset");
    read_draft(s);
    if (video_settings_preset_dialog(s,exporting,1)) {
        pc_video_options current=s->current;
        moon_options store=s->store;
        char name[64]; snprintf(name,sizeof name,"%s",s->personal_preset);
        s->current=s->draft;
        snprintf(s->store.preset,sizeof s->store.preset,"%s",name);
        s->store.modified=0;
        enter(vm,self,data,argc,argv,ret);
        s->current=current;
        s->store=store;
        snprintf(s->personal_preset,sizeof s->personal_preset,"%s",name);
        s->preset_draft=-1;
        ui_set_tooltips(s->page, exporting ? "Preset saved. OK applies the current draft; Cancel keeps your settings." : "Preset loaded as a preview. Review the options, then OK to apply or Cancel.");
    } else if (s->message[0]) ui_set_tooltips(s->page,s->message);
    *ret=ah_int(2);
    return 0;
}
#endif
static int leave(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv, ah_val *ret)
{
    (void)vm;
    (void)self;
    (void)argc;
    (void)argv;
    quality(&((video_settings *)data)->current);
    *ret = ah_int(0);
    return 0;
}
static int cancel(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv, ah_val *ret)
{
    (void)vm;
    (void)self;
    (void)argc;
    (void)argv;
    video_settings *s = data;
    menu_play_effect(s->owner, "cancel");
    quality(&s->current);
    menu_native_page(s->owner, s->option_root);
    *ret = ah_int(2);
    return 0;
}
static int apply(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv, ah_val *ret)
{
    (void)vm;
    (void)self;
    (void)argc;
    (void)argv;
    video_settings *s = data;
    read_draft(s);
    SDL_Window *window = plat_sdl_window();
#if !defined(__SWITCH__) && !defined(__ANDROID__)
    /* Alt+Enter can change the host while this draft is open. Preserve it
       unless the player explicitly edited Window Mode in this page. */
    if (window && s->draft.video.fullscreen == s->current.video.fullscreen)
        s->draft.video.fullscreen = s->current.video.fullscreen =
            (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
#endif
    int mode_changed = s->draft.video.fullscreen != s->current.video.fullscreen;
    int size_changed = s->draft.video.width != s->current.video.width ||
                       s->draft.video.height != s->current.video.height;
    if (mode_changed &&
        SDL_SetWindowFullscreen(window, s->draft.video.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP
                                                                  : 0) < 0) {
        ui_set_tooltips(s->page, "The display mode could not be changed.");
        *ret = ah_int(2);
        return 0;
    }
    /* Keep user maximization and manual resizing when resolution is unchanged. */
    if (!s->draft.video.fullscreen && size_changed) {
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED)
            SDL_RestoreWindow(window);
        SDL_SetWindowSize(window, s->draft.video.width, s->draft.video.height);
    }
    video_performance_options(s->draft.low_latency, s->draft.fps_limit);
#ifndef __SWITCH__
    r3d_scene_options(s->draft.spatial_visibility, s->draft.resident_reflections);
#endif
    if (!video_set_vsync(s->draft.video.vsync))
        s->draft.video.vsync = s->current.video.vsync;
    if (!video_set_upscale(s->draft.video.upscale))
        s->draft.video.upscale = 0;
#ifdef __SWITCH__
    video_switch_scene_resolution(s->draft.scene_resolution);
#endif
    video_set_aspect(s->draft.aspect);
    race_hud_set_widescreen(s->draft.aspect); /* D_006184F0 */
    s->current = s->draft;
#ifndef __SWITCH__
    if(!s->current.input_hints) ui_input_hints(menu_ui(s->owner),0);
#endif
    quality(&s->current);
#if !defined(__SWITCH__) && !defined(__ANDROID__)
    moon_options_set(&s->store, "audio.custom_music", s->choices[34]->focus_index ? "1" : "0");
    moon_options_set(&s->store, "audio.jukebox_folders", s->choices[35]->focus_index ? "1" : "0");
    moon_options_set(&s->store, "audio.jukebox_groups", s->choices[36]->focus_index ? "1" : "0");
#endif
    save(s);
    apply_discord(s);
    if (s->message[0])
        ui_set_tooltips(s->page, s->message);
    else {
        menu_play_effect(s->owner, "select");
        menu_native_page(s->owner, s->option_root);
    }
    *ret = ah_int(2);
    return 0;
}
void video_settings_attach(video_settings *s, menu *m)
{
    ui_project *ui = menu_ui(m);
    s->root = s->option_root = -1;
    for (int i = 0; i < ui_root_count(ui); i++) {
        if (!strcmp(ui_root_name(ui, i), "ScreenRoot"))
            s->root = i;
        if (!strcmp(ui_root_name(ui, i), "OptionRoot"))
            s->option_root = i;
    }
    if (s->root < 0 || s->option_root < 0)
        return;
    s->owner = m;
    s->page = ui_root_widget(ui, s->root);
    ui_widget *pane = ui_widget_child(s->page, "Pane"), *row = ui_widget_child(pane, "Wide"),
              *buttons = ui_widget_child(pane, "Buttons");
    if (!pane || !row || !buttons)
        return;
    /* Reuse the authored Screen Settings pane, frame, fonts, dropdowns and
       Cancel/OK buttons. Keep its authored centre clear of the tooltip bar. */
    float pane_center = pane->y + pane->h * .5f;
    ui_widget *space = NULL;
#ifdef __SWITCH__
    const int rows = 10;
#else
#ifdef __ANDROID__
    const int rows = 18;
#else
    const int rows = 43;
#endif
#endif
#ifndef __SWITCH__
    /* The PC page owns its row positions. Authored VBox/FBox packing would
       restore 32-pixel rows and push the additional controls off-screen. */
    pane->type = "Composite";
    pane->x = 40;
    pane->w = 560;
    ui_widget *title = ui_widget_child(pane, "Title");
    if (title) {
        title->w = pane->w;
        for (int i = 0; i < title->nkids; i++)
            title->kids[i]->w = pane->w;
    }
#endif
#ifdef __SWITCH__
    int pitch = 29;
    pane->type = "Composite";
#elif defined(__ANDROID__)
    int pitch = 44;
    pane->type = "Composite";
#else
    int pitch = VIDEO_ROW_PITCH;
#endif
    pane->y = rows == 7 ? 78 : fmaxf(0, pane_center - 370 * .5f);
#if !defined(__SWITCH__) && !defined(__ANDROID__)
    pc_pane_center = pane_center;
#endif
    pane->h = rows == 7 ? 278 : 370;
    ui_widget *bg = ui_widget_child(pane, "bg");
    if (bg) {
        bg->h = pane->h;
        bg->w = pane->w;
    }
    const char *names[] = {"AspectRatio",
                           "WindowMode",
                           "Resolution",
                           "UIFilter",
                           "TextSharpness",
                           "VSync",
                           "Upscaling",
                           "DesktopInput",
                           "PCPreset",
                           "MapEdgePan",
                           "InputHints",
                           "SpatialVisibility",
                           "ResidentReflections",
                           "MainMenuOverview",
                           "SmoothCursor",
                           "HoverFeedback",
                           "CompactCars",
                           "AntiAliasing",
                           "RenderScale",
                           "TextureSmoothing", "TextureFiltering", "CarShadows", "MaterialShading", "ReflectionQuality", "ShadowMaps", "Lighting", "Daylight", "SolarTime", "DaylightCycle", "HDR", "SunRays", "PresetFiles", "SaveTransfer", "AmbientOcclusion",
                           "CustomMusic", "JukeboxFolders", "LibraryGroups", "GraphicsSwapKey",
                           "ControllerTiles", "CarLamps", "NightSky", "ShadowFilter", "PreviewMode"};
    const char *titles[] = {
        "Aspect Ratio",      "Window Mode",   "Resolution",    "UI Filter",
        "Text Clarity",      "VSync",         "Upscaling",     "Desktop Input",
        "PC Preset",         "Map Edge Pan",  "Input Hints",   "Spatial Visibility",
        "GPU Reflections",   "Main Menu",     "Smooth Cursor", "Hover Feedback",
        "Compact Car Strip", "Anti-Aliasing", "3D Resolution", "Texture Smoothing", "Texture Filtering", "Car Shadows", "Material Shading", "Reflection Quality", "Shadow Maps", "Realistic Lighting", "Realistic Skybox", "Solar Time", "Sun / Moon Cycle", "HDR", "Sun Rays", "Preset Files", "Save Transfer", "Ambient Occlusion",
        "Custom Music", "Jukebox Folders", "Library Groups", "Graphics Swap Key",
        "Controller Tiles", "Realistic Car Lamps", "Night Sky", "Shadow Filter", "Preview Mode"};
    ui_widget *sample = ui_widget_copy(ui, row);
    for (int i = pane->nkids - 1; i >= 0; i--) {
        ui_widget *w = pane->kids[i];
        if (w != bg && w != buttons && strcmp(w->name, "Title"))
            ui_widget_detach(w);
    }
    ui_widget_detach(buttons);
#ifndef __SWITCH__
    ui_widget *body=ui_widget_from_text(ui,s->root,
        "ColorFace { name string {\"PCBody\"} geometry rectangle {2 20 556 348} color [1] { RGBA {150 150 155 255} } }");
    ui_widget_append(pane,body);
    /* One viewport for the option rows, shared by every PC page. The generic
       clip/space pair is the same scroll the rest of the UI uses: the wheel and
       touch drags move the space, and the title/footer stay outside it. */
    ui_widget *clip = ui_widget_from_text(ui, s->root,
        "Composite { name string {\"clip\"} geometry rectangle {0 38 560 204} clip digit {1} }");
    space = ui_widget_from_text(ui, s->root,
        "Composite { name string {\"space\"} geometry rectangle {0 0 560 204} }");
    if (clip && space) {
        ui_widget_append(clip, space);
        ui_widget_append(pane, clip);
    } else
        space = NULL;

#endif
    const char *ratios[] = {"4:3", "16:9"};
    const char *modes[] = {"Windowed", "Full Screen"},
               *resolutions[] = {"1280 x 720",  "1280 x 960",  "1600 x 900", "1920 x 1080",
                                 "2560 x 1440", "3840 x 2160", "Custom"};
    const char *filters[] = {"Smooth", "Sharp", "Original"},
               *clarity[] = {"0%", "25%", "50%", "75%", "100%"}, *toggle[] = {"Off", "On"};
    const char *upscalers[] = {"Native", "FSR Ultra Quality", "FSR Quality", "FSR Balanced",
                               "FSR Performance"};
#if VS_MENU_MODIFIED == 4
    const char *presets[] = {"Classic", "Enhanced", "Performance", "Toaster", "Modified"};
#elif VS_MENU_MODIFIED == 3
    const char *presets[] = {"Classic", "Enhanced", "Performance", "Modified"};
#else
    const char *presets[] = {"Classic", "Enhanced", "Modified"};
#endif
    const char *controller_tiles[] = {"Original", "By Player"};
    const char *const *lists[] = {
        ratios, modes,  resolutions, filters, clarity, toggle, upscalers, toggle, presets,  toggle,
        toggle, toggle, toggle,      toggle,  toggle,  toggle, toggle,    vs_aa,  vs_scale, toggle, vs_texture_filtering, vs_car_shadows, vs_material_shading, vs_reflection_quality, vs_shadow_maps, vs_modern_lighting,vs_off_on,vs_hours,vs_cycle,vs_off_on,vs_rays,toggle,toggle,toggle,toggle,toggle,toggle,toggle,controller_tiles,toggle,toggle,vs_shadow_filter,vs_preview_mode};
    int counts[] = {2, 2, 7, 3, 5, 2, 5, 2, VS_MENU_MODIFIED + 1, 9, 2, 2, 2, 2, 2, 2, 2, 5, 6, 2, 5, 2, 2, 3, 4, 2, 2, 25, 3, 2, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2};
#ifdef __SWITCH__
    const char *scene_sizes[]={"Classic","Balanced","Performance"};
    names[7]="SceneResolution"; titles[7]="3D Resolution"; titles[8]="Preset";
    lists[7]=scene_sizes;counts[7]=3;
    names[9] = "TouchMenus";
    titles[9] = "Touch Menus";
#endif
#ifdef __ANDROID__
    const char *mobile_sizes[] = {"Small", "Medium", "Large"};
    const char *mobile_opacity[] = {"20%", "40%", "60%", "80%", "100%"};
    const char *mobile_layout[] = {"Steering left", "Steering right"};
    const char *mobile_steer[] = {"Analog drag", "Digital"};
    const char *android_scales[] = {"100%", "75%", "50%", "35%"};
    names[17] = "AndroidRenderScale";
    titles[17] = "3D Resolution";
    lists[17] = android_scales;
    counts[17] = 4;
    titles[7] = "Touch Menus";
    titles[9] = "Race Touch Controls";
    titles[10] = "Control Size";
    titles[13] = "Control Opacity";
    titles[14] = "Control Layout";
    titles[15] = "Steering";
    titles[8] = "Preset";
    lists[10] = mobile_sizes;
    counts[10] = 3;
    lists[13] = mobile_opacity;
    counts[13] = 5;
    lists[14] = mobile_layout;
    lists[15] = mobile_steer;
#endif
    for (int i = 0; i < rows; i++) {
        /* row 41 of the list is Shadow Filter, kept after the Tweaks rows 41..55 */
        const int at = i < 41 ? i : i == 41 ? 56 : 57;
        ui_widget *w = ui_widget_copy(ui, sample);
        w->name = names[i];
#ifndef __SWITCH__
        w->type = "Composite";
        w->h = (float)pitch;
        s->rows[at] = w;
        w->x = 24;
        widen(w,512);
#endif
#ifdef __SWITCH__
        w->h = (float)pitch;
#endif
        w->y = 0;
        text(ui_widget_child(w, "label"), titles[i]);
        ui_widget_append(space ? space : pane, w);
        ui_widget *choice = ui_widget_child(w, "OptionMenu");
        s->choices[at] = choice;
#ifndef __SWITCH__
        choice->x=256;
        widen(choice,256);
        /* P: PC labels use the authored row's available column width. */
        ui_widget *label = ui_widget_child(w, "label");
        if (label) {
            label->w = choice->x - label->x - 8;
            ui_text_adjust_scale(ui, label);
        }
#endif
        values(ui, choice, lists[i], counts[i]);
#ifndef __SWITCH__
        ui_widget *popup=ui_widget_child(choice,"popup");
        ui_widget *box=popup?ui_widget_child(popup,"vbox"):NULL;
        ui_widget *popup_bg=box?ui_widget_child(box,"bg"):NULL;
        if(popup_bg) {
            popup_bg->type="ColorFace";
            popup_bg->has_runtime_color=1;
            const uint8_t gray[]={150,150,155,255};
            memcpy(popup_bg->runtime_color,gray,4);
        }
#endif
        menu_native_method(m, choice, "onValueChanged", changed, s);
        menu_native_method(m, choice, "onFocusEnter", focus, s);
    }
#ifndef __SWITCH__
    {
        const int tweaks = video_settings_tweak_count();
        for (int i = 0; i < tweaks; i++) {
            int idx = 41 + i;
            const video_setting_info *in = video_settings_tweak(i);
            if (!in)
                continue;
            ui_widget *w = ui_widget_copy(ui, sample);
            w->name = in->label;
            w->type = "Composite";
            w->h = (float)pitch;
            s->rows[idx] = w;
            w->x = 24;
            widen(w, 512);
            w->y = 0;
            text(ui_widget_child(w, "label"), in->label);
            ui_widget_append(space ? space : pane, w);
            ui_widget *choice = ui_widget_child(w, "OptionMenu");
            s->choices[idx] = choice;
            choice->x = 256;
            widen(choice, 256);
            ui_widget *label = ui_widget_child(w, "label");
            if (label) {
                label->w = choice->x - label->x - 8;
                ui_text_adjust_scale(ui, label);
            }
            values(ui, choice, in->names, in->count);
            if (idx == 41)
                choice->focus_index = s->tweaks_open;
            if (idx == tweak_row_index(VS_FIELD_SIZE)) {
                static const char *const edge_names[4] = {"warn_top", "warn_bottom",
                                                          "warn_left", "warn_right"};
                const uint8_t red[] = {235, 54, 54, 255};
                s->field_size_row = w;
                s->field_size_label = label;
                ui_widget *bg = ui_widget_child(w, "bg");
                for (int e = 0; e < 4 && bg; e++) {
                    ui_widget *edge = ui_widget_copy(ui, bg);
                    edge->name = edge_names[e];
                    edge->has_geometry = 1;
                    edge->visible = 0;
                    edge->has_runtime_color = 1;
                    memcpy(edge->runtime_color, red, 4);
                    edge->x = e == 3 ? 511.f : 0.f;
                    edge->y = e == 1 ? 21.f : 0.f;
                    edge->w = e < 2 ? 512.f : 1.f;
                    edge->h = e < 2 ? 1.f : 22.f;
                    ui_widget_append(w, edge);
                    s->field_size_warn[e] = edge;
                }
            }
            ui_widget *popup = ui_widget_child(choice, "popup");
            ui_widget *box = popup ? ui_widget_child(popup, "vbox") : NULL;
            ui_widget *popup_bg = box ? ui_widget_child(box, "bg") : NULL;
            if (popup_bg) {
                popup_bg->type = "ColorFace";
                popup_bg->has_runtime_color = 1;
                const uint8_t gray[] = {150, 150, 155, 255};
                memcpy(popup_bg->runtime_color, gray, 4);
            }
            menu_native_method(m, choice, "onValueChanged", changed, s);
            menu_native_method(m, choice, "onFocusEnter", focus, s);
        }
    }
#ifdef __ANDROID__
    s->rows[8]->y = 64;
#else
    s->rows[8]->y = 60;
#endif
#ifdef __ANDROID__
    const char *tab_names[] = {"DisplayTab", "RenderingTab", "ControlsTab"};
    const char *tab_titles[] = {"Display", "Rendering", "Controls"};
    const int tab_count = VIDEO_TAB_COUNT;
#else
    const char *tab_names[] = {""};
    const char *tab_titles[] = {""};
    const int tab_count = 0;
#endif
    ui_widget *button_sample = ui_widget_child(buttons, "OK");
    for (int i = 0; i < tab_count; i++) {
        ui_widget *tab = ui_widget_copy(ui, button_sample);
        tab->name = tab_names[i];
        const float widths[] = {72, 88, 80, 80, 110, 80};
        float width = VIDEO_TAB_COUNT == 6 ? widths[i] : 160;
        widen(tab, width);
        tab->x = 24 + i * 176;
        if (VIDEO_TAB_COUNT == 6) {
            tab->x = 5;
            for (int j = 0; j < i; j++) tab->x += widths[j] + 8;
        }
        tab->y = 24;
        ui_widget *tab_label=ui_widget_child(tab,"label");
        text(tab_label,tab_titles[i]);
        if (VIDEO_TAB_COUNT==6 && tab_label) {
            /* Keep an inset for the rounded face and actual gaps between tabs.
               Font textures may not be resident yet, so do not rely on a
               measurement-only fit during attachment. */
            tab_label->x=6; tab_label->y=0;
            tab_label->w=width-12; tab_label->h=tab->h;
            tab_label->has_geometry=1;
            tab_label->has_runtime_align=1; tab_label->runtime_align=4;
            tab_label->scale_x*=.85f; tab_label->scale_y*=.85f;
        }
        ui_text_adjust_scale(ui,tab_label);
        ui_widget_append(pane, tab);
        s->tabs[i] = tab;
        menu_native_method(m, tab, "onActivate", tab_activate, s);
    }
    /* The authored footer background was laid out for a 320-wide pane.
       Moving only its parent leaves that retained rectangle behind. Match
       the PC body's inset and explicitly reallocate the decorative face. */
    buttons->x = body->x;
    buttons->w = body->w;
    ui_widget *footer_bg=ui_widget_child(buttons,"bg");
    if (footer_bg) {
        footer_bg->x=footer_bg->y=0;
        footer_bg->w=buttons->w;
        footer_bg->h=buttons->h;
        footer_bg->has_geometry=1;
    }
    ui_widget *cancel_button=ui_widget_child(buttons,"Cancel");
    ui_widget *ok_button=ui_widget_child(buttons,"OK");
    float left=(buttons->w-cancel_button->w-ok_button->w-16)*.5f;
    cancel_button->x=left;ok_button->x=left+cancel_button->w+16;
#ifdef __ANDROID__
    menu_native_method(m, s->page, "onKeyPress", tab_key, s);
#else
    menu_native_method(m, s->page, "onKeyPress", tweak_key, s);
#endif
#endif
#ifdef _WIN32
    /* Optional file actions reuse the original pill buttons, outside option rows. */
    for (int i=0;i<2;i++) {
        ui_widget *preset_button=ui_widget_copy(ui,ui_widget_child(buttons,"OK"));
        preset_button->name=i ? "SavePreset" : "LoadPreset";
        widen(preset_button,160);
        preset_button->x=104+i*176;
        preset_button->y=305;
        text(ui_widget_child(preset_button,"label"),i ? "Save Preset..." : "Load Preset...");
        ui_widget_append(pane,preset_button);
        menu_native_method(m,preset_button,"onActivate",preset_file_action,s);
        menu_native_method(m,preset_button,"onKeyPress",preset_button_key,s);
    }
#endif
    buttons->y = rows == 7 ? 244 : 332;
    ui_widget_append(pane, buttons);
    if (!video_fsr_available()) {
        /* No FSR on this platform (Android/Switch have no desktop GL 4.3 core
           context): the four FSR choices are refused out loud and greyed out
           instead of a silent no-op. */
        fprintf(stderr, "fsr: unavailable on this platform\n");
        for (int i = 1; i < 5; i++)
            ui_option_item(s->choices[6], i)->insensitive = 1;
    }
    menu_native_method(m, s->page, "onLoad", idle, s);
    menu_native_method(m, s->page, "onInitialize", idle, s);
    menu_native_method(m, s->page, "enter", enter, s);
    menu_native_method(m, s->page, "onFinalize", leave, s);
    menu_native_method(m, s->page, "onCancel", cancel, s);
    menu_native_method(m, ui_widget_child(buttons, "Cancel"), "onActivate", cancel, s);
    menu_native_method(m, ui_widget_child(buttons, "OK"), "onActivate", apply, s);
}

/* The two user-facing systems keep older preset keys as compatibility data. */
void video_options_realistic_skybox(pc_video_options *v, int enabled)
{
    v->daylight=v->night_sky=enabled!=0;
    v->solar_hour=enabled?24:5;
    v->solar_cycle=enabled!=0;
}
void video_options_realistic_lighting(pc_video_options *v, int enabled)
{
    int on=enabled!=0;
    v->modern_lighting=v->material_shading=v->hdr=v->car_shadows=v->ambient_occlusion=on;
    v->shadow_maps=on*2;
    v->sun_rays=0; /* clean atmosphere; shafts are not a global grey exposure lift */
}

int video_settings_speed_fov(void) { return settings_active ? settings_active->current.speed_fov : 0; }
int video_settings_speed_fov_degrees(void) { return settings_active ? settings_active->current.speed_fov_degrees : 0; }
int video_settings_speed_cam_lag(void) { return settings_active ? settings_active->current.speed_cam_lag : 0; }
int video_settings_speed_shake(void) { return settings_active ? settings_active->current.speed_shake : 0; }
int video_settings_speed_wind(void) { return settings_active ? settings_active->current.speed_wind : 0; }
int video_settings_speed_road_roar(void) { return settings_active ? settings_active->current.speed_road_roar : 0; }
int video_settings_speed_blur(void) { return settings_active ? settings_active->current.speed_blur : 0; }
int video_settings_speed_blur_amount(void) { return settings_active ? settings_active->current.speed_blur_amount : 0; }
int video_settings_speed_vignette(void) { return settings_active ? settings_active->current.speed_vignette : 0; }
int video_settings_skip_intro(void) { return settings_active ? settings_active->current.skip_intro : 0; }
int video_settings_assist_line(void)
{
    const char *test = getenv("MOON_ASSIST_LINE");
    if (test)
        return atoi(test) != 0;
    return settings_active ? settings_active->current.assist_line : 0;
}
int video_settings_field_size_warning(int field_size)
{
    return field_size > 20;
}
