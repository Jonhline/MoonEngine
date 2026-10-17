#include "frame_pacing.h"
uint64_t moon_frame_deadline(moon_frame_pacer *pacer, uint64_t now, unsigned rate)
{
    if (!rate) {
        pacer->rate = 0;
        return now;
    }
    uint64_t deadline = pacer->anchor + pacer->frame * 1000000000ull / rate;
    if (pacer->rate != rate || now > deadline + 1000000000ull / rate) {
        pacer->anchor = now;
        pacer->frame = 1;
        pacer->rate = rate;
        return now;
    }
    pacer->frame++;
    return deadline;
}
