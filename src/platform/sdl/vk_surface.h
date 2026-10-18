#pragma once
#include <vulkan/vulkan.h>

/* The renderer's final blit is not pre-rotated. Request compositor rotation
   rather than claiming currentTransform was already applied to the image.
   Khronos surface_rotation sample: no-pre-rotation presentation path. */
static inline VkSurfaceTransformFlagBitsKHR moon_vk_surface_transform(
    VkSurfaceTransformFlagsKHR supported, VkSurfaceTransformFlagBitsKHR current)
{
    return (supported & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
               ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : current;
}
