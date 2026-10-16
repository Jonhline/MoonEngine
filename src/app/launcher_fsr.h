#ifndef MOON_LAUNCHER_FSR_H
#define MOON_LAUNCHER_FSR_H
#include <stdint.h>
typedef struct launcher_fsr launcher_fsr;
launcher_fsr *launcher_fsr_open(void);
void launcher_fsr_close(launcher_fsr *filter);
uint8_t *launcher_fsr_image(launcher_fsr *filter,const uint8_t *rgba,int width,int height,int output_width,int output_height);
#endif
