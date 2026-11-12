/* P: deterministic artistic daylight, independent of vehicle simulation.
   Hours are solar time on an equinox at latitude 35 degrees; no location/date
   is inferred from course assets. The original daytime helper remains 07..17;
   the optional full-day helper below adds the night path. */
#ifndef MOON_DAYLIGHT_H
#define MOON_DAYLIGHT_H
#include <math.h>
typedef struct {
    float sun[3], direct[3], sky[3], haze[3];
    float hour;
    float moon[3]; /* P: stylized full moon opposite the sun, not an ephemeris. */
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
/* P: continuous 24-hour extension; original daytime helper remains unchanged. */
static inline moon_daylight moon_night_at(float hour)
{
    float h=isfinite(hour)?fmodf(fmodf(hour,24)+24,24):12;
    moon_daylight d=moon_daylight_at(fminf(17,fmaxf(7,h)));
    d.hour=h;
    float angle=(h-12)*.2617993877991494f,latitude=.6108652381980153f;
    d.sun[0]=-sinf(angle);d.sun[1]=cosf(latitude)*cosf(angle);d.sun[2]=-sinf(latitude)*cosf(angle);
    for (int c = 0; c < 3; c++)
        d.moon[c] = -d.sun[c];
    float daylight=fminf(1,fmaxf(0,(d.sun[1]+.12f)/.2f));daylight=daylight*daylight*(3-2*daylight);
    float direct=fminf(1,fmaxf(0,d.sun[1]/.08f));
    for(int c=0;c<3;c++) {
        const float night[3]={.002f,.003f,.006f};
        d.direct[c]*=direct;
        d.sky[c]=night[c]*(1-daylight)+d.sky[c]*daylight;
        d.haze[c]=night[c]*(1-daylight)+d.haze[c]*daylight;
    }
    return d;
}
/* Fixed authored course direction: the primary enabled Env0 light, normalized.
   This does not invent an ephemeris or rotate the source sky. Noon/high sun is
   nearly neutral; excess optical air mass warms light near the horizon. */
static inline moon_daylight moon_daylight_authored(const float direction[3], int night)
{
    moon_daylight d=moon_night_at(night?22:12);
    float length=sqrtf(direction[0]*direction[0]+direction[1]*direction[1]+direction[2]*direction[2]);
    if (!(length>1e-6f)) {
        /* No authored directional source: do not invent a solar position. */
        for(int c=0;c<3;c++){d.sun[c]=0;d.moon[c]=0;d.direct[c]=0;}
        return d;
    }
    for(int c=0;c<3;c++) {
        d.sun[c]=direction[c]/length*(night?-1:1);
        d.moon[c]=-d.sun[c];
    }
    if(night) {for(int c=0;c<3;c++)d.direct[c]=0;return d;}
    float y=fminf(1,fmaxf(0,d.sun[1])),mass=1/fmaxf(y,.08f)-1,luminance=0;
    const float optical[3]={.0428f,.1f,.24414f},sky[3]={.65f,.78f,1},horizon[3]={1,.69f,.4f};
    for(int c=0;c<3;c++){d.direct[c]=expf(-optical[c]*mass);luminance+=d.direct[c]*(c==0?.2126f:c==1?.7152f:.0722f);}
    for(int c=0;c<3;c++){d.direct[c]/=fmaxf(luminance,1e-6f);d.sky[c]=sky[c]*(.65f+.35f*y);d.haze[c]=horizon[c]*(1-y)+sky[c]*y;}
    return d;
}
/* P: same twilight interval as atmosphereSky; never a track-name rule. */
static inline float moon_authored_night_weight(int authored, float sun_y)
{
    if (!authored)
        return 0;
    float t = fminf(1, fmaxf(0, (sun_y + .12f) / .2f));
    return 1 - t * t * (3 - 2 * t);
}
#endif
