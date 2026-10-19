#ifndef MOON_VIDEO_SETTINGS_H
#define MOON_VIDEO_SETTINGS_H
#include "platform/platform.h"
#include "ui.h"
#include "menu.h"
#include "options_store.h"
#include "desktop_input.h"
#ifdef __ANDROID__
#include "mobile_controls.h"
#endif

typedef struct {
    plat_video_desc video;
    int filter, text, aspect;
    int renderer, low_latency, fps_limit;
    int desktop, edge_pan, input_hints, main_menu, smooth_cursor, hover_feedback;
    int compact_cars, preset_tools, save_tools, graphics_swap;
    int spatial_visibility, resident_reflections;
    int antialiasing, render_scale, texture_smoothing, texture_filtering, car_shadows, material_shading, reflection_quality, shadow_maps, modern_lighting, daylight, solar_hour, solar_cycle, hdr, sun_rays, ambient_occlusion;
#ifdef __ANDROID__
    int touch_menu;
    moon_mobile_options mobile;
#endif
#ifdef __SWITCH__
    int scene_resolution, touch_menu;
#endif
} pc_video_options;
typedef struct {
    pc_video_options current, draft;
    moon_options store;
    int preset_draft;
    char personal_preset[64];
    int personal_modified;
    int persist, root, option_root;
    touch_back_state touch_back;
    uint64_t touch_back_clock;
    menu *owner;
    ui_widget *page, *choices[38];
#ifndef __SWITCH__
    ui_widget *rows[38], *tabs[6];
    int active_tab;
#endif
    char path[1024], message[128];
} video_settings;
void video_settings_init(video_settings *s, int persist, const char *config_file);
void video_settings_attach(video_settings *s, menu *m);
/* The graphics API for the next start (the first-run setup's choice), saved now. */
void video_settings_set_renderer(video_settings *s, int backend);
/* The same PC settings as the game's Screen Settings, as a list the launcher can show:
   group ("Video"/"Interface"), label, the choices' names. Setting one saves it. */
typedef struct {
    const char *group, *label, *help;
    int count;
    const char *const *names;
} video_setting_info;
int video_setting_count(void);
const video_setting_info *video_setting(int i);
int video_setting_get(const video_settings *s, int i);
void video_setting_set(video_settings *s, int i, int value);
/* Applies what can change on a running window (vsync, upscaling, filters, performance,
   scene options); the window mode and the API are applied by their own owners. */
void video_settings_apply_current(video_settings *s);
/* Transient studio comparison: graphics only, never writes preferences. */
void video_settings_preview_graphics(const video_settings *s, int classic);
void video_settings_preview_options(const pc_video_options *v);
/* The Classic (0) or Enhanced (1) preset's values, written into v only. */
void video_settings_preset_values(pc_video_options *v, int enhanced);
/* P: shared named-preset interchange. draft=1 waits for the in-game OK. */
int video_settings_preset_import(video_settings *s, const char *path, int draft);
int video_settings_preset_export(video_settings *s, const char *path, const char *name, int draft);
int video_settings_preset_dialog(video_settings *s, int exporting, int draft);
#endif
