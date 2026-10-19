#ifndef MOON_MENU_REPEAT_H
#define MOON_MENU_REPEAT_H
#include <stdint.h>

/* 00250678/002506A4: delay 20 fields, interval 6. 002517E0 advances
   the counter after the first physical press (00251890 initializes it to 1). */
static inline int gt4_menu_repeat_step(uint16_t *counter)
{
    if (!*counter)
        return 0;
    unsigned previous = (*counter)++;
    if (*counter >= 26)
        *counter = 20;
    return previous == 20;
}
#endif
