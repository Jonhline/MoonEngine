#include "vk_surface.h"
#include <stdio.h>
int main(int argc,char **argv)
{
    (void)argv;
    /* Android advertises all rotations. Image content stays in logical
       window coordinates: identity must be declared in every orientation. */
    for(unsigned current=1;current<=128;current<<=1) {
        VkSurfaceTransformFlagsKHR supported=255;
        VkSurfaceTransformFlagBitsKHR selected=moon_vk_surface_transform(
            supported,(VkSurfaceTransformFlagBitsKHR)current);
        if(argc>1) selected=(VkSurfaceTransformFlagBitsKHR)current;
        if(selected!=VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
            fprintf(stderr,"unrotated frame declared pre-rotated: %u\n",current);return 1;
        }
    }
    /* Do not request a transform a presentation engine does not support. */
    if(moon_vk_surface_transform(VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR,
        VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR)!=VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR) return 1;
    puts("PASS compositor orientation for eight surface rotations/mirrors");return 0;
}
