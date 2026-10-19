/* RaceSteeringDisplay: 003A1EE0, 003A5D30 and 003A5D20. */
#ifndef MOON_RACE_HUD_STEERING_H
#define MOON_RACE_HUD_STEERING_H
#include "race_display.h"

static int hud_steering_visible(int mode, int seen, int kind, unsigned flags)
{
    return (mode < 8 || mode >= 10) && seen && (kind == 0 || kind == 3) &&
           (flags & RACE_DISPLAY_STEERING) != 0;
}

static void hud_steering_layers(float current, float previous, int valid, float angles[4])
{
    float step = valid ? (current - previous) * .25f : 0;
    float angle = current - step * 4;
    for (int i = 0; i < 4; i++) {
        angles[i] = angle;
        angle += step;
    }
}
#endif
