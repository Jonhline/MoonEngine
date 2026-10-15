#include "fsr.h"
int fsr_available(void)
{
    return 0;
}
GLuint fsr_resolve(GLuint input, int w, int h, int output_width, int output_height)
{
    (void)w;
    (void)h;
    (void)output_width;
    (void)output_height;
    return input;
}
