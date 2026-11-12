#ifndef MOON_VIDEO_SETTINGS_H
#define MOON_VIDEO_SETTINGS_H
#include "platform/platform.h"
#include "ui.h"
#include "menu.h"
#include "options_store.h"
#include "desktop_input.h"
#include "race_limits.h"
#ifdef __ANDROID__
#include "mobile_controls.h"
#endif

typedef struct {
    plat_video_desc video;
    int filter, text, aspect;
    int renderer, low_latency, fps_limit;
    int desktop, edge_pan, input_hints, main_menu, smooth_cursor, hover_feedback;
    int car_lamps, night_sky;
    int compact_cars, preset_tools, save_tools, graphics_swap, controller_tiles;
    int spatial_visibility, resident_reflections;
    int antialiasing, render_scale, texture_smoothing, texture_filtering, car_shadows, material_shading, reflection_quality, shadow_maps, modern_lighting, daylight, solar_hour, solar_cycle, hdr, sun_rays, ambient_occlusion, shadow_filter;
    int speed_fov, speed_fov_degrees, speed_cam_lag, speed_shake, speed_wind, speed_road_roar;
    int speed_blur, speed_blur_amount, speed_vignette;
    int skip_intro;
    int assist_line;
    int phantom_opponents; /* opt-in: the stock six plus phantoms */
    int field_size;        /* total field when phantoms are on (conductor..MOON_MAX_CARS) */
    int discord_presence;
    int preview_mode; /* 0 before/after, 1 benchmark */
    int scene_scale; /* 3D target scale in percent, 25..200 */
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
    ui_widget *page, *choices[64];
    ui_widget *field_size_row, *field_size_label, *field_size_warn[4];
#ifndef __SWITCH__
    ui_widget *rows[64], *tabs[8];
    int active_tab;
    int tweaks_open;
    int scroll_focus_row; /* last row the pad focused, -1 after a tab change */
#endif
    char path[1024], message[128];
} video_settings;
/* Resolve Solar Time to 0..23, or -1 for the course default. */
int video_settings_solar_hour(const pc_video_options *v);
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
/* The Tweaks rows (group header then its switches), shared by both settings
   pages so they cannot drift apart. */
int video_settings_tweak_count(void);
const video_setting_info *video_settings_tweak(int i);
int video_setting_tweak_group(void);
int video_setting_tweak_is_child(int id);
int video_setting_get(const video_settings *s, int i);
void video_setting_set(video_settings *s, int i, int value);
/* Applies what can change on a running window (vsync, upscaling, filters, performance,
   scene options); the window mode and the API are applied by their own owners. */
void video_settings_apply_current(video_settings *s);
/* Transient studio comparison: graphics only, never writes preferences. */
void video_settings_preview_graphics(const video_settings *s, int classic);
void video_settings_preview_options(const pc_video_options *v);
/* The Classic (0), Enhanced (1) or Performance (2) preset's values, written into v only. */
void video_settings_preset_values(pc_video_options *v, int preset);
/* The built-in preset a store names: 0 Classic, 1 Enhanced, 2 Performance, -1 none. */
int video_settings_builtin_preset(const char *name);
/* Engine read side for the TWEAKS page; one owner, the active settings. */
int video_settings_speed_fov(void);
int video_settings_speed_fov_degrees(void);
int video_settings_speed_cam_lag(void);
int video_settings_speed_shake(void);
int video_settings_speed_wind(void);
int video_settings_speed_road_roar(void);
int video_settings_speed_blur(void);
int video_settings_speed_blur_amount(void);
int video_settings_speed_vignette(void);
int video_settings_skip_intro(void);
int video_settings_assist_line(void);
/* The one experimental warning both settings pages use. */
int video_settings_field_size_warning(int field_size);
int video_setting_is_field_size(int id);
/* Keeps the focused in-game settings row inside its viewport (pad Up/Down). */
void video_settings_scroll_tick(void);
/* The one speed intensity every speed-driven effect reads: smoothstep 60..240 km/h,
   smoothed over SPEED_FEEL_SMOOTH_SECONDS. The race calls the frame once. */
void video_settings_speed_frame(float kmh, double dt);
float video_settings_speed_level(void);
void video_options_realistic_skybox(pc_video_options *v, int enabled);
void video_options_realistic_lighting(pc_video_options *v, int enabled);
/* P: shared named-preset interchange. draft=1 waits for the in-game OK. */
int video_settings_preset_import(video_settings *s, const char *path, int draft);
int video_settings_preset_export(video_settings *s, const char *path, const char *name, int draft);
int video_settings_preset_dialog(video_settings *s, int exporting, int draft);
#endif
