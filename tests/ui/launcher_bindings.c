#include "pad_config.h"
#include <stdio.h>
#include <string.h>
#include "../../src/app/launcher_bindings.inc"
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "binding check %d: %s\n", __LINE__, #x);                               \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(int argc, char **argv)
{
    gt4_pad_config c, before, other;
    for (int i = 0; i < 16; i++)
        c.button[i] = -1;
    for (int i = 0; i < 6; i++)
        c.analog[i] = -1;
    c.analog[GT4_PAD_LX] = -2;
    c.button[GT4_PAD_CROSS] = GT4_PAD_ACCEL;
    c.button[GT4_PAD_SQUARE] = GT4_PAD_BRAKE;
    before = other = c;
    studio_binding_choice p;
    studio_binding_begin(&p, &c, 1, 1);
    for (int i = 0; i < 42; i++) {
        studio_binding_move(&p, 1);
        if (argc > 1)
            studio_binding_set(&c, p.action, p.value);
        CHECK(!memcmp(&c,&before,sizeof c));
    }
    for (int port = 0; port < 2; port++)
        for (int action = 0; action < 11; action++) {
            studio_binding_begin(&p, &c, action, port);
            for (int step = 0; step < 42; step++) {
                studio_binding_move(&p, step % 3 ? 11 : -11);
                CHECK(p.value>=0 && p.value<(action?21:4));
                CHECK(!memcmp(&c,&before,sizeof c));
            }
            p.open = 0;
        }
    p.open = 0;
    CHECK(!memcmp(&c,&before,sizeof c));
    studio_binding_begin(&p, &c, 1, 1);
    p.value = GT4_PAD_R2 + 1;
    studio_binding_confirm(&p, &c);
    CHECK(!p.open && studio_binding_get(&c,1)==GT4_PAD_R2+1);
    CHECK(studio_binding_get(&c,2)==GT4_PAD_SQUARE+1 && !memcmp(&other,&before,sizeof other));
    studio_binding_begin(&p, &c, 1, 1);
    p.value = GT4_PAD_SQUARE + 1;
    CHECK(studio_binding_get(&c,2)==GT4_PAD_SQUARE+1);
    studio_binding_confirm(&p, &c);
    CHECK(studio_binding_get(&c,1)==GT4_PAD_SQUARE+1 && studio_binding_get(&c,2)==0);
    c = before;
    c.analog[GT4_PAD_RYF] = GT4_PAD_ACCEL;
    c.button[GT4_PAD_R1] = GT4_PAD_BRAKE;
    gt4_pad_config multiple = c;
    CHECK(studio_binding_conflict(&c,1,GT4_PAD_SQUARE+1)==2);
    CHECK(studio_binding_conflict(&c,1,GT4_PAD_R1+1)==2);
    studio_binding_begin(&p, &c, 1, 0);
    studio_binding_confirm(&p, &c);
    CHECK(!memcmp(&c,&multiple,sizeof c));
    studio_binding_begin(&p, &c, 1, 0);
    p.value = GT4_PAD_R2 + 1;
    studio_binding_confirm(&p, &c);
    CHECK(c.button[GT4_PAD_R1]==GT4_PAD_BRAKE && c.button[GT4_PAD_SQUARE]==GT4_PAD_BRAKE);
    CHECK(c.button[GT4_PAD_R2]==GT4_PAD_ACCEL && c.analog[GT4_PAD_LX]==-2);
    puts("PASS browsing/cancel preserve mappings; only confirmation changes assignments");
    return 0;
}
