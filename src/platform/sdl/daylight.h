/* P: deterministic artistic daylight, independent of vehicle simulation.
   Hours are solar time on an equinox at latitude 35 degrees; no location/date
   is inferred from course assets. No night: the selectable range is 07..17. */
#ifndef MOON_DAYLIGHT_H
#define MOON_DAYLIGHT_H
#include <math.h>
typedef struct {
    float sun[3], direct[3], sky[3], haze[3];
    float hour;
} moon_daylight;
static inline moon_daylight moon_daylight_at(float hour)
{
    moon_daylight d = {0};
    d.hour = isfinite(hour) ? fminf(17, fmaxf(7, hour)) : 12;
    const float rad = .0174532925199433f;
    float angle = (d.hour - 12) * 15 * rad, latitude = 35 * rad;
    d.sun[0] = -sinf(angle);
    d.sun[1] = cosf(latitude) * cosf(angle);
    d.sun[2] = -sinf(latitude) * cosf(angle);
    const float optical[3] = {.0428f, .1f, .24414f};
    const float sky[3] = {.65f, .78f, 1.f};
    const float horizon[3] = {1.f, .69f, .4f};
    float mass = 1 / fmaxf(d.sun[1], .08f), luminance = 0;
    for (int c = 0; c < 3; c++) {
        d.direct[c] = expf(-optical[c] * mass);
        luminance += d.direct[c] * (c == 0 ? .2126f : c == 1 ? .7152f : .0722f);
    }
    for (int c = 0; c < 3; c++) {
        d.direct[c] /= fmaxf(luminance, 1e-6f);
        d.sky[c] = sky[c] * (.65f + .35f * d.sun[1]);
        d.haze[c] = horizon[c] * (1 - d.sun[1]) + sky[c] * d.sun[1];
    }
    return d;
}
#endif
