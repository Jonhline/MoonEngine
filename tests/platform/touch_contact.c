#define SDL_MAIN_HANDLED
#include "../../src/platform/switch/touch_contact.h"
#include <stdio.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "touch contact line %d\n", __LINE__);                                  \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(int argc, char **argv)
{
    (void)argv;
    moon_touch_contact s = {0};
    s.mode = 1;
    moon_touch_point points[2] = {{7, 640, 360, 0}, {8, 100, 100, 0}};
    plat_key_event ev = {0};
    CHECK(moon_touch_sample(&s,points,1,1,1,&ev) && ev.touch==1 && ev.touch_x==.5f && ev.touch_y==.5f);
    ev = (plat_key_event){0};
    CHECK(!moon_touch_sample(&s,points,2,1,1,&ev));
    points[0].x = 700;
    ev = (plat_key_event){0};
    CHECK(moon_touch_sample(&s,points,2,1,1,&ev) && ev.touch==2);
    ev = (plat_key_event){0};
    CHECK(moon_touch_sample(&s,points+1,1,1,1,&ev) && ev.touch==3 && ev.touch_id==7);
    ev = (plat_key_event){0};
    CHECK(!moon_touch_sample(&s,points+1,1,1,1,&ev));
    ev = (plat_key_event){0};
    CHECK(!moon_touch_sample(&s,NULL,0,1,1,&ev));
    ev = (plat_key_event){0};
    CHECK(moon_touch_sample(&s,points,1,1,1,&ev) && ev.touch==1);
    ev = (plat_key_event){0};
    CHECK(moon_touch_sample(&s,points,1,1,0,&ev) && ev.touch==4);
    ev = (plat_key_event){0};
    CHECK(!moon_touch_sample(&s,points,1,1,1,&ev));
    moon_touch_sample(&s, NULL, 0, 1, 1, &ev);
    ev = (plat_key_event){0};
    moon_touch_sample(&s, points, 1, 1, 1, &ev);
    ev = (plat_key_event){0};
    moon_touch_sample(&s, points, 1, 0, 1, &ev);
    if (argc > 1)
        ev.touch = 3;
    CHECK(ev.touch==4);
    puts("Native touch: down/move/up, owner isolation, focus and docking cancellation PASS");
    return 0;
}
