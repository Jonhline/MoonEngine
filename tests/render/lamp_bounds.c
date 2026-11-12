/* P: clip-plane bounds against an independent transformed-corner reference. */
#include "render3d_frustum.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
static uint32_t seed = 0x927ab53u;
static float sample(float lo, float hi)
{
    seed = seed * 1664525u + 1013904223u;
    return lo + (hi - lo) * (seed >> 8) / 16777216.f;
}
int main(int argc, char **argv)
{
    unsigned retained = 0, rejected = 0, straddled = 0;
    for (int i = 0; i < 20000; i++) {
        float projection[16] = {1, 0, 0, 0, 0, 1, 0, 0, .5f, .5f, 1.f / 60, 1, 0, 0, 0, 0};
        r3d_view v = {0};
        float angle = sample(-3.14f, 3.14f);
        v.rot[0] = v.rot[8] = cosf(angle);
        v.rot[2] = sinf(angle);
        v.rot[6] = -sinf(angle);
        v.rot[4] = 1;
        v.pos[0] = sample(-70, 70);
        v.pos[1] = sample(-30, 30);
        v.pos[2] = sample(-20, 80);
        float lo[3], hi[3], planes[6][4], maximum[6];
        for (int k = 0; k < 3; k++) {
            lo[k] = sample(-8, 0);
            hi[k] = sample(0, 8);
        }
        for (int p = 0; p < 6; p++)
            maximum[p] = -1e30f;
        float near_lo = 1e30f, near_hi = -1e30f;
        for (int corner = 0; corner < 8; corner++) {
            float world[3], q[4] = {0};
            for (int r = 0; r < 3; r++) {
                world[r] = v.pos[r];
                for (int k = 0; k < 3; k++)
                    world[r] += v.rot[r * 3 + k] * ((corner & (1 << k)) ? hi[k] : lo[k]);
            }
            for (int r = 0; r < 4; r++) {
                q[r] = projection[12 + r];
                for (int k = 0; k < 3; k++)
                    q[r] += projection[k * 4 + r] * world[k];
            }
            float clip[6] = {q[0], q[3] - q[0], q[1], q[3] - q[1], q[3] - .05f, 1 - q[2]};
            for (int p = 0; p < 6; p++)
                maximum[p] = fmaxf(maximum[p], clip[p]);
            near_lo = fminf(near_lo, clip[4]);
            near_hi = fmaxf(near_hi, clip[4]);
        }
        int expected = 1, edge = 0;
        for (int p = 0; p < 6; p++) {
            expected &= maximum[p] >= -.001f;
            edge |= fabsf(maximum[p] + .001f) < 1e-4f;
        }
        if (edge)
            continue;
        r3d_lamp_frustum(projection, &v, planes);
        if (argc > 1)
            planes[0][3] += 20;
        int actual = r3d_bounds_visible(planes, lo, hi);
        if (actual != expected) {
            fprintf(stderr, "lamp bounds mismatch case=%d expected=%d actual=%d\n", i, expected,
                    actual);
            return 1;
        }
        retained += actual;
        rejected += !actual;
        straddled += near_lo < 0 && near_hi > 0;
    }
    if (retained < 1000 || rejected < 1000 || straddled < 1000)
        return 1;
    printf("PASS 20000 boxes: retained=%u rejected=%u near-straddling=%u\n", retained, rejected,
           straddled);
    return 0;
}
