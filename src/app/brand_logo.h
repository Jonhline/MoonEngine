#ifndef MOON_BRAND_LOGO_H
#define MOON_BRAND_LOGO_H
#include <stdint.h>
/* The version as shown: dev builds say so (MOON_DEV_BUILD, CMakeLists.txt). */
#if MOON_DEV_BUILD
#define MOON_VERSION_LABEL "v" MOON_VERSION " dev"
#else
#define MOON_VERSION_LABEL "v" MOON_VERSION
#endif
/* P: the Moon Engine logo (project art, res/logo_*.png) embedded in the program.
   variant 0: dark lettering (light backgrounds), 1: white lettering (dark ones).
   Returns RGBA decoded once and kept; NULL if it cannot be decoded. */
const uint8_t *brand_logo_rgba(int variant, int *width, int *height);
/* The square icon (res/moonengine_icon.png), RGBA 256x256; for window icons. */
const uint8_t *brand_icon_rgba(int *width, int *height);
struct plat_texture;
/* A cached platform texture of the same; NULL on failure. */
struct plat_texture *brand_logo_texture(int variant, int *width, int *height);
#endif
