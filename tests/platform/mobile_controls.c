#include "mobile_controls.h"
#include "pad_config.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
static void touch(moon_mobile_controls *s, int phase, int id, int control, float x)
{
    moon_mobile_rect r = moon_mobile_bounds(s, control);
    assert(moon_mobile_contact(s, phase, id, r.x + r.w * x, r.y + r.h * .5f));
}
int main(int argc, char **argv)
{
    gt4_pad_config config;
    gt4_pad_defaults(&config, 0);
    moon_mobile_controls s = {0};
    s.options = (moon_mobile_options){0, 1, 2, 0, 0};
    s.active = 1;
    plat_pad_state p;
    assert(!moon_mobile_contact(&s, 1, 1, .9f, .7f)); // Classic must not synthesize a pad.
    assert(!moon_mobile_pad(&s, &p) && !p.buttons);
    s.options.enabled = 1;
    touch(&s, 1, 10, MOON_MOBILE_STEER, .7f);
    touch(&s, 1, 20, MOON_MOBILE_ACCEL, .5f);
    assert(moon_mobile_pad(&s, &p));
    if (argc > 1) {
        (void)argv;
        p.buttons ^= 1u << GT4_PAD_CROSS;
        if (p.buttons != (1u << GT4_PAD_CROSS))
            return 1;
    }
    assert(fabsf(p.axes[0] - .5f) < .0001f && p.buttons == (1u << GT4_PAD_CROSS));
    gt4_pad_actions actions;
    gt4_pad_evaluate(&config, &p, &actions);
    assert(actions.action[GT4_PAD_ACCEL] == 1 && fabsf(actions.steer + .5f) < .0001f);
    assert(!moon_mobile_contact(&s, 2, 999, .1f, .8f)); // Unknown contacts cannot steal capture.
    moon_mobile_rect r = moon_mobile_bounds(&s, MOON_MOBILE_STEER);
    assert(!moon_mobile_contact(&s, 1, 30, r.x, r.y)); // A second steering finger cannot take over.
    assert(!moon_mobile_contact(&s, 1, 31, NAN, .5f));
    assert(moon_mobile_contact(&s, 2, 20, .5f, .5f));
    moon_mobile_pad(&s, &p);
    assert(!p.buttons); // Moving outside releases a pedal.
    touch(&s, 2, 20, MOON_MOBILE_ACCEL, .5f);
    touch(&s, 3, 20, MOON_MOBILE_ACCEL, .5f);
    moon_mobile_pad(&s, &p);
    assert(!p.buttons && p.axes[0] > .49f);
    moon_mobile_contact(&s, 4, 0, 0, 0);
    moon_mobile_pad(&s, &p);
    assert(!p.buttons && !p.axes[0]);
    touch(&s, 1, 40, MOON_MOBILE_BRAKE, .5f);
    s.controller = 1;
    assert(!moon_mobile_pad(&s, &p));
    moon_mobile_contact(&s, 2, 40, .5f, .5f);
    s.controller = 0;
    moon_mobile_pad(&s, &p);
    assert(!p.buttons); // No stuck pedal after controller takeover.
    for (int layout = 0; layout < 2; layout++)
        for (int size = 0; size < 3; size++) {
            s.options.layout = layout;
            s.options.size = size;
            for (int c = 0; c < MOON_MOBILE_COUNT; c++) {
                r = moon_mobile_bounds(&s, c);
                assert(r.x >= 0 && r.y >= 0 && r.x + r.w <= 1 && r.y + r.h <= 1);
            }
        }
    s.options.layout = 1;
    s.options.steering = 1;
    touch(&s, 1, 50, MOON_MOBILE_STEER, .7f);
    moon_mobile_pad(&s, &p);
    assert(p.axes[0] == 1);
    s.active = 0;
    moon_mobile_contact(&s, 4, 0, 0, 0);
    s.active = 1;
    moon_mobile_pad(&s, &p);
    assert(!p.buttons && !p.axes[0]);
    puts("PASS: Classic/Enhanced, multitouch, capture, cancel, external pad, layout and size.");
    return 0;
}
