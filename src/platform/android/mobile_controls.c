#include "mobile_controls.h"
#include "pad_config.h"
#include <math.h>
#include <string.h>
void moon_mobile_reset(moon_mobile_controls *s)
{
    memset(s->contacts, 0, sizeof s->contacts);
}
moon_mobile_rect moon_mobile_bounds(const moon_mobile_controls *s, int control)
{
    static const moon_mobile_rect boxes[MOON_MOBILE_COUNT] = {
        {.06f, .53f, .26f, .36f}, {.83f, .57f, .12f, .31f}, {.68f, .64f, .12f, .24f},
        {.85f, .34f, .10f, .14f}, {.71f, .39f, .10f, .14f}, {.53f, .72f, .10f, .15f},
        {.07f, .14f, .10f, .13f}, {.85f, .14f, .10f, .13f}, {.4f, .72f, .10f, .15f}};
    moon_mobile_rect r = boxes[control];
    float scale = s->options.size == 0 ? .75f : s->options.size == 2 ? 1.25f : 1.f;
    r.x -= r.w * (scale - 1) * .5f;
    r.y -= r.h * (scale - 1) * .5f;
    r.w *= scale;
    r.h *= scale;
    if (s->options.layout)
        r.x = 1 - r.x - r.w;
    return r;
}
int moon_mobile_contact(moon_mobile_controls *s, int phase, int64_t id, float x, float y)
{
    if (!s->active || !s->options.enabled || s->controller || phase == 4) {
        moon_mobile_reset(s);
        return 0;
    }
    if (phase < 1 || phase > 3 || !isfinite(x) || !isfinite(y))
        return 0;
    int slot = -1;
    for (int i = 0; i < 10; i++)
        if (s->contacts[i].used && s->contacts[i].id == id) {
            slot = i;
            break;
        }
    if (phase == 1 && slot < 0) {
        int hit = -1;
        for (int c = 0; c < MOON_MOBILE_COUNT; c++) {
            moon_mobile_rect r = moon_mobile_bounds(s, c);
            if (x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h) {
                hit = c;
                break;
            }
        }
        if (hit < 0)
            return 0;
        for (int i = 0; i < 10; i++)
            if (s->contacts[i].used && s->contacts[i].control == hit)
                return 0;
        for (int i = 0; i < 10; i++)
            if (!s->contacts[i].used) {
                slot = i;
                break;
            }
        if (slot < 0)
            return 0;
        s->contacts[slot].used = 1;
        s->contacts[slot].id = id;
        s->contacts[slot].control = hit;
    }
    if (slot < 0)
        return 0;
    s->contacts[slot].x = x;
    s->contacts[slot].y = y;
    if (phase == 3)
        s->contacts[slot].used = 0;
    return 1;
}
int moon_mobile_pad(const moon_mobile_controls *s, plat_pad_state *pad)
{
    memset(pad, 0, sizeof *pad);
    if (!s->active || !s->options.enabled || s->controller)
        return 0;
    static const int keys[MOON_MOBILE_COUNT] = {-1,
                                                GT4_PAD_CROSS,
                                                GT4_PAD_SQUARE,
                                                GT4_PAD_R2,
                                                GT4_PAD_L2,
                                                GT4_PAD_CIRCLE,
                                                GT4_PAD_SELECT,
                                                GT4_PAD_START,
                                                GT4_PAD_TRIANGLE};
    for (int i = 0; i < 10; i++)
        if (s->contacts[i].used) {
            int c = s->contacts[i].control;
            moon_mobile_rect r = moon_mobile_bounds(s, c);
            // Leaving a button releases it, but the same contact retains capture.
            float x = s->contacts[i].x, y = s->contacts[i].y;
            if (c == MOON_MOBILE_STEER) {
                float value = (x - r.x - r.w * .5f) / (r.w * .4f);
                value = fmaxf(-1, fminf(1, value));
                if (s->options.steering)
                    value = fabsf(value) < .15f ? 0 : value < 0 ? -1 : 1;
                pad->axes[0] = value;
            } else if (x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h)
                pad->buttons |= 1u << keys[c];
        }
    return 1;
}
