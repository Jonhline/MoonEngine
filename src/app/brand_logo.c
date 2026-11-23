/* P: the Moon Engine logo, embedded (see brand_logo.h). */
#include "brand_logo.h"
#include "platform/platform.h"
#include <stddef.h>
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "../../third_party/stb/stb_image.h"

extern const unsigned char moon_logo_black[], moon_logo_white[], moon_logo_icon[];
extern const size_t moon_logo_black_size, moon_logo_white_size, moon_logo_icon_size;

static uint8_t *pixels[2];
static int sizes[2][2], tried[2];
static plat_texture *textures[2];

const uint8_t *brand_logo_rgba(int variant, int *width, int *height)
{
    variant = variant ? 1 : 0;
    if (!tried[variant]) {
        tried[variant] = 1;
        int n;
        pixels[variant] =
            stbi_load_from_memory(variant ? moon_logo_white : moon_logo_black,
                                  (int)(variant ? moon_logo_white_size : moon_logo_black_size),
                                  &sizes[variant][0], &sizes[variant][1], &n, 4);
    }
    if (width)
        *width = sizes[variant][0];
    if (height)
        *height = sizes[variant][1];
    return pixels[variant];
}

plat_texture *brand_logo_texture(int variant, int *width, int *height)
{
    variant = variant ? 1 : 0;
    const uint8_t *rgba = brand_logo_rgba(variant, width, height);
    if (!rgba)
        return NULL;
    if (!textures[variant])
        textures[variant] = plat_texture_create(sizes[variant][0], sizes[variant][1], rgba);
    return textures[variant];
}

const uint8_t *brand_icon_rgba(int *width, int *height)
{
    static uint8_t *icon;
    static int w, h, tried;
    if (!tried) {
        int n;
        tried = 1;
        icon = stbi_load_from_memory(moon_logo_icon, (int)moon_logo_icon_size, &w, &h, &n, 4);
    }
    *width = w;
    *height = h;
    return icon;
}
