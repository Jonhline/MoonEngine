#include "mobile_controls.h"
#include "pad_config.h"
#include <math.h>
#include <string.h>

/* Analog mode keeps the original single wide steering slab; digital mode turns
   it into two round buttons (left / right half of the same area). Every other
   control is a circle, so its bounds are the square that circumscribes it. */
static const moon_mobile_rect k_steer_analog = {.06f, .53f, .26f, .36f};
static const moon_mobile_rect k_digital[MOON_MOBILE_COUNT] = {
    /* STEER_LEFT  */ {.078125f, .6475f, .09375f, .125f},
    /* STEER_RIGHT */ {.21875f, .6475f, .09375f, .125f},
    /* ACCEL       */ {.834375f, .6125f, .13125f, .175f},
    /* BRAKE       */ {.66375f, .695f, .1125f, .15f},
    /* UP          */ {.8578125f, .35375f, .084375f, .1125f},
    /* DOWN        */ {.7178125f, .40375f, .084375f, .1125f},
    /* HAND        */ {.5378125f, .73875f, .084375f, .1125f},
    /* CAMERA      */ {.0778125f, .14875f, .084375f, .1125f},
    /* PAUSE       */ {.8578125f, .14875f, .084375f, .1125f},
    /* REVERSE     */ {.4078125f, .73875f, .084375f, .1125f}};

void moon_mobile_reset(moon_mobile_controls *s)
{
    memset(s->contacts, 0, sizeof s->contacts);
}

/* Size preset, right-hand layout mirror and the gesture-bar floor. */
static moon_mobile_rect mobile_fit(const moon_mobile_controls *s, moon_mobile_rect r)
{
    if (r.w <= 0 || r.h <= 0)
        return (moon_mobile_rect){0};
    float scale = s->options.size == 0 ? .75f : s->options.size == 2 ? 1.25f : 1.f;
    r.x -= r.w * (scale - 1) * .5f;
    r.y -= r.h * (scale - 1) * .5f;
    r.w *= scale;
    r.h *= scale;
    if (s->options.layout)
        r.x = 1 - r.x - r.w;
    /* No hit box may reach into the gesture bar. */
    if (r.y + r.h > .93f)
        r.y = .93f - r.h;
    if (r.y < 0)
        r.y = 0;
    return r;
}

/* The axis reference: the original wide steering area, in both modes. It keeps
   analog values and digital sign thresholds byte-for-byte as they shipped. */
static moon_mobile_rect mobile_steer_reference(const moon_mobile_controls *s)
{
    return mobile_fit(s, k_steer_analog);
}

moon_mobile_rect moon_mobile_bounds(const moon_mobile_controls *s, int control)
{
    if (control < 0 || control >= MOON_MOBILE_COUNT)
        return (moon_mobile_rect){0};
    if (!s->options.steering) {
        if (control == MOON_MOBILE_STEER_LEFT)
            return mobile_fit(s, k_steer_analog);
        if (control == MOON_MOBILE_STEER_RIGHT)
            return (moon_mobile_rect){0};
    }
    return mobile_fit(s, k_digital[control]);
}

/* Circle controls: distance test with a 1.15x finger margin. The analog
   steering pad keeps its rectangle. */
static int mobile_hit(const moon_mobile_controls *s, int control, float x, float y)
{
    moon_mobile_rect r = moon_mobile_bounds(s, control);
    if (r.w <= 0 || r.h <= 0)
        return 0;
    if (control == MOON_MOBILE_STEER_LEFT && !s->options.steering)
        return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
    float cx = r.x + r.w * .5f, cy = r.y + r.h * .5f;
    float radius = fminf(r.w * 640.f, r.h * 480.f) * .5f * 1.15f;
    float dx = (x - cx) * 640.f, dy = (y - cy) * 480.f;
    return dx * dx + dy * dy <= radius * radius;
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
            if (mobile_hit(s, c, x, y)) {
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
                                                -1,
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
            // Leaving a button releases it, but the same contact retains capture.
            float x = s->contacts[i].x, y = s->contacts[i].y;
            if (c == MOON_MOBILE_STEER_LEFT || c == MOON_MOBILE_STEER_RIGHT) {
                moon_mobile_rect r = mobile_steer_reference(s);
                float value = (x - r.x - r.w * .5f) / (r.w * .4f);
                value = fmaxf(-1, fminf(1, value));
                if (s->options.steering)
                    value = fabsf(value) < .15f ? 0 : value < 0 ? -1 : 1;
                pad->axes[0] = value;
            } else if (mobile_hit(s, c, x, y))
                pad->buttons |= 1u << keys[c];
        }
    return 1;
}
