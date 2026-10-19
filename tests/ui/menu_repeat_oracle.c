/* Integer menu repeat clock against the original leaf function. */
#include "ee.h"
#include "menu_repeat.h"
#include <stdio.h>
#include <stdlib.h>
ee_fn recomp_lookup(uint32_t pc);
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"repeat oracle line %d: %s\n",__LINE__,#x); return 1; } } while (0)
int main(int argc, char **argv)
{
    CHECK(argc >= 2);
    ee_ctx c;
    CHECK(!ee_init(&c));
    c.lookup = recomp_lookup;
    CHECK(ee_load_image(&c, argv[1], 0x100000) > 0);
    const uint32_t object = 0x1800000;
    ee_w16(&c, object + 0xa4, 20);
    ee_w16(&c, object + 0xa6, 6);
    for (unsigned start = 0; start <= 26; start++) {
        uint16_t counter = (uint16_t)start;
        ee_w16(&c, object + 0x94, counter);
        for (int frame = 0; frame < 300; frame++) {
            c.r[4].uw[0] = object;
            c.r[5].uw[0] = 0;
            recomp_lookup(0x2517e0)(&c);
            int fire = gt4_menu_repeat_step(&counter);
            if (argc > 2 && frame == 0) fire = !fire;
            CHECK(fire == (int)c.r[2].uw[0]);
            CHECK(counter == ee_r16(&c, object + 0x94));
        }
    }
    ee_free(&c);
    puts("8100 repeat ticks match original 002517E0 exactly (20-field delay, 6-field interval)");
    return 0;
}
