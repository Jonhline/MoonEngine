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
#endif
