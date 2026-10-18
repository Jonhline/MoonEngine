/* Contact transitions independent of an SDL window or GPU backend. */
#ifndef MOON_TOUCH_CONTACT_H
#define MOON_TOUCH_CONTACT_H
#include "plat_sdl.h"
typedef struct {
    unsigned id, x, y;
    int ended;
} moon_touch_point;
typedef struct {
    int active, blocked, mode;
    unsigned id, x, y;
} moon_touch_contact;
static inline int moon_touch_sample(moon_touch_contact *s, const moon_touch_point *points,
                                    int count, int handheld, int focused, plat_key_event *ev)
{
    if (s->mode != handheld || !focused) {
        s->mode = handheld;
        s->blocked = 1;
        if (s->active) {
            s->active = 0;
            ev->touch = 4;
            return 1;
        }
    }
    if (!count)
        s->blocked = 0;
    if (!s->active && !s->blocked && count && handheld && focused) {
        s->id = points[0].id;
        s->x = points[0].x;
        s->y = points[0].y;
        s->active = 1;
        ev->touch = 1;
    } else if (s->active) {
        const moon_touch_point *finger = NULL;
        for (int i = 0; i < count; i++)
            if (points[i].id == s->id && !points[i].ended)
                finger = &points[i];
        if (!finger) {
            s->active = 0;
            s->blocked = count != 0;
            ev->touch = 3;
        } else if (finger->x != s->x || finger->y != s->y) {
            s->x = finger->x;
            s->y = finger->y;
            ev->touch = 2;
        }
    }
    if (!ev->touch)
        return 0;
    ev->touch_id = s->id;
    ev->touch_x = s->x / 1280.f;
    ev->touch_y = s->y / 720.f;
    return 1;
}
#endif
