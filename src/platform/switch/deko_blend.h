/* Native mapping of the desktop material blend boundary. */
#ifndef MOON_DEKO_BLEND_H
#define MOON_DEKO_BLEND_H
#include <deko3d.h>

static inline DkBlendFactor nd_blend_factor(int a, int b, DkBlendFactor coefficient,
                                            DkBlendFactor inverse, bool *negative)
{
    if (!a)
        return b ? DkBlendFactor_One : DkBlendFactor_Zero;
    if (a == 1 && !b)
        return coefficient;
    if (a == -1 && b)
        return inverse;
    if (a == -1) {
        *negative = true;
        return coefficient;
    }
    /* Match render3d.c set_blend: hardware factors cannot express 1+C.
       The desktop fallback is One, not C. This is not exact GS blending. */
    return DkBlendFactor_One;
}

static inline void nd_material_blend(bool enabled, unsigned equation, DkColorState *color,
                                     DkBlendState *blend)
{
    dkColorStateDefaults(color);
    dkBlendStateDefaults(blend);
    color->blendEnableMask = enabled ? 1 : 0;
    if (!enabled)
        return;
    /* 00455708 decodes GS ALPHA: (A-B)*C+D. Keep the existing desktop
       boundary, including its explicitly approximate 1+C fallback. */
    unsigned A = equation & 3, B = (equation >> 2) & 3, C = (equation >> 4) & 3,
             D = (equation >> 6) & 3;
    DkBlendFactor coefficient = C == 0   ? DkBlendFactor_SrcAlpha
                                : C == 1 ? DkBlendFactor_DstAlpha
                                         : DkBlendFactor_ConstAlpha;
    DkBlendFactor inverse = C == 0   ? DkBlendFactor_InvSrcAlpha
                            : C == 1 ? DkBlendFactor_InvDstAlpha
                                     : DkBlendFactor_InvConstAlpha;
    bool source_negative = false, destination_negative = false;
    blend->srcColorBlendFactor =
        nd_blend_factor((A == 0) - (B == 0), D == 0, coefficient, inverse, &source_negative);
    blend->dstColorBlendFactor =
        nd_blend_factor((A == 1) - (B == 1), D == 1, coefficient, inverse, &destination_negative);
    if (source_negative && !destination_negative)
        blend->colorBlendOp = DkBlendOp_RevSub;
    else if (destination_negative && !source_negative)
        blend->colorBlendOp = DkBlendOp_Sub;
    else if (source_negative && destination_negative)
        blend->srcColorBlendFactor = blend->dstColorBlendFactor = DkBlendFactor_Zero;
}
#endif
