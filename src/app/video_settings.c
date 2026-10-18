#include "moon_pref.h"
#ifdef __ANDROID__
#include "mobile_options.h"
#endif
#include "platform/switch_file.h"
#include "video_settings.h"
#include "platform/renderer_backend.h"
#include "video_gl.h"
#include "render3d.h"
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
#endif

static const int sizes[][2] = {{1280, 720},  {1280, 960},  {1600, 900},
                               {1920, 1080}, {2560, 1440}, {3840, 2160}};
static void quality(const pc_video_options *v)
{
    video_ui_quality(v->filter, v->text / 100.f);
#if !defined(__SWITCH__) && !defined(__ANDROID__)
    video_scene_quality(v->antialiasing, v->render_scale);
    r3d_texture_smoothing(v->texture_smoothing);
#endif
}

void video_settings_init(video_settings *s, int persist, const char *config_file)
{
    memset(s, 0, sizeof *s);
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
#ifndef __SWITCH__
    s->current.desktop = s->current.edge_pan = s->current.input_hints = s->current.main_menu = s->current.smooth_cursor = s->current.hover_feedback = 1;
    s->current.compact_cars = 1;
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
#if !defined(__ANDROID__)
    s->current.antialiasing = moon_options_int(&s->store, "graphics.antialiasing", 0, 0, 3);
    s->current.render_scale = moon_options_int(&s->store, "graphics.render_scale", 2, 0, 2);
    s->current.texture_smoothing =
        moon_options_int(&s->store, "graphics.texture_smoothing", 0, 0, 1);
#endif
    s->current.compact_cars = moon_options_int(&s->store,"interface.compact_car_strip",enhanced,0,1);
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
            if (!strcmp(key, "fps_limit") && value >= 0 && value <= 500)
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
                         "graphics.text_sharpness", "graphics.aspect_ratio", "graphics.low_latency", "graphics.fps_limit", "controls.desktop_menu", "controls.gtmode_edge_pan", "interface.input_hints", "interface.main_menu_overview", "graphics.spatial_visibility", "graphics.resident_reflections", "interface.smooth_mouse_cursor", "interface.mouse_hover_feedback", "interface.compact_car_strip"};
    int values[] = {v->video.width, v->video.height, v->video.fullscreen, v->video.vsync,
                    v->video.upscale, v->filter, v->text, v->aspect, v->low_latency, v->fps_limit, v->desktop, v->edge_pan, v->input_hints, v->main_menu, v->spatial_visibility, v->resident_reflections, v->smooth_cursor, v->hover_feedback, v->compact_cars};
    for (int i = 0; i < (int)(sizeof values / sizeof values[0]); i++) {
        char value[32];
        snprintf(value, sizeof value, "%d", values[i]);
        moon_options_set(&s->store, keys[i], value);
    }
#if !defined(__SWITCH__) && !defined(__ANDROID__)
    const char *quality_keys[] = {"graphics.antialiasing", "graphics.render_scale",
                                  "graphics.texture_smoothing"};
    const int quality_values[] = {v->antialiasing, v->render_scale, v->texture_smoothing};
    for (int i = 0; i < 3; i++) {
        char value[16];
        snprintf(value, sizeof value, "%d", quality_values[i]);
        moon_options_set(&s->store, quality_keys[i], value);
    }
#endif
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
    if (s->preset_draft >= 0) {
        snprintf(s->store.preset, sizeof s->store.preset, "%s", s->preset_draft ? "Enhanced" : "Classic");
        s->store.modified = 0;
    }
    if (s->persist && !moon_options_save(&s->store))
        snprintf(s->message, sizeof s->message, "Preferences applied; could not save settings.");
}

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
    VS_COUNT
};
static const char *const vs_preset[] = {"Classic", "Enhanced", "Modified"};
static const char *const vs_fps[] = {"Unlimited", "60 FPS",  "90 FPS", "120 FPS",
                                     "144 FPS",   "165 FPS", "240 FPS"};
