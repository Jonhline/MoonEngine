/* Original linear allocator; widget queries are fixture boundaries. */
#include "ee.h"
#include "ui_pack.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
ee_fn recomp_lookup(uint32_t pc);
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "pack oracle %d case %d: %s\n", __LINE__, sample, #x);                 \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
static int sample, count;
static ui_pack_item items[8];
static ui_pack_rect actual[8];
static float pad[4], main_size, cross_size, requested, gap;
static unsigned first = 0x1801000;
static void putf(ee_ctx *c, unsigned at, float f)
{
    memcpy(c->ram + at, &f, 4);
}
static uint32_t boundary_pc;
static void fixture_query(ee_ctx *c)
{
    uint32_t pc = boundary_pc;
    unsigned a0 = c->r[4].uw[0], a1 = c->r[5].uw[0];
    int i = (int)((a1 - first) / 0x100);
    switch (pc) {
    case 0x2e9600:
        for (int k = 0; k < 4; k++)
            putf(c, c->r[5 + k].uw[0], pad[k]);
        break;
    case 0x2e96c0:
        putf(c, a1, requested);
        putf(c, c->r[6].uw[0], cross_size);
        break;
    case 0x2e9790:
        c->fpr[0].f = main_size;
        break;
    case 0x2e97b8:
        c->fpr[0].f = cross_size;
        break;
    case 0x2e94e0:
        c->fpr[0].f = gap;
        break;
    case 0x206868:
        c->r[2].uw[0] = first;
        break;
    case 0x263c40:
        break;
    case 0x2073d8:
        c->r[2].uw[0] = 1;
        break;
    case 0x2e97e0:
        putf(c, c->r[6].uw[0], items[i].main_size);
        putf(c, c->r[7].uw[0], items[i].cross_size);
        break;
    case 0x2e9540:
        c->r[2].uw[0] = items[i].expand_main;
        break;
    case 0x2e95a0:
        c->r[2].uw[0] = items[i].fill_main;
        break;
    case 0x2e9570:
        c->r[2].uw[0] = items[i].expand_cross;
        break;
    case 0x2e95d0:
        c->r[2].uw[0] = items[i].fill_cross;
        break;
    case 0x2e9830:
        actual[i] = (ui_pack_rect){c->fpr[12].f, c->fpr[13].f, c->fpr[14].f, c->fpr[15].f};
        break;
    case 0x207378:
        break;
    case 0x25c300:
        i = (int)((a0 - first) / 0x100);
        c->r[2].uw[0] = i + 1 < count ? first + (i + 1) * 0x100 : 0;
        break;
    default:
        fprintf(stderr,"unhandled fixture query %08x\n",pc);
        exit(2);
    }
}
static ee_fn boundary(uint32_t pc)
{
    if (pc == 0x266338 || pc == 0x2e99e0) return NULL; /* Original pack_reverse accessor executes. */
    boundary_pc = pc;
    return fixture_query;
}
int main(int argc, char **argv)
{
    CHECK(argc>=2);
    ee_ctx c;
    CHECK(!ee_init(&c));
    c.lookup = recomp_lookup;
    c.hook = boundary;
    CHECK(ee_load_image(&c,argv[1],0x100000)>0);
    for (sample = 0; sample < 4096; sample++) {
        count = 1 + sample % 8;
        for (int k = 0; k < 4; k++)
            pad[k] = (float)((sample >> (k * 2)) % 8);
        gap = (float)(sample % 5);
        main_size = 160 + sample % 170;
        cross_size = 80 + sample % 63;
        requested = pad[0] + pad[1] + gap * (count - 1);
        int expanded = 0;
        for (int i = 0; i < count; i++) {
            unsigned flags = (unsigned)(sample + i * 37);
            items[i] = (ui_pack_item){10 + i * 3.f,     12 + i * 2.f,     flags & 1,
                                      (flags >> 1) & 1, (flags >> 2) & 1, (flags >> 3) & 1,
                                      (flags >> 4) & 1};
            requested += items[i].main_size;
            expanded += items[i].expand_main;
            ee_w32(&c, first + i * 0x100 + 0x9c, items[i].reverse ? 4 : 0);
        }
        unsigned box = 0x1800000;
        ee_w32(&c, box + 0xc8, count);
        ee_w32(&c, box + 0xcc, expanded);
        c.r[29].uw[0] = 0x1ef0000;
        c.r[4].uw[0] = box;
        c.r[5].uw[0] = 0;
        recomp_lookup(0x2e99e0)(&c);
        ui_pack_cursor cursor = {pad[0],
                                 main_size - pad[1],
                                 pad[2],
                                 cross_size - (pad[2] + pad[3]),
                                 expanded ? (main_size - requested) / expanded : 0,
                                 count > 1 ? gap : 0};
        for (int i = 0; i < count; i++) {
            if (argc > 2 && sample == 0)
                items[i].reverse ^= 1;
            ui_pack_rect r = ui_pack_next(&cursor, items[i]);
            CHECK(fabsf(r.main-actual[i].main)<.0001f && fabsf(r.cross-actual[i].cross)<.0001f);
            CHECK(fabsf(r.main_size-actual[i].main_size)<.0001f && fabsf(r.cross_size-actual[i].cross_size)<.0001f);
        }
    }
    ee_free(&c);
    puts("PASS 4096 original linear allocations, both ends, expansion, fill and padding");
    return 0;
}
