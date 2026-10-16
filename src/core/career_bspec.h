#ifndef MOON_CAREER_BSPEC_H
#define MOON_CAREER_BSPEC_H
#include <stdint.h>
/* Persistent statistics (0x180), course skill bytes (8), live style (0x40). */
int gt4_career_bspec_band(float rating);
void gt4_career_bspec_seed(const uint8_t experience[0x180], const uint8_t course[8], int band,
                           uint8_t style[0x40]);
void gt4_career_bspec_commit(uint8_t experience[0x180], uint8_t course[8], int band,
                             int course_kind, int bonus, const uint8_t style[0x40]);
#endif
