#include "moon_pref.h"
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
    moon_options_load(&s->store,NULL,NULL);
#else
    s->current.video = (plat_video_desc){1280, 960, 0, 1, 0};
#endif
    /* Classic starts with texture/glyph coverage from the disc. Optional
       PC sharpening remains available and existing preferences still load. */
    s->current.filter = 0;
    s->current.text = 0;
#ifndef __SWITCH__
    s->current.desktop = s->current.edge_pan = s->current.input_hints = s->current.main_menu = s->current.smooth_cursor = s->current.hover_feedback = 1;
    s->current.compact_cars = 1;
    s->current.spatial_visibility = s->current.resident_reflections = 1;
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
    moon_options_set(&s->store, "graphics.renderer", moon_renderer_name(v->renderer));
#ifdef __SWITCH__
    char scene_value[8];
    snprintf(scene_value,sizeof scene_value,"%d",v->scene_resolution);
    moon_options_set(&s->store,MOON_SWITCH_SCENE_KEY,scene_value);
#endif
    if (s->preset_draft >= 0) {
        snprintf(s->store.preset, sizeof s->store.preset, "%s", s->preset_draft ? "Enhanced" : "Classic");
        s->store.modified = 0;
    }
    if (s->persist && !moon_options_save(&s->store))
        snprintf(s->message, sizeof s->message, "Preferences applied; could not save settings.");
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
#endif
#ifndef __SWITCH__
    v->desktop = s->choices[7]->focus_index;
    v->edge_pan = s->choices[9]->focus_index;
    v->input_hints = s->choices[10]->focus_index;
    v->main_menu = s->choices[13]->focus_index;
    v->smooth_cursor = s->choices[14]->focus_index;
    v->hover_feedback = s->choices[15]->focus_index;
    v->compact_cars = s->choices[16]->focus_index;
    v->spatial_visibility = s->choices[11]->focus_index;
    v->resident_reflections = s->choices[12]->focus_index;
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
        read_draft(s);
    } else {
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
    "Use smaller car thumbnails inspired by the supplied Spec II reference. Off retains disc geometry.",
    "Separate regulation row backgrounds with gaps taken from the authored bottom padding. Off retains original backgrounds."
};
static int focus(ah_vm *vm, ah_obj *self, void *data, int argc, const ah_val *argv, ah_val *ret)
{
    (void)vm;
    (void)argc;
    (void)argv;
    video_settings *s = data;
    ui_widget *w = ah_host_data(self);
#ifdef __SWITCH__
    if (w == s->choices[7]) {
        ui_set_tooltips(s->page,"Classic: PS2 render size. Balanced: sharper 3D. Performance: lower 3D resolution for 60 FPS; HUD stays sharp.");
        *ret=ah_int(2);return 0;
    }
    if (w == s->choices[8]) {
        ui_set_tooltips(s->page,"Classic selects PS2-sized 3D; Enhanced selects Performance. Individual edits mark the preset modified.");
        *ret=ah_int(2);return 0;
    }
#endif
    for (int i = 0; i < 17; i++)
        if (s->choices[i] == w)
            ui_set_tooltips(s->page, hints[i]);
    *ret = ah_int(2);
    return 0;
}
#ifndef __SWITCH__
/* P: PC settings use three compact pages with a shared, uncommitted draft.
   Stable choice indices belong to the store, not to their visible positions. */
static const int tab_rows[3][6] = {{0, 1, 2, 5, -1, -1}, {3, 4, 6, 11, 12, 16}, {7, 9, 10, 13, 14, 15}};
static void select_tab(video_settings *s, int tab)
{
    s->active_tab = tab;
    for (int i = 0; i < 17; i++)
        s->rows[i]->visible = i == 8;
    for (int row = 0; row < 6 && tab_rows[tab][row] >= 0; row++) {
        ui_widget *w = s->rows[tab_rows[tab][row]];
        w->visible = 1;
        w->y = 100 + row * (tab == 2 ? 28 : 32);
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
    for (int i = 0; i < 17; i++)
        if (s->choices[i]->option_open)
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
    s->choices[8]->focus_index=s->store.modified?2:strcmp(s->store.preset,"Classic")!=0;
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
    const int rows = 9;
#else
    const int rows = 17;
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
    int pitch = 32;
    pane->y = rows == 7 ? 78 : fmaxf(0, pane_center - 370 * .5f);
    pane->h = rows == 7 ? 278 : 370;
    ui_widget *bg = ui_widget_child(pane, "bg");
    if (bg) {
        bg->h = pane->h;
        bg->w = pane->w;
    }
    const char *names[] = {"AspectRatio",   "WindowMode", "Resolution", "UIFilter",
                           "TextSharpness", "VSync",      "Upscaling", "DesktopInput", "PCPreset", "MapEdgePan", "InputHints", "SpatialVisibility", "ResidentReflections", "MainMenuOverview", "SmoothCursor", "HoverFeedback", "CompactCars"};
    const char *titles[] = {"Aspect Ratio", "Window Mode", "Resolution", "UI Filter",
                            "Text Clarity", "VSync",       "Upscaling", "Desktop Input", "PC Preset", "Map Edge Pan", "Input Hints", "Spatial Visibility", "GPU Reflections", "Main Menu", "Smooth Cursor", "Hover Feedback", "Compact Car Strip"};
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
    const char *const *lists[] = {ratios, modes, resolutions, filters, clarity, toggle, upscalers, toggle, presets, toggle, toggle, toggle, toggle, toggle, toggle, toggle, toggle};
    int counts[] = {2, 2, 7, 3, 5, 2, 5, 2, 3, 2, 2, 2, 2, 2, 2, 2, 2};
#ifdef __SWITCH__
    const char *scene_sizes[]={"Classic","Balanced","Performance"};
    names[7]="SceneResolution"; titles[7]="3D Resolution"; titles[8]="Preset";
    lists[7]=scene_sizes;counts[7]=3;
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
    buttons->x = 24;
    buttons->w = 512;
    ui_widget *cancel_button=ui_widget_child(buttons,"Cancel");
    ui_widget *ok_button=ui_widget_child(buttons,"OK");
    float left=(512-cancel_button->w-ok_button->w-16)*.5f;
    cancel_button->x=left;ok_button->x=left+cancel_button->w+16;
    menu_native_method(m, s->page, "onKeyPress", tab_key, s);
#endif
    buttons->y = rows == 7 ? 244 : 332;
    ui_widget_append(pane, buttons);
    if (!video_fsr_available())
        for (int i = 1; i < 5; i++)
            ui_option_item(s->choices[6], i)->insensitive = 1;
    menu_native_method(m, s->page, "onLoad", idle, s);
    menu_native_method(m, s->page, "onInitialize", idle, s);
    menu_native_method(m, s->page, "enter", enter, s);
    menu_native_method(m, s->page, "onFinalize", leave, s);
    menu_native_method(m, s->page, "onCancel", cancel, s);
    menu_native_method(m, ui_widget_child(buttons, "Cancel"), "onActivate", cancel, s);
    menu_native_method(m, ui_widget_child(buttons, "OK"), "onActivate", apply, s);
}
