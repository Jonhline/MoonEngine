#ifndef OPENGT4_LAUNCHER_SKIN_H
#define OPENGT4_LAUNCHER_SKIN_H
#include "mproject.h"
#include "launcher_fsr.h"
#include "font.h"
#include <stdint.h>
enum { LAUNCHER_SKIN_IMAGES = 34 };
typedef struct {
    int width, height;
    uint8_t *rgba;
} launcher_skin_image;
typedef struct {
    mp_node *project;
    launcher_skin_image images[LAUNCHER_SKIN_IMAGES];
    launcher_skin_image filtered[LAUNCHER_SKIN_IMAGES];
    launcher_fsr *filter;
    gt4_font fonts[3];
    uint8_t *font_data[2];
    size_t font_size[2];
    void *raster_cache; /* Win32 premultiplied images, owned by this skin. */
    unsigned raster_builds, raster_hits;
    int cache_hit;
    char cache_file[1600];
} launcher_skin;
int launcher_skin_open(launcher_skin *skin, const char *disc, const char *cache);
void launcher_skin_close(launcher_skin *skin);
const mp_node *launcher_skin_widget(const launcher_skin *skin, const char *type, const char *name);
const char *launcher_skin_asset(int index);
#ifdef _WIN32
#include <windows.h>
void launcher_skin_image_draw(const launcher_skin *skin, HDC dc, int image, RECT rect);
void launcher_skin_background(const launcher_skin *skin, HDC dc, int width, int height);
void launcher_skin_group(const launcher_skin *skin, HDC dc, RECT rect);
void launcher_skin_text(const launcher_skin *skin, HDC dc, const mp_node *style, RECT rect,
                        const wchar_t *text);
void launcher_skin_button(const launcher_skin *skin, HDC dc, RECT rect, float selected);
void launcher_skin_field(const launcher_skin *skin, HDC dc, RECT rect, int selected);
void launcher_skin_control_text(const launcher_skin *skin, HDC dc, RECT rect, const wchar_t *text,
                                int center);
void launcher_skin_label(const launcher_skin *skin, HDC dc, RECT rect, const wchar_t *text,
                         int face, float height, const uint8_t color[4], int center);
void launcher_skin_filter(launcher_skin *skin);
HCURSOR launcher_skin_cursor(const launcher_skin *skin);
int launcher_skin_paragraph(const launcher_skin *skin, HDC dc, const mp_node *style, RECT rect,
                            const wchar_t *text);
void launcher_skin_highlight(const launcher_skin *skin, HDC dc, RECT rect, float amount);
#endif
#endif
