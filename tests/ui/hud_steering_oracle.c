/* Execute 003A5D30 including its image geometry; only graphics services
   and the image handle are boundaries. No steering decisions are stubbed. */
#include "ee.h"
#include "race_hud_steering.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t pc);
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "steering oracle %d: %s\n", __LINE__, #x);                             \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
typedef struct {
    float x, y, w, h, angle, alpha;
} draw;
static struct {
    float x, y, sx, sy, angle;
} matrix, saved[4];
static draw draws[4];
static float alpha;
static int depth, count;
static void noop(ee_ctx *c)
{
    (void)c;
}
static void push(ee_ctx *c)
{
    (void)c;
    saved[depth++] = matrix;
}
static void pop(ee_ctx *c)
{
    (void)c;
    matrix = saved[--depth];
}
static void translate(ee_ctx *c)
{
    matrix.x += c->fpr[12].f;
    matrix.y += c->fpr[13].f;
}
static void scale(ee_ctx *c)
{
    matrix.sx *= c->fpr[12].f;
    matrix.sy *= c->fpr[13].f;
}
static void rotate(ee_ctx *c)
{
    matrix.angle += c->fpr[12].f;
}
static void color(ee_ctx *c)
{
    alpha = c->fpr[15].f;
}
static void align_position(ee_ctx *c)
{
    c->r[2].uw[0] = c->r[4].uw[0];
}
static void quad(ee_ctx *c)
{
    if (count >= 4)
        return;
    draw *d = draws + count++;
    d->x = matrix.x + c->fpr[12].f * matrix.sx;
    d->y = matrix.y + c->fpr[13].f * matrix.sy;
    d->w = (c->fpr[14].f - c->fpr[12].f) * matrix.sx;
    d->h = (c->fpr[15].f - c->fpr[13].f) * matrix.sy;
    d->angle = matrix.angle;
    d->alpha = alpha;
}
static ee_fn boundary(uint32_t pc)
{
    switch (pc) {
    case 0x4a53f8:
        return push;
    case 0x4a5400:
        return pop;
    case 0x4a7844:
        return translate;
    case 0x4a7890:
        return scale;
    case 0x4a79d8:
        return rotate;
    case 0x4aa1d8:
        return color;
    case 0x3afd78:
        return align_position;
    case 0x3af6e0:
        return quad;
    case 0x4a2808:
    case 0x4a2c58:
    case 0x4a2b10:
    case 0x499728:
    case 0x49cc48:
        return noop;
    default:
        return NULL;
    }
}
static void putf(ee_ctx *c, uint32_t at, float f)
{
    memcpy(c->ram + at, &f, 4);
}
int main(int argc, char **argv)
{
    CHECK(argc >= 2);
    ee_ctx c;
    CHECK(!ee_init(&c));
    CHECK(ee_load_image(&c, argv[1], 0x100000) > 0);
    c.lookup = recomp_lookup;
    c.hook = boundary;
    const uint32_t object = 0x1800000, image = object + 0x100, descriptor = image + 0x100;
    for (int mode = 0; mode < 14; mode++)
        for (int seen = 0; seen < 2; seen++)
            for (int kind = -1; kind <= 6; kind++)
                for (unsigned flags = 0; flags < 8; flags++) {
                    ee_w32(&c, object + 0x1c, mode);
                    ee_w8(&c, object + 0x46, seen);
                    ee_w8(&c, object + 0x48, kind);
                    ee_w32(&c, object + 0x58, flags);
                    c.r[4].uw[0] = object;
                    recomp_lookup(0x3a1ee0)(&c);
                    CHECK(hud_steering_visible(mode, seen, kind, flags) == (int)c.r[2].uw[0]);
                }
    memset(c.ram + object, 0, 0x100);
    ee_w16(&c, object + 4, 320);
    ee_w16(&c, object + 6, 450);
    ee_w16(&c, object + 8, 85);
    ee_w16(&c, object + 10, 85);
    ee_w8(&c, object + 1, 2);
    ee_w8(&c, object + 2, 2);
    ee_w32(&c, object + 0x18, image);
    ee_w32(&c, image + 0x18, descriptor);
    /* The disc's replay_sign_gtforce is 170 x 170 (checked by hud_detail). */
    ee_w64(&c, descriptor + 0x20, ((uint64_t)169 << 14) | ((uint64_t)169 << 34));
    for (int sample = 0; sample < 1000; sample++) {
        float current = (sample % 181) - 90.f, previous = ((sample * 7) % 181) - 90.f;
        int valid = sample % 7 != 0;
        putf(&c, object + 0x1c, current);
        if (valid)
            putf(&c, object + 0x20, previous);
        else
            ee_w32(&c, object + 0x20, 0x7fffffff);
        putf(&c, object + 0xc, .75f);
        matrix.x = matrix.y = matrix.angle = 0;
        matrix.sx = matrix.sy = 1;
        count = depth = 0;
        c.r[29].uw[0] = 0x1ef0000;
        c.r[4].uw[0] = object;
        recomp_lookup(0x3a5d30)(&c);
        CHECK(count == 4 && depth == 0);
        float angles[4];
        hud_steering_layers(current, previous, valid, angles);
        if (argc > 2 && sample == 0)
            angles[0] += 1; /* Omit the original first layer. */
        for (int i = 0; i < 4; i++) {
            CHECK(draws[i].x == 235 && draws[i].y == 365);
            CHECK(draws[i].w == 170 && draws[i].h == 170);
            CHECK(fabsf(draws[i].angle - angles[i]) < .0001f);
            CHECK(draws[i].alpha == .75f * .4375f);
        }
    }
    ee_free(&c);
    puts("PASS 1792 original visibility cases and 1000 four-layer steering draws");
    return 0;
}
