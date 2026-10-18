#include "options_store.h"
#ifdef __ANDROID__
#include "mobile_options.h"
#endif
#include "platform/switch_scene.h"
#ifdef __SWITCH__
#include "platform/switch_file.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
const char *moon_options_config_path(const char *config, const char *launcher_owned)
{
    if (!config || config != launcher_owned || !*config)
        return config;
    const char *base = strrchr(config, '/');
    const char *backslash = strrchr(config, '\\');
    if (!base || (backslash && backslash > base))
        base = backslash;
    return !strcmp(base ? base + 1 : config, "video.ini") ? NULL : config;
}
const char *moon_options_get(const moon_options *s, const char *key, const char *fallback)
{
    for (int i = 0; i < s->count; i++)
        if (!strcmp(s->entries[i].key, key))
            return s->entries[i].value;
    return fallback;
}
int moon_options_int(const moon_options *s, const char *key, int fallback, int low, int high)
{
    const char *v = moon_options_get(s, key, "");
    char *end;
    long n = strtol(v, &end, 10);
    return !*v || *end || n < low || n > high ? fallback : (int)n;
}
int moon_options_set(moon_options *s, const char *key, const char *value)
{
    if (!key || !value || !*key || strlen(key) >= 64 || strlen(value) >= 256 ||
        strpbrk(key, "=\r\n") || strpbrk(value, "\r\n"))
        return 0;
    int i = 0;
    while (i < s->count && strcmp(s->entries[i].key, key))
        i++;
    if (i == s->count && s->count == 128)
        return 0;
    if (i < s->count && !strcmp(s->entries[i].value, value))
        return 1;
    if (i == s->count)
        s->count++;
    snprintf(s->entries[i].key, 64, "%s", key);
    snprintf(s->entries[i].value, 256, "%s", value);
    s->modified = 1;
    return 1;
}
int moon_options_save(const moon_options *s)
{
    if (!s->path[0])
        return 1;
    char tmp[1040];
    snprintf(tmp, sizeof tmp, "%s.tmp", s->path);
    FILE *f = fopen(tmp, "wb");
    if (!f)
        return 0;
    int ok = fprintf(f, "preset=%s\nmodified=%d\n", s->preset, s->modified) > 0;
    for (int i = 0; i < s->count; i++)
        if (fprintf(f, "%s=%s\n", s->entries[i].key, s->entries[i].value) < 0)
            ok = 0;
    if (fclose(f))
        ok = 0;
#ifdef _WIN32
    if (ok)
        ok = MoveFileExA(tmp, s->path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#elif defined(__SWITCH__)
    if (ok)
        ok = plat_switch_replace_file(tmp, s->path);
#else
    if (ok)
        ok = rename(tmp, s->path) == 0;
#endif
    if (!ok)
        remove(tmp);
    return ok;
}
static const char *canonical_key(const char *key, char out[64])
{
    const char *video[] = {"width",     "height",      "fullscreen",     "vsync",
                           "upscaling", "ui_filter",   "text_sharpness", "aspect_ratio",
                           "renderer",  "low_latency", "fps_limit"};
    for (unsigned i = 0; i < sizeof video / sizeof *video; i++)
        if (!strcmp(key, video[i])) {
            snprintf(out, 64, "graphics.%s", key);
            return out;
        }
    return key;
}
int moon_options_load(moon_options *s, const char *path, const char *legacy)
{
    memset(s, 0, sizeof *s);
    snprintf(s->preset, sizeof s->preset, "Enhanced");
    if (path)
        snprintf(s->path, sizeof s->path, "%s", path);
    if (!path || !*path)
        return 1;
    FILE *f = fopen(path, "rb");
    int migrated = !f && legacy && *legacy;
    if (migrated)
        f = fopen(legacy, "rb");
    if (!f)
        return 1;
    char line[512];
    int modified = 0;
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        char *eq = strchr(line, '=');
        if (!eq)
            continue;
        *eq++ = 0;
        if (!strcmp(line, "preset"))
            snprintf(s->preset, sizeof s->preset, "%.63s", eq);
        else if (!strcmp(line, "modified"))
            modified = !strcmp(eq, "1");
        else {
            char canonical[64];
            moon_options_set(s, canonical_key(line, canonical), eq);
        }
    }
    int ok = !ferror(f);
    fclose(f);
    s->modified = migrated ? 1 : modified;
    return ok && (!migrated || moon_options_save(s));
}
/* Keep ISO/save bootstrap paths in config.ini; all presentation preferences
   migrate to the same canonical store used by the desktop Options page. */
int moon_options_switch_load(moon_options *s, const char *legacy)
{
    char path[1024];
    const char *slash = strrchr(legacy, '/');
    const char *backslash = strrchr(legacy, '\\');
    if (!slash || (backslash && backslash > slash))
        slash = backslash;
    size_t parent = slash ? (size_t)(slash - legacy + 1) : 0;
    if (parent + sizeof "options.ini" > sizeof path)
        return 0;
    memcpy(path, legacy, parent);
    memcpy(path + parent, "options.ini", sizeof "options.ini");
    if (!moon_options_load(s, path, legacy))
        return 0;
    // Preserve the old 960x540/1280x720 choice during migration.
    if (!*moon_options_get(s, MOON_SWITCH_SCENE_KEY, ""))
        moon_options_set(s, MOON_SWITCH_SCENE_KEY, "1");
    return moon_options_save(s);
}
void moon_options_preset(moon_options *s, int enhanced)
{
#if defined(__SWITCH__) || defined(MOON_SWITCH_OPTIONS_TEST)
    moon_options_set(s, MOON_SWITCH_SCENE_KEY, enhanced ? "2" : "0");
    moon_options_set(s, "controls.touch_menu", enhanced ? "1" : "0");
#endif
#ifdef __ANDROID__
    moon_mobile_options_preset(s, enhanced);
#endif
    /* Only registered improvements belong to presets. Display/device selection
       is a host requirement; the original in-game aspect option is unchanged. */
    moon_options_set(s, "controls.desktop_menu", enhanced ? "1" : "0");
    moon_options_set(s, "controls.gtmode_edge_pan", enhanced ? "1" : "0");
    moon_options_set(s, "interface.input_hints", enhanced ? "1" : "0");
    moon_options_set(s, "interface.main_menu_overview", enhanced ? "1" : "0");
    moon_options_set(s, "interface.smooth_mouse_cursor", enhanced ? "1" : "0");
    moon_options_set(s, "interface.mouse_hover_feedback", enhanced ? "1" : "0");
    moon_options_set(s, "interface.compact_car_strip", enhanced ? "1" : "0");
    moon_options_set(s, "graphics.spatial_visibility", enhanced ? "1" : "0");
    moon_options_set(s, "graphics.resident_reflections", enhanced ? "1" : "0");
    moon_options_set(s, "graphics.upscaling", "0");
    moon_options_set(s, "graphics.ui_filter", enhanced ? "0" : "2");
    moon_options_set(s, "graphics.text_sharpness", "0");
    snprintf(s->preset, sizeof s->preset, "%s", enhanced ? "Enhanced" : "Classic");
    s->modified = 0;
}
