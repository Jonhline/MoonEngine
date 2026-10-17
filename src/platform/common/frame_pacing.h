#ifndef MOON_FRAME_PACING_H
#define MOON_FRAME_PACING_H
#include <stdint.h>
typedef struct {
    uint64_t anchor, frame;
    unsigned rate;
} moon_frame_pacer;
/* Absolute rational deadlines avoid rounding drift and catch-up bursts. */
uint64_t moon_frame_deadline(moon_frame_pacer *pacer, uint64_t now, unsigned rate);
#endif
