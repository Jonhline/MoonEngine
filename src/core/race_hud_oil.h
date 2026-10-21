/* Presentation only: 00344640 -> 003437B0 -> 00350830.
   0039E2F0 maps its boolean through D_006A06A0 = {0,2}. */
#ifndef MOON_RACE_HUD_OIL_H
#define MOON_RACE_HUD_OIL_H
static inline int race_hud_oil_sign(float distance, float threshold)
{
    return threshold <= distance ? 2 : 0;
}
/* 00350830 calls 00350678 before reading +20. Arcade's no-wear
   engines can leave that lazy cache cold. Resolve only the displayed value,
   without changing the simulation's cache or settings. */
static inline int race_hud_oil_sign_settings(float distance, float threshold,
                                              unsigned initialized, unsigned onset)
{
    return race_hud_oil_sign(distance, initialized ? threshold : (float)(onset * 100000u));
}
#endif
