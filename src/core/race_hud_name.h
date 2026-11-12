/* Replay watched-car caption, 0039CDF0 -> 0039EC40 -> 0039CFD8. */
#ifndef MOON_RACE_HUD_NAME_H
#define MOON_RACE_HUD_NAME_H
#include <math.h>
#include "race_display.h"

typedef struct {
    float value, delay, hold, rise, fall, remaining;
    int state;
} hud_name_fade;
/* 003A98E8 with the caption's 0039D0B0 quarter-second rise/fall. */
static void hud_name_start(hud_name_fade *f, float duration)
{
    *f = (hud_name_fade){0, 0, duration - .25f, 4, 4, duration - .25f, 1};
}
/* 003A9780: transitions keep their current frame's slope. */
static void hud_name_step(hud_name_fade *f, float dt)
{
    float slope = 0;
    if (f->state == 1) {
        if (f->hold >= 0) {
            f->remaining -= dt;
            if (f->remaining < 0)
                f->state = 2;
        }
        slope = f->rise;
    } else if (f->state == 2) {
        f->remaining = 0;
        slope = -f->fall;
    }
    f->value = fminf(fmaxf(f->value + slope * dt, 0), 1);
}
/* 0039CCF0, replay modes: Normal=4, Dive=3, instrument display=2. */
static unsigned hud_name_flags(int mode, unsigned flags)
{
    return mode == 4 ? RACE_DISPLAY_CAR_NAME : flags;
}
#endif
