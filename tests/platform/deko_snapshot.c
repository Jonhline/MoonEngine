/* Independent float vertex projection probes the conservative copied span. */
#include "../../src/platform/switch/deko_snapshot.h"
#include <stdio.h>
#include <string.h>
static unsigned random_state = 17;
static float random_unit(void)
{
    random_state = random_state * 1664525u + 1013904223u;
    return (random_state >> 8) / 16777216.f;
}
int main(int argc, char **argv)
{
    int negative = argc > 1 && !strcmp(argv[1], "--negative");
    unsigned reduced = 0;
    for (unsigned trial = 0; trial < 1000; ++trial) {
        float p[64][4] = {{0}}, lo[3], hi[3];
        int viewport[4] = {37, 11, 886, 500};
        p[0][0] = p[1][1] = p[2][2] = 1;
        p[3][0] = (random_unit() - .5f) * 30;
        p[3][2] = 40 + random_unit() * 100;
        float angle = (random_unit() - .5f) * 1.5f;
        p[0][0] = p[2][2] = cosf(angle);
        p[0][2] = sinf(angle);
        p[2][0] = -sinf(angle);
        p[4][0] = (random_unit() - .5f) * 10;
        p[4][1] = (random_unit() - .5f) * 10;
        p[5][0] = .99f;
        p[5][1] = .1f;
        p[6][1] = p[7][2] = 1;
        p[7][3] = trial & 1;
        p[8][0] = .6f + random_unit() * 2;
        p[8][1] = .8f + random_unit() * 2;
        p[8][2] = .2f;
        p[15][0] = (random_unit() - .5f) * .2f;
        p[15][1] = (random_unit() - .5f) * .2f;
        for (unsigned k = 0; k < 3; ++k) {
            lo[k] = -1 - random_unit() * 4;
            hi[k] = 1 + random_unit() * 4;
        }
        struct nd_snapshot_span span = nd_snapshot_horizontal(p, lo, hi, viewport, 960);
        struct nd_snapshot_rect rect = nd_snapshot_bounds(p, lo, hi, viewport, 960, 540);
        reduced += span.width < 960 && rect.height < 540;
        if (negative)
            span.width = 0;
        for (unsigned sample = 0; sample < 100; ++sample) {
            float pos[3], world[3], rel[3], x = 0, y = 0, z = 0;
            for (unsigned k = 0; k < 3; ++k)
                pos[k] = lo[k] + (hi[k] - lo[k]) * random_unit();
            for (unsigned row = 0; row < 3; ++row) {
                world[row] =
                    p[row][0] * pos[0] + p[row][1] * pos[1] + p[row][2] * pos[2] + p[3][row];
            }
            if (p[7][3] > 0)
                world[1] = -world[1];
            for (unsigned row = 0; row < 3; ++row) {
                rel[row] = world[row] - p[4][row];
                x += rel[row] * p[5][row];
                y += rel[row] * p[6][row];
                z += rel[row] * p[7][row];
            }
            float clip = x * p[8][0];
            clip += p[15][0] * z;
            float pixel = viewport[0] + (.5f + .5f * (clip / z)) * viewport[2];
            float y_clip = y * p[8][1] + p[15][1] * z;
            float pixel_y = viewport[1] + (.5f - .5f * (y_clip / z)) * viewport[3];
            if (pixel_y >= 0 && pixel_y < 540 &&
                (pixel_y < rect.y || pixel_y >= rect.y + rect.height))
                return 1;
            if (pixel >= 0 && pixel < 960 && (pixel < span.x || pixel >= span.x + span.width))
                return 1;
        }
        p[3][2] = 0; /* A box crossing the camera must retain the full snapshot. */
        span = nd_snapshot_horizontal(p, lo, hi, viewport, 960);
        if (span.x || span.width != 960)
            return 1;
        p[14][0] = 1;
        span = nd_snapshot_horizontal(p, lo, hi, viewport, 960);
        if (span.width != 960)
            return 1;
    }
    if (reduced != 1000)
        return 1;
    puts("100000 projected vertices retained; 1000 spans reduced; near/skin fallback pass");
    return 0;
}
