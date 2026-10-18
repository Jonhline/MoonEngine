# Android packages the native game as SDLActivity's libmain.so.
set_target_properties(moonengine PROPERTIES OUTPUT_NAME main)
target_sources(moonengine PRIVATE src/platform/android/android_host.c src/app/renderer_startup.c src/app/disc_checkpoint.c src/app/mobile_options.c)
target_include_directories(moonengine PRIVATE src/platform/android)
target_sources(moon_platform_sdl PRIVATE src/platform/android/android_state.c src/platform/android/mobile_controls.c)
target_include_directories(moon_platform_sdl PUBLIC src/platform/android)
target_compile_definitions(moon_platform_common PRIVATE _FILE_OFFSET_BITS=64)
target_link_libraries(moonengine PRIVATE moon_vk_render moon_vulkan log android)
add_library(moon_vulkan STATIC src/platform/sdl/vulkan_device.c)
target_include_directories(moon_vulkan PUBLIC src/platform/sdl)
target_link_libraries(moon_vulkan PRIVATE Vulkan::Headers SDL2::SDL2)

target_sources(moonengine PRIVATE "${sdl2_SOURCE_DIR}/src/main/android/SDL_android_main.c")
