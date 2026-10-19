#ifndef MOON_MOBILE_CONTROLS_H
#define MOON_MOBILE_CONTROLS_H
#include "platform/platform.h"
#include <stdint.h>
/* P: contact capture in normalized, safe-area coordinates. No physics state. */
enum {
    MOON_MOBILE_STEER,
    MOON_MOBILE_ACCEL,
    MOON_MOBILE_BRAKE,
    MOON_MOBILE_UP,
    MOON_MOBILE_DOWN,
    MOON_MOBILE_HAND,
    MOON_MOBILE_CAMERA,
    MOON_MOBILE_PAUSE,
    MOON_MOBILE_REVERSE,
    MOON_MOBILE_COUNT
};
typedef struct {
    float x, y, w, h;
} moon_mobile_rect;
typedef struct {
    int enabled, size, opacity, layout, steering;
    /* Android 3D scene render-scale: 0 full, 1 = 75%, 2 = 50%, 3 = 35%. */
    int render_scale;
} moon_mobile_options;
typedef struct {
    moon_mobile_options options;
    struct {
        int used, control;
        int64_t id;
        float x, y;
    } contacts[10];
    int active, controller;
} moon_mobile_controls;
void moon_mobile_reset(moon_mobile_controls *s);
moon_mobile_rect moon_mobile_bounds(const moon_mobile_controls *s, int control);
int moon_mobile_contact(moon_mobile_controls *s, int phase, int64_t id, float x, float y);
int moon_mobile_pad(const moon_mobile_controls *s, plat_pad_state *pad);
#endif
