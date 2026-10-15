#include "ee.h"
#include "career_bspec.h"
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t);
enum { EXP = 0x1800000, STYLE = 0x1810000, COURSE = 0x1820000, STACK = 0x1ef0000 };
static void experience(ee_ctx *c)
{
    c->r[2].uw[0] = EXP;
}
static void course(ee_ctx *c)
{
    c->r[2].uw[0] = COURSE;
}
static void label(ee_ctx *c)
{
    c->r[2].ud[0] = c->r[5].ud[0];
}
static void store(ee_ctx *c)
{
    memcpy(c->ram + COURSE + 0x228, c->ram + c->r[5].uw[0], 8);
}
static ee_fn boundary(uint32_t pc)
{
    switch (pc) {
    case 0x4683a8:
        return experience;
    case 0x468690:
        return label;
    case 0x433c10:
        return course;
    case 0x4336a0:
        return store;
    default:
        return NULL;
    }
}
static ee_fn lookup(uint32_t pc)
{
    ee_fn f = boundary(pc);
    return f ? f : recomp_lookup(pc);
}
static uint32_t next(uint32_t *rng)
{
    return *rng = *rng * 1664525u + 1013904223u;
}
static int call(ee_ctx *c, uint32_t pc, float rating, int kind, int bonus)
{
    memset(c->r, 0, sizeof c->r);
    c->r[4].uw[0] = STYLE;
    c->r[5].uw[0] = 0x1830000;
    c->r[6].sd[0] = kind;
    c->r[7].uw[0] = bonus;
    c->r[29].uw[0] = STACK;
    c->fpr[12].f = rating;
    jmp_buf jump;
    c->exit_jmp = &jump;
    if (setjmp(jump)) {
        fprintf(stderr, "stop %x %s\n", c->end_pc, c->end_detail);
        return 0;
    }
    recomp_lookup(pc)(c);
    return 1;
}
int main(int argc, char **argv)
{
    ee_ctx c;
    if (argc < 2 || ee_init(&c) || ee_load_image(&c, argv[1], 0x100000) < 0x500000)
        return 2;
    c.lookup = lookup;
    c.hook = boundary;
    uint32_t rng = 0x14102026;
    uint8_t exp[0x180], local[8], style[0x40];
    for (int test = 0; test < 4096; test++) {
        for (int i = 0; i < 0x180; i++)
            exp[i] = (uint8_t)next(&rng);
        for (int i = 0; i < 8; i++)
            local[i] = (uint8_t)next(&rng);
        float rating = (float)(test % 1025) / 1024;
        int band = gt4_career_bspec_band(rating);
        memcpy(c.ram + EXP, exp, sizeof exp);
        memcpy(c.ram + COURSE + 0x228, local, 8);
        if (!call(&c, 0x468710, rating, 0, 0))
            return 1;
        gt4_career_bspec_seed(exp, local, band, style);
        if (memcmp(style, c.ram + STYLE, sizeof style)) {
            fprintf(stderr, "seed %d band%d\n", test, band);
            return 1;
        }
        /* Real statistics only increase; test near/full saturation, unchanged,
           zero, both track kinds, optional victory bonus and no track writes. */
        for (int i = 0; i < 32; i++) {
            unsigned old = style[i * 2] | (unsigned)style[i * 2 + 1] << 8;
            unsigned add = test % 7 ? next(&rng) % 8192 : 0, value = old + add;
            if (value > 65535)
                value = 65535;
            style[i * 2] = (uint8_t)value;
            style[i * 2 + 1] = (uint8_t)(value >> 8);
        }
        memcpy(c.ram + STYLE, style, sizeof style);
        int kind = test % 3 - 1, bonus = test % 2;
        if (!call(&c, 0x468860, rating, kind, bonus))
            return 1;
        gt4_career_bspec_commit(exp, local, band, kind, bonus, style);
        if (argc > 2)
            exp[0] ^= 1;
        if (memcmp(exp, c.ram + EXP, sizeof exp) || memcmp(local, c.ram + COURSE + 0x228, 8)) {
            fprintf(stderr, "commit %d band%d kind%d bonus%d\n", test, band, kind, bonus);
            for (int i = 0; i < 0x180; i++)
                if (exp[i] != c.ram[EXP + i])
                    fprintf(stderr, "exp %x %u/%u\n", i, exp[i], c.ram[EXP + i]);
            for (int i = 0; i < 8; i++)
                if (local[i] != c.ram[COURSE + 0x228 + i])
                    fprintf(stderr, "course %d %u/%u\n", i, local[i], c.ram[COURSE + 0x228 + i]);
            return 1;
        }
    }
    ee_free(&c);
    puts(
        "PASS B-Spec: 4096 seed/update tables exact against original, including course skill and saturation");
    return 0;
}
