#include "career_bspec.h"
#include <string.h>
static unsigned word(const uint8_t *p)
{
    return p[0] | (unsigned)p[1] << 8;
}
static void put(uint8_t *p, unsigned value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}
static float constant(uint32_t bits)
{
    float f;
    memcpy(&f, &bits, 4);
    return f;
}
int gt4_career_bspec_band(float rating)
{
    /* Original: func_004683B8 +0x00 / func_00468418 +0x10 */
    float f = (rating - constant(0x3eb33333)) / constant(0x3f0ccccc);
    if (f < 0)
        f = 0;
    if (f > 1)
        f = 1;
    int band = (int)(f * 8);
    return band < 8 ? band : 7;
}
void gt4_career_bspec_seed(const uint8_t experience[0x180], const uint8_t course[8], int band,
                           uint8_t style[0x40])
{
    /* Original: func_00468710 +0x00..0x130; 00468690 removes course suffix 24. */
    memset(style, 0, 0x40);
    for (int row = 0; row < 3; row++)
        for (int col = 0; col < 8; col++)
            if (band || col != 7)
                put(style + row * 16 + col * 2, word(experience + row * 128 + col * 16 + band * 2));
    if (course)
        for (int i = 0; i < 8; i++)
            put(style + 0x30 + i * 2, (unsigned)course[i] << 8);
}
void gt4_career_bspec_commit(uint8_t experience[0x180], uint8_t course[8], int band,
                             int course_kind, int bonus, const uint8_t style[0x40])
{
    /* Original: func_00468860 +0x40..0xC8 */
    for (int row = 0; row < 3; row++)
        for (int col = 0; col < 8; col++) {
            if (!band && col == 7)
                continue;
            uint8_t *dst = experience + row * 128 + col * 16 + band * 2;
            int value = (int)word(style + row * 16 + col * 2);
            if (bonus) {
                value = (int)word(dst) + 2 * (value - (int)word(dst));
                if (value > 65535)
                    value = 65535;
            }
            put(dst, (unsigned)value);
        }
    if (course_kind < 0 || !course)
        return;
    /* Original: func_00468860 +0x108..0x1B8: one group off-road, two road;
       delta is added to the aggregate u16 without saturation. */
    int groups = course_kind ? 2 : 1;
    if (!course_kind)
        memset(course + 4, 0, 4); /* Unwritten temporary bytes are zero in the host boundary. */
    for (int row = 0; row < groups; row++)
        for (int col = 0; col < 4; col++) {
            int i = row * 4 + col, old = course[i], value = (int)word(style + 0x30 + i * 2) >> 8;
            if (bonus) {
                value = old + 2 * (value - old);
                if (value >= 256)
                    value = 255;
            }
            uint8_t *dst = experience + 0xf0 + row * 128;
            put(dst, word(dst) + (unsigned)(value - old));
            course[i] = (uint8_t)value;
        }
}
