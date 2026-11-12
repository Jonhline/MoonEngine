/* Recompiled comparator, including original rational atan; no math stubs. */
#include "ee.h"
#include "menu_navigation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t pc);
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "navigation oracle line %d case %u: %s\n", __LINE__, sample, #x);      \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static unsigned sample;
static void putf(ee_ctx *c, uint32_t at, float f)
{
    memcpy(c->ram + at, &f, 4);
}
static void rect(ee_ctx *c, uint32_t at, menu_nav_rect r)
{
    memcpy(c->ram + at, &r, sizeof r);
}
static void item(ee_ctx *c, uint32_t at, const menu_nav_item *v)
{
    ee_w32(c, at, v->id);
    ee_w32(c, at + 4, v->group);
    rect(c, at + 8, v->source);
    rect(c, at + 24, v->target);
}
static uint32_t seed = 271018;
static unsigned random_word(void)
{
    seed = seed * 1664525u + 1013904223u;
    return seed;
}
static menu_nav_rect random_rect(void)
{
    float x = (int)(random_word() % 1300) - 650, y = (int)(random_word() % 1000) - 500;
    return (menu_nav_rect){x, y, x + (random_word() % 180), y + (random_word() % 90)};
}
int main(int argc, char **argv)
{
    CHECK(argc >= 2);
    ee_ctx c;
    CHECK(!ee_init(&c));
    c.lookup = recomp_lookup;
    CHECK(ee_load_image(&c, argv[1], 0x100000)>0);
    const uint32_t nav = 0x1800000, first = nav + 0x100, second = first + 0x40;
    for (sample = 0; sample < 40000; sample++) {
        menu_nav n = {(float[]){0, 20, 45, 50}[sample % 4], (sample >> 2) & 1, (sample >> 3) & 1,
                      10, random_rect()};
        menu_nav_item a = {sample % 7 == 0 ? 10 : 11, (int)(random_word() % 3), random_rect(),
                           random_rect()};
        menu_nav_item b = {sample % 11 == 0 ? 10 : 12, (int)(random_word() % 3), random_rect(),
                           random_rect()};
        ee_w32(&c, nav + 0x10, n.start_front);
        ee_w32(&c, nav + 0x14, n.wraparound);
        ee_w32(&c, nav + 0x38, n.current);
        putf(&c, nav + 0xc, n.clip_angle);
        rect(&c, nav + 0x18, n.source);
        item(&c, first, &a);
        item(&c, second, &b);
        memset(c.r, 0, sizeof c.r);
        c.r[29].uw[0] = 0x1ef0000;
        c.r[4].uw[0] = nav;
        c.r[5].uw[0] = first;
        c.r[6].uw[0] = second;
        c.fpr[12].f = 0;
        recomp_lookup(0x26f2b8)(&c);
        int actual = menu_nav_less_shift(&n, &a, &b, 0);
        if (argc > 2 && sample == 0)
            actual = !actual;
        CHECK(actual==(int)c.r[2].uw[0]);
        menu_nav_rect r = random_rect();
        float dx = (float[]){1, 0, -1, 0}[sample % 4], dy = (float[]){0, 1, 0, -1}[sample % 4];
        ee_w32(&c, nav, 0);
        putf(&c, nav + 4, dx);
        putf(&c, nav + 8, dy);
        rect(&c, first, r);
        c.r[4].uw[0] = nav;
        c.r[5].uw[0] = first;
        c.r[6].uw[0] = first + 4;
        c.r[7].uw[0] = first + 8;
        c.r[8].uw[0] = first + 12;
        recomp_lookup(0x26f630)(&c);
        menu_nav_rect projected = menu_nav_project(r, dx, dy), original;
        memcpy(&original, c.ram + first, sizeof original);
        CHECK(projected.l==original.l && projected.t==original.t && projected.r==original.r && projected.b==original.b);
    }
    for (sample = 0; sample < 40000; sample++) {
        menu_nav_rect a = random_rect(), b = random_rect();
        const uint32_t stack = first + 0x100;
        rect(&c, stack, a);
        rect(&c, second, b);
        ee_w32(&c, nav + 0x44, stack);
        ee_w32(&c, nav + 0x48, stack + sizeof a);
        ee_w32(&c, nav + 0x4c, stack + 2 * sizeof a);
        c.r[4].uw[0] = nav;
        c.r[5].uw[0] = second;
        recomp_lookup(0x26f868)(&c);
        menu_nav_rect actual = menu_nav_intersect(a, b), original;
        memcpy(&original, c.ram + stack + sizeof a, sizeof original);
        CHECK(!memcmp(&actual, &original, sizeof actual));
    }
    ee_free(&c);
    puts(
        "PASS 40000 directional comparisons vs 0026F2B8, original atan and 40000 clip-stack intersections included");
    return 0;
}
