/* Original name-message envelope and replay option-word selection. */
#include "ee.h"
#include "race_hud_name.h"
#include <stdio.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t pc);
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "name oracle %d: %s\n", __LINE__, #x);                                 \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static void putf(ee_ctx *c, unsigned at, float f)
{
    memcpy(c->ram + at, &f, 4);
}
static int layout;
static void get_layout(ee_ctx *c)
{
    c->r[2].ud[0] = layout;
}
static void clear(ee_ctx *c)
{
    memset(c->ram + c->r[4].uw[0], c->r[5].uw[0], c->r[6].uw[0]);
}
static void no_op(ee_ctx *c)
{
    (void)c;
}
static void position(ee_ctx *c)
{
    ee_w16(c, c->r[4].uw[0] + 4, c->r[5].uw[0]);
    ee_w16(c, c->r[4].uw[0] + 6, c->r[6].uw[0]);
}
static void size(ee_ctx *c)
{
    ee_w16(c, c->r[4].uw[0] + 8, c->r[5].uw[0]);
    ee_w16(c, c->r[4].uw[0] + 10, c->r[6].uw[0]);
}
static void alignment(ee_ctx *c)
{
    ee_w8(c, c->r[4].uw[0] + 1, c->r[5].uw[0]);
    ee_w8(c, c->r[4].uw[0] + 2, c->r[6].uw[0]);
}
static ee_fn boundary(uint32_t pc)
{
    switch (pc) {
    case 0x3a18a8:
        return get_layout;
    case 0x5a48d8:
        return clear;
    case 0x3aec80:
    case 0x3aec90:
        return no_op;
    default:
        return NULL;
    }
}
static ee_fn lookup(uint32_t pc)
{
    if (pc == 0x1000010)
        return position;
    if (pc == 0x1000020)
        return size;
    if (pc == 0x1000030)
        return alignment;
    return recomp_lookup(pc);
}
int main(int argc, char **argv)
{
    CHECK(argc>=2);
    ee_ctx c;
    CHECK(!ee_init(&c));
    c.lookup = lookup;
    c.hook = boundary;
    CHECK(ee_load_image(&c,argv[1],0x100000)>0);
    unsigned display = 0x1800000, object = display + 0x23dc;
    c.r[29].uw[0] = 0x1ef0000;
    for (int mode = 2; mode <= 4; mode++)
        for (unsigned flags = 0; flags < 1024; flags++) {
            ee_w8(&c, display + 0x43, 0);
            ee_w32(&c, display + 0x60, flags);
            c.r[4].uw[0] = display;
            c.r[5].uw[0] = mode;
            recomp_lookup(0x39ccf0)(&c);
            CHECK((ee_r32(&c,display+0x58)&RACE_DISPLAY_CAR_NAME)==
              (hud_name_flags(mode,flags)&RACE_DISPLAY_CAR_NAME));
        }
    for (int rate = 0; rate < 3; rate++)
        for (int duration = 1; duration <= 5; duration++) {
            memset(c.ram + object, 0, 0x100);
            putf(&c, object + 0x18, .25f);
            putf(&c, object + 0x1c, .25f);
            c.r[4].uw[0] = object + 0x54;
            recomp_lookup(0x3a99a8)(&c);
            c.r[4].uw[0] = object;
            c.fpr[12].f = (float)duration;
            c.fpr[13].f = 0;
            recomp_lookup(0x3a4e00)(&c);
            hud_name_fade f;
            hud_name_start(&f, (float)duration);
            float dt = (float[]){1.f / 60, 1.f / 30, .125f}[rate];
            for (int frame = 0; frame < 420; frame++) {
                c.r[4].uw[0] = object + 0x54;
                c.fpr[12].f = dt;
                recomp_lookup(0x3a9780)(&c);
                hud_name_step(&f, dt);
                if (argc > 2 && frame == 10)
                    f.value = 1 - f.value;
                CHECK(fabsf(f.value-(*(float *)(c.ram+object+0x54)))<.0001f);
                CHECK(f.state==(int)ee_r32(&c,object+0x6c));
            }
        }
    /* Run the original layout table; only widget geometry setters are replaced. */
    unsigned vtable = 0x1805000;
    for (int i = 0; i < 0x4000; i += 4)
        ee_w32(&c, display + i, vtable);
    ee_w32(&c, vtable + 0x38, 0);
    ee_w32(&c, vtable + 0x3c, 0x1000010);
    ee_w32(&c, vtable + 0x40, 0);
    ee_w32(&c, vtable + 0x44, 0x1000020);
    ee_w32(&c, vtable + 0x48, 0);
    ee_w32(&c, vtable + 0x4c, 0x1000030);
    for (layout = 0; layout < 2; layout++) {
        c.r[4].uw[0] = display;
        c.r[5].uw[0] = layout;
        recomp_lookup(0x3a1970)(&c);
        CHECK(ee_r16(&c,object+4)==(layout?320:24));
        CHECK(ee_r16(&c,object+6)==(layout?430:328));
        CHECK(ee_r16(&c,object+8)==(layout?240:280));
        CHECK(ee_r8(&c,object+1)==(layout?2:4) && ee_r8(&c,object+2)==1);
    }
    ee_free(&c);
    puts("PASS 3072 replay masks and 6300 original caption-envelope steps");
    return 0;
}
