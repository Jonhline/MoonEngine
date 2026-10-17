#ifndef MOON_VIDEO_SETTINGS_H
#define MOON_VIDEO_SETTINGS_H
#include "platform/platform.h"
#include "ui.h"
#include "menu.h"
#include "options_store.h"

typedef struct {
    plat_video_desc video;
    int filter, text, aspect;
    int renderer, low_latency, fps_limit;
    int desktop, edge_pan, input_hints, main_menu, smooth_cursor, hover_feedback;
    int compact_cars;
    int spatial_visibility, resident_reflections;
#ifdef __SWITCH__
    int scene_resolution;
#endif
} pc_video_options;
typedef struct {
    pc_video_options current, draft;
    moon_options store;
    int preset_draft;
    int persist, root, option_root;
    menu *owner;
    ui_widget *page, *choices[17];
#ifndef __SWITCH__
    ui_widget *rows[17], *tabs[3];
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
#endif
