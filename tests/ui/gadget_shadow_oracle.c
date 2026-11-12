/* Original scalar presentation roots, never linked into the game. */
#include "ee.h"
#include "course_gadget.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "gadget shadow %d: %s\n", __LINE__, #x);                               \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
extern ee_fn recomp_lookup(uint32_t);
static int selected;
static void draw(ee_ctx *e)
{
    selected = (int)e->r[5].uw[0];
}
static ee_fn hook(uint32_t a)
{
    return a == 0x455708 ? draw : NULL;
}
static ee_fn lookup(uint32_t a)
{
    ee_fn h = hook(a);
    return h ? h : recomp_lookup(a);
}
static void put(ee_ctx *e, unsigned a, float f)
{
    memcpy(e->ram + a, &f, 4);
}
static void call(ee_ctx *e, unsigned a)
{
    e->r[29].uw[0] = 0x1ef0000;
    recomp_lookup(a)(e);
}
int main(int argc, char **argv)
{
    CHECK(argc>=2);
    ee_ctx e;
    CHECK(!ee_init(&e));
    CHECK(ee_load_image(&e,argv[1],0x100000)>0x5a3900);
    e.lookup = lookup;
    e.hook = hook;
    jmp_buf j;
    e.exit_jmp = &j;
    if (setjmp(j)) {
        fprintf(stderr, "original oracle failed: %s\n", e.end_detail);
        return 1;
    }
    const unsigned obj = 0x1800000, out = 0x1810000, env = 0x1820000, desc = 0x1830000;
    for (unsigned i = 0; i < 2048; i++) {
        gt4_course_gadget g = {0};
        float d[3] = {.11f * (int)(i % 17) - .8f, -.3f - .01f * (i % 23), .07f * (i % 19)};
        g.normal[0] = .01f * (i % 31);
        g.normal[1] = .6f + .01f * (i % 11);
        g.normal[2] = -.02f * (i % 13);
        g.plane = .17f * (int)(i % 53) - 4;
        memcpy(e.ram + obj + 0x34, g.normal, 12);
        put(&e, obj + 0x40, g.plane);
        memcpy(e.ram + 0x621d18, d, 12);
        e.r[4].uw[0] = obj;
        e.r[5].uw[0] = out;
        call(&e, 0x3e4968);
        float m[16], expected[16];
        gt4_course_gadget_shadow_matrix(&g, d, m);
        memcpy(expected, e.ram + out, 64);
        if (argc > 2 && !strcmp(argv[2], "--matrix-negative"))
            m[12] += .1f;
        for (int k = 0; k < 16; k++)
            CHECK(fabsf(m[k]-expected[k])<=2e-6f*fmaxf(1,fabsf(expected[k])));
        float sun = (i % 256) / 255.f, shade = ((i * 7) % 256) / 255.f, lit = .8f, dark = .25f;
        put(&e, env + 0xab0, lit);
        put(&e, env + 0xab8, dark);
        e.r[4].uw[0] = env;
        e.fpr[12].f = sun;
        e.fpr[13].f = shade;
        call(&e, 0x3983e0);
        float alpha = gt4_course_gadget_shadow_alpha(sun, shade, lit, dark);
        if (argc > 2 && !strcmp(argv[2], "--alpha-negative"))
            alpha += .1f;
        CHECK(fabsf(alpha-e.fpr[0].f)<=2e-6f);
        unsigned type = i % 32;
        int model = (int)(i % 47);
        ee_w32(&e, 0x6d6054, desc);
        ee_w16(&e, desc + 32 + type * 160, (uint16_t)model);
        ee_w32(&e, desc + 16, 0x1840000);
        for (int shadow = 0; shadow < 2; shadow++) {
            e.r[4].uw[0] = type;
            e.r[5].uw[0] = shadow;
            selected = -1;
            call(&e, 0x3e53b8);
            int actual = gt4_course_gadget_model_index(model, shadow);
            if (argc > 2 && !strcmp(argv[2], "--model-negative"))
                actual = model * 2;
            CHECK(actual==selected);
        }
    }
    ee_free(&e);
    puts("PASS 2048 matrices/alpha and 4096 paired original model selections, tolerance 2e-6");
    return 0;
}
