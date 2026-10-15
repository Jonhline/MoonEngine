/* Full-target GPU composition vertices, independent of presentation aspect. */
#ifndef OPENGT4_DEKO_COMPOSITION_H
#define OPENGT4_DEKO_COMPOSITION_H
#include <string.h>

typedef struct nd_ui_vertex {
    float x, y, u, v, color[4];
} nd_ui_vertex;

static inline void nd_full_image_quad(nd_ui_vertex vertices[6])
{
    /* Studio layers are 640x448 and environment maps are square. Fitting
       16:9 here leaves uninitialized bands (44 rows on 640x448, 56 on 256x256).
       Presentation fitting belongs to the later UI draw, not this image mix. */
    static const nd_ui_vertex full[6] = {
        {-1, 1, 0, 0, {1, 1, 1, 1}},  {1, 1, 1, 0, {1, 1, 1, 1}}, {-1, -1, 0, 1, {1, 1, 1, 1}},
        {-1, -1, 0, 1, {1, 1, 1, 1}}, {1, 1, 1, 0, {1, 1, 1, 1}}, {1, -1, 1, 1, {1, 1, 1, 1}},
    };
    memcpy(vertices, full, sizeof full);
}
#endif