static const int vs_fps_values[] = {0, 60, 90, 120, 144, 165, 240};
static const char *const vs_off_on[] = {"Off", "On"};
static const char *const vs_api[] = {"OpenGL", "Vulkan"};
static const char *const vs_mode[] = {"Windowed", "Full Screen"};
static const char *const vs_size[] = {"1280 x 720",  "1280 x 960",  "1600 x 900",
                                      "1920 x 1080", "2560 x 1440", "3840 x 2160"};
static const char *const vs_ratio[] = {"4:3", "16:9"};
static const char *const vs_fsr[] = {"Native", "FSR Ultra Quality", "FSR Quality", "FSR Balanced",
                                     "FSR Performance"};
static const char *const vs_filter[] = {"Smooth", "Sharp", "Original"};
static const char *const vs_aa[] = {"Original", "Off", "SSAA 150%", "SSAA 200%"};
static const char *const vs_scale[] = {"50%", "75%", "100%"};
static const char *const vs_text[] = {"0%", "25%", "50%", "75%", "100%"};
static const video_setting_info vs_info[VS_COUNT] = {
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
     "Classic: the PS2 game. Enhanced: the recommended improvements. Any change makes it Modified.",
     2, vs_preset},
    {"Video", "Frame Limit", "The most frames drawn a second.", 7, vs_fps},
    {"Quality", "Anti-Aliasing", "SSAA smooths 3D edges and increases GPU work.", 4, vs_aa},
    {"Quality", "3D Resolution", "Scene resolution; menus and HUD keep their output resolution.", 3,
     vs_scale},
    {"Quality", "Texture Smoothing", "Smooth model surfaces that use point sampling.", 2,
     vs_off_on},
};
int video_setting_count(void)
{
    return VS_COUNT;
}
const video_setting_info *video_setting(int i)
{
    return i >= 0 && i < VS_COUNT ? &vs_info[i] : NULL;
}
int video_setting_get(const video_settings *s, int i)
{
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
    case VS_AA:
        return v->antialiasing;
    case VS_SCALE:
        return v->render_scale;
    case VS_TEXTURE_SMOOTH:
        return v->texture_smoothing;
    case VS_PRESET: return s->store.modified ? 2 : strcmp(s->store.preset, "Classic") != 0;
    case VS_FPS:
        for (int k = 0; k < 7; k++)
            if (vs_fps_values[k] == v->fps_limit)
                return k;
        return 0;
    }
    return 0;
}
void video_setting_set(video_settings *s, int i, int value)
{
    if (!s || i < 0 || i >= VS_COUNT)
        return;
    int n = vs_info[i].count;
    value = ((value % n) + n) % n;
    pc_video_options *v = &s->current;
    switch (i) {
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
    case VS_PRESET: {
        /* as the Screen Settings page applies it: the registered PC options at the
           preset's values; host size, mode and the API are kept */
        int enhanced = value == 1;
        v->antialiasing = enhanced ? 2 : 0;
        v->render_scale = 2;
        v->texture_smoothing = enhanced;
        v->filter = enhanced ? 0 : 2;
        v->text = 0;
        v->video.upscale = 0;
        v->desktop = v->edge_pan = v->input_hints = v->main_menu = enhanced;
        v->smooth_cursor = v->hover_feedback = v->compact_cars = enhanced;
        v->spatial_visibility = v->resident_reflections = enhanced;
        s->draft = s->current;
        s->preset_draft = enhanced;
        save(s);
        s->preset_draft = -1;
        return;
    }
    }
    s->draft = s->current;
    s->preset_draft = -1; /* an individual change: the preset reads Modified */
    save(s);
}
void video_settings_apply_current(video_settings *s)
{
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
static void read_draft(video_settings *s)
{
    pc_video_options *v = &s->draft;
    v->aspect = s->choices[0]->focus_index;
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
    v->render_scale = s->choices[18]->focus_index;
    v->texture_smoothing = s->choices[19]->focus_index;
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
    v->desktop = v->edge_pan = 0;
    v->renderer = 1;
    v->video.fullscreen = 1;
#endif
}
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
#ifndef __SWITCH__
    ui_widget *sender = ah_host_data(self);
    if (sender == s->choices[8] && s->choices[8]->focus_index < 2) {
        int enhanced = s->choices[8]->focus_index;
        s->preset_draft = enhanced;
#ifndef __ANDROID__
        s->choices[17]->focus_index = enhanced ? 2 : 0;
        s->choices[18]->focus_index = 2;
        s->choices[19]->focus_index = enhanced;
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
#endif
        read_draft(s);
    } else {
#ifndef __ANDROID__
        if (sender == s->choices[17]) {
            s->choices[18]->focus_index = 2;
            s->choices[6]->focus_index = 0;
        } else if (sender == s->choices[18] || sender == s->choices[6]) {
            s->choices[17]->focus_index = 0;
            if (sender == s->choices[18])
                s->choices[6]->focus_index = 0;
            else
                s->choices[18]->focus_index = 2;
        }
        read_draft(s);
#endif
        s->preset_draft = -1;
        s->choices[8]->focus_index = 2;
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
    quality(&s->draft);
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
    "Apply Classic or Enhanced defaults to the registered PC options. Individual edits mark the preset as modified.",
    "Move the GT Mode map near its edges with the mouse. Off keeps the original icon-based navigation.",
    "Show keyboard or controller prompts for the last device used. Off retains the original PS2 symbols.",
    "Reject off-screen spatial blocks before individual draw groups. Off uses the retained flat visibility path; original course lists remain active.",
    "Keep reflection views and composition on the GPU. Off uses the retained CPU remap. Both use the same six scene views.",
    "Show five main menu entries in the original vertical style. Use the wheel to scroll. Off restores the original layout.",
    "Add brief mouse position/angle easing and smooth cursor icon changes. Off follows the mouse immediately.",
    "Gently enlarge otherwise-static Options icons and labels. Existing game hover animations are preserved.",
    "Use smaller car thumbnails in garage and dealer lists. Off retains the original layout.",
    "Original retains the previous renderer. Off disables MSAA. SSAA smooths edges at 150% or 200% 3D resolution; costs GPU time and disables FSR.",
    "Lower only the 3D resolution for performance. HUD and menus stay sharp. Selecting this disables SSAA and FSR.",
    "Smooth point-sampled model textures. Off keeps the authored texture sampling."};
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
    const int mobile_rows[] = {7, 9, 10, 13, 14, 15};
    const char *mobile_hints[] = {
        "Touch Menus: first tap previews, second tap confirms; drag lists and the GT Mode map.",
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
/* P: PC settings use three compact pages with a shared, uncommitted draft.
   Stable choice indices belong to the store, not to their visible positions. */
#ifdef __ANDROID__
static const int tab_rows[3][9] = {
    {0, 1, 2, 5, -1}, {3, 4, 6, 11, 12, 16, -1}, {7, 9, 10, 13, 14, 15, -1}};
#else
static const int tab_rows[3][9] = {
    {0, 1, 2, 5, -1}, {17, 18, 19, 3, 4, 6, 11, 12, 16}, {7, 9, 10, 13, 14, 15, -1}};
#endif
static void select_tab(video_settings *s, int tab)
{
    s->active_tab = tab;
    for (int i = 0; i < (int)(sizeof s->choices / sizeof s->choices[0]); i++)
        if (s->rows[i])
            s->rows[i]->visible = i == 8;
    for (int row = 0; row < 9 && tab_rows[tab][row] >= 0; row++) {
        ui_widget *w = s->rows[tab_rows[tab][row]];
        w->visible = 1;
        w->y = 100 + row * (tab == 1 ? 24 : tab == 2 ? 28 : 32);
    }
    for (int i = 0; i < 3; i++) {
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
    for (int i = 0; i < 3; i++)
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
    select_tab(s, (s->active_tab + (key.u.i == 0xffc8 ? 2 : 1)) % 3);
    menu_play_effect(s->owner, "cursor");
    *ret = ah_int(2);
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
    s->choices[17]->focus_index = s->current.antialiasing;
    s->choices[18]->focus_index = s->current.render_scale;
    s->choices[19]->focus_index = s->current.texture_smoothing;
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
    s->choices[8]->focus_index = s->store.modified ? 2 : strcmp(s->store.preset, "Classic") != 0;
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
#endif
    quality(&s->current);
#ifndef __SWITCH__
    select_tab(s, 0);
#else
    menu_native_focus(s->owner, s->choices[0]);
#endif
    ui_set_tooltips(s->page, hints[0]);
    *ret = ah_int(0);
    return 0;
}
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
    save(s);
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
#ifdef __SWITCH__
    const int rows = 10;
#else
#ifdef __ANDROID__
    const int rows = 17;
#else
    const int rows = 20;
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
#else
    int pitch = 32;
#endif
    pane->y = rows == 7 ? 78 : fmaxf(0, pane_center - 370 * .5f);
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
                           "TextureSmoothing"};
    const char *titles[] = {
        "Aspect Ratio",      "Window Mode",   "Resolution",    "UI Filter",
        "Text Clarity",      "VSync",         "Upscaling",     "Desktop Input",
        "PC Preset",         "Map Edge Pan",  "Input Hints",   "Spatial Visibility",
        "GPU Reflections",   "Main Menu",     "Smooth Cursor", "Hover Feedback",
        "Compact Car Strip", "Anti-Aliasing", "3D Resolution", "Texture Smoothing"};
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
#endif
    const char *ratios[] = {"4:3", "16:9"};
    const char *modes[] = {"Windowed", "Full Screen"},
               *resolutions[] = {"1280 x 720",  "1280 x 960",  "1600 x 900", "1920 x 1080",
                                 "2560 x 1440", "3840 x 2160", "Custom"};
    const char *filters[] = {"Smooth", "Sharp", "Original"},
               *clarity[] = {"0%", "25%", "50%", "75%", "100%"}, *toggle[] = {"Off", "On"};
    const char *upscalers[] = {"Native", "FSR Ultra Quality", "FSR Quality", "FSR Balanced",
                               "FSR Performance"};
    const char *presets[] = {"Classic", "Enhanced", "Modified"};
    const char *const *lists[] = {
        ratios, modes,  resolutions, filters, clarity, toggle, upscalers, toggle, presets,  toggle,
        toggle, toggle, toggle,      toggle,  toggle,  toggle, toggle,    vs_aa,  vs_scale, toggle};
    int counts[] = {2, 2, 7, 3, 5, 2, 5, 2, 3, 2, 2, 2, 2, 2, 2, 2, 2, 4, 3, 2};
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
        ui_widget *w = ui_widget_copy(ui, sample);
        w->name = names[i];
#ifndef __SWITCH__
        w->type = "Composite";
        w->h = (float)pitch;
        s->rows[i] = w;
        w->x = 24;
        widen(w,512);
#endif
#ifdef __SWITCH__
        w->h = (float)pitch;
#endif
        w->y = 20 + i * pitch;
        text(ui_widget_child(w, "label"), titles[i]);
        ui_widget_append(pane, w);
        ui_widget *choice = ui_widget_child(w, "OptionMenu");
        s->choices[i] = choice;
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
    s->rows[8]->y = 60;
    const char *tab_names[] = {"DisplayTab", "RenderingTab", "ControlsTab"};
    const char *tab_titles[] = {"Display", "Rendering", "Controls"};
    ui_widget *button_sample = ui_widget_child(buttons, "OK");
    for (int i = 0; i < 3; i++) {
        ui_widget *tab = ui_widget_copy(ui, button_sample);
        tab->name = tab_names[i];
        widen(tab,160);
        tab->x = 24 + i * 176;
        tab->y = 24;
        text(ui_widget_child(tab, "label"), tab_titles[i]);
        ui_text_adjust_scale(ui, ui_widget_child(tab, "label"));
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
    menu_native_method(m, s->page, "onKeyPress", tab_key, s);
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
