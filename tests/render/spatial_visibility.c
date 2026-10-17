#include "render3d_visibility.h"
#include "render3d_frustum.h"
#include <stdio.h>

typedef struct {
    float lo[3], hi[3];
} box;
static uint32_t random_state = 0x56782345;
static float sample(void)
{
    random_state = random_state * 1664525u + 1013904223u;
    return (random_state >> 8) / 16777216.f;
}
int main(int argc, char **argv)
{
    int negative = argc > 1 && !strcmp(argv[1], "--negative");
    enum { COUNT = 4096 };
    box *boxes = calloc(COUNT, sizeof *boxes);
    if (!boxes)
        return 1;
    for (int i = 0; i < COUNT; i++)
        for (int k = 0; k < 3; k++) {
            boxes[i].lo[k] = (sample() - .5f) * 4000;
            boxes[i].hi[k] = boxes[i].lo[k] + sample() * 4;
        }
    /* Also retain degenerate boxes, including the tolerance boundary. */
    boxes[0] = (box){{0, 0, -.001f}, {0, 0, -.001f}};
    r3d_visibility tree = {0};
    if (!r3d_visibility_build(&tree, boxes, sizeof *boxes, COUNT))
        return 1;
    uint64_t flat_tests = 0, tree_tests = 0, rejected = 0;
    int ok = 1;
    for (int trial = 0; trial < 1000 && ok; trial++) {
        r3d_view view = {0};
        view.rot[0] = view.rot[4] = view.rot[8] = 1;
        view.r[0] = view.u[1] = view.f[2] = 1;
        float angle = sample() * 6.2831853f;
        view.r[0] = view.f[2] = cosf(angle);
        view.r[2] = -sinf(angle);
        view.f[0] = sinf(angle);
        for (int k = 0; k < 3; k++)
            view.cam[k] = (sample() - .5f) * 2000;
        view.focal = 240 + sample() * 900;
        view.focal_y = view.focal;
        view.mirror = trial & 1;
        float planes[6][4];
        r3d_frustum(&view, trial & 2 ? .75f : 1, planes);
        unsigned first = trial % 3 ? 0 : COUNT / 3, end = trial % 3 ? COUNT : COUNT / 2;
        r3d_visibility_select(&tree, planes, boxes, sizeof *boxes, first, end);
        if (negative && !trial)
            tree.visible[first] ^= 1;
        for (unsigned i = first; i < end; i++)
            if (tree.visible[i] != r3d_bounds_visible(planes, boxes[i].lo, boxes[i].hi)) {
                fprintf(stderr, "visibility mismatch trial=%d group=%u\n", trial, i);
                ok = 0;
                break;
            }
        flat_tests += end - first;
        tree_tests += tree.tests;
        rejected += tree.rejected;
    }
    printf("spatial support tests=%llu flat=%llu rejected-in-blocks=%llu\n",
           (unsigned long long)tree_tests, (unsigned long long)flat_tests,
           (unsigned long long)rejected);
    ok &= tree_tests < flat_tests && rejected > 0;
    r3d_visibility_free(&tree);
    boxes[0].lo[0] = NAN;
    ok &= !r3d_visibility_build(&tree, boxes, sizeof *boxes, COUNT);
    r3d_visibility_free(&tree);
    free(boxes);
    return ok ? 0 : 1;
}
