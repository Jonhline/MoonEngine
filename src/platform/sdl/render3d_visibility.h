/* Conservative spatial hierarchy. Primitive order remains in the caller's mesh. */
#ifndef MOON_RENDER3D_VISIBILITY_H
#define MOON_RENDER3D_VISIBILITY_H
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    float lo[3], hi[3];
    unsigned first, count, right, min_group, max_group;
} r3d_visibility_node;
typedef struct {
    r3d_visibility_node *nodes;
    unsigned *groups;
    uint8_t *visible;
    unsigned count, used;
    uint64_t tests, rejected;
} r3d_visibility;

static const float *visibility_bounds(const void *bounds, size_t stride, unsigned group)
{
    return (const float *)((const uint8_t *)bounds + stride * group);
}
static unsigned visibility_build_node(r3d_visibility *tree, const void *bounds, size_t stride,
                                      unsigned first, unsigned count)
{
    unsigned index = tree->used++;
    r3d_visibility_node *node = tree->nodes + index;
    node->first = first;
    node->count = count;
    node->min_group = ~0u;
    for (int k = 0; k < 3; k++) {
        node->lo[k] = INFINITY;
        node->hi[k] = -INFINITY;
    }
    for (unsigned i = first; i < first + count; i++) {
        unsigned group = tree->groups[i];
        const float *box = visibility_bounds(bounds, stride, group);
        if (group < node->min_group)
            node->min_group = group;
        if (group > node->max_group)
            node->max_group = group;
        for (int k = 0; k < 3; k++) {
            node->lo[k] = fminf(node->lo[k], box[k]);
            node->hi[k] = fmaxf(node->hi[k], box[k + 3]);
        }
    }
    if (count <= 8)
        return index;
    int axis = 0;
    for (int k = 1; k < 3; k++)
        if (node->hi[k] - node->lo[k] > node->hi[axis] - node->lo[axis])
            axis = k;
    float middle = node->lo[axis] + (node->hi[axis] - node->lo[axis]) * .5f;
    unsigned split = first;
    for (unsigned i = first; i < first + count; i++) {
        const float *box = visibility_bounds(bounds, stride, tree->groups[i]);
        if ((box[axis] + box[axis + 3]) * .5f < middle) {
            unsigned swap = tree->groups[split];
            tree->groups[split++] = tree->groups[i];
            tree->groups[i] = swap;
        }
    }
    /* Cap depth even for highly overlapping/degenerate authored geometry. */
    if (split - first < count / 4 || split - first > count - count / 4)
        split = first + count / 2;
    visibility_build_node(tree, bounds, stride, first, split - first);
    node->right = visibility_build_node(tree, bounds, stride, split, first + count - split);
    return index;
}
static void r3d_visibility_free(r3d_visibility *tree)
{
    free(tree->nodes);
    free(tree->groups);
    free(tree->visible);
    memset(tree, 0, sizeof *tree);
}
static int r3d_visibility_build(r3d_visibility *tree, const void *bounds, size_t stride,
                                unsigned count)
{
    memset(tree, 0, sizeof *tree);
    if (!count || count > SIZE_MAX / (2 * sizeof *tree->nodes))
        return 0;
    tree->nodes = calloc((size_t)count * 2, sizeof *tree->nodes);
    tree->groups = malloc((size_t)count * sizeof *tree->groups);
    tree->visible = malloc(count);
    if (!tree->nodes || !tree->groups || !tree->visible) {
        r3d_visibility_free(tree);
        return 0;
    }
    tree->count = count;
    for (unsigned i = 0; i < count; i++) {
        const float *box = visibility_bounds(bounds, stride, i);
        for (int k = 0; k < 3; k++)
            if (!isfinite(box[k]) || !isfinite(box[k + 3]) || box[k] > box[k + 3]) {
                r3d_visibility_free(tree);
                return 0;
            }
        tree->groups[i] = i;
    }
    visibility_build_node(tree, bounds, stride, 0, count);
    return 1;
}
/* Use the same support-point expression and tolerance as the retained flat
   test. Union bounds can only reject groups outside the same plane. */
static void visibility_select_node(r3d_visibility *tree, unsigned index, const float planes[6][4],
                                   const void *bounds, size_t stride, unsigned first, unsigned end,
                                   unsigned mask)
{
    const r3d_visibility_node *node = tree->nodes + index;
    if (node->max_group < first || node->min_group >= end)
        return;
    tree->tests++;
    for (int i = 0; i < 6; i++) {
        if (!(mask & (1u << i)))
            continue;
        float maximum = planes[i][3], minimum = maximum;
        for (int k = 0; k < 3; k++) {
            float n = planes[i][k];
            maximum += n * (n >= 0 ? node->hi[k] : node->lo[k]);
            minimum += n * (n >= 0 ? node->lo[k] : node->hi[k]);
        }
        if (maximum < -.001f) {
            tree->rejected += node->count;
            return;
        }
        if (minimum >= -.001f)
            mask &= ~(1u << i);
    }
    if (mask && node->right) {
        visibility_select_node(tree, index + 1, planes, bounds, stride, first, end, mask);
        visibility_select_node(tree, node->right, planes, bounds, stride, first, end, mask);
        return;
    }
    for (unsigned i = node->first; i < node->first + node->count; i++) {
        unsigned group = tree->groups[i];
        if (group < first || group >= end)
            continue;
        int visible = 1;
        if (mask) {
            const float *box = visibility_bounds(bounds, stride, group);
            tree->tests++;
            for (int plane = 0; plane < 6; plane++) {
                if (!(mask & (1u << plane)))
                    continue;
                float d = planes[plane][3];
                for (int k = 0; k < 3; k++)
                    d += planes[plane][k] * (planes[plane][k] >= 0 ? box[k + 3] : box[k]);
                if (d < -.001f) {
                    visible = 0;
                    break;
                }
            }
        }
        tree->visible[group] = (uint8_t)visible;
    }
}
static void r3d_visibility_select(r3d_visibility *tree, const float planes[6][4],
                                  const void *bounds, size_t stride, unsigned first, unsigned end)
{
    memset(tree->visible + first, 0, end - first);
    tree->tests = tree->rejected = 0;
    visibility_select_node(tree, 0, planes, bounds, stride, first, end, 63);
}
#endif
