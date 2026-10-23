#ifndef MOON_RACE_LIGHTS_H
#define MOON_RACE_LIGHTS_H
#include "course.h"
#include "render3d.h"
/* Immutable RNW4 / Env0 car lighting, 0038E7F8 -> 0038EFF8.
   triangle <0 represents an unavailable surface. No solver state access. */
int race_lights_sample(const gt4_course *, int triangle, const float pos[3], float sun, float level,
                       r3d_lights *);
int race_lights_sample_environment(const gt4_course *, int triangle, const float pos[3], float sun,
                                   float level, const uint8_t *environment, size_t environment_size,
                                   r3d_lights *);
typedef struct {
    int count;
    struct { float way[3], rgb[3], alpha; } lamp[4];
} race_lamp_shadows;
/* 0038E7F8 selection -> 0038F168 shadow uniforms; no simulation writes. */
int race_lights_shadows(const gt4_course *, int triangle, const float pos[3],
                        const uint8_t *environment, size_t environment_size,
                        race_lamp_shadows *);
#endif
