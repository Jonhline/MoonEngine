#include "android_state.h"
/* One Android app/SDL thread owns this input adapter. No simulation state. */
static moon_mobile_controls controls;
moon_mobile_controls *moon_android_controls(void)
{
    return &controls;
}
int moon_android_pad(plat_pad_state *pad)
{
    return moon_mobile_pad(&controls, pad);
}
