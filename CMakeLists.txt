cmake_minimum_required(VERSION 3.20)
project(moonengine LANGUAGES C CXX)

# The source default wins over a stale cache; an explicit override still works.
if(DEFINED MOON_VERSION_OVERRIDE)
  set(MOON_VERSION "${MOON_VERSION_OVERRIDE}" CACHE STRING "Release version (X.Y.Z)" FORCE)
else()
  set(MOON_VERSION "0.4.0" CACHE STRING "Release version (X.Y.Z)" FORCE)
endif()
# Dev build (default, what we work on): the developer keys (PgUp/PgDn projects,
# F1/F2 menu roots) and the F4 debug overlay. Stable (OFF): what players get,
# none of those, and the GitHub release check (src/app/app_update.c).
option(MOON_DEV_BUILD "Developer build: debug keys and overlay; no release check" ON)
if(MOON_DEV_BUILD)
  add_compile_definitions(MOON_DEV_BUILD=1)
else()
  add_compile_definitions(MOON_DEV_BUILD=0)
endif()
set(MOON_UPDATE_CONFIG "$ENV{APPDATA}/MoonEngine/publish.ini" CACHE FILEPATH "External private update configuration")
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/tools/configure_update.py" "${MOON_UPDATE_CONFIG}")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/configure_update.py"
  "${MOON_VERSION}" "${MOON_UPDATE_CONFIG}" "${CMAKE_BINARY_DIR}/generated/launcher_release.h"
  COMMAND_ERROR_IS_FATAL ANY)
include_directories("${CMAKE_BINARY_DIR}/generated")

if(ANDROID)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# libm: fminf/fmaxf/fmodf and friends are libm symbols on Linux/macOS; MSVC keeps
# them in the CRT, which is why the tests linked on Windows and not here.
#
# The core/platform stack: moon_core calls plat_file_* (in moon_platform_common)
# and moon_platform_common calls back into core, so NO linear order resolves both --
# MSVC's link.exe rescans archives and never noticed, GNU ld does not. Every target that
# needs these two gets a rescan group on everything but MSVC, and a plain list on MSVC.
if(WIN32)
  # MSVC keeps libm in the CRT: there is no m.lib to link.
  set(MOON_CORE_STACK moon_core moon_platform_common)
  set(MOON_SDL_STACK  moon_core moon_platform_common moon_platform_sdl)
else()
  link_libraries(m)
  set(MOON_CORE_STACK -Wl,--start-group moon_core moon_platform_common -Wl,--end-group)
  set(MOON_SDL_STACK  -Wl,--start-group moon_core moon_platform_common moon_platform_sdl -Wl,--end-group)
endif()

# ---- Third-party libraries, fetched from their official repositories ----
include(FetchContent)
FetchContent_Declare(miniz
  GIT_REPOSITORY https://github.com/richgel999/miniz.git
  GIT_TAG 3.0.2
  GIT_SHALLOW TRUE)
# miniz 3.0.2 declares an old cmake_minimum_required that CMake 4 rejects.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_MakeAvailable(miniz)
unset(CMAKE_POLICY_VERSION_MINIMUM)

if(NINTENDO_SWITCH)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SWITCH_SDL REQUIRED IMPORTED_TARGET sdl2)
  add_library(SDL2::SDL2-static ALIAS PkgConfig::SWITCH_SDL)
  add_library(SDL2::SDL2main INTERFACE IMPORTED)
else()
# SDL2: window, 2D rendering, input. Static, so the program is one file.
if(ANDROID)
  set(SDL_SHARED ON CACHE BOOL "" FORCE)
  set(SDL_STATIC OFF CACHE BOOL "" FORCE)
else()
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
endif()
set(SDL_TEST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL2
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-2.30.9
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(SDL2)
# SDL 2's PipeWire backend predates PipeWire 1.6's const pointers; GCC 14+ makes that an error
if(NOT MSVC AND TARGET SDL2-static)
  target_compile_options(SDL2-static PRIVATE -Wno-error=incompatible-pointer-types)
endif()
if(ANDROID)
  add_library(SDL2::SDL2-static ALIAS SDL2)
endif()
# Tests and probes were written against the Windows CRT and call _putenv_s() unguarded.
# On non-Windows the shim maps that name onto the POSIX call (see the header: an empty
# value must REMOVE the variable, as MSVC does). Placed after the FetchContent calls so
# it applies to this project's targets only, never to SDL2/miniz.
if(NOT WIN32)
  add_compile_options(-include "${CMAKE_SOURCE_DIR}/src/platform/common/msvc_compat.h")
endif()

endif()

if(NINTENDO_SWITCH)
  set(MOON_SWITCH_RENDERER "deko3d" CACHE STRING "Switch renderer: deko3d or mesa")
  set_property(CACHE MOON_SWITCH_RENDERER PROPERTY STRINGS deko3d mesa)
  if(NOT MOON_SWITCH_RENDERER MATCHES "^(deko3d|mesa)$")
    message(FATAL_ERROR "MOON_SWITCH_RENDERER must be deko3d or mesa")
  endif()
endif()

# ---- Native port ----
# Platform-independent game code: data formats, UI, game systems.
set_source_files_properties(src/core/album_slide.c src/core/course_gadget.c src/core/license_image.c src/platform/sdl/license_scene.c PROPERTIES COMPILE_OPTIONS "$<$<C_COMPILER_ID:MSVC>:/fp:strict>;$<$<NOT:$<C_COMPILER_ID:MSVC>>:-ffp-contract=off;-frounding-math>")
add_library(moon_core STATIC
  src/core/placeholder.c
  src/core/inflate.c
  src/core/vol.c
  src/core/mod_merge.c
  src/core/mod_manifest.c
  src/core/model_write.c
  src/core/model.c
  src/core/driver_model.c
  src/core/driver_pose.c
  src/core/course.c
  src/core/assist_line.c
  src/core/course_gadget.c
  src/core/photo_mode.c
  src/core/album_slide.c
  src/core/specdb.c
  src/core/favorites.c
  src/core/spectables.c
  src/core/tex1.c
  src/core/gpb.c
  src/core/mproject.c
  src/core/ui.c
  src/core/ui_motion.c
  src/core/race_hud.c
  src/core/race_smoke.c
  src/core/race_leaves.c
  src/core/race_result.c src/core/race_mission.c src/core/license_image.c src/core/license_presentation.c src/core/license_concourse.c src/core/license_motion.c src/core/logger.c src/core/logger_view.c
  src/core/race_pause.c src/core/race_bspec.c src/core/race_pitmenu.c src/core/race_pit_crew.c
  src/core/race_pit_walk.c
  src/core/course_motion.c
  src/core/course_pregrid.c
  src/core/course_tvcam.c
  src/core/strobe_player.c
  src/core/music.c
  src/core/pcm_music.c
  src/core/race_sound.c
  src/core/spu.c
  src/core/font_layout.c
  src/core/sound.c src/core/sound_sequence.c src/core/sound_sequence_clock.c
  src/core/pss_audio.c
  src/core/ipic.c
  src/core/strobe.c
  src/core/boot_notice.c
  src/core/mot1.c src/core/car_service.c
  src/core/glyph_sdf.c
  src/core/ui_hd.c
  src/core/font.c
  src/core/rtext.c
  src/core/career_blob.c
  src/core/gtmode_game.c
  src/core/career_original.c
  src/core/career_race_lookup.c
  src/core/career_admission.c
  src/core/career_license.c
  src/core/career_license_disc.c
  src/core/career_event.c
  src/core/career_race_lookup_disc.c
  src/core/career_setting.c
  src/core/career_parts.c
  src/core/career_parts_disc.c
  src/core/career_calendar.c
  src/core/career_bspec.c
  src/core/career_home.c
  src/core/career_calendar_disc.c
  src/core/career_prices.c
  src/core/career_prices_disc.c
  src/core/career_oil.c
  src/core/career_oil_disc.c
  src/core/save.c
  src/core/replay_file.c
  src/core/disc_replay.c
  src/core/photo_pause.c
  src/core/ranking_code.c
  src/core/ps2_media.c
  third_party/ps2float/ps2float.c
  src/core/save_state.c
  src/core/save_ps2.c
  src/core/menu.c
  src/core/pad_config.c
  src/core/arcade.c
  src/core/adhoc/adhoc_code.c
  src/core/adhoc/adhoc_vm.c)
target_include_directories(moon_core PUBLIC include src/core)
# Original pose arithmetic keeps intermediate float operations separate.
if(MSVC)
  set_source_files_properties(src/core/driver_pose.c src/core/course_pregrid.c src/core/font_layout.c src/core/career_original.c src/core/career_oil.c src/core/license_presentation.c src/core/license_concourse.c src/core/license_motion.c src/core/race_pit_crew.c src/core/race_pit_walk.c src/core/logger.c PROPERTIES COMPILE_OPTIONS "/fp:strict")
else()
  set_source_files_properties(src/core/driver_pose.c src/core/course_pregrid.c src/core/font_layout.c src/core/career_original.c src/core/career_oil.c src/core/license_presentation.c src/core/license_concourse.c src/core/license_motion.c src/core/race_pit_crew.c src/core/race_pit_walk.c src/core/logger.c PROPERTIES COMPILE_OPTIONS "-ffp-contract=off;-frounding-math")
endif()
target_link_libraries(moon_core PRIVATE miniz)
if(MSVC)
  set_source_files_properties(src/core/sound_sequence_clock.c src/core/car_service.c src/core/mot1.c PROPERTIES COMPILE_OPTIONS "/fp:strict")
else()
  set_source_files_properties(src/core/sound_sequence_clock.c src/core/car_service.c src/core/mot1.c PROPERTIES COMPILE_OPTIONS "-ffp-contract=off;-frounding-math")
endif()

# Vehicle physics rewritten from the original (docs/analysis/PHYSICS.md).
add_library(moon_physics STATIC src/core/physics/fpu_env.c src/core/physics/curve.c src/core/physics/mathf.c src/core/physics/setup.c src/core/physics/record.c src/core/physics/garage.c src/core/physics/runway.c src/core/physics/lines.c src/core/physics/init.c src/core/physics/session.c src/core/physics/laps.c
  src/core/physics/course_objects.c
  src/core/physics/license_session.c
  src/core/physics/engine.c src/core/physics/car.c src/core/physics/aero.c
  src/core/physics/tyre.c src/core/physics/drivetrain.c
  src/core/physics/wheels.c src/core/physics/clutch.c
  src/core/physics/gearbox.c src/core/physics/body.c
  src/core/physics/suspension.c src/core/physics/chassis.c
  src/core/physics/view.c src/core/physics/surface.c src/core/physics/ground.c
  src/core/physics/collision.c src/core/physics/walls.c src/core/physics/driver.c
  src/core/physics/ai.c src/core/physics/slip.c
  src/core/physics/wear.c src/core/physics/race.c
  src/core/physics/misc.c src/core/physics/step.c
  src/core/physics/steer.c src/core/physics/aids.c src/core/physics/race_run.c)
target_include_directories(moon_physics PUBLIC src/core/physics)
# the native race (race_run.c) reads the disc through the core's volume
target_link_libraries(moon_physics PUBLIC ${MOON_CORE_STACK})
# PS2 float semantics: the host rounding mode is set toward zero at run time,
# so no folding across fesetround. (Signed zeros in clamps: gt4_select.)
if(MSVC)
  target_compile_options(moon_physics PRIVATE /fp:strict)
endif()

include(cmake/ReplayMath.cmake)

# Career car construction reuses the native car APIs without adding EE code.
add_library(moon_career_vehicle STATIC src/core/career_vehicle.c)
target_link_libraries(moon_career_vehicle PUBLIC moon_physics ${MOON_CORE_STACK})
if(MSVC)
  target_compile_options(moon_career_vehicle PRIVATE /fp:strict)
else()
  target_compile_options(moon_career_vehicle PRIVATE -ffp-contract=off -frounding-math)
endif()

# Platform storage, including native PNG decoding through miniz.
add_library(moon_platform_common STATIC src/platform/common/frame_pacing.c src/platform/common/file_stdio.c src/platform/common/album_storage.c src/platform/common/jpeg_storage.c src/platform/common/renderer_backend.c)
target_link_libraries(moon_platform_common PRIVATE miniz)
target_include_directories(moon_platform_common PUBLIC include)

# Command-line tools built on the port's own code.
add_executable(gt4vol tools/c/gt4vol.c)
target_link_libraries(gt4vol PRIVATE ${MOON_CORE_STACK})
add_executable(gt4driver_survey tools/c/gt4driver_survey.c)
target_link_libraries(gt4driver_survey PRIVATE ${MOON_CORE_STACK})
# The native program: SDL2 backend + core.
# AMD FidelityFX FSR 1 (MIT, third_party/fsr1) embedded for the GLSL shaders
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/fsr_source.c
  COMMAND ${CMAKE_COMMAND} -DOUT=${CMAKE_BINARY_DIR}/fsr_source.c -DDIR=${CMAKE_SOURCE_DIR}/third_party/fsr1
          -P ${CMAKE_SOURCE_DIR}/third_party/fsr1/embed.cmake
  DEPENDS ${CMAKE_SOURCE_DIR}/third_party/fsr1/ffx_a.h ${CMAKE_SOURCE_DIR}/third_party/fsr1/ffx_fsr1.h
          ${CMAKE_SOURCE_DIR}/third_party/fsr1/embed.cmake)
# The Moon Engine logo (project art, res/logo_*.png) embedded for the title corners
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/moon_logo.c
  COMMAND ${CMAKE_COMMAND} -DOUT=${CMAKE_BINARY_DIR}/moon_logo.c -DDIR=${CMAKE_SOURCE_DIR}/res
          -P ${CMAKE_SOURCE_DIR}/cmake/embed_logo.cmake
  DEPENDS ${CMAKE_SOURCE_DIR}/res/logo_black.png ${CMAKE_SOURCE_DIR}/res/logo_white.png
          ${CMAKE_SOURCE_DIR}/res/moonengine_icon.png
          ${CMAKE_SOURCE_DIR}/cmake/embed_logo.cmake)
if(NINTENDO_SWITCH OR ANDROID)
  set(MOON_FSR_SOURCES src/platform/sdl/fsr_disabled.c)
else()
  set(MOON_FSR_SOURCES src/platform/sdl/fsr.c ${CMAKE_BINARY_DIR}/fsr_source.c)
endif()
if(NINTENDO_SWITCH AND MOON_SWITCH_RENDERER STREQUAL "deko3d")
  set(MOON_GRAPHICS_SOURCES src/platform/switch/deko_device.cpp
    src/platform/switch/deko_video.cpp src/platform/switch/deko_render.cpp
    src/platform/switch/deko_profile_stubs.c)
else()
  set(MOON_GRAPHICS_SOURCES src/platform/sdl/video_gl.c src/platform/sdl/gl.c
    src/platform/sdl/render3d.c src/platform/sdl/switch_render.c ${MOON_FSR_SOURCES})
endif()
add_library(moon_platform_sdl STATIC src/platform/sdl/music_override.c src/platform/sdl/plat_sdl.c src/platform/sdl/moon_pref.c src/platform/sdl/pad_sdl.c ${MOON_GRAPHICS_SOURCES}
  src/platform/sdl/movie.c src/platform/sdl/showroom.c
  src/platform/sdl/screen_setting.c
  src/platform/sdl/car_model.c src/platform/sdl/race_view.c src/platform/sdl/race_lights.c src/platform/sdl/license_scene.c
  src/platform/sdl/plat_process.c src/platform/sdl/dev_overlay.c)
target_include_directories(moon_platform_sdl PUBLIC include src/platform/sdl src/core)
# Android's picker lives in android_host.c (it needs the Activity); the other
# backends keep their own native dialog here.
if(NOT ANDROID)
  target_sources(moon_platform_sdl PRIVATE src/platform/sdl/plat_pick_disc.c)
endif()
target_link_libraries(moon_platform_sdl PUBLIC SDL2::SDL2-static moon_physics)
if(WIN32)
  target_link_libraries(moon_platform_sdl PUBLIC opengl32)   # GL 1.1 entry points
elseif(NOT ANDROID)
  # Linux/macOS: fsr.c calls GL entry points directly (glBindTexture, glEnable...),
  # so the loader needs a GL library at link time even though the rest goes through
  # SDL_GL_GetProcAddress.
  find_package(OpenGL REQUIRED)
  target_link_libraries(moon_platform_sdl PUBLIC OpenGL::GL)
endif()
# The player executable is a window application; test harnesses keep stdout.
option(MOON_WINDOWS_GUI "Build MoonEngine.exe without a console window" ON)
find_program(MOON_FFMPEG NAMES ffmpeg DOC "FFmpeg executable for menu MPEG-2 playback (not a launcher shim)")
# a release looks for FFmpeg beside the exe and on PATH only, never at a
# path of the machine it was built on
if(MOON_FFMPEG AND NOT MOON_WINDOWS_GUI)
  target_compile_definitions(moon_platform_sdl PRIVATE MOON_FFMPEG="${MOON_FFMPEG}")
endif()
# ---- In-process movie decoding (G6.41) ----
# tools/build_libav.sh builds one pinned FFmpeg feature set for the host, Windows
# and Android arm64. When those archives are present they are linked straight into
# the binary: no side DLL, no download, no child process. Switch keeps its own
# pkg-config route in cmake/SwitchTargets.cmake and defines MOON_LINKED_MOVIES too.
set(MOON_LIBAV_TARGET "")
if(ANDROID)
  if(CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
    set(MOON_LIBAV_TARGET android-arm64)
  endif()
elseif(MSVC AND CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "^MultiThreaded(Debug)?$")
  set(MOON_LIBAV_TARGET windows-msvc-mt) # static CRT (the release build)
elseif(MSVC)
  set(MOON_LIBAV_TARGET windows-msvc)
elseif(WIN32)
  set(MOON_LIBAV_TARGET windows)
elseif(UNIX AND NOT APPLE)
  set(MOON_LIBAV_TARGET linux)
endif()
set(MOON_LIBAV_FOUND OFF)
if(MOON_LIBAV_TARGET)
  set(MOON_LIBAV_DIR "${CMAKE_SOURCE_DIR}/build/libav/${MOON_LIBAV_TARGET}")
  set(MOON_LIBAV_ARCHIVES
    "${MOON_LIBAV_DIR}/lib/libavfilter.a"
    "${MOON_LIBAV_DIR}/lib/libavformat.a"
    "${MOON_LIBAV_DIR}/lib/libavcodec.a"
    "${MOON_LIBAV_DIR}/lib/libswresample.a"
    "${MOON_LIBAV_DIR}/lib/libswscale.a"
    "${MOON_LIBAV_DIR}/lib/libavutil.a")
  set(MOON_LIBAV_FOUND ON)
  foreach(archive IN LISTS MOON_LIBAV_ARCHIVES)
    if(NOT EXISTS "${archive}")
      set(MOON_LIBAV_FOUND OFF)
    endif()
  endforeach()
endif()
option(MOON_LINKED_MOVIES "Link libav into the binary (tools/build_libav.sh)"
  ${MOON_LIBAV_FOUND})
if(MOON_LINKED_MOVIES)
  if(NOT MOON_LIBAV_FOUND)
    message(FATAL_ERROR
      "MOON_LINKED_MOVIES=ON but the ${MOON_LIBAV_TARGET} archives are missing; "
      "run tools/build_libav.sh first (or configure with -DMOON_LINKED_MOVIES=OFF).")
  endif()
  target_sources(moon_platform_sdl PRIVATE
    src/platform/switch/movie_ffmpeg.c src/platform/switch/movie_decode.c)
  # PUBLIC: the custom music (src/app/music_library.c) decodes through it too
  set(MOON_LIBAV_INCLUDE_DIRS "${MOON_LIBAV_DIR}/../ffmpeg-src" "${MOON_LIBAV_DIR}/build")
  target_include_directories(moon_platform_sdl PUBLIC ${MOON_LIBAV_INCLUDE_DIRS})
  target_compile_definitions(moon_platform_sdl PUBLIC MOON_LINKED_MOVIES=1)
  if(MSVC)
    target_link_libraries(moon_platform_sdl PUBLIC ${MOON_LIBAV_ARCHIVES})
  else()
    target_link_libraries(moon_platform_sdl PUBLIC
      -Wl,--start-group ${MOON_LIBAV_ARCHIVES} -Wl,--end-group)
  endif()
  if(ANDROID)
    target_link_libraries(moon_platform_sdl PUBLIC z)
  endif()
  if(WIN32)
    target_link_libraries(moon_platform_sdl PUBLIC bcrypt) # libavutil random_seed
  endif()
  message(STATUS "Movies: in-process libav from ${MOON_LIBAV_DIR}/lib")
else()
  if(NOT NINTENDO_SWITCH)
    message(STATUS "Movies: external ffmpeg (linked libav not found)")
  endif()
endif()
# The race's per-car sound (func_003B5E80/00392640) on the native race's state.
add_library(moon_race_sound STATIC src/core/race_car_sound.c src/core/sound_driver.c)
target_link_libraries(moon_race_sound PUBLIC moon_physics ${MOON_CORE_STACK})
if(MSVC)
  target_compile_options(moon_race_sound PRIVATE /fp:strict)
else()
  target_compile_options(moon_race_sound PRIVATE -ffp-contract=off -frounding-math)
endif()
if(NINTENDO_SWITCH)
  set(MOON_LAUNCHER_SOURCES src/app/switch_startup.c src/app/switch_fpu.c src/app/launcher_config.c)
else()
  set(MOON_LAUNCHER_SOURCES src/app/launcher.c src/app/launcher_ui.c src/app/launcher_text.c src/app/launcher_config.c src/app/launcher_photos.c src/app/launcher_icons.c src/app/launcher_gallery.c src/app/launcher_audio.c src/app/launcher_mixer.c src/app/prepare_ffmpeg.c src/app/moon_http.c)
  if(MOON_LINKED_MOVIES)
    list(REMOVE_ITEM MOON_LAUNCHER_SOURCES src/app/prepare_ffmpeg.c)
  endif()
endif()
if(ANDROID)
  add_library(moonengine SHARED)
else()
  add_executable(moonengine)
endif()
target_sources(moonengine PRIVATE src/app/music_library.c src/app/main.c ${MOON_LAUNCHER_SOURCES} src/app/photo_travel.c src/app/pregrid_audio.c src/app/race_audio.c src/app/loading_screen.c src/app/video_settings.c src/app/desktop_input.c src/app/options_store.c src/app/save_ui.c src/app/save_package.c src/app/race_game.c src/app/discord_presence.c
  src/app/brand_logo.c ${CMAKE_BINARY_DIR}/moon_logo.c src/app/app_update.c src/app/moon_http.c src/app/launcher_update.c)
set_target_properties(moonengine PROPERTIES OUTPUT_NAME MoonEngine)   # MoonEngine.exe
target_link_libraries(moonengine PRIVATE moon_race_sound moon_career_vehicle ${MOON_SDL_STACK} moon_physics miniz SDL2::SDL2main)
if(NOT WIN32)
  find_package(CURL REQUIRED)
  target_link_libraries(moonengine PRIVATE CURL::libcurl)
endif()
if(WIN32)
  target_link_libraries(moonengine PRIVATE winhttp bcrypt)
  # ONE FILE: link the GCC/C++ runtime statically so a fresh Windows machine does
  # not need libgcc_s_seh-1.dll / libstdc++-6.dll (nor libwinpthread) beside the
  # program. Windows system DLLs are always imported, never shipped.
  target_link_options(moonengine PRIVATE -static -static-libgcc -static-libstdc++)
endif()
if(WIN32)
  target_sources(moonengine PRIVATE src/app/moonengine.rc)
  target_link_libraries(moonengine PRIVATE comdlg32 user32 gdi32 shell32 ole32 msimg32)
endif()
if(WIN32 AND MOON_WINDOWS_GUI)
  set_target_properties(moonengine PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

if(NINTENDO_SWITCH)
  include(cmake/SwitchTargets.cmake)
  return()
endif()

# PC Vulkan foundation: headers at build time, system loader at runtime.
# The SDK and loader import library are not required. No Vulkan code enters Switch.
FetchContent_Declare(vulkan_headers
  GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
  GIT_TAG v1.3.296
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(vulkan_headers)
set(VOLK_PULL_IN_VULKAN OFF CACHE BOOL "" FORCE)
FetchContent_Declare(volk GIT_REPOSITORY https://github.com/zeux/volk.git
  GIT_TAG vulkan-sdk-1.3.296.0 GIT_SHALLOW TRUE)
set(ENABLE_OPT OFF CACHE BOOL "" FORCE)
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
set(ENABLE_SPVREMAPPER OFF CACHE BOOL "" FORCE)
set(GLSLANG_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glslang GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
  GIT_TAG 14.3.0 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(volk glslang)
# glslang 14.3.0 uses uint32_t without <cstdint>; GCC 15+ (Fedora 45) no longer gets it implicitly
if(NOT MSVC)
  foreach(t glslang SPIRV MachineIndependent GenericCodeGen OSDependent glslang-default-resource-limits)
    if(TARGET ${t})
      target_compile_options(${t} PRIVATE "SHELL:-include cstdint")
    endif()
  endforeach()
endif()
target_link_libraries(volk PUBLIC Vulkan::Headers)
add_library(moon_vk_render STATIC src/platform/sdl/vk_recipes.cpp src/platform/sdl/vk_draw_state.cpp src/platform/sdl/vk_cache.cpp src/platform/sdl/vk_frames.cpp src/platform/sdl/vk_context.cpp
  src/platform/sdl/vk_pipeline.cpp src/platform/sdl/vk_commands.cpp)
target_include_directories(moon_vk_render PUBLIC src/platform/sdl)
target_link_libraries(moon_vk_render PRIVATE volk glslang SPIRV glslang-default-resource-limits SDL2::SDL2-static)
target_link_libraries(moon_platform_sdl PUBLIC moon_vk_render)
target_compile_definitions(moon_platform_sdl PUBLIC MOON_PC_VULKAN=1)
if(ANDROID)
  include(cmake/AndroidTargets.cmake)
  return()
endif()
add_executable(mobile_options_tests tests/platform/mobile_options.c src/app/mobile_options.c src/app/options_store.c)
target_include_directories(mobile_options_tests PRIVATE include src/app src/platform/android)
add_test(NAME mobile_options COMMAND mobile_options_tests "${CMAKE_BINARY_DIR}/mobile-options-test.ini")
add_test(NAME mobile_options_negative COMMAND mobile_options_tests "${CMAKE_BINARY_DIR}/mobile-options-negative.ini" --negative)
set_tests_properties(mobile_options_negative PROPERTIES WILL_FAIL TRUE)
add_executable(mobile_controls_tests tests/platform/mobile_controls.c src/platform/android/mobile_controls.c)
target_include_directories(mobile_controls_tests PRIVATE include src/core src/platform/android)
target_link_libraries(mobile_controls_tests PRIVATE ${MOON_CORE_STACK})
add_test(NAME mobile_controls COMMAND mobile_controls_tests)
add_test(NAME mobile_controls_negative COMMAND mobile_controls_tests --negative)
set_tests_properties(mobile_controls_negative PROPERTIES WILL_FAIL TRUE)
if(NOT WIN32)
  add_executable(discord_presence_tests tests/ui/discord_presence.c src/app/discord_presence.c)
  target_include_directories(discord_presence_tests PRIVATE src/app)
  target_compile_definitions(discord_presence_tests PRIVATE MOON_DISCORD_PRESENCE_TEST=1)
  add_test(NAME discord_presence COMMAND discord_presence_tests)
  add_test(NAME discord_presence_negative COMMAND discord_presence_tests --negative)
endif()
add_executable(frame_pacing_tests tests/render/frame_pacing.c src/platform/common/frame_pacing.c)
target_include_directories(frame_pacing_tests PRIVATE src/platform/common)
add_test(NAME frame_pacing COMMAND frame_pacing_tests)
add_test(NAME frame_pacing_negative COMMAND frame_pacing_tests --negative)
set_tests_properties(frame_pacing_negative PROPERTIES WILL_FAIL TRUE)
add_executable(vulkan_surface_tests tests/render/vulkan_surface.c)
target_include_directories(vulkan_surface_tests PRIVATE src/platform/sdl)
target_link_libraries(vulkan_surface_tests PRIVATE Vulkan::Headers)
add_test(NAME vulkan_surface COMMAND vulkan_surface_tests)
add_test(NAME vulkan_surface_negative COMMAND vulkan_surface_tests --negative)
set_tests_properties(vulkan_surface_negative PROPERTIES WILL_FAIL TRUE)
add_executable(reflection_quality_tests tests/render/reflection_quality.c)
target_include_directories(reflection_quality_tests PRIVATE src/platform/sdl)
target_link_libraries(reflection_quality_tests PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
foreach(api opengl vulkan)
  add_test(NAME reflection_quality_${api} COMMAND reflection_quality_tests)
  add_test(NAME reflection_quality_${api}_negative COMMAND reflection_quality_tests --negative)
  set_tests_properties(reflection_quality_${api} reflection_quality_${api}_negative PROPERTIES RUN_SERIAL TRUE ENVIRONMENT "MOON_GRAPHICS_API=${api};SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1")
  set_tests_properties(reflection_quality_${api}_negative PROPERTIES WILL_FAIL TRUE)
endforeach()
add_executable(material_shading_tests tests/render/material_shading.c)
target_include_directories(material_shading_tests PRIVATE src/platform/sdl)
target_link_libraries(material_shading_tests PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
foreach(api opengl vulkan)
  add_test(NAME material_shading_${api} COMMAND material_shading_tests)
  add_test(NAME material_shading_${api}_negative COMMAND material_shading_tests --negative)
  set_tests_properties(material_shading_${api} material_shading_${api}_negative PROPERTIES RUN_SERIAL TRUE ENVIRONMENT "MOON_GRAPHICS_API=${api};SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1")
  set_tests_properties(material_shading_${api}_negative PROPERTIES WILL_FAIL TRUE)
endforeach()
add_executable(hdr_depth_tests tests/render/hdr_depth.c)
target_include_directories(hdr_depth_tests PRIVATE src/platform/sdl src/app)
target_link_libraries(hdr_depth_tests PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
foreach(api opengl vulkan)
  add_test(NAME hdr_depth_${api} COMMAND hdr_depth_tests)
  add_test(NAME hdr_depth_${api}_negative COMMAND hdr_depth_tests --negative)
  set_tests_properties(hdr_depth_${api} hdr_depth_${api}_negative PROPERTIES RUN_SERIAL TRUE ENVIRONMENT "MOON_GRAPHICS_API=${api};SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1")
  set_tests_properties(hdr_depth_${api}_negative PROPERTIES WILL_FAIL TRUE)
endforeach()
add_test(NAME hdr_depth_opengl_classic COMMAND hdr_depth_tests)
add_test(NAME hdr_depth_opengl_classic_negative COMMAND hdr_depth_tests --negative)
set_tests_properties(hdr_depth_opengl_classic hdr_depth_opengl_classic_negative PROPERTIES RUN_SERIAL TRUE ENVIRONMENT "MOON_GRAPHICS_API=opengl;MOON_CLASSIC_DEPTH=1;SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1")
set_tests_properties(hdr_depth_opengl_classic_negative PROPERTIES WILL_FAIL TRUE)
add_executable(daylight_tests tests/render/daylight.c)
target_include_directories(daylight_tests PRIVATE src/platform/sdl)
target_link_libraries(daylight_tests PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
foreach(api opengl vulkan)
  add_test(NAME daylight_${api} COMMAND daylight_tests)
  add_test(NAME daylight_${api}_negative COMMAND daylight_tests --negative)
  add_test(NAME daylight_${api}_rays_negative COMMAND daylight_tests --rays-negative)
  add_test(NAME daylight_${api}_sky_negative COMMAND daylight_tests --sky-negative)
  add_test(NAME daylight_${api}_aa_negative COMMAND daylight_tests --aa-negative)
  add_test(NAME daylight_${api}_rough_road_negative COMMAND daylight_tests --rough-road-negative)
  set_tests_properties(daylight_${api}_rough_road_negative PROPERTIES WILL_FAIL TRUE RUN_SERIAL TRUE ENVIRONMENT "MOON_GRAPHICS_API=${api};SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1")
  add_test(NAME daylight_${api}_sheen_negative COMMAND daylight_tests --sheen-negative)
  set_tests_properties(daylight_${api}_sheen_negative PROPERTIES WILL_FAIL TRUE RUN_SERIAL TRUE ENVIRONMENT "MOON_GRAPHICS_API=${api};SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1")
  add_test(NAME daylight_${api}_volume_negative COMMAND daylight_tests --volume-negative)
  add_test(NAME daylight_${api}_ao_negative COMMAND daylight_tests --ao-negative)
  set_tests_properties(daylight_${api}_volume_negative daylight_${api}_ao_negative PROPERTIES WILL_FAIL TRUE RUN_SERIAL TRUE ENVIRONMENT "MOON_GRAPHICS_API=${api};SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1")
  set_tests_properties(daylight_${api}_aa_negative PROPERTIES WILL_FAIL TRUE RUN_SERIAL TRUE ENVIRONMENT "MOON_GRAPHICS_API=${api};SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1")
  set_tests_properties(daylight_${api} daylight_${api}_negative daylight_${api}_rays_negative daylight_${api}_sky_negative PROPERTIES RUN_SERIAL TRUE ENVIRONMENT "MOON_GRAPHICS_API=${api};SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1")
  set_tests_properties(daylight_${api}_negative daylight_${api}_rays_negative daylight_${api}_sky_negative PROPERTIES WILL_FAIL TRUE)
endforeach()
add_executable(modern_lighting_tests tests/render/modern_lighting.c)
target_include_directories(modern_lighting_tests PRIVATE src/platform/sdl)
target_link_libraries(modern_lighting_tests PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
foreach(api opengl vulkan)
  add_test(NAME modern_lighting_${api} COMMAND modern_lighting_tests)
  add_test(NAME modern_lighting_${api}_coat_negative COMMAND modern_lighting_tests --coat-negative)
  set_tests_properties(modern_lighting_${api}_coat_negative PROPERTIES WILL_FAIL TRUE RUN_SERIAL TRUE ENVIRONMENT "MOON_GRAPHICS_API=${api};SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1")
  add_test(NAME modern_lighting_${api}_negative COMMAND modern_lighting_tests --negative)
  add_test(NAME modern_lighting_${api}_integration_negative COMMAND modern_lighting_tests --integration-negative)
  set_tests_properties(modern_lighting_${api}_integration_negative PROPERTIES WILL_FAIL TRUE RUN_SERIAL TRUE ENVIRONMENT "MOON_GRAPHICS_API=${api};SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1")
  add_test(NAME modern_lighting_${api}_texture_negative COMMAND modern_lighting_tests --texture-negative)
  set_tests_properties(modern_lighting_${api}_texture_negative PROPERTIES WILL_FAIL TRUE RUN_SERIAL TRUE ENVIRONMENT "MOON_GRAPHICS_API=${api};SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1")
  add_test(NAME modern_lighting_${api}_paint_negative COMMAND modern_lighting_tests --paint-negative)
  set_tests_properties(modern_lighting_${api}_paint_negative PROPERTIES WILL_FAIL TRUE RUN_SERIAL TRUE ENVIRONMENT "MOON_GRAPHICS_API=${api};SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1")
  add_test(NAME modern_lighting_${api}_reflection_negative COMMAND modern_lighting_tests --reflection-negative)
  set_tests_properties(modern_lighting_${api}_reflection_negative PROPERTIES WILL_FAIL TRUE RUN_SERIAL TRUE ENVIRONMENT "MOON_GRAPHICS_API=${api};SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1")
  add_test(NAME modern_lighting_${api}_hemisphere_negative COMMAND modern_lighting_tests --hemisphere-negative)
  set_tests_properties(modern_lighting_${api}_hemisphere_negative PROPERTIES WILL_FAIL TRUE RUN_SERIAL TRUE ENVIRONMENT "MOON_GRAPHICS_API=${api};SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1")
  set_tests_properties(modern_lighting_${api} modern_lighting_${api}_negative PROPERTIES RUN_SERIAL TRUE ENVIRONMENT "MOON_GRAPHICS_API=${api};SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1")
  set_tests_properties(modern_lighting_${api}_negative PROPERTIES WILL_FAIL TRUE)
endforeach()
add_executable(shadow_map_tests tests/render/shadow_maps.c)
target_include_directories(shadow_map_tests PRIVATE src/platform/sdl)
target_link_libraries(shadow_map_tests PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
foreach(api opengl vulkan)
  add_test(NAME shadow_maps_${api} COMMAND shadow_map_tests)
  add_test(NAME shadow_maps_${api}_negative COMMAND shadow_map_tests --negative)
  add_test(NAME shadow_maps_${api}_contact_negative COMMAND shadow_map_tests --contact-negative)
  set_tests_properties(shadow_maps_${api}_contact_negative PROPERTIES WILL_FAIL TRUE RUN_SERIAL TRUE ENVIRONMENT "MOON_GRAPHICS_API=${api};SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1")
  set_tests_properties(shadow_maps_${api} shadow_maps_${api}_negative PROPERTIES RUN_SERIAL TRUE ENVIRONMENT "MOON_GRAPHICS_API=${api};SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1")
  set_tests_properties(shadow_maps_${api}_negative PROPERTIES WILL_FAIL TRUE)
endforeach()
add_executable(car_shadow_tests tests/render/car_shadows.c)
target_include_directories(car_shadow_tests PRIVATE src/platform/sdl)
target_link_libraries(car_shadow_tests PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
foreach(api opengl vulkan)
  add_test(NAME car_shadows_${api} COMMAND car_shadow_tests)
  add_test(NAME car_shadows_${api}_ambient_negative COMMAND car_shadow_tests --ambient-negative)
  set_tests_properties(car_shadows_${api}_ambient_negative PROPERTIES WILL_FAIL TRUE RUN_SERIAL TRUE ENVIRONMENT "MOON_GRAPHICS_API=${api};SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1")
  add_test(NAME car_shadows_${api}_contact_negative COMMAND car_shadow_tests --contact-negative)
  set_tests_properties(car_shadows_${api}_contact_negative PROPERTIES WILL_FAIL TRUE RUN_SERIAL TRUE ENVIRONMENT "MOON_GRAPHICS_API=${api};SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1")
  add_test(NAME car_shadows_${api}_negative COMMAND car_shadow_tests --negative)
  set_tests_properties(car_shadows_${api} car_shadows_${api}_negative PROPERTIES RUN_SERIAL TRUE ENVIRONMENT "MOON_GRAPHICS_API=${api};SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1")
  set_tests_properties(car_shadows_${api}_negative PROPERTIES WILL_FAIL TRUE)
endforeach()
add_executable(texture_filtering_tests tests/render/texture_filtering.c)
target_include_directories(texture_filtering_tests PRIVATE src/platform/sdl)
target_link_libraries(texture_filtering_tests PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
foreach(api opengl vulkan)
  add_test(NAME texture_filtering_${api} COMMAND texture_filtering_tests)
  add_test(NAME texture_filtering_${api}_negative COMMAND texture_filtering_tests --negative)
  set_tests_properties(texture_filtering_${api} texture_filtering_${api}_negative PROPERTIES RUN_SERIAL TRUE ENVIRONMENT "MOON_GRAPHICS_API=${api};SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1")
  set_tests_properties(texture_filtering_${api}_negative PROPERTIES WILL_FAIL TRUE)
endforeach()
add_executable(vulkan_frame_tests tests/render/vulkan_frame.c)
add_executable(shared_batches_tests tests/render/shared_batches.c)
target_link_libraries(shared_batches_tests PRIVATE moon_platform_sdl moon_core moon_platform_common miniz SDL2::SDL2main)
foreach(backend opengl vulkan)
  add_test(NAME shared_batches_${backend} COMMAND shared_batches_tests ${backend})
  add_test(NAME shared_batches_${backend}_negative COMMAND shared_batches_tests ${backend} --negative)
  set_tests_properties(shared_batches_${backend} shared_batches_${backend}_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 60)
  set_tests_properties(shared_batches_${backend}_negative PROPERTIES WILL_FAIL TRUE)
endforeach()
add_executable(car_material_tests tests/render/car_materials.c)
target_link_libraries(car_material_tests PRIVATE moon_platform_sdl moon_core moon_platform_common miniz SDL2::SDL2main)
foreach(backend opengl vulkan)
  add_test(NAME car_material_${backend} COMMAND car_material_tests ${backend})
  add_test(NAME car_material_${backend}_negative COMMAND car_material_tests ${backend} --negative)
  set_tests_properties(car_material_${backend} car_material_${backend}_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 60)
  set_tests_properties(car_material_${backend}_negative PROPERTIES WILL_FAIL TRUE)
endforeach()
target_link_libraries(vulkan_frame_tests PRIVATE moon_platform_sdl moon_core moon_platform_common miniz SDL2::SDL2main)
add_test(NAME vulkan_frame COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/render/vulkan_compare.py" "$<TARGET_FILE:vulkan_frame_tests>")
add_test(NAME vulkan_frame_negative COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/render/vulkan_compare.py" "$<TARGET_FILE:vulkan_frame_tests>" --negative)
add_test(NAME vulkan_implicit_close COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/render/vulkan_compare.py" "$<TARGET_FILE:vulkan_frame_tests>")
set_tests_properties(vulkan_implicit_close PROPERTIES ENVIRONMENT "MOON_TEST_IMPLICIT_VK_CLOSE=1;SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1")
add_test(NAME vulkan_implicit_close_negative COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/render/vulkan_compare.py" "$<TARGET_FILE:vulkan_frame_tests>" --negative)
set_tests_properties(vulkan_implicit_close_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "MOON_TEST_IMPLICIT_VK_CLOSE=1;SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1")
set_tests_properties(vulkan_frame vulkan_frame_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 120)
set_tests_properties(vulkan_frame_negative PROPERTIES WILL_FAIL TRUE)
add_test(NAME vulkan_geometry_negative COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/render/vulkan_compare.py" "$<TARGET_FILE:vulkan_frame_tests>")
set_tests_properties(vulkan_geometry_negative PROPERTIES WILL_FAIL TRUE RUN_SERIAL TRUE TIMEOUT 120
  ENVIRONMENT "MOON_VK_STREAM_GEOMETRY=1")
add_test(NAME vulkan_variants COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/render/vulkan_variants.py" "$<TARGET_FILE:vulkan_frame_tests>" "${CMAKE_BINARY_DIR}/vulkan-variants")
add_test(NAME vulkan_variants_negative COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/render/vulkan_variants.py" "$<TARGET_FILE:vulkan_frame_tests>" "${CMAKE_BINARY_DIR}/vulkan-variants-negative" --negative)
set_tests_properties(vulkan_variants vulkan_variants_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 180)
set_tests_properties(vulkan_variants_negative PROPERTIES WILL_FAIL TRUE)
if(EXISTS "${MOON_TEST_DISC}")
  add_test(NAME vulkan_commands COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/render/vulkan_commands.py" "$<TARGET_FILE:race_inspect_probe>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/vulkan-commands")
  add_test(NAME vulkan_commands_negative COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/render/vulkan_commands.py" "$<TARGET_FILE:race_inspect_probe>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/vulkan-commands-negative" --negative)
  set_tests_properties(vulkan_commands vulkan_commands_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 900)
  set_tests_properties(vulkan_commands_negative PROPERTIES WILL_FAIL TRUE)
  foreach(backend opengl vulkan)
    add_test(NAME scene_pipeline_${backend} COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/render/scene_pipeline.py" "$<TARGET_FILE:race_inspect_probe>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/scene-pipeline-${backend}" ${backend})
    add_test(NAME scene_pipeline_${backend}_negative COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/render/scene_pipeline.py" "$<TARGET_FILE:race_inspect_probe>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/scene-pipeline-${backend}-negative" ${backend} --negative)
    set_tests_properties(scene_pipeline_${backend} scene_pipeline_${backend}_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 900)
    set_tests_properties(scene_pipeline_${backend}_negative PROPERTIES WILL_FAIL TRUE)
  endforeach()
  foreach(backend opengl vulkan)
    add_test(NAME shared_render_${backend} COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/render/shared_render.py" "$<TARGET_FILE:race_inspect_probe>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/shared-render-${backend}" ${backend})
    add_test(NAME shared_render_${backend}_negative COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/render/shared_render.py" "$<TARGET_FILE:race_inspect_probe>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/shared-render-${backend}-negative" ${backend} --negative)
    set_tests_properties(shared_render_${backend} shared_render_${backend}_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 900)
    set_tests_properties(shared_render_${backend}_negative PROPERTIES WILL_FAIL TRUE)
  endforeach()
endif()
add_test(NAME vulkan_cache COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/render/vulkan_cache.py" "$<TARGET_FILE:vulkan_frame_tests>" "${CMAKE_BINARY_DIR}/vulkan-cache")
add_test(NAME vulkan_cache_negative COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/render/vulkan_cache.py" "$<TARGET_FILE:vulkan_frame_tests>" "${CMAKE_BINARY_DIR}/vulkan-cache-negative" --negative)
set_tests_properties(vulkan_cache vulkan_cache_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 90)
set_tests_properties(vulkan_cache_negative PROPERTIES WILL_FAIL TRUE)
if(EXISTS "${MOON_TEST_DISC}")
  add_test(NAME vulkan_queue COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/render/vulkan_queue.py" "$<TARGET_FILE:race_inspect_probe>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/vulkan-queue")
  add_test(NAME vulkan_queue_negative COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/render/vulkan_queue.py" "$<TARGET_FILE:race_inspect_probe>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/vulkan-queue-negative" --negative)
  set_tests_properties(vulkan_queue vulkan_queue_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 900)
  set_tests_properties(vulkan_queue_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME video_performance COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/video_performance.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/video-performance")
  add_test(NAME video_performance_negative COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/video_performance.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/video-performance-negative" --negative)
  set_tests_properties(video_performance video_performance_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 120)
  set_tests_properties(video_performance_negative PROPERTIES WILL_FAIL TRUE)
endif()
add_test(NAME vulkan_descriptors_negative COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/render/vulkan_compare.py" "$<TARGET_FILE:vulkan_frame_tests>")
set_tests_properties(vulkan_descriptors_negative PROPERTIES WILL_FAIL TRUE RUN_SERIAL TRUE TIMEOUT 120
  ENVIRONMENT "MOON_VK_UNCACHED_DESCRIPTORS=1")
if(EXISTS "${MOON_TEST_DISC}")
  add_test(NAME vulkan_submission COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/render/vulkan_submission.py" "$<TARGET_FILE:race_inspect_probe>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/vulkan-submission")
  add_test(NAME vulkan_submission_negative COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/render/vulkan_submission.py" "$<TARGET_FILE:race_inspect_probe>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/vulkan-submission-negative" --negative)
  set_tests_properties(vulkan_submission vulkan_submission_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 180)
  set_tests_properties(vulkan_submission_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME vulkan_targets_negative COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/render/vulkan_submission.py" "$<TARGET_FILE:race_inspect_probe>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/vulkan-targets-negative" --targets-negative)
  set_tests_properties(vulkan_targets_negative PROPERTIES WILL_FAIL TRUE RUN_SERIAL TRUE TIMEOUT 180)
endif()
add_library(moon_vulkan STATIC src/platform/sdl/vulkan_device.c)
target_include_directories(moon_vulkan PUBLIC src/platform/sdl)
target_link_libraries(moon_vulkan PRIVATE Vulkan::Headers SDL2::SDL2-static)
target_sources(moonengine PRIVATE src/app/renderer_startup.c)
target_link_libraries(moonengine PRIVATE moon_vulkan)

add_executable(vulkan_device_tests tests/render/vulkan_device.c)
target_link_libraries(vulkan_device_tests PRIVATE moon_vulkan SDL2::SDL2-static SDL2::SDL2main)
add_test(NAME vulkan_device COMMAND vulkan_device_tests)
add_test(NAME vulkan_device_negative COMMAND vulkan_device_tests --negative)
add_test(NAME vulkan_missing_loader COMMAND vulkan_device_tests --missing-loader)
set_tests_properties(vulkan_device vulkan_device_negative PROPERTIES SKIP_RETURN_CODE 77)
set_tests_properties(vulkan_device_negative PROPERTIES WILL_FAIL TRUE)
add_executable(renderer_backend_tests tests/render/renderer_backend.c)
target_link_libraries(renderer_backend_tests PRIVATE moon_platform_common)
add_test(NAME renderer_backend COMMAND renderer_backend_tests)
add_test(NAME renderer_backend_negative COMMAND renderer_backend_tests --negative)
set_tests_properties(renderer_backend_negative PROPERTIES WILL_FAIL TRUE)
add_test(NAME renderer_startup COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_SOURCE_DIR}/tests/render/renderer_startup.py" "$<TARGET_FILE:moonengine>")
add_test(NAME renderer_startup_negative COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_SOURCE_DIR}/tests/render/renderer_startup.py" "$<TARGET_FILE:moonengine>" --negative)
set_tests_properties(renderer_startup_negative PROPERTIES WILL_FAIL TRUE)

# Disc-independent picker and runtime Options artwork/configuration contract.
add_executable(launcher_tests tests/ui/launcher.c src/app/launcher.c src/app/launcher_config.c src/app/launcher_photos.c src/app/launcher_icons.c src/app/launcher_ui.c src/app/launcher_text.c)
target_include_directories(launcher_tests PRIVATE src/app)
target_link_libraries(launcher_tests PRIVATE ${MOON_SDL_STACK} miniz SDL2::SDL2-static SDL2::SDL2main)
if(WIN32)
  target_link_libraries(launcher_tests PRIVATE comdlg32 user32 gdi32 shell32 ole32 msimg32 winhttp bcrypt)
endif()

add_executable(launcher_fsr_tests tests/ui/launcher_fsr.c src/app/launcher_fsr.c)
target_include_directories(launcher_fsr_tests PRIVATE src/app)
target_link_libraries(launcher_fsr_tests PRIVATE ${MOON_SDL_STACK} miniz SDL2::SDL2main)
if(WIN32)
  add_test(NAME launcher_fsr COMMAND launcher_fsr_tests)
  add_test(NAME launcher_fsr_negative COMMAND launcher_fsr_tests --negative)
  set_tests_properties(launcher_fsr_negative PROPERTIES WILL_FAIL TRUE)
endif()

add_executable(adhoc_array_move tests/ui/adhoc_array_move.c)
target_link_libraries(adhoc_array_move PRIVATE ${MOON_CORE_STACK})
add_test(NAME adhoc_array_move COMMAND adhoc_array_move)
add_test(NAME adhoc_array_move_negative COMMAND adhoc_array_move --negative)
set_tests_properties(adhoc_array_move_negative PROPERTIES WILL_FAIL TRUE)
add_executable(custom_music_tests tests/ui/custom_music.c src/app/music_library.c src/app/options_store.c src/platform/sdl/music_override.c)
target_include_directories(custom_music_tests PRIVATE src/app src/platform/sdl include)
target_link_libraries(custom_music_tests PRIVATE SDL2::SDL2 ${MOON_CORE_STACK})
if(MOON_LINKED_MOVIES)
  target_include_directories(custom_music_tests PRIVATE ${MOON_LIBAV_INCLUDE_DIRS})
  target_compile_definitions(custom_music_tests PRIVATE MOON_LINKED_MOVIES=1)
  target_link_libraries(custom_music_tests PRIVATE ${MOON_LIBAV_ARCHIVES} $<$<BOOL:${WIN32}>:bcrypt>)
endif()
add_test(NAME custom_music COMMAND custom_music_tests "${CMAKE_BINARY_DIR}/custom-music")
add_test(NAME custom_music_negative COMMAND custom_music_tests "${CMAKE_BINARY_DIR}/custom-music-negative" --negative)
set_tests_properties(custom_music_negative PROPERTIES WILL_FAIL TRUE)
add_executable(launcher_mixer_tests tests/ui/launcher_mixer.c src/app/launcher_mixer.c src/app/options_store.c)
target_include_directories(launcher_mixer_tests PRIVATE src/app include)
target_link_libraries(launcher_mixer_tests PRIVATE SDL2::SDL2)
add_test(NAME launcher_mixer COMMAND launcher_mixer_tests "${CMAKE_BINARY_DIR}/launcher-mixer")
add_test(NAME launcher_mixer_negative COMMAND launcher_mixer_tests "${CMAKE_BINARY_DIR}/launcher-mixer-negative" --negative)
set_tests_properties(launcher_mixer_negative PROPERTIES WILL_FAIL TRUE)
add_executable(launcher_audio_tests tests/ui/launcher_audio.c src/app/launcher_audio.c)
target_include_directories(launcher_audio_tests PRIVATE src/app)
target_link_libraries(launcher_audio_tests PRIVATE ${MOON_SDL_STACK} miniz SDL2::SDL2main)
if(MOON_TEST_DISC)
  add_test(NAME launcher_audio COMMAND launcher_audio_tests "${MOON_TEST_DISC}")
  set_tests_properties(launcher_audio PROPERTIES ENVIRONMENT "MOON_LAUNCHER_FIRST_TRACK=map")
  add_test(NAME launcher_audio_negative COMMAND launcher_audio_tests "${MOON_TEST_DISC}" --negative)
  set_tests_properties(launcher_audio_negative PROPERTIES WILL_FAIL TRUE)
endif()

add_executable(launcher_update_tests tests/ui/launcher_update.c src/app/launcher_update.c)
target_include_directories(launcher_update_tests PRIVATE src/app)
if(WIN32)
  target_link_libraries(launcher_update_tests PRIVATE winhttp bcrypt)
endif()
add_executable(app_update_tests tests/ui/app_update_parse.c src/app/app_update.c)
target_include_directories(app_update_tests PRIVATE src/app)
target_link_libraries(app_update_tests PRIVATE miniz)
target_compile_definitions(app_update_tests PRIVATE MOON_UPDATE_FORCE)
if(WIN32)
  target_link_libraries(app_update_tests PRIVATE winhttp bcrypt)
else()
  target_link_libraries(app_update_tests PRIVATE CURL::libcurl)
endif()
add_test(NAME app_update_parse COMMAND app_update_tests)
add_test(NAME app_update_parse_negative COMMAND app_update_tests --negative)
set_tests_properties(app_update_parse_negative PROPERTIES WILL_FAIL TRUE)
add_test(NAME launcher_update COMMAND launcher_update_tests)
add_test(NAME launcher_update_negative COMMAND launcher_update_tests --negative)
set_tests_properties(launcher_update_negative PROPERTIES WILL_FAIL TRUE)
add_test(NAME launcher_publish COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/launcher_publish.py")
option(MOON_UPDATE_LIVE_TEST "Run update HTTPS tests against external publish.ini" OFF)
if(MOON_UPDATE_LIVE_TEST AND EXISTS "${MOON_UPDATE_CONFIG}")
  add_test(NAME launcher_update_https COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_SOURCE_DIR}/tests/ui/launcher_update_https.py" "$<TARGET_FILE:launcher_update_tests>"
    "${MOON_UPDATE_CONFIG}" "${MOON_VERSION}")
  set_tests_properties(launcher_update_https PROPERTIES TIMEOUT 120)
endif()

add_executable(photo_mode_tests tests/ui/test_photo_mode.c)
target_link_libraries(photo_mode_tests PRIVATE ${MOON_SDL_STACK} miniz SDL2::SDL2main)
add_executable(photo_travel_probe tests/ui/photo_travel_probe.c src/app/photo_travel.c)
target_include_directories(photo_travel_probe PRIVATE src/app)
target_link_libraries(photo_travel_probe PRIVATE moon_race_sound ${MOON_SDL_STACK} miniz SDL2::SDL2main)
if(MOON_TEST_DISC)
  add_test(NAME photo_mode_data COMMAND photo_mode_tests "${MOON_TEST_DISC}")
  add_test(NAME photo_mode_data_negative COMMAND photo_mode_tests "${MOON_TEST_DISC}" --negative)
  set_tests_properties(photo_mode_data_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME photo_travel_flow COMMAND photo_travel_probe "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/photo-flow" "${CMAKE_BINARY_DIR}/photo-flow.png")
  add_test(NAME photo_travel_flow_negative COMMAND photo_travel_probe "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/photo-flow-negative" "${CMAKE_BINARY_DIR}/photo-flow-negative.png" --negative)
  set_tests_properties(photo_travel_flow_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME photo_tool_hold_negative COMMAND photo_travel_probe "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/photo-hold-negative" "${CMAKE_BINARY_DIR}/photo-hold-negative.png")
  set_tests_properties(photo_tool_hold_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_PHOTO_HOLD_NEGATIVE=1" RUN_SERIAL TRUE TIMEOUT 90)
  set_tests_properties(photo_travel_flow photo_travel_flow_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 90)
endif()

add_executable(gt4course tools/c/gt4course.c)
target_link_libraries(gt4course PRIVATE ${MOON_CORE_STACK})

add_executable(gt4spec tools/c/gt4spec.c)
target_link_libraries(gt4spec PRIVATE ${MOON_CORE_STACK})
add_executable(gt4model_survey tools/c/gt4model_survey.c)
target_link_libraries(gt4model_survey PRIVATE ${MOON_CORE_STACK} miniz)
add_executable(gt4car_survey tools/c/gt4car_survey.c src/platform/sdl/car_model.c)
target_include_directories(gt4car_survey PRIVATE src/platform/sdl)
target_link_libraries(gt4car_survey PRIVATE ${MOON_CORE_STACK} miniz)

# The mod SDK: a car/course gets its own rows, and a .glb becomes a CAR4 model.
add_executable(gt4table_dump tools/c/gt4table_dump.c)
target_link_libraries(gt4table_dump PRIVATE ${MOON_CORE_STACK})
add_executable(moonmod_addcar tools/c/moonmod_addcar.c)
target_link_libraries(moonmod_addcar PRIVATE ${MOON_CORE_STACK})
add_executable(mod_car_specs_ui tests/ui/mod_car_specs_ui.c)
target_link_libraries(mod_car_specs_ui PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
add_executable(mod_car_specs_screen tests/ui/mod_car_specs_ui.c)
target_compile_definitions(mod_car_specs_screen PRIVATE MOON_SPECS_SCREENSHOT)
target_include_directories(mod_car_specs_screen PRIVATE src/platform/sdl)
target_link_libraries(mod_car_specs_screen PRIVATE moon_career_vehicle ${MOON_SDL_STACK} miniz SDL2::SDL2main)
add_executable(mod_car_specs_probe tests/ui/mod_car_specs_probe.c)
target_link_libraries(mod_car_specs_probe PRIVATE moon_physics ${MOON_CORE_STACK})
add_executable(moonmod_addtrack tools/c/moonmod_addtrack.c)
target_link_libraries(moonmod_addtrack PRIVATE ${MOON_CORE_STACK})
add_executable(mod_car_functions tests/ui/mod_car_functions.c)
target_link_libraries(mod_car_functions PRIVATE ${MOON_CORE_STACK})
add_test(NAME mod_car_functions COMMAND mod_car_functions)
add_test(NAME mod_car_functions_negative COMMAND mod_car_functions --negative)
set_tests_properties(mod_car_functions_negative PROPERTIES WILL_FAIL TRUE)
add_test(NAME mod_car_function_export COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/mod_car_function_export.py" "$<TARGET_FILE:moonmod_glbcar>" "$<TARGET_FILE:mod_car_functions>")
add_test(NAME mod_car_function_export_negative COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/mod_car_function_export.py" "$<TARGET_FILE:moonmod_glbcar>" "$<TARGET_FILE:mod_car_functions>" --negative)
set_tests_properties(mod_car_function_export_negative PROPERTIES WILL_FAIL TRUE)
add_executable(gt4model_write tools/c/gt4model_write.c)
target_link_libraries(gt4model_write PRIVATE ${MOON_CORE_STACK})
add_executable(assist_line_check tools/c/assist_line_check.c)
target_include_directories(assist_line_check PRIVATE src/core/physics)
target_link_libraries(assist_line_check PRIVATE ${MOON_CORE_STACK} moon_physics)
add_executable(moonmod_glbcar tools/c/moonmod_glbcar.c)
target_link_libraries(moonmod_glbcar PRIVATE ${MOON_CORE_STACK})
if(MOON_TEST_DISC)
  add_test(NAME mod_car_rig_sound COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/mod_car_rig_sound.py" "$<TARGET_FILE_DIR:moonmod_glbcar>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/sdk-rig-sound-tests")
  add_test(NAME mod_car_rig_sound_negative COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/mod_car_rig_sound.py" "$<TARGET_FILE_DIR:moonmod_glbcar>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/sdk-rig-sound-negative" --negative)
  set_tests_properties(mod_car_rig_sound_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME mod_car_specs COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/mod_car_specs.py" "$<TARGET_FILE_DIR:moonmod_addcar>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/sdk-spec-tests")
  add_test(NAME mod_car_specs_negative COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/mod_car_specs.py" "$<TARGET_FILE_DIR:moonmod_addcar>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/sdk-spec-negative" --negative)
  set_tests_properties(mod_car_specs_negative PROPERTIES WILL_FAIL TRUE)
endif()

add_executable(moonmod_img tools/c/moonmod_img.c)
target_link_libraries(moonmod_img PRIVATE ${MOON_CORE_STACK})

# The disc's own nation -> maker catalogue, for the SDK's inspector.
add_executable(moonmod_catalog tools/c/moonmod_catalog.c)
target_link_libraries(moonmod_catalog PRIVATE ${MOON_CORE_STACK})

add_executable(gt4hdui tools/c/gt4hdui.c)
target_link_libraries(gt4hdui PRIVATE ${MOON_CORE_STACK} miniz)

add_executable(gt4adhoc tools/c/gt4adhoc.c)
target_link_libraries(gt4adhoc PRIVATE ${MOON_CORE_STACK})

add_executable(gt4img tools/c/gt4img.c)
target_link_libraries(gt4img PRIVATE ${MOON_CORE_STACK} miniz)

# EE runtime for recompiled code: memory, MMIO, HLE kernel, trace.
add_library(moon_ee STATIC src/ee/mem.c src/ee/syscall.c src/ee/sif.c src/ee/thread.c src/ee/fpu.c src/ee/mmi.c)
target_include_directories(moon_ee PUBLIC src/ee)

# Runtime unit tests (no game data needed).
add_executable(ee_unit_tests tests/ee_unit/test_mem.c)
target_link_libraries(ee_unit_tests PRIVATE moon_ee)
add_executable(ee_sif_tests tests/ee_unit/test_sif.c)
target_link_libraries(ee_sif_tests PRIVATE moon_ee)
enable_testing()
include(cmake/SwitchMovieTests.cmake)
add_test(NAME launcher_config COMMAND launcher_tests "${CMAKE_BINARY_DIR}/launcher-tests")
add_test(NAME launcher_config_negative COMMAND launcher_tests "${CMAKE_BINARY_DIR}/launcher-negative" --negative)
set_tests_properties(launcher_config_negative PROPERTIES WILL_FAIL TRUE)
if(WIN32)
  add_test(NAME launcher_icons_negative COMMAND launcher_tests "${CMAKE_BINARY_DIR}/launcher-icons-negative" --icons-negative)
  set_tests_properties(launcher_icons_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME launcher_without_disc COMMAND launcher_tests --launcher --launcher-config "${CMAKE_BINARY_DIR}/launcher-no-disc.ini" --launcher-shot "${CMAKE_BINARY_DIR}/launcher-no-disc.png")
  set_tests_properties(launcher_without_disc PROPERTIES RUN_SERIAL TRUE TIMEOUT 30)
endif()

add_executable(media_tests tests/ui/test_media.c)
target_link_libraries(media_tests PRIVATE ${MOON_CORE_STACK})
add_test(NAME native_media COMMAND media_tests)
add_executable(replay_file_tests tests/ui/test_replay_file.c)
target_link_libraries(replay_file_tests PRIVATE ${MOON_CORE_STACK})
add_test(NAME replay_file COMMAND replay_file_tests)
if(WIN32)
  add_executable(windows_file_test tests/ui/windows_file.c)
  target_include_directories(windows_file_test PRIVATE include)
  add_test(NAME windows_file_replace COMMAND windows_file_test)
  add_test(NAME windows_file_replace_negative COMMAND windows_file_test --no-retry)
  set_tests_properties(windows_file_replace_negative PROPERTIES WILL_FAIL TRUE)
endif()

add_executable(replay_hud_test tests/ui/replay_hud.c)
target_link_libraries(replay_hud_test PRIVATE ${MOON_SDL_STACK} miniz SDL2::SDL2main)
if(MOON_TEST_DISC)
  add_test(NAME race_hud_warmup COMMAND replay_hud_test "${MOON_TEST_DISC}" --warm)
  add_test(NAME race_hud_warmup_negative COMMAND replay_hud_test "${MOON_TEST_DISC}" --warm-negative)
  set_tests_properties(race_hud_warmup_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME replay_hud COMMAND replay_hud_test "${MOON_TEST_DISC}")
  add_test(NAME replay_hud_negative COMMAND replay_hud_test "${MOON_TEST_DISC}" --negative)
  set_tests_properties(replay_hud_negative PROPERTIES WILL_FAIL TRUE)
endif()
add_executable(ui_text_tests tests/ui/test_text.c)
target_link_libraries(ui_text_tests PRIVATE ${MOON_CORE_STACK})
add_test(NAME ui_text COMMAND ui_text_tests)
add_executable(sound_tests tests/ui/test_sound.c)
target_link_libraries(sound_tests PRIVATE ${MOON_CORE_STACK})
add_test(NAME native_sound COMMAND sound_tests)
add_test(NAME menu_music_prefix_negative COMMAND sound_tests)
set_tests_properties(menu_music_prefix_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_MUSIC_PREFIX_NEGATIVE=1")
if(MOON_TEST_DISC)
  add_test(NAME gtauto_service_sound COMMAND sound_tests --services "${MOON_TEST_DISC}")
  add_test(NAME gtauto_service_sound_negative COMMAND sound_tests --service-negative "${MOON_TEST_DISC}")
  set_tests_properties(gtauto_service_sound_negative PROPERTIES WILL_FAIL TRUE)
endif()
# B-Spec: the player's car given to its driver, its orders (tests/physics/bspec_run.c)
add_executable(bspec_tests tests/physics/bspec_run.c)
target_link_libraries(bspec_tests PRIVATE moon_physics ${MOON_CORE_STACK})
target_include_directories(bspec_tests PRIVATE src/core/physics)
target_compile_options(bspec_tests PRIVATE $<$<C_COMPILER_ID:MSVC>:/fp:strict>)
if(MOON_TEST_DISC)
  add_test(NAME bspec_driver COMMAND bspec_tests "${MOON_TEST_DISC}")
  add_test(NAME bspec_driver_negative COMMAND bspec_tests "${MOON_TEST_DISC}" --negative)
  add_test(NAME bspec_pit_menu COMMAND bspec_tests "${MOON_TEST_DISC}" --pit)
endif()
add_executable(quick_tune_tests tests/physics/quick_tune_run.c)
target_link_libraries(quick_tune_tests PRIVATE moon_physics ${MOON_CORE_STACK})
target_compile_options(quick_tune_tests PRIVATE $<$<C_COMPILER_ID:MSVC>:/fp:strict>)
if(MOON_TEST_DISC)
  add_test(NAME arcade_quick_tune COMMAND quick_tune_tests "${MOON_TEST_DISC}")
  add_test(NAME arcade_quick_tune_gearbox COMMAND quick_tune_tests "${MOON_TEST_DISC}" gearbox impreza_wrc_03)
  add_test(NAME arcade_quick_tune_negative COMMAND quick_tune_tests "${MOON_TEST_DISC}" --negative)
  set_tests_properties(arcade_quick_tune_negative PROPERTIES WILL_FAIL TRUE)
endif()
add_executable(machine_test_tests tests/physics/machine_test_run.c)
target_link_libraries(machine_test_tests PRIVATE moon_physics ${MOON_CORE_STACK})
target_compile_options(machine_test_tests PRIVATE $<$<C_COMPILER_ID:MSVC>:/fp:strict>)
add_executable(practice_session_tests tests/physics/practice_session_run.c)
target_link_libraries(practice_session_tests PRIVATE moon_physics ${MOON_CORE_STACK})
target_compile_options(practice_session_tests PRIVATE $<$<C_COMPILER_ID:MSVC>:/fp:strict>)
if(MOON_TEST_DISC)
  add_test(NAME gtmode_machine_test COMMAND machine_test_tests "${MOON_TEST_DISC}")
  add_test(NAME gtmode_machine_test_negative COMMAND machine_test_tests "${MOON_TEST_DISC}" --negative)
  set_tests_properties(gtmode_machine_test_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME gtmode_practice_session COMMAND practice_session_tests "${MOON_TEST_DISC}")
  add_test(NAME gtmode_practice_session_negative COMMAND practice_session_tests "${MOON_TEST_DISC}" --negative)
  set_tests_properties(gtmode_practice_session_negative PROPERTIES WILL_FAIL TRUE)
endif()
add_executable(model_tests tests/ui/test_model.c)
target_link_libraries(model_tests PRIVATE ${MOON_CORE_STACK})
add_test(NAME native_model COMMAND model_tests)
add_executable(model_stream_tests tests/ui/test_model_stream.c)
target_link_libraries(model_stream_tests PRIVATE ${MOON_CORE_STACK})
add_test(NAME model_stream COMMAND model_stream_tests)
add_executable(driver_model_tests tests/ui/test_driver_model.c)
target_link_libraries(driver_model_tests PRIVATE ${MOON_CORE_STACK})
add_test(NAME driver_model COMMAND driver_model_tests)
add_executable(course_tests tests/ui/test_course.c)
target_link_libraries(course_tests PRIVATE ${MOON_CORE_STACK})
add_test(NAME native_course COMMAND course_tests)
add_executable(showroom_probe tests/ui/showroom_probe.c)
target_link_libraries(showroom_probe PRIVATE moon_career_vehicle ${MOON_SDL_STACK} miniz SDL2::SDL2main)
if(MOON_TEST_DISC)
  foreach(backend opengl vulkan)
    add_test(NAME gtauto_part_fade_${backend} COMMAND showroom_probe "${MOON_TEST_DISC}" s2000_a_99 "${CMAKE_BINARY_DIR}/gtauto-parts-${backend}.png")
    set_tests_properties(gtauto_part_fade_${backend} PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;MOON_GRAPHICS_API=${backend};MOON_PROBE_PART_FADE=1")
  endforeach()
  add_test(NAME gtauto_part_fade_negative COMMAND showroom_probe "${MOON_TEST_DISC}" s2000_a_99 "${CMAKE_BINARY_DIR}/gtauto-parts-negative.png")
  set_tests_properties(gtauto_part_fade_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;MOON_PROBE_PART_FADE=1;MOON_PROBE_PART_FADE_NEGATIVE=1")
  foreach(service oil wash)
    add_test(NAME gtauto_${service}_crew_fade COMMAND showroom_probe "${MOON_TEST_DISC}" s2000_a_99 "${CMAKE_BINARY_DIR}/gtauto-${service}-fade.png")
    set_tests_properties(gtauto_${service}_crew_fade PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;MOON_PROBE_SERVICE=${service};MOON_PROBE_SERVICE_FADE=1")
  endforeach()
  add_test(NAME gtauto_crew_fade_negative COMMAND showroom_probe "${MOON_TEST_DISC}" s2000_a_99 "${CMAKE_BINARY_DIR}/gtauto-fade-negative.png")
  set_tests_properties(gtauto_crew_fade_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;MOON_PROBE_SERVICE=oil;MOON_PROBE_SERVICE_FADE=1;MOON_PROBE_SERVICE_FADE_NEGATIVE=1")
endif()
add_executable(driver_skin_probe tests/ui/driver_skin_probe.c)
target_link_libraries(driver_skin_probe PRIVATE ${MOON_SDL_STACK} miniz SDL2::SDL2main)
add_executable(race_inspect_probe tests/ui/race_inspect_probe.c src/app/race_audio.c src/app/video_settings.c src/app/options_store.c src/app/desktop_input.c src/app/discord_presence.c)
target_include_directories(race_inspect_probe PRIVATE src/app)
target_link_libraries(race_inspect_probe PRIVATE moon_race_sound moon_physics ${MOON_SDL_STACK} miniz SDL2::SDL2main)
# the Time Trial's ghost through the game's race (tests/ui/tt_game_probe.c)
# the race display's timing lines through the game's race (tests/ui/hud_lines_probe.c)
add_executable(hud_lines_probe tests/ui/hud_lines_probe.c src/app/race_audio.c src/app/video_settings.c src/app/options_store.c src/app/desktop_input.c src/app/discord_presence.c)
target_include_directories(hud_lines_probe PRIVATE src/app)
target_link_libraries(hud_lines_probe PRIVATE moon_race_sound moon_physics ${MOON_SDL_STACK} miniz SDL2::SDL2main)
# the tyres' smoke through the game's race (tests/ui/smoke_probe.c)
add_executable(smoke_probe tests/ui/smoke_probe.c src/app/race_audio.c src/app/video_settings.c src/app/options_store.c src/app/desktop_input.c src/app/discord_presence.c)
target_include_directories(smoke_probe PRIVATE src/app)
target_link_libraries(smoke_probe PRIVATE moon_race_sound moon_physics ${MOON_SDL_STACK} miniz SDL2::SDL2main)
add_executable(tt_game_probe tests/ui/tt_game_probe.c src/app/race_audio.c src/app/video_settings.c src/app/options_store.c src/app/desktop_input.c src/app/discord_presence.c)
target_include_directories(tt_game_probe PRIVATE src/app)
target_link_libraries(tt_game_probe PRIVATE moon_race_sound moon_physics ${MOON_SDL_STACK} miniz SDL2::SDL2main)
add_executable(pregrid_probe tests/ui/pregrid_probe.c src/app/race_audio.c src/app/video_settings.c src/app/options_store.c src/app/desktop_input.c src/app/discord_presence.c)
target_include_directories(pregrid_probe PRIVATE src/app)
target_link_libraries(pregrid_probe PRIVATE moon_race_sound moon_physics ${MOON_SDL_STACK} miniz SDL2::SDL2main)
add_executable(wheel_accum_probe tests/ui/wheel_accum_probe.c)
add_executable(license_objects_probe tests/ui/license_objects_probe.c src/app/race_audio.c src/app/video_settings.c src/app/options_store.c src/app/desktop_input.c src/app/discord_presence.c)
target_include_directories(license_objects_probe PRIVATE src/app)
target_link_libraries(license_objects_probe PRIVATE moon_race_sound moon_physics ${MOON_SDL_STACK} miniz SDL2::SDL2main)
if(MOON_TEST_DISC)
  add_test(NAME license_objects_opening COMMAND license_objects_probe "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/license-objects")
  add_test(NAME license_objects_opening_negative COMMAND license_objects_probe "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/license-objects-negative")
  set_tests_properties(license_objects_opening_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_START_CAR_NEGATIVE=1")
  add_test(NAME license_objects_clock_negative COMMAND license_objects_probe "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/license-objects-clock-negative")
  set_tests_properties(license_objects_clock_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_START_CLOCK_NEGATIVE=1")
endif()
add_executable(license_menu_probe tests/ui/license_menu_probe.c src/app/race_audio.c src/app/video_settings.c src/app/options_store.c src/app/desktop_input.c src/app/discord_presence.c)
target_include_directories(license_menu_probe PRIVATE src/app)
target_link_libraries(license_menu_probe PRIVATE moon_race_sound moon_physics ${MOON_SDL_STACK} miniz SDL2::SDL2main)
add_executable(disc_demo_probe tests/ui/disc_demo_probe.c src/app/race_audio.c src/app/video_settings.c src/app/options_store.c src/app/desktop_input.c src/app/discord_presence.c)
target_include_directories(disc_demo_probe PRIVATE src/app)
target_link_libraries(disc_demo_probe PRIVATE moon_race_sound moon_physics ${MOON_SDL_STACK} SDL2::SDL2main)
add_executable(disc_replay_menu tests/ui/disc_replay_menu.c)
target_link_libraries(disc_replay_menu PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
add_executable(pause_confirm tests/ui/pause_confirm.c)
target_link_libraries(pause_confirm PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
add_executable(course_ranking tests/ui/course_ranking.c)
target_link_libraries(course_ranking PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
if(EXISTS "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  find_program(MOON_PYTHON NAMES python python3 REQUIRED)
  set(gen_outside "${CMAKE_BINARY_DIR}/gen/outside_original.c")
  add_custom_command(OUTPUT ${gen_outside}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --root 0x001C5F80 --extra-roots 0x001C60B0,0x001C6160,0x00434770,0x001C6BE8
      --exclude 0x001C6918,0x001C6AA0,0x0030BB18,0x002FE278,0x003285A8,0x003285F8,0x002FC870,0x00329CB0,0x00309378 --hooks --out ${gen_outside}
    DEPENDS tools/recomp/ee_recomp.py extracted/core.bin splat/asm/core/text.s)
  add_executable(outside_menu tests/ui/outside_menu.c ${gen_outside})
  target_link_libraries(outside_menu PRIVATE moon_ee ${MOON_SDL_STACK} miniz SDL2::SDL2main)
  if(MOON_TEST_DISC)
    add_test(NAME outside_menu COMMAND outside_menu "${MOON_TEST_DISC}" "${CMAKE_SOURCE_DIR}/extracted/core.bin" "${CMAKE_BINARY_DIR}/outside-fixture")
    foreach(part state geometry shuffle cycle demo)
      add_test(NAME outside_${part}_negative COMMAND outside_menu "${MOON_TEST_DISC}" "${CMAKE_SOURCE_DIR}/extracted/core.bin" "${CMAKE_BINARY_DIR}/outside-${part}-fixture" --${part}-negative)
      set_tests_properties(outside_${part}_negative PROPERTIES WILL_FAIL TRUE)
    endforeach()
  endif()
endif()
add_executable(controller_menu tests/ui/controller_menu.c)
add_executable(favorites_menu tests/ui/favorites_menu.c)
target_link_libraries(favorites_menu PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
if(MOON_TEST_DISC)
  add_test(NAME favorites_menu COMMAND favorites_menu "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/favorites-menu-save")
  add_test(NAME favorites_menu_negative COMMAND favorites_menu "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/favorites-menu-negative-save" --negative)
  set_tests_properties(favorites_menu_negative PROPERTIES WILL_FAIL TRUE)
endif()
target_link_libraries(controller_menu PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
add_executable(controller_input tests/ui/controller_input.c src/app/race_audio.c src/app/video_settings.c src/app/options_store.c src/app/desktop_input.c src/app/discord_presence.c)
add_executable(pause_buttons tests/ui/pause_buttons.c src/app/race_audio.c src/app/video_settings.c src/app/options_store.c src/app/desktop_input.c src/app/discord_presence.c)
add_executable(hud_kerning tests/ui/hud_kerning.c)
add_executable(hud_suggested_gear tests/ui/hud_suggested_gear.c)
add_executable(suggested_gear_hud tests/physics/suggested_gear_hud.c)
target_include_directories(suggested_gear_hud PRIVATE src/core/physics)
target_link_libraries(suggested_gear_hud PRIVATE moon_physics ${MOON_CORE_STACK})
target_link_libraries(hud_suggested_gear PRIVATE moon_core moon_platform_sdl moon_platform_common SDL2::SDL2main)
add_executable(launcher_quality tests/ui/launcher_quality.c src/app/video_settings.c src/app/options_store.c src/app/desktop_input.c src/app/discord_presence.c)
target_include_directories(launcher_quality PRIVATE src/app)
target_link_libraries(launcher_quality PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
add_test(NAME launcher_quality COMMAND launcher_quality "${CMAKE_BINARY_DIR}/launcher-quality.ini")
add_test(NAME launcher_quality_negative COMMAND launcher_quality "${CMAKE_BINARY_DIR}/launcher-quality-negative.ini" --negative)
set_tests_properties(launcher_quality_negative PROPERTIES WILL_FAIL TRUE)
target_link_libraries(hud_kerning PRIVATE moon_core moon_platform_sdl moon_platform_common SDL2::SDL2main)
target_include_directories(pause_buttons PRIVATE src/app)
target_link_libraries(pause_buttons PRIVATE moon_replay_state moon_race_sound moon_physics moon_core moon_platform_sdl moon_platform_common SDL2::SDL2main)
if(MOON_TEST_DISC)
  add_test(NAME pause_buttons COMMAND pause_buttons "${MOON_TEST_DISC}")
  add_test(NAME hud_kerning COMMAND hud_kerning "${MOON_TEST_DISC}")
  add_test(NAME hud_suggested_gear COMMAND hud_suggested_gear "${MOON_TEST_DISC}")
  add_test(NAME suggested_gear_hud COMMAND suggested_gear_hud "${MOON_TEST_DISC}")
  add_test(NAME suggested_gear_hud_negative COMMAND suggested_gear_hud "${MOON_TEST_DISC}" --negative)
  set_tests_properties(suggested_gear_hud_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME hud_suggested_gear_negative COMMAND hud_suggested_gear "${MOON_TEST_DISC}" --negative)
  set_tests_properties(hud_suggested_gear_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME hud_kerning_negative COMMAND hud_kerning "${MOON_TEST_DISC}" --negative)
  set_tests_properties(hud_kerning_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME pause_buttons_negative COMMAND pause_buttons "${MOON_TEST_DISC}" --negative)
  set_tests_properties(pause_buttons_negative PROPERTIES WILL_FAIL TRUE)
endif()
target_include_directories(controller_input PRIVATE src/app)
target_link_libraries(controller_input PRIVATE moon_replay_state moon_race_sound moon_physics ${MOON_SDL_STACK} SDL2::SDL2main)
add_test(NAME controller_input COMMAND controller_input)
add_test(NAME controller_input_negative COMMAND controller_input --negative)
set_tests_properties(controller_input_negative PROPERTIES WILL_FAIL TRUE)
add_test(NAME controller_reverse_tap_negative COMMAND controller_input)
set_tests_properties(controller_reverse_tap_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_REVERSE_TAP_NEGATIVE=1")
add_test(NAME controller_start_key_negative COMMAND controller_input)
set_tests_properties(controller_start_key_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_START_KEY_NEGATIVE=1")
add_test(NAME controller_right_stick_negative COMMAND controller_input)
set_tests_properties(controller_right_stick_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_RIGHT_STICK_NEGATIVE=1")
if(MOON_TEST_DISC)
  add_test(NAME controller_menu COMMAND controller_menu "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/controller-options-test.ini")
  add_test(NAME controller_menu_negative COMMAND controller_menu "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/controller-options-negative.ini" --negative)
  set_tests_properties(controller_menu_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME controller_closed_option_negative COMMAND controller_menu "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/controller-closed-negative.ini")
  set_tests_properties(controller_closed_option_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_CLOSED_OPTION_NEGATIVE=1")
endif()
add_library(moon_replay_state STATIC src/app/disc_checkpoint.c)
target_include_directories(moon_replay_state PRIVATE src/app)
target_link_libraries(moon_replay_state PUBLIC moon_physics)
foreach(replay_app moonengine pregrid_probe license_objects_probe license_menu_probe disc_demo_probe tt_game_probe hud_lines_probe smoke_probe race_inspect_probe)
  target_link_libraries(${replay_app} PRIVATE moon_replay_state)
endforeach()
if(MOON_TEST_DISC)
  add_test(NAME race_cinematic COMMAND race_inspect_probe "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/race-cinematic")
  set_tests_properties(race_cinematic PROPERTIES ENVIRONMENT "PROBE_CINEMA_TEST=1")
  add_test(NAME race_cinematic_negative COMMAND race_inspect_probe "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/race-cinematic-negative")
  set_tests_properties(race_cinematic_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "PROBE_CINEMA_TEST=1;PROBE_CINEMA_NEGATIVE=1")
  add_test(NAME license_menu_demo COMMAND license_menu_probe "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/license-menu-demo.png")
  add_test(NAME license_pause_retry_negative COMMAND license_menu_probe "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/license-pause-retry-negative.png")
  set_tests_properties(license_pause_retry_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_PAUSE_RETRY_NEGATIVE=1")
  add_test(NAME license_menu_demo_negative COMMAND license_menu_probe "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/license-menu-demo-negative.png")
  set_tests_properties(license_menu_demo_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_MENU_DEMO_NEGATIVE=1")
  add_test(NAME license_tv_target_negative COMMAND license_menu_probe "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/license-tv-target-negative.png")
  set_tests_properties(license_tv_target_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_TV_TARGET_NEGATIVE=1")
  add_test(NAME license_menu_sync_negative COMMAND license_menu_probe "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/license-menu-sync-negative.png")
  set_tests_properties(license_menu_sync_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_MENU_SYNC_NEGATIVE=1")
  add_test(NAME disc_demo_menu COMMAND disc_replay_menu "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/disc-demo/menu-save")
  add_test(NAME pause_confirm COMMAND pause_confirm "${MOON_TEST_DISC}")
  add_test(NAME course_ranking COMMAND course_ranking "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/course-ranking-save")
  add_test(NAME course_ranking_passcode_negative COMMAND course_ranking "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/course-ranking-code-negative" --negative)
  set_tests_properties(course_ranking_passcode_negative PROPERTIES WILL_FAIL TRUE RUN_SERIAL TRUE)
  add_test(NAME disc_demo_menu_negative COMMAND disc_replay_menu "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/disc-demo/menu-negative" --negative)
  set_tests_properties(disc_demo_menu_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME license_teacher_gold COMMAND disc_demo_probe "${MOON_TEST_DISC}")
  add_test(NAME license_teacher_gold_negative COMMAND disc_demo_probe "${MOON_TEST_DISC}")
  set_tests_properties(license_teacher_gold PROPERTIES ENVIRONMENT "GT4_DEMO_TEST_PLAYER_GRADE=1")
  set_tests_properties(license_teacher_gold_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_DEMO_TEST_PLAYER_GRADE=1;GT4_DEMO_TEST_GRADE_NEGATIVE=1")
  add_test(NAME disc_demo_playback COMMAND disc_demo_probe "${MOON_TEST_DISC}")
  add_test(NAME disc_theater_playback COMMAND disc_demo_probe "${MOON_TEST_DISC}" --theater)
  add_test(NAME disc_bspec_playback COMMAND disc_demo_probe "${MOON_TEST_DISC}" --disc-bspec)
  add_test(NAME disc_bspec_playback_negative COMMAND disc_demo_probe "${MOON_TEST_DISC}" --disc-bspec-negative)
  set_tests_properties(disc_bspec_playback_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME disc_demo_playback_negative COMMAND disc_demo_probe "${MOON_TEST_DISC}" --negative)
  set_tests_properties(disc_demo_playback_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME disc_demo_telemetry_negative COMMAND disc_demo_probe "${MOON_TEST_DISC}" --telemetry-negative)
  set_tests_properties(disc_demo_telemetry_negative PROPERTIES WILL_FAIL TRUE)
endif()
target_include_directories(wheel_accum_probe PRIVATE src/platform/sdl)
target_link_libraries(wheel_accum_probe PRIVATE ${MOON_SDL_STACK} miniz SDL2::SDL2main)
foreach(api opengl vulkan)
  add_test(NAME wheel_accum_gpu_${api} COMMAND wheel_accum_probe)
  add_test(NAME wheel_accum_gpu_${api}_negative COMMAND wheel_accum_probe --negative)
  set_tests_properties(wheel_accum_gpu_${api} wheel_accum_gpu_${api}_negative PROPERTIES RUN_SERIAL TRUE ENVIRONMENT "MOON_GRAPHICS_API=${api};SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1")
  set_tests_properties(wheel_accum_gpu_${api}_negative PROPERTIES WILL_FAIL TRUE)
endforeach()
if(MOON_TEST_DISC)
  foreach(api opengl vulkan)
    foreach(car ka_01 legacy_b4_20gt_03 esprit_v8_02)
      file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/wheel-visibility-${api}-${car}")
      add_test(NAME wheel_visibility_${api}_${car} COMMAND race_inspect_probe "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/wheel-visibility-${api}-${car}/positive" --wheel-test)
      add_test(NAME wheel_visibility_${api}_${car}_negative COMMAND race_inspect_probe "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/wheel-visibility-${api}-${car}/negative" --wheel-negative)
      set_tests_properties(wheel_visibility_${api}_${car} wheel_visibility_${api}_${car}_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 180 ENVIRONMENT "MOON_GRAPHICS_API=${api};SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;MOON_OVERLAY=${CMAKE_BINARY_DIR}/wheel-visibility-empty;PROBE_CAR=${car};PROBE_COURSE=tsukuba;PROBE_GRID=1;PROBE_WARMUP=0;PROBE_FRAMES=60;PROBE_SOLAR_HOUR=12;PROBE_MODERN_LIGHTING=1;PROBE_MATERIAL_SHADING=1")
      set_tests_properties(wheel_visibility_${api}_${car}_negative PROPERTIES WILL_FAIL TRUE)
    endforeach()
  endforeach()
endif()

add_executable(brake_material_test tests/ui/brake_material.c)
target_link_libraries(brake_material_test PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
add_test(NAME brake_material COMMAND brake_material_test)
add_test(NAME brake_material_negative COMMAND brake_material_test --negative)
set_tests_properties(brake_material brake_material_negative PROPERTIES RUN_SERIAL TRUE)
set_tests_properties(brake_material_negative PROPERTIES WILL_FAIL TRUE)
if(MOON_TEST_DISC)
  add_test(NAME brake_material_focus COMMAND brake_material_test "${MOON_TEST_DISC}")
  set_tests_properties(brake_material_focus PROPERTIES RUN_SERIAL TRUE)
endif()
add_executable(model_mask_probe tests/ui/model_mask_probe.c)
target_link_libraries(model_mask_probe PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
add_executable(license_image_probe tests/ui/license_image_probe.c)
add_executable(screen_setting_test tests/ui/screen_setting.c src/app/race_audio.c src/app/video_settings.c src/app/options_store.c src/app/desktop_input.c src/app/discord_presence.c)
target_include_directories(screen_setting_test PRIVATE src/app)
target_link_libraries(screen_setting_test PRIVATE moon_replay_state moon_race_sound moon_physics ${MOON_SDL_STACK} SDL2::SDL2main)
add_test(NAME screen_setting COMMAND screen_setting_test "${CMAKE_BINARY_DIR}/screen-setting-save")
add_test(NAME screen_setting_negative COMMAND screen_setting_test "${CMAKE_BINARY_DIR}/screen-setting-negative-save" --negative)
set_tests_properties(screen_setting screen_setting_negative PROPERTIES RUN_SERIAL TRUE)
set_tests_properties(screen_setting_negative PROPERTIES WILL_FAIL TRUE)
target_link_libraries(license_image_probe PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
# Optional GL probe; run explicitly on a machine with a video driver.
add_executable(ui_blend_probe tests/ui/blend_probe.c)
target_link_libraries(ui_blend_probe PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
add_test(NAME model_viewport COMMAND ui_blend_probe)
add_test(NAME model_viewport_negative COMMAND ui_blend_probe)
set_tests_properties(model_viewport_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_MODEL_VIEWPORT_NEGATIVE=1")
add_test(NAME vulkan_blend COMMAND ui_blend_probe)
add_test(NAME vulkan_blend_negative COMMAND ui_blend_probe)
set_tests_properties(vulkan_blend PROPERTIES ENVIRONMENT "MOON_GRAPHICS_API=vulkan;MOON_WINDOW_HIDDEN=1")
set_tests_properties(vulkan_blend_negative PROPERTIES WILL_FAIL TRUE
  ENVIRONMENT "MOON_GRAPHICS_API=vulkan;MOON_WINDOW_HIDDEN=1;GT4_MODEL_VIEWPORT_NEGATIVE=1")
add_executable(showroom_math_tests tests/ui/test_showroom_math.c)
target_include_directories(showroom_math_tests PRIVATE src/platform/sdl)
add_test(NAME showroom_math COMMAND showroom_math_tests)

add_executable(deko_contract_test tests/platform/deko_contract.c)
add_test(NAME deko_contract COMMAND deko_contract_test)
add_test(NAME deko_contract_negative COMMAND deko_contract_test --negative)
set_tests_properties(deko_contract_negative PROPERTIES WILL_FAIL TRUE)
add_test(NAME deko_shader_control_negative COMMAND deko_contract_test --shader-negative)
set_tests_properties(deko_shader_control_negative PROPERTIES WILL_FAIL TRUE)
add_test(NAME deko_probe_geometry_negative COMMAND deko_contract_test --probe-negative)
set_tests_properties(deko_probe_geometry_negative PROPERTIES WILL_FAIL TRUE)
add_test(NAME deko_draw_range_negative COMMAND deko_contract_test --draw-negative)
set_tests_properties(deko_draw_range_negative PROPERTIES WILL_FAIL TRUE)
add_executable(deko_events_test tests/platform/deko_events.c)
target_link_libraries(deko_events_test PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
add_test(NAME deko_events COMMAND deko_events_test)
add_test(NAME deko_events_negative COMMAND deko_events_test --negative)
set_tests_properties(deko_events_negative PROPERTIES WILL_FAIL TRUE)
add_executable(deko_materials_test tests/platform/deko_materials.c)
target_link_libraries(deko_materials_test PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
add_test(NAME deko_materials COMMAND deko_materials_test ${CMAKE_SOURCE_DIR}/src/platform/switch/shaders/mesh.frag)
foreach(NEGATIVE negative date-negative address-negative vertex-negative vertex-alpha-negative extent-negative)
  add_test(NAME deko_materials_${NEGATIVE} COMMAND deko_materials_test ${CMAKE_SOURCE_DIR}/src/platform/switch/shaders/mesh.frag --${NEGATIVE})
  set_tests_properties(deko_materials_${NEGATIVE} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(deko_ui_clip_test tests/platform/deko_ui_clip.cpp)
add_test(NAME deko_ui_clip COMMAND deko_ui_clip_test)
add_test(NAME deko_ui_clip_negative COMMAND deko_ui_clip_test --negative)
set_tests_properties(deko_ui_clip_negative PROPERTIES WILL_FAIL TRUE)
add_test(NAME deko_viewport_negative COMMAND deko_ui_clip_test --viewport-negative)
set_tests_properties(deko_viewport_negative PROPERTIES WILL_FAIL TRUE)
add_executable(deko_composition_test tests/platform/deko_composition.c)
target_link_libraries(deko_composition_test PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
add_test(NAME deko_composition COMMAND deko_composition_test ${CMAKE_SOURCE_DIR}/src/platform/switch/shaders)
add_test(NAME deko_composition_negative COMMAND deko_composition_test ${CMAKE_SOURCE_DIR}/src/platform/switch/shaders --negative)
set_tests_properties(deko_composition_negative PROPERTIES WILL_FAIL TRUE)

# Optional SDK-header check: execute native transfer commands with delayed engines.
find_path(MOON_DEKO3D_HEADERS deko3d.h
  HINTS "$ENV{DEVKITPRO}/libnx/include" "C:/devkitPro/libnx/include")
if(MOON_DEKO3D_HEADERS)
  add_executable(deko_submission_test tests/platform/deko_submission.cpp)
  target_include_directories(deko_submission_test PRIVATE ${MOON_DEKO3D_HEADERS})
  add_test(NAME deko_submission COMMAND deko_submission_test)
  foreach(NEGATIVE negative duplicate-negative bytes-negative)
    add_test(NAME deko_submission_${NEGATIVE} COMMAND deko_submission_test --${NEGATIVE})
    set_tests_properties(deko_submission_${NEGATIVE} PROPERTIES WILL_FAIL TRUE)
  endforeach()
  add_executable(deko_index_test tests/platform/deko_index.cpp)
  add_test(NAME deko_index COMMAND deko_index_test)
  add_test(NAME deko_index_negative COMMAND deko_index_test --negative)
  set_tests_properties(deko_index_negative PROPERTIES WILL_FAIL TRUE)
  add_executable(deko_pipeline_cache_test tests/platform/deko_pipeline_cache.cpp)
  target_include_directories(deko_pipeline_cache_test PRIVATE ${MOON_DEKO3D_HEADERS})
  add_test(NAME deko_pipeline_cache COMMAND deko_pipeline_cache_test)
  foreach(NEGATIVE shader layout reset)
    add_test(NAME deko_pipeline_cache_${NEGATIVE}_negative COMMAND deko_pipeline_cache_test --${NEGATIVE}-negative)
    set_tests_properties(deko_pipeline_cache_${NEGATIVE}_negative PROPERTIES WILL_FAIL TRUE)
  endforeach()
  add_executable(deko_clear_test tests/platform/deko_clear.cpp)
  target_include_directories(deko_clear_test PRIVATE ${MOON_DEKO3D_HEADERS})
  add_test(NAME deko_clear COMMAND deko_clear_test)
  foreach(NEGATIVE depth mask alpha)
    add_test(NAME deko_clear_${NEGATIVE}_negative COMMAND deko_clear_test --${NEGATIVE}-negative)
    set_tests_properties(deko_clear_${NEGATIVE}_negative PROPERTIES WILL_FAIL TRUE)
  endforeach()
  add_executable(deko_blend_test tests/platform/deko_blend.c tests/platform/deko_blend_bridge.cpp)
  target_include_directories(deko_blend_test PRIVATE ${MOON_DEKO3D_HEADERS})
  target_link_libraries(deko_blend_test PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
  add_test(NAME deko_blend COMMAND deko_blend_test)
  add_test(NAME deko_blend_negative COMMAND deko_blend_test --negative)
  set_tests_properties(deko_blend_negative PROPERTIES WILL_FAIL TRUE)
  foreach(NEGATIVE separate destination)
    add_test(NAME deko_blend_sdk_${NEGATIVE}_negative COMMAND deko_blend_test --sdk-${NEGATIVE}-negative)
    set_tests_properties(deko_blend_sdk_${NEGATIVE}_negative PROPERTIES WILL_FAIL TRUE)
  endforeach()
  add_executable(deko_transfer_test tests/platform/deko_transfer.cpp)
  target_include_directories(deko_transfer_test PRIVATE ${MOON_DEKO3D_HEADERS})
  add_test(NAME deko_transfer COMMAND deko_transfer_test)
  foreach(NEGATIVE before after cache snapshot)
    add_test(NAME deko_transfer_${NEGATIVE}_negative COMMAND deko_transfer_test --${NEGATIVE}-negative)
    set_tests_properties(deko_transfer_${NEGATIVE}_negative PROPERTIES WILL_FAIL TRUE)
  endforeach()
endif()

# Host-clock fixture for Switch-only passive timing and nested/threaded accounting.
add_executable(switch_profile_test tests/platform/switch_profile.c)
target_include_directories(switch_profile_test PRIVATE include)
add_test(NAME switch_profile COMMAND switch_profile_test)
add_test(NAME switch_profile_negative COMMAND switch_profile_test --negative)
set_tests_properties(switch_profile_negative PROPERTIES WILL_FAIL TRUE)

add_executable(switch_save_test tests/platform/switch_save.c src/platform/common/switch_file.c src/core/save.c)
target_include_directories(switch_save_test PRIVATE include src/core)
target_compile_definitions(switch_save_test PRIVATE MOON_SWITCH_FILE_TEST=1 MOON_SWITCH_SAVE_TEST=1)
target_link_libraries(switch_save_test PRIVATE ${MOON_CORE_STACK})
add_test(NAME switch_save COMMAND switch_save_test "${CMAKE_BINARY_DIR}/switch-save-test")
add_test(NAME switch_save_negative COMMAND switch_save_test "${CMAKE_BINARY_DIR}/switch-save-negative" --negative)
set_tests_properties(switch_save_negative PROPERTIES WILL_FAIL TRUE)

add_executable(deko_snapshot_test tests/platform/deko_snapshot.c)
add_test(NAME deko_snapshot COMMAND deko_snapshot_test)
add_test(NAME deko_snapshot_negative COMMAND deko_snapshot_test --negative)
set_tests_properties(deko_snapshot_negative PROPERTIES WILL_FAIL TRUE)
add_executable(deko_snapshot_gpu_test tests/platform/deko_snapshot_gpu.c)
target_link_libraries(deko_snapshot_gpu_test PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
add_test(NAME deko_snapshot_gpu COMMAND deko_snapshot_gpu_test ${CMAKE_SOURCE_DIR}/src/platform/switch/shaders/snapshot.frag)
add_test(NAME deko_snapshot_gpu_negative COMMAND deko_snapshot_gpu_test ${CMAKE_SOURCE_DIR}/src/platform/switch/shaders/snapshot.frag --negative)
set_tests_properties(deko_snapshot_gpu_negative PROPERTIES WILL_FAIL TRUE)
add_test(NAME deko_mask_gpu COMMAND deko_snapshot_gpu_test ${CMAKE_SOURCE_DIR}/src/platform/switch/shaders/mask.frag --mask)
add_test(NAME deko_mask_gpu_negative COMMAND deko_snapshot_gpu_test ${CMAKE_SOURCE_DIR}/src/platform/switch/shaders/mask.frag --mask --negative)
set_tests_properties(deko_mask_gpu_negative PROPERTIES WILL_FAIL TRUE)
add_test(NAME deko_mask_materials COMMAND deko_materials_test ${CMAKE_SOURCE_DIR}/src/platform/switch/shaders/mesh.frag --redmask)
add_test(NAME deko_mask_materials_negative COMMAND deko_materials_test ${CMAKE_SOURCE_DIR}/src/platform/switch/shaders/mesh.frag --redmask-negative)
set_tests_properties(deko_mask_materials_negative PROPERTIES WILL_FAIL TRUE)
add_executable(deko_environment_test tests/platform/deko_environment.c src/platform/sdl/switch_render.c)
target_compile_definitions(deko_environment_test PRIVATE MOON_SWITCH_RENDER_TEST=1)
target_link_libraries(deko_environment_test PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
add_test(NAME deko_environment COMMAND deko_environment_test ${CMAKE_SOURCE_DIR}/src/platform/switch/shaders/env.frag)
add_test(NAME deko_environment_negative COMMAND deko_environment_test ${CMAKE_SOURCE_DIR}/src/platform/switch/shaders/env.frag --negative)
set_tests_properties(deko_environment_negative PROPERTIES WILL_FAIL TRUE)
add_executable(switch_render_test tests/platform/switch_render.c src/platform/sdl/switch_render.c
  src/platform/sdl/video_gl.c src/platform/sdl/gl.c)
target_compile_definitions(switch_render_test PRIVATE MOON_SWITCH_RENDER_TEST=1)
target_link_libraries(switch_render_test PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
add_test(NAME switch_render COMMAND switch_render_test)
add_test(NAME switch_render_negative COMMAND switch_render_test --negative)
set_tests_properties(switch_render_negative PROPERTIES WILL_FAIL TRUE)
add_test(NAME switch_render_quality_negative COMMAND switch_render_test --quality-negative)
set_tests_properties(switch_render_quality_negative PROPERTIES WILL_FAIL TRUE)
add_test(NAME switch_render_cache_negative COMMAND switch_render_test --cache-negative)
set_tests_properties(switch_render_cache_negative PROPERTIES WILL_FAIL TRUE)
add_test(NAME switch_render_batch_negative COMMAND switch_render_test --batch-negative)
set_tests_properties(switch_render_batch_negative PROPERTIES WILL_FAIL TRUE)
add_test(NAME switch_render_shader_negative COMMAND switch_render_test --shader-negative)
set_tests_properties(switch_render_shader_negative PROPERTIES WILL_FAIL TRUE)
add_test(NAME switch_render_state_negative COMMAND switch_render_test --state-negative)
set_tests_properties(switch_render_state_negative PROPERTIES WILL_FAIL TRUE)
add_test(NAME switch_render_state_depth_negative COMMAND switch_render_test --state-depth-negative)
set_tests_properties(switch_render_state_depth_negative PROPERTIES WILL_FAIL TRUE)
add_executable(steering_model_tests tests/ui/test_steering.c)
target_link_libraries(steering_model_tests PRIVATE ${MOON_CORE_STACK})
add_test(NAME steering_model COMMAND steering_model_tests)
add_executable(spatial_visibility_tests tests/render/spatial_visibility.c)
target_include_directories(spatial_visibility_tests PRIVATE src/platform/sdl src/core)
if(NOT MSVC)
  target_link_libraries(spatial_visibility_tests PRIVATE m)
endif()
add_test(NAME spatial_visibility COMMAND spatial_visibility_tests)
add_test(NAME spatial_visibility_negative COMMAND spatial_visibility_tests --negative)
set_tests_properties(spatial_visibility_negative PROPERTIES WILL_FAIL TRUE)
add_executable(render3d_frustum_tests tests/ui/test_render3d_frustum.c)
target_include_directories(render3d_frustum_tests PRIVATE src/platform/sdl src/core)
add_test(NAME render3d_frustum COMMAND render3d_frustum_tests)
add_executable(model_attachment_tests tests/ui/test_model_attachments.c)
target_link_libraries(model_attachment_tests PRIVATE ${MOON_CORE_STACK})
add_test(NAME model_attachments COMMAND model_attachment_tests)
add_executable(specdb_tests tests/ui/test_specdb.c)
target_link_libraries(specdb_tests PRIVATE ${MOON_CORE_STACK})
add_test(NAME specdb COMMAND specdb_tests)
add_executable(movie_ipic tests/ui/movie_ipic.c)
target_link_libraries(movie_ipic PRIVATE moon_core moon_platform_sdl moon_platform_common SDL2::SDL2main)
if(WIN32 AND MOON_TEST_DISC)
  add_test(NAME movie_ipic COMMAND movie_ipic "${MOON_TEST_DISC}")
  add_test(NAME movie_ipic_negative COMMAND movie_ipic "${MOON_TEST_DISC}" --negative)
  set_tests_properties(movie_ipic_negative PROPERTIES WILL_FAIL TRUE)
endif()
add_executable(menu_tests tests/ui/test_menu.c)
target_link_libraries(menu_tests PRIVATE ${MOON_CORE_STACK})
add_test(NAME menu_navigation COMMAND menu_tests)
add_test(NAME menu_page_prepare_negative COMMAND menu_tests)
set_tests_properties(menu_page_prepare_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_PAGE_PREPARE_NEGATIVE=1")
add_executable(arcade_prepare tests/ui/arcade_prepare.c)
target_link_libraries(arcade_prepare PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
if(MOON_TEST_DISC)
  add_test(NAME arcade_prepare COMMAND arcade_prepare "${MOON_TEST_DISC}")
  add_test(NAME arcade_prepare_negative COMMAND arcade_prepare "${MOON_TEST_DISC}" --negative)
  set_tests_properties(arcade_prepare_negative PROPERTIES WILL_FAIL TRUE)
endif()
add_executable(pregrid_menu tests/ui/pregrid_menu.c)
target_link_libraries(pregrid_menu PRIVATE ${MOON_CORE_STACK})
if(MOON_TEST_DISC)
  add_test(NAME prize_model_loading COMMAND pregrid_menu "${MOON_TEST_DISC}")
  add_test(NAME prize_model_environment_negative COMMAND pregrid_menu "${MOON_TEST_DISC}")
  set_tests_properties(prize_model_environment_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_PRIZE_ENV_NEGATIVE=1")
  add_test(NAME prize_model_loading_negative COMMAND pregrid_menu "${MOON_TEST_DISC}")
  set_tests_properties(prize_model_loading_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_PRIZE_LOADING_NEGATIVE=1")
endif()
add_executable(machine_flow tests/ui/machine_flow.c src/app/race_audio.c src/app/video_settings.c src/app/options_store.c src/app/desktop_input.c src/app/discord_presence.c)
target_include_directories(machine_flow PRIVATE src/app)
target_link_libraries(machine_flow PRIVATE moon_replay_state moon_race_sound moon_physics ${MOON_SDL_STACK} miniz SDL2::SDL2main)
if(MOON_TEST_DISC)
  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/machine-finish/captures")
  add_test(NAME gtmode_machine_flow COMMAND machine_flow "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/machine-finish/captures")
  add_test(NAME gtmode_machine_flow_negative COMMAND machine_flow "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/machine-finish/captures" --negative)
  set_tests_properties(gtmode_machine_flow gtmode_machine_flow_negative PROPERTIES RUN_SERIAL TRUE)
  set_tests_properties(gtmode_machine_flow_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME gtmode_machine_menu_return COMMAND pregrid_menu "${MOON_TEST_DISC}")
  add_test(NAME quick_pcm_return_negative COMMAND pregrid_menu "${MOON_TEST_DISC}")
  set_tests_properties(quick_pcm_return_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_QUICK_PCM_RETURN_NEGATIVE=1")
  add_test(NAME gtmode_machine_menu_return_negative COMMAND pregrid_menu "${MOON_TEST_DISC}")
  set_tests_properties(gtmode_machine_menu_return_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_MT_RETURN_NEGATIVE=1")
endif()
add_executable(gtmode_event_request tests/gtmode/event_request_run.c)
# Integration harness also needs the locally developed career event bridge.
if(EXISTS "${CMAKE_SOURCE_DIR}/src/core/career_event.h")
  add_executable(gtmode_sunday_playable tests/gtmode/sunday_playable.c)
  target_link_libraries(gtmode_sunday_playable PRIVATE moon_career_vehicle ${MOON_CORE_STACK} moon_physics)
endif()
add_executable(gtmode_championship_cycle tests/gtmode/championship_cycle_run.c)
target_link_libraries(gtmode_championship_cycle PRIVATE ${MOON_CORE_STACK} moon_physics)
add_executable(gtmode_award_delivery tests/gtmode/award_delivery_run.c)
target_link_libraries(gtmode_award_delivery PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
add_executable(gtmode_course_dialog tests/gtmode/course_dialog.c)
target_link_libraries(gtmode_course_dialog PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
add_executable(gtmode_regulation_dialog tests/gtmode/regulation_dialog.c)
target_link_libraries(gtmode_regulation_dialog PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
if(EXISTS "${MOON_TEST_DISC}")
  add_test(NAME gtmode_regulation_text COMMAND gtmode_regulation_dialog "${MOON_TEST_DISC}")
  add_test(NAME gtmode_regulation_text_negative COMMAND gtmode_regulation_dialog "${MOON_TEST_DISC}")
  set_tests_properties(gtmode_regulation_text_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_REGULATION_TEXT_NEGATIVE=1")
endif()
add_executable(gtmode_series_playable tests/gtmode/series_playable.c)
target_link_libraries(gtmode_series_playable PRIVATE moon_career_vehicle ${MOON_CORE_STACK} moon_physics)
if(MOON_TEST_DISC)
  foreach(check positive record continuity course reverse)
    set(continuity_negative)
    if(NOT check STREQUAL "positive")
      set(continuity_negative --negative ${check})
    endif()
    add_test(NAME gtmode_career_continuity_${check}
      COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tests/gtmode/career_continuous.py
      "${MOON_TEST_DISC}" --smoke --bin ${CMAKE_BINARY_DIR}/Release ${continuity_negative})
    set_tests_properties(gtmode_career_continuity_${check} PROPERTIES
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 600 RUN_SERIAL TRUE)
    if(NOT check STREQUAL "positive")
      set_tests_properties(gtmode_career_continuity_${check} PROPERTIES WILL_FAIL TRUE)
    endif()
  endforeach()
endif()

add_executable(gtmode_present_cycle tests/gtmode/present_cycle.c)
target_link_libraries(gtmode_present_cycle PRIVATE moon_career_vehicle ${MOON_CORE_STACK} moon_physics)
if(MOON_TEST_DISC)
  add_test(NAME gtmode_present_cycle COMMAND gtmode_present_cycle "${MOON_TEST_DISC}")
  foreach(negative DUPLICATE BODY COLOR)
    string(TOLOWER ${negative} suffix)
    add_test(NAME gtmode_present_${suffix}_negative COMMAND gtmode_present_cycle "${MOON_TEST_DISC}")
    set_tests_properties(gtmode_present_${suffix}_negative PROPERTIES
      WILL_FAIL TRUE ENVIRONMENT "GT4_PRESENT_${negative}_NEGATIVE=1")
  endforeach()
  add_test(NAME gtmode_present_cycle_negative COMMAND gtmode_present_cycle "${MOON_TEST_DISC}")
  set_tests_properties(gtmode_present_cycle_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_PRESENT_CYCLE_NEGATIVE=1")
endif()
add_executable(gtmode_career_profile tests/gtmode/career_profile_run.c)
target_link_libraries(gtmode_career_profile PRIVATE moon_career_vehicle ${MOON_CORE_STACK} moon_physics)
add_executable(gtmode_series_pace tests/gtmode/series_pace.c)
target_link_libraries(gtmode_series_pace PRIVATE moon_career_vehicle ${MOON_CORE_STACK} moon_physics)
add_executable(gtmode_series_fixtures tests/gtmode/series_fixtures.c)
target_link_libraries(gtmode_series_fixtures PRIVATE moon_career_vehicle ${MOON_CORE_STACK} moon_physics)
add_executable(gtmode_series_chassis tests/gtmode/series_chassis.c)
target_link_libraries(gtmode_series_chassis PRIVATE moon_career_vehicle ${MOON_CORE_STACK} moon_physics)
add_executable(gtmode_trajectory_native tests/gtmode/trajectory_native.c)
target_link_libraries(gtmode_trajectory_native PRIVATE moon_career_vehicle ${MOON_CORE_STACK} moon_physics)
if(WIN32)
  target_link_libraries(gtmode_trajectory_native PRIVATE bcrypt)
endif()
add_executable(gtmode_series_awards tests/gtmode/series_awards.c)
target_link_libraries(gtmode_series_awards PRIVATE moon_career_vehicle ${MOON_CORE_STACK} moon_physics)
add_executable(gtmode_championship_bonus tests/gtmode/championship_bonus.c)
target_link_libraries(gtmode_championship_bonus PRIVATE moon_career_vehicle ${MOON_CORE_STACK} moon_physics)
add_executable(gtmode_endurance_catalog tests/gtmode/endurance_catalog.c)
target_link_libraries(gtmode_endurance_catalog PRIVATE moon_career_vehicle ${MOON_CORE_STACK} moon_physics)
target_link_libraries(gtmode_event_request PRIVATE ${MOON_CORE_STACK} moon_physics)
add_executable(pregrid_pcm tests/ui/pregrid_pcm.c)
target_link_libraries(pregrid_pcm PRIVATE ${MOON_CORE_STACK})
add_executable(pregrid_audio_probe tests/ui/pregrid_audio.c src/app/pregrid_audio.c src/platform/sdl/music_override.c)
target_include_directories(pregrid_audio_probe PRIVATE src/app src/platform/sdl)
target_link_libraries(pregrid_audio_probe PRIVATE ${MOON_CORE_STACK} SDL2::SDL2)
if(MOON_TEST_DISC)
  add_test(NAME album_pcm_bridge COMMAND pregrid_audio_probe "${MOON_TEST_DISC}")
  add_test(NAME menu_pcm_loading_gate_negative COMMAND pregrid_audio_probe "${MOON_TEST_DISC}")
  set_tests_properties(menu_pcm_loading_gate_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_PCM_LOAD_GATE_NEGATIVE=1")
  add_test(NAME album_pcm_bridge_negative COMMAND pregrid_audio_probe "${MOON_TEST_DISC}" --negative)
  set_tests_properties(album_pcm_bridge_negative PROPERTIES WILL_FAIL TRUE)
endif()
# the Time Trial built from the disc against the TT captures (tests/physics/tt_build.c)
set(MOON_TEST_DISC "" CACHE FILEPATH "User's GT4 disc for native licence tests")
if(EXISTS "${MOON_TEST_DISC}")
  add_test(NAME vulkan_disc COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/render/vulkan_disc.py"
    "$<TARGET_FILE:showroom_probe>" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/vulkan-disc")
  add_test(NAME vulkan_disc_negative COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/render/vulkan_disc.py"
    "$<TARGET_FILE:showroom_probe>" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/vulkan-disc-negative" --negative)
  set_tests_properties(vulkan_disc vulkan_disc_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 300)
  set_tests_properties(vulkan_disc_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME vulkan_race COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/render/vulkan_race.py"
    "$<TARGET_FILE:showroom_probe>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/vulkan-race")
  add_test(NAME vulkan_race_negative COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/render/vulkan_race.py"
    "$<TARGET_FILE:showroom_probe>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/vulkan-race-negative" --negative)
  set_tests_properties(vulkan_race vulkan_race_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 120)
  set_tests_properties(vulkan_race_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME vulkan_simulation COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/render/vulkan_simulation.py"
    "$<TARGET_FILE:race_inspect_probe>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/vulkan-simulation")
  add_test(NAME vulkan_simulation_negative COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/render/vulkan_simulation.py"
    "$<TARGET_FILE:race_inspect_probe>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/vulkan-simulation-negative" --negative)
  set_tests_properties(vulkan_simulation vulkan_simulation_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 240)
  set_tests_properties(vulkan_simulation_negative PROPERTIES WILL_FAIL TRUE)
endif()
if(EXISTS "${MOON_TEST_DISC}")
  add_test(NAME gtmode_series_awards COMMAND gtmode_series_awards "${MOON_TEST_DISC}")
  add_test(NAME gtmode_series_awards_negative COMMAND gtmode_series_awards "${MOON_TEST_DISC}")
  set_tests_properties(gtmode_series_awards_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_SERIES_AWARDS_NEGATIVE=1")
  add_test(NAME gtmode_series_gifts_negative COMMAND gtmode_series_awards "${MOON_TEST_DISC}")
  set_tests_properties(gtmode_series_gifts_negative PROPERTIES WILL_FAIL TRUE RUN_SERIAL TRUE TIMEOUT 1200 ENVIRONMENT "GT4_SERIES_GIFTS_NEGATIVE=1")
  add_test(NAME gtmode_championship_bonus COMMAND gtmode_championship_bonus "${MOON_TEST_DISC}")
  add_test(NAME gtmode_championship_bonus_negative COMMAND gtmode_championship_bonus "${MOON_TEST_DISC}")
  set_tests_properties(gtmode_championship_bonus_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_CHAMPIONSHIP_BONUS_NEGATIVE=1")
  add_test(NAME gtmode_endurance_catalog COMMAND gtmode_endurance_catalog "${MOON_TEST_DISC}")
  add_test(NAME gtmode_endurance_catalog_negative COMMAND gtmode_endurance_catalog "${MOON_TEST_DISC}")
  set_tests_properties(gtmode_endurance_catalog_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_ENDURANCE_CATALOG_NEGATIVE=1")
  foreach(kind series championship)
    if(kind STREQUAL "series")
      set(physical_args am_sunday_0000 corvette_z06_00 1)
    else()
      set(physical_args pr_tuning_0500 minolta_toyota_88cv_89 1)
    endif()
    add_test(NAME gtmode_${kind}_physical COMMAND gtmode_series_playable "${MOON_TEST_DISC}" --physical ${physical_args})
    add_test(NAME gtmode_${kind}_physical_negative COMMAND gtmode_series_playable "${MOON_TEST_DISC}" --physical ${physical_args})
    set_tests_properties(gtmode_${kind}_physical_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_SERIES_PHYSICAL_NEGATIVE=1")
    set_tests_properties(gtmode_${kind}_physical gtmode_${kind}_physical_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 600)
  endforeach()
  foreach(kind ff fr 4wd mr dirt snow ice kcar spider pickup)
    set(fixture_parts "")
    if(kind STREQUAL "ff")
      set(physical_args am_ff_0000 srt4_03 1)
      set(fixture_parts "GT4_SERIES_PARTS=14=3,20=3,8=3")
    elseif(kind STREQUAL "fr")
      set(physical_args am_fr_0000 cerbera_speed12_00 1)
    elseif(kind STREQUAL "4wd")
      set(physical_args am_4wd_0000 zz2_00 1)
    elseif(kind STREQUAL "mr")
      set(physical_args am_mr_0000 zonda_c12s_gmv_02 1)
    elseif(kind STREQUAL "dirt")
      set(physical_args rh_grandcanyon_e_0000 rsc_tmv_lm_01 1)
      set(fixture_parts "GT4_SERIES_PARTS=25=11,26=11")
    elseif(kind STREQUAL "snow")
      set(physical_args rh_nursnow_e_0000 rsc_tmv_lm_01 1)
      set(fixture_parts "GT4_SERIES_PARTS=25=12,26=12")
    elseif(kind STREQUAL "ice")
      set(physical_args rh_icerace_e_0000 rsc_tmv_lm_01 1)
      set(fixture_parts "GT4_SERIES_PARTS=25=12,26=12")
    elseif(kind STREQUAL "kcar")
      set(physical_args am_kcar_0000 cappuccino_95 1)
      set(fixture_parts "GT4_SERIES_PARTS=14=3,20=3,8=3,25=10,26=10")
    elseif(kind STREQUAL "spider")
      set(physical_args am_spider_0000 viper_srt10_03 1)
    else()
      set(physical_args am_pickup_0000 silverado_sst_02 1)
      set(fixture_parts "GT4_SERIES_PARTS=13=3,20=3,8=3,25=10,26=10")
    endif()
    add_test(NAME gtmode_${kind}_series_physical COMMAND gtmode_series_playable "${MOON_TEST_DISC}" --physical ${physical_args})
    add_test(NAME gtmode_${kind}_series_physical_negative COMMAND gtmode_series_playable "${MOON_TEST_DISC}" --physical ${physical_args})
    set_tests_properties(gtmode_${kind}_series_physical PROPERTIES ENVIRONMENT "${fixture_parts}")
    set_tests_properties(gtmode_${kind}_series_physical_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "${fixture_parts};GT4_SERIES_PHYSICAL_NEGATIVE=1")
    set_tests_properties(gtmode_${kind}_series_physical gtmode_${kind}_series_physical_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 600)
  endforeach()
  add_test(NAME gtmode_endurance_physical COMMAND gtmode_series_playable "${MOON_TEST_DISC}" --endurance en_infinion_alms_0000 minolta_toyota_88cv_89 1)
  add_test(NAME gtmode_endurance_physical_negative COMMAND gtmode_series_playable "${MOON_TEST_DISC}" --endurance en_infinion_alms_0000 minolta_toyota_88cv_89 1)
  set_tests_properties(gtmode_endurance_physical_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_ENDURANCE_RULE_NEGATIVE=1")
  set_tests_properties(gtmode_series_awards gtmode_series_awards_negative gtmode_championship_bonus gtmode_championship_bonus_negative gtmode_endurance_catalog gtmode_endurance_catalog_negative gtmode_endurance_physical gtmode_endurance_physical_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 1200)
  add_test(NAME gtmode_career_cycle COMMAND gtmode_sunday_playable "${MOON_TEST_DISC}" --career-cycle)
  add_test(NAME gtmode_career_mileage_negative COMMAND gtmode_sunday_playable "${MOON_TEST_DISC}" --career-cycle)
  set_tests_properties(gtmode_career_mileage_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_CAREER_MILEAGE_NEGATIVE=1")
  set_tests_properties(gtmode_career_cycle gtmode_career_mileage_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 600)
  add_test(NAME gtmode_course_dialog COMMAND gtmode_course_dialog "${MOON_TEST_DISC}")
  add_test(NAME gtmode_course_dialog_negative COMMAND gtmode_course_dialog "${MOON_TEST_DISC}")
  set_tests_properties(gtmode_course_dialog_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_COURSE_DIALOG_NEGATIVE=1")
endif()
if(EXISTS "${MOON_TEST_DISC}")
  foreach(part disc cycle movie drive award profile)
    if(part STREQUAL "drive")
      set(license_args l0b0001 980)
    else()
      set(license_args "")
    endif()
    add_test(NAME gtmode_license_${part} COMMAND gtmode_license_${part} "${MOON_TEST_DISC}" ${license_args})
    add_test(NAME gtmode_license_${part}_negative COMMAND gtmode_license_${part} "${MOON_TEST_DISC}" ${license_args})
    string(TOUPPER "${part}" license_env)
    set_tests_properties(gtmode_license_${part}_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_${license_env}_NEGATIVE=1")
  endforeach()
  add_test(NAME gtmode_license_time_bridge_negative COMMAND gtmode_license_cycle "${MOON_TEST_DISC}")
  set_tests_properties(gtmode_license_time_bridge_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_TIME_BRIDGE_NEGATIVE=1")
  add_test(NAME gtmode_license_trophy_bridge_negative COMMAND gtmode_license_movie "${MOON_TEST_DISC}")
  set_tests_properties(gtmode_license_trophy_bridge_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_TROPHY_BRIDGE_NEGATIVE=1")
  add_test(NAME gtmode_license_color_bridge_negative COMMAND gtmode_license_movie "${MOON_TEST_DISC}")
  set_tests_properties(gtmode_license_color_bridge_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_COLOR_BRIDGE_NEGATIVE=1")
  add_test(NAME gtmode_license_timer_text_negative COMMAND gtmode_license_movie "${MOON_TEST_DISC}")
  set_tests_properties(gtmode_license_timer_text_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_TIMER_TEXT_NEGATIVE=1")
  add_test(NAME gtmode_license_fail_vector_negative COMMAND gtmode_license_movie "${MOON_TEST_DISC}")
  set_tests_properties(gtmode_license_fail_vector_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_FAIL_VECTOR_NEGATIVE=1")
  add_test(NAME gtmode_license_fail_hole_negative COMMAND gtmode_license_movie "${MOON_TEST_DISC}")
  set_tests_properties(gtmode_license_fail_hole_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_FAIL_HOLE_NEGATIVE=1")
  add_test(NAME gtmode_license_screen_negative COMMAND gtmode_license_movie "${MOON_TEST_DISC}")
  set_tests_properties(gtmode_license_screen_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_SCREEN_NEGATIVE=1")
  add_test(NAME gtmode_license_fail_drive COMMAND gtmode_license_drive "${MOON_TEST_DISC}" l0b0001 1420 900)
  add_test(NAME gtmode_license_fail_drive_negative COMMAND gtmode_license_drive "${MOON_TEST_DISC}" l0b0001 1420 900)
  set_tests_properties(gtmode_license_fail_drive_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_DRIVE_NEGATIVE=1")
  add_test(NAME gtmode_license_profile_return_negative COMMAND gtmode_license_profile "${MOON_TEST_DISC}")
  set_tests_properties(gtmode_license_profile_return_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_PROFILE_RETURN_NEGATIVE=1")
  add_test(NAME gtmode_license_cursor_return_negative COMMAND gtmode_license_profile "${MOON_TEST_DISC}")
  set_tests_properties(gtmode_license_cursor_return_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_CURSOR_RETURN_NEGATIVE=1")
  add_test(NAME gtmode_license_return_negative COMMAND gtmode_license_cycle "${MOON_TEST_DISC}")
  set_tests_properties(gtmode_license_return_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_RETURN_NEGATIVE=1")
  add_test(NAME gtmode_license_gate_negative COMMAND gtmode_license_cycle "${MOON_TEST_DISC}")
  set_tests_properties(gtmode_license_gate_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_GATE_NEGATIVE=1")
endif()
add_executable(gtmode_license_award tests/gtmode/license_award_run.c)
target_link_libraries(gtmode_license_award PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
add_executable(gtmode_license_disc tests/gtmode/license_disc_run.c)
target_link_libraries(gtmode_license_disc PRIVATE moon_physics ${MOON_CORE_STACK})
add_executable(gtmode_mission_disc tests/gtmode/mission_disc_run.c)
target_link_libraries(gtmode_mission_disc PRIVATE moon_physics ${MOON_CORE_STACK})
add_executable(gtmode_mission_cycle tests/gtmode/mission_cycle_run.c)
target_link_libraries(gtmode_mission_cycle PRIVATE ${MOON_CORE_STACK} moon_physics)
add_executable(gtmode_mission_catalog tests/gtmode/mission_catalog_run.c)
target_link_libraries(gtmode_mission_catalog PRIVATE ${MOON_CORE_STACK} moon_physics)
if(EXISTS "${MOON_TEST_DISC}")
  add_test(NAME gtmode_mission_catalog COMMAND gtmode_mission_catalog "${MOON_TEST_DISC}")
  add_test(NAME gtmode_mission_catalog_negative COMMAND gtmode_mission_catalog "${MOON_TEST_DISC}")
  set_tests_properties(gtmode_mission_catalog_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_MISSION_CATALOG_NEGATIVE=1")
  foreach(mission 22 24)
    set(slipstream_input "GT4_MISSION_DRAFT=1;GT4_MISSION_PASSING_GAP=20")
    if(mission STREQUAL "22")
      list(APPEND slipstream_input "GT4_MISSION_GAIN=0.7")
    else()
      list(APPEND slipstream_input "GT4_MISSION_GAIN=1.8")
    endif()
    add_test(NAME gtmode_mission_cycle_${mission} COMMAND gtmode_mission_cycle "${MOON_TEST_DISC}" l0m00${mission})
    add_test(NAME gtmode_mission_cycle_${mission}_negative COMMAND gtmode_mission_cycle "${MOON_TEST_DISC}" l0m00${mission})
    set_tests_properties(gtmode_mission_cycle_${mission} PROPERTIES ENVIRONMENT "${slipstream_input}")
    set_tests_properties(gtmode_mission_cycle_${mission}_negative PROPERTIES
      WILL_FAIL TRUE ENVIRONMENT "${slipstream_input};GT4_MISSION_CYCLE_NEGATIVE=1")
  endforeach()
  add_test(NAME gtmode_mission_cycle_21 COMMAND gtmode_mission_cycle "${MOON_TEST_DISC}" l0m0021)
  add_test(NAME gtmode_mission_cycle_21_negative COMMAND gtmode_mission_cycle "${MOON_TEST_DISC}" l0m0021)
  set_tests_properties(gtmode_mission_cycle_21 PROPERTIES ENVIRONMENT "GT4_MISSION_DRAFT=1;GT4_MISSION_GAIN=1.2")
  set_tests_properties(gtmode_mission_cycle_21_negative PROPERTIES
    WILL_FAIL TRUE ENVIRONMENT "GT4_MISSION_DRAFT=1;GT4_MISSION_GAIN=1.2;GT4_MISSION_CYCLE_NEGATIVE=1")
  foreach(mission 01 04)
    set(mission_input "GT4_MISSION_GAIN=1.2")
    if(mission STREQUAL "01")
      list(APPEND mission_input "GT4_MISSION_PREDICTIVE=1")
    endif()
    add_test(NAME gtmode_mission_cycle_${mission} COMMAND gtmode_mission_cycle "${MOON_TEST_DISC}" l0m00${mission})
    add_test(NAME gtmode_mission_cycle_${mission}_negative COMMAND gtmode_mission_cycle "${MOON_TEST_DISC}" l0m00${mission})
    set_tests_properties(gtmode_mission_cycle_${mission} PROPERTIES ENVIRONMENT "${mission_input}")
    set_tests_properties(gtmode_mission_cycle_${mission}_negative PROPERTIES
      WILL_FAIL TRUE ENVIRONMENT "${mission_input};GT4_MISSION_CYCLE_NEGATIVE=1")
  endforeach()
  add_test(NAME gtmode_mission_cycle_08 COMMAND gtmode_mission_cycle "${MOON_TEST_DISC}" l0m0008)
  add_test(NAME gtmode_mission_cycle_08_negative COMMAND gtmode_mission_cycle "${MOON_TEST_DISC}" l0m0008)
  set_tests_properties(gtmode_mission_cycle_08 PROPERTIES ENVIRONMENT "GT4_MISSION_TAKEOVER=1")
  set_tests_properties(gtmode_mission_cycle_08_negative PROPERTIES
    WILL_FAIL TRUE ENVIRONMENT "GT4_MISSION_TAKEOVER=1;GT4_MISSION_CYCLE_NEGATIVE=1")
  add_test(NAME gtmode_mission_cycle COMMAND gtmode_mission_cycle "${MOON_TEST_DISC}")
  add_test(NAME gtmode_mission_cycle_negative COMMAND gtmode_mission_cycle "${MOON_TEST_DISC}")
  set_tests_properties(gtmode_mission_cycle_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_MISSION_CYCLE_NEGATIVE=1")
  add_test(NAME gtmode_mission_text_negative COMMAND gtmode_mission_cycle "${MOON_TEST_DISC}")
  set_tests_properties(gtmode_mission_text_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_MISSION_TEXT_NEGATIVE=1")
endif()
add_executable(gtmode_license_profile tests/gtmode/license_profile_run.c)
target_link_libraries(gtmode_license_profile PRIVATE ${MOON_CORE_STACK})
add_executable(gtmode_license_cycle tests/gtmode/license_cycle_run.c)
target_link_libraries(gtmode_license_cycle PRIVATE moon_physics ${MOON_CORE_STACK})
add_executable(gtmode_license_drive tests/gtmode/license_drive_run.c)
target_link_libraries(gtmode_license_drive PRIVATE moon_physics ${MOON_CORE_STACK})
add_executable(gtmode_license_movie tests/gtmode/license_movie_run.c)
target_link_libraries(gtmode_license_movie PRIVATE ${MOON_CORE_STACK})
add_executable(tt_build tests/physics/tt_build.c)
target_link_libraries(tt_build PRIVATE moon_physics ${MOON_CORE_STACK})
target_include_directories(tt_build PRIVATE src/core/physics)
add_executable(race_audio_probe tests/ui/race_audio.c src/platform/sdl/music_override.c)
target_include_directories(race_audio_probe PRIVATE src/app src/platform/sdl)
target_link_libraries(race_audio_probe PRIVATE moon_race_sound moon_physics ${MOON_CORE_STACK} SDL2::SDL2)
add_executable(race_engine_bank_capture tests/ui/race_engine_bank_capture.c)
target_link_libraries(race_engine_bank_capture PRIVATE moon_race_sound ${MOON_CORE_STACK})
add_executable(race_audio_clock tests/ui/race_audio_clock.c src/app/video_settings.c src/app/options_store.c src/app/desktop_input.c src/app/discord_presence.c)
add_executable(split_audio tests/ui/split_audio.c src/app/video_settings.c src/app/options_store.c src/app/desktop_input.c src/app/discord_presence.c)
target_include_directories(split_audio PRIVATE src/app)
target_link_libraries(split_audio PRIVATE moon_race_sound moon_physics ${MOON_SDL_STACK} moon_replay_state miniz SDL2::SDL2main)
add_executable(pause_resident tests/ui/pause_resident.c)
target_link_libraries(pause_resident PRIVATE ${MOON_SDL_STACK} miniz SDL2::SDL2main)
if(MOON_TEST_DISC)
  add_test(NAME split_audio COMMAND split_audio "${MOON_TEST_DISC}")
  add_test(NAME split_audio_negative COMMAND split_audio "${MOON_TEST_DISC}" --negative)
  set_tests_properties(split_audio_negative PROPERTIES WILL_FAIL TRUE)
  foreach(api opengl vulkan)
    add_test(NAME pause_resident_${api} COMMAND pause_resident "${MOON_TEST_DISC}")
    add_test(NAME pause_resident_${api}_negative COMMAND pause_resident "${MOON_TEST_DISC}" --negative)
    set_tests_properties(pause_resident_${api}_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "MOON_GRAPHICS_API=${api};SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1")
    set_tests_properties(pause_resident_${api} PROPERTIES ENVIRONMENT "MOON_GRAPHICS_API=${api};SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1")
  endforeach()
  set_tests_properties(split_audio split_audio_negative PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1")
endif()
target_include_directories(race_audio_clock PRIVATE src/app)
target_link_libraries(race_audio_clock PRIVATE moon_race_sound moon_physics ${MOON_SDL_STACK} moon_replay_state miniz SDL2::SDL2main)
add_executable(pit_scene_capture tests/ui/pit_scene_capture.c src/app/race_audio.c src/app/video_settings.c src/app/options_store.c src/app/desktop_input.c src/app/discord_presence.c)
target_include_directories(pit_scene_capture PRIVATE src/app)
target_link_libraries(pit_scene_capture PRIVATE moon_race_sound moon_physics ${MOON_SDL_STACK} moon_replay_state miniz SDL2::SDL2main)
if(MOON_TEST_DISC)
  add_test(NAME race_audio_clock COMMAND race_audio_clock "${MOON_TEST_DISC}")
  add_test(NAME race_music_pause COMMAND race_audio_clock "${MOON_TEST_DISC}" --pause-music)
  add_test(NAME race_music_pause_negative COMMAND race_audio_clock "${MOON_TEST_DISC}" --pause-music-negative)
  set_tests_properties(race_music_pause_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME race_wind_pause COMMAND race_audio_clock "${MOON_TEST_DISC}" --wind-pause)
  add_test(NAME race_wind_pause_negative COMMAND race_audio_clock "${MOON_TEST_DISC}" --wind-pause-negative)
  set_tests_properties(race_wind_pause_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME race_audio_camera_cuts COMMAND race_audio_clock "${MOON_TEST_DISC}" --camera-cuts)
  add_test(NAME race_audio_camera_cuts_negative COMMAND race_audio_clock "${MOON_TEST_DISC}" --cut-negative)
  set_tests_properties(race_audio_camera_cuts_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME race_audio_clock_negative COMMAND race_audio_clock "${MOON_TEST_DISC}" --negative)
  add_test(NAME race_capture_stems COMMAND race_audio_clock "${MOON_TEST_DISC}" --capture-stems)
  add_test(NAME race_capture_stems_negative COMMAND race_audio_clock "${MOON_TEST_DISC}" --capture-stems-negative)
  set_tests_properties(race_capture_stems_negative PROPERTIES WILL_FAIL TRUE)
  set_tests_properties(race_audio_clock_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME race_pit_audio COMMAND race_audio_probe "${MOON_TEST_DISC}" --pit-only)
  add_test(NAME race_pit_audio_negative COMMAND race_audio_probe "${MOON_TEST_DISC}" --pit-negative)
  set_tests_properties(race_pit_audio_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME race_field_size_audit COMMAND race_audio_probe "${MOON_TEST_DISC}" --field-audit)
endif()
add_test(NAME ee_memory COMMAND ee_unit_tests)
add_test(NAME ee_sif COMMAND ee_sif_tests)
add_executable(ee_thread_tests tests/ee_unit/test_thread.c)
target_link_libraries(ee_thread_tests PRIVATE moon_ee)
add_test(NAME ee_thread COMMAND ee_thread_tests)
add_executable(ee_sched_tests tests/ee_unit/test_sched.c)
target_link_libraries(ee_sched_tests PRIVATE moon_ee)
add_test(NAME ee_sched COMMAND ee_sched_tests)
add_executable(ee_fpu_tests tests/ee_unit/test_fpu.c)
target_link_libraries(ee_fpu_tests PRIVATE moon_ee)
add_test(NAME ee_fpu COMMAND ee_fpu_tests)
add_executable(ee_mmi_tests tests/ee_unit/test_mmi.c)
target_link_libraries(ee_mmi_tests PRIVATE moon_ee)
add_test(NAME ee_mmi COMMAND ee_mmi_tests)

# ---- Recompiled test chunks ----
# These need the user's own extracted/core.bin and splat output. The generated
# C is derived from game code and stays in the build directory.
option(MOON_RECOMP_TESTS "Build tests that recompile code from extracted/core.bin" OFF)
if(WIN32)
  set(MOON_PYTHON_DEFAULT "${CMAKE_SOURCE_DIR}/.venv/Scripts/python.exe")
else()
  # The tree carries a Windows virtualenv; on Linux the interpreter CMake found
  # for the replay-math generator is the one to use.
  set(MOON_PYTHON_DEFAULT "${Python3_EXECUTABLE}")
endif()
set(MOON_PYTHON "${MOON_PYTHON_DEFAULT}" CACHE FILEPATH
    "Python with rabbitizer installed")
if(WIN32 AND MOON_TEST_DISC AND EXISTS "${MOON_PYTHON}")
  add_test(NAME quick_options_app_return COMMAND "${MOON_PYTHON}" "${CMAKE_SOURCE_DIR}/tests/ui/quick_options_return.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/quick-options-app")
  add_test(NAME quick_options_app_return_negative COMMAND "${MOON_PYTHON}" "${CMAKE_SOURCE_DIR}/tests/ui/quick_options_return.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/quick-options-app-negative" --negative)
  set_tests_properties(quick_options_app_return quick_options_app_return_negative PROPERTIES RUN_SERIAL TRUE)
  set_tests_properties(quick_options_app_return_negative PROPERTIES WILL_FAIL TRUE)
endif()
if(WIN32 AND MOON_WINDOWS_GUI AND EXISTS "${MOON_PYTHON}")
  add_test(NAME launcher_gui COMMAND "${MOON_PYTHON}" "${CMAKE_SOURCE_DIR}/tests/ui/launcher_gui.py" "$<TARGET_FILE:moonengine>")
  add_test(NAME launcher_gui_negative COMMAND "${MOON_PYTHON}" "${CMAKE_SOURCE_DIR}/tests/ui/launcher_gui.py" "$<TARGET_FILE:launcher_tests>")
  set_tests_properties(launcher_gui_negative PROPERTIES WILL_FAIL TRUE)
endif()
if(MOON_RECOMP_TESTS)
  add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/screen_setting_orig.c
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --root 0x003EFEA8 --extra-roots 0x003EFEE8 --exclude 0x0057D6C0 --hooks
      --out ${CMAKE_BINARY_DIR}/screen_setting_orig.c
    DEPENDS tools/recomp/ee_recomp.py extracted/core.bin splat/asm/core/text.s VERBATIM)
  add_executable(screen_setting_oracle tests/ui/screen_setting_oracle.c ${CMAKE_BINARY_DIR}/screen_setting_orig.c)
  target_link_libraries(screen_setting_oracle PRIVATE ${MOON_CORE_STACK} moon_ee)
  add_test(NAME screen_setting_oracle COMMAND screen_setting_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin)
  add_test(NAME screen_setting_oracle_negative COMMAND screen_setting_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin --negative)
  set_tests_properties(screen_setting_oracle_negative PROPERTIES WILL_FAIL TRUE)
  add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/favorites_orig.c
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --root 0x00430320 --extra-roots 0x004303B0,0x004303F8,0x00430460,0x00430528,0x00430558,0x00430570,0x00430660,0x0043A448
      --out ${CMAKE_BINARY_DIR}/favorites_orig.c
    DEPENDS tools/recomp/ee_recomp.py extracted/core.bin splat/asm/core/text.s VERBATIM)
  add_executable(favorites_oracle tests/ui/favorites_oracle.c ${CMAKE_BINARY_DIR}/favorites_orig.c)
  target_link_libraries(favorites_oracle PRIVATE ${MOON_CORE_STACK} moon_ee)
  add_test(NAME favorites_oracle COMMAND favorites_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin)
  add_test(NAME favorites_oracle_negative COMMAND favorites_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin --negative)
  set_tests_properties(favorites_oracle_negative PROPERTIES WILL_FAIL TRUE)
  add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/controller_defaults_orig.c
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --root 0x0043B808 --out ${CMAKE_BINARY_DIR}/controller_defaults_orig.c
    DEPENDS tools/recomp/ee_recomp.py extracted/core.bin splat/asm/core/text.s VERBATIM)
  add_executable(controller_defaults_oracle tests/ui/controller_defaults_oracle.c ${CMAKE_BINARY_DIR}/controller_defaults_orig.c)
  target_link_libraries(controller_defaults_oracle PRIVATE moon_core moon_ee moon_platform_common)
  add_test(NAME controller_defaults_oracle COMMAND controller_defaults_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin)
  add_test(NAME controller_defaults_oracle_negative COMMAND controller_defaults_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin --negative)
  set_tests_properties(controller_defaults_oracle_negative PROPERTIES WILL_FAIL TRUE)
  add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/disc_replay_orig.c
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --root 0x00345D70 --extra-roots 0x00345BE0,0x001D3B20,0x001D3D70 --out ${CMAKE_BINARY_DIR}/disc_replay_orig.c
    DEPENDS tools/recomp/ee_recomp.py extracted/core.bin splat/asm/core/text.s VERBATIM)
  add_executable(disc_replay_oracle tests/ui/disc_replay_oracle.c ${CMAKE_BINARY_DIR}/disc_replay_orig.c)
  target_link_libraries(disc_replay_oracle PRIVATE ${MOON_CORE_STACK} moon_ee)
  add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/disc_setup_orig.c
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --root 0x003DDAD8 --out ${CMAKE_BINARY_DIR}/disc_setup_orig.c
    DEPENDS tools/recomp/ee_recomp.py extracted/core.bin splat/asm/core/text.s VERBATIM)
  add_executable(disc_setup_oracle tests/ui/disc_setup_oracle.c ${CMAKE_BINARY_DIR}/disc_setup_orig.c)
  target_link_libraries(disc_setup_oracle PRIVATE moon_physics ${MOON_CORE_STACK} moon_ee)
  if(MOON_TEST_DISC)
    add_test(NAME disc_replay_setup COMMAND disc_setup_oracle "${MOON_TEST_DISC}")
    add_test(NAME disc_replay_setup_negative COMMAND disc_setup_oracle "${MOON_TEST_DISC}" --negative)
    set_tests_properties(disc_replay_setup_negative PROPERTIES WILL_FAIL TRUE)
    add_test(NAME disc_replay_bspec_runtime COMMAND disc_setup_oracle "${MOON_TEST_DISC}" --runtime)
    add_test(NAME disc_replay_bspec_runtime_negative COMMAND disc_setup_oracle "${MOON_TEST_DISC}" --runtime-negative)
    set_tests_properties(disc_replay_bspec_runtime_negative PROPERTIES WILL_FAIL TRUE)
  endif()
  add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/disc_checkpoint_orig.c
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py --root 0x0035ABA8 --out ${CMAKE_BINARY_DIR}/disc_checkpoint_orig.c
    DEPENDS tools/recomp/ee_recomp.py extracted/core.bin splat/asm/core/text.s VERBATIM)
  add_executable(disc_checkpoint_oracle tests/ui/disc_checkpoint_oracle.c tests/physics/replay_arithmetic.c ${CMAKE_BINARY_DIR}/disc_checkpoint_orig.c)
  target_include_directories(disc_checkpoint_oracle PRIVATE src/app)
  target_link_libraries(disc_checkpoint_oracle PRIVATE moon_replay_state ${MOON_CORE_STACK} moon_ee)
  if(MOON_TEST_DISC)
    add_test(NAME disc_replay_commands COMMAND disc_replay_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin" "${MOON_TEST_DISC}")
    add_test(NAME disc_replay_commands_negative COMMAND disc_replay_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin" "${MOON_TEST_DISC}" --negative)
    add_test(NAME disc_replay_checkpoint COMMAND disc_checkpoint_oracle "${MOON_TEST_DISC}")
    add_test(NAME disc_replay_checkpoint_negative COMMAND disc_checkpoint_oracle "${MOON_TEST_DISC}" --negative)
    add_test(NAME disc_replay_icons COMMAND disc_replay_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin" "${MOON_TEST_DISC}" --icons)
    add_test(NAME disc_replay_icons_negative COMMAND disc_replay_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin" "${MOON_TEST_DISC}" --icon-negative)
    set_tests_properties(disc_replay_icons_negative PROPERTIES WILL_FAIL TRUE)
    set_tests_properties(disc_replay_commands_negative disc_replay_checkpoint_negative PROPERTIES WILL_FAIL TRUE)
  endif()
endif()
if(MOON_RECOMP_TESTS)
  set(gen_logger "${CMAKE_BINARY_DIR}/gen/gtmode_logger.c")
  add_custom_command(OUTPUT ${gen_logger}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_logger} --root 0x00123050:0x0012307C
      --extra-roots 0x0011E400,0x0011E450,0x0011DBB0,0x0011DBE8,0x0011DC80,0x0011DD10
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_logger_oracle tests/gtmode/logger_run.c ${gen_logger})
  target_link_libraries(gtmode_logger_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  add_executable(gtmode_logger_drive tests/gtmode/logger_drive_run.c ${gen_logger})
  target_link_libraries(gtmode_logger_drive PRIVATE moon_ee moon_physics ${MOON_CORE_STACK})
  if(EXISTS "${MOON_TEST_DISC}")
    add_test(NAME gtmode_logger_drive COMMAND gtmode_logger_drive "${MOON_TEST_DISC}" ${CMAKE_SOURCE_DIR}/extracted/core.bin)
    add_test(NAME gtmode_logger_drive_negative COMMAND gtmode_logger_drive "${MOON_TEST_DISC}" ${CMAKE_SOURCE_DIR}/extracted/core.bin)
    set_tests_properties(gtmode_logger_drive_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LOGGER_DRIVE_NEGATIVE=1")
  endif()
  add_test(NAME gtmode_logger COMMAND gtmode_logger_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin)
  add_test(NAME gtmode_logger_negative COMMAND gtmode_logger_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin)
  set_tests_properties(gtmode_logger_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LOGGER_NEGATIVE=1")
  set(gen_admission "${CMAKE_BINARY_DIR}/gen/career_admission.c")
  set(gen_aspiration "${CMAKE_BINARY_DIR}/gen/career_aspiration.c")
  set(gen_integer_string "${CMAKE_BINARY_DIR}/gen/integer_string.c")
  add_custom_command(OUTPUT ${gen_integer_string}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_integer_string} --root 0x002FD0B0 --extra-roots 0x002FCF80,0x002FD148,0x002FD018,0x00314A40
      --exclude 0x002FE278,0x002FC870,0x003285A8,0x003285F8 --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_integer_string_oracle tests/gtmode/integer_string_oracle.c ${gen_integer_string})
  target_link_libraries(gtmode_integer_string_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  add_test(NAME gtmode_integer_string COMMAND gtmode_integer_string_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin)
  add_test(NAME gtmode_integer_string_negative COMMAND gtmode_integer_string_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin)
  set_tests_properties(gtmode_integer_string_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_INTEGER_STRING_NEGATIVE=1")
  add_custom_command(OUTPUT ${gen_aspiration}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_aspiration} --root 0x0013F598 --extra-roots 0x0013F668,0x001467A0,0x0013F488
      --exclude 0x0013BDC0,0x00147D80,0x002FE278,0x002FC870,0x0013BD68,0x003285A8,0x003285F8,0x00448820,0x00443ED0 --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_aspiration_oracle tests/gtmode/aspiration_oracle.c ${gen_aspiration})
  target_link_libraries(gtmode_aspiration_oracle PRIVATE moon_career_vehicle moon_ee ${MOON_CORE_STACK} moon_physics)
  if(MOON_TEST_DISC)
    add_test(NAME gtmode_aspiration COMMAND gtmode_aspiration_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin ${MOON_TEST_DISC})
    add_test(NAME gtmode_aspiration_negative COMMAND gtmode_aspiration_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin ${MOON_TEST_DISC})
    set_tests_properties(gtmode_aspiration_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_ASPIRATION_NEGATIVE=1")
    add_test(NAME gtmode_series_fixtures COMMAND gtmode_series_fixtures ${MOON_TEST_DISC})
    add_test(NAME gtmode_series_fixtures_negative COMMAND gtmode_series_fixtures ${MOON_TEST_DISC})
    set_tests_properties(gtmode_series_fixtures gtmode_series_fixtures_negative PROPERTIES LABELS "slow")
    set_tests_properties(gtmode_series_fixtures_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_SERIES_FIXTURES_NEGATIVE=1")
  endif()
  add_custom_command(OUTPUT ${gen_admission}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            --out ${gen_admission} --root 0x001A1C60 --extra-roots 0x001A2BB0,0x00448CD0,0x004489A0,0x004490F8,0x004493F8,0x00110498
            --exclude 0x001A0B50,0x002EFBF0,0x0057F260,0x005C2630,0x00314B20,0x002EEEB0,0x00309378,0x00312318,0x003285A8,0x003285F8,0x002ED5C0,0x001A0AF8,0x00449298,0x00447CA0,0x005A4724,0x004312B0 --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_admission_oracle tests/gtmode/admission_oracle.c ${gen_admission})
  target_link_libraries(gtmode_admission_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  if(MOON_TEST_DISC)
    add_test(NAME gtmode_open_car COMMAND gtmode_admission_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin ${MOON_TEST_DISC})
    add_test(NAME gtmode_open_car_negative COMMAND gtmode_admission_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin ${MOON_TEST_DISC})
    set_tests_properties(gtmode_open_car_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_OPEN_CAR_NEGATIVE=1")
    add_test(NAME gtmode_open_tuned_negative COMMAND gtmode_admission_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin ${MOON_TEST_DISC})
    set_tests_properties(gtmode_open_tuned_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_OPEN_TUNED_NEGATIVE=1")
  endif()
  set(gen_license_motion "${CMAKE_BINARY_DIR}/gen/gtmode_license_motion.c")
  add_custom_command(OUTPUT ${gen_license_motion}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_license_motion} --root 0x00427698
      --extra-roots 0x00429BC0,0x0042C348,0x0042DAA0
      --exclude 0x0042B440 --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_license_motion_oracle tests/gtmode/license_motion_run.c ${gen_license_motion})
  target_link_libraries(gtmode_license_motion_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  add_executable(gtmode_license_menu_motion_oracle tests/gtmode/license_menu_motion_run.c ${gen_license_motion})
  target_link_libraries(gtmode_license_menu_motion_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  set(gen_pit_motion "${CMAKE_BINARY_DIR}/gen/pit_motion.c")
  add_custom_command(OUTPUT ${gen_pit_motion}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_pit_motion} --root 0x00427698
      --extra-roots 0x00429BC0,0x0042C348,0x00429740,0x0035BBD8,0x0035BEB0,0x003CF6F0,0x003CF730,0x003CF780,0x00429D20,0x00429AD8,0x003CD620,0x00429E10,0x003CF938,0x003D1488,0x003CB2C0,0x003D6FE0,0x003D6F40,0x003D72A0,0x003D2C48,0x003D0DF0 --exclude 0x0042B440,0x0034C190,0x004A7844,0x004A79B0,0x004A7988,0x004A79D8,0x004A7698,0x0046BEC0,0x0057D8A0,0x003D3030,0x003C9F00,0x003C9F18,0x003CDB20,0x003CE930,0x003CDFD8,0x003CE4A0 --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(pit_motion_oracle tests/ui/pit_motion_oracle.c ${gen_pit_motion})
  target_link_libraries(pit_motion_oracle PRIVATE moon_ee ${MOON_CORE_STACK} moon_physics)
  add_executable(pit_transform_oracle tests/ui/pit_transform_oracle.c ${gen_pit_motion})
  target_link_libraries(pit_transform_oracle PRIVATE moon_ee ${MOON_CORE_STACK} moon_physics)
  target_compile_options(pit_transform_oracle PRIVATE $<$<C_COMPILER_ID:MSVC>:/fp:strict> $<$<NOT:$<C_COMPILER_ID:MSVC>>:-ffp-contract=off;-frounding-math>)
  add_test(NAME pit_transform COMMAND pit_transform_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin)
  add_test(NAME pit_transform_negative COMMAND pit_transform_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin --negative)
  add_test(NAME pit_parent_negative COMMAND pit_transform_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin --negative-parent)
  add_test(NAME pit_mounted_negative COMMAND pit_transform_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin --negative-mounted)
  add_test(NAME pit_draw_negative COMMAND pit_transform_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin --negative-draw)
  add_test(NAME pit_root_negative COMMAND pit_transform_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin --negative-root)
  set_tests_properties(pit_transform_negative pit_parent_negative pit_mounted_negative pit_draw_negative pit_root_negative PROPERTIES WILL_FAIL TRUE)
  set(gen_pit_cable "${CMAKE_BINARY_DIR}/gen/pit_cable.c")
  set(gen_pit_walk "${CMAKE_BINARY_DIR}/gen/pit_walk.c")
  add_custom_command(OUTPUT ${gen_pit_walk}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_pit_walk} --root 0x003D4C38 --extra-roots 0x003D50C8,0x004569F0,0x003D4AA8,0x003D8C48
      --exclude 0x0034C190,0x004573B8,0x0057D8A0,0x0057D118,0x0057D738 --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(pit_walk_oracle tests/ui/pit_walk_oracle.c ${gen_pit_walk})
  target_link_libraries(pit_walk_oracle PRIVATE moon_ee ${MOON_CORE_STACK} moon_physics)
  target_compile_options(pit_walk_oracle PRIVATE $<$<C_COMPILER_ID:MSVC>:/fp:strict> $<$<NOT:$<C_COMPILER_ID:MSVC>>:-ffp-contract=off;-frounding-math>)
  add_test(NAME pit_walk COMMAND pit_walk_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin)
  add_test(NAME pit_walk_negative COMMAND pit_walk_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin --negative)
  add_test(NAME pit_walk_step_negative COMMAND pit_walk_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin --negative-step)
  add_test(NAME pit_walk_light_negative COMMAND pit_walk_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin --negative-light)
  add_test(NAME pit_material_negative COMMAND pit_walk_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin --negative-material)
  add_test(NAME pit_monitor_negative COMMAND pit_walk_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin --negative-monitor)
  set_tests_properties(pit_walk_negative pit_walk_step_negative pit_walk_light_negative pit_material_negative pit_monitor_negative PROPERTIES WILL_FAIL TRUE)
  if(MOON_TEST_DISC)
    add_test(NAME pit_walk_models COMMAND pit_walk_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin ${MOON_TEST_DISC})
    add_test(NAME pit_walk_phase_negative COMMAND pit_walk_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin ${MOON_TEST_DISC} --negative-phase)
    set_tests_properties(pit_walk_phase_negative PROPERTIES WILL_FAIL TRUE)
    add_executable(pit_monitor_probe tests/ui/pit_monitor_probe.c)
    target_link_libraries(pit_monitor_probe PRIVATE ${MOON_SDL_STACK} moon_physics)
    add_test(NAME pit_monitor_flow COMMAND pit_monitor_probe ${MOON_TEST_DISC})
    add_test(NAME pit_monitor_flow_negative COMMAND pit_monitor_probe ${MOON_TEST_DISC} --negative)
    set_tests_properties(pit_monitor_flow_negative PROPERTIES WILL_FAIL TRUE)
  endif()
  add_custom_command(OUTPUT ${gen_pit_cable}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_pit_cable} --root 0x003CA698 --extra-roots 0x003CAAC8
      --exclude 0x004A1638,0x004AB040,0x004A2808,0x004AA1D8,0x004A5B70,0x0049FD50,0x0057D8A0 --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(pit_cable_oracle tests/ui/pit_cable_oracle.c ${gen_pit_cable})
  target_link_libraries(pit_cable_oracle PRIVATE moon_ee ${MOON_CORE_STACK} moon_physics)
  target_compile_options(pit_cable_oracle PRIVATE $<$<C_COMPILER_ID:MSVC>:/fp:strict> $<$<NOT:$<C_COMPILER_ID:MSVC>>:-ffp-contract=off;-frounding-math>)
  add_test(NAME pit_cable COMMAND pit_cable_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin)
  add_test(NAME pit_cable_negative COMMAND pit_cable_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin --negative)
  set_tests_properties(pit_cable_negative PROPERTIES WILL_FAIL TRUE)
  set(gen_pit_controller "${CMAKE_BINARY_DIR}/gen/pit_controller.c")
  add_custom_command(OUTPUT ${gen_pit_controller}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_pit_controller} --root 0x003CFA98 --extra-roots 0x00427698,0x0042B440,0x0042AB50,0x0042ABB0,0x0042AC18,0x0042AC90,0x0042AC98,0x0042ACA0,0x0042AD00,0x0042AD40,0x0042AD48,0x0042D800,0x0042D818,0x003CD730,0x003CBB28,0x003CC010,0x0043F3A8,0x003D3208,0x003D3228,0x003D3248,0x003CC498,0x003CF7C8,0x003C9FC8,0x003CA038,0x003CA0C8,0x003A59D8
      --exclude 0x0034C190,0x0040C130,0x003D5610,0x003CC808,0x003CC8B0,0x0046BEC0,0x0035BEE0,0x00393428,0x0035BE38,0x0057F238,0x0057F260,0x0057B3A0,0x0057D8A0,0x003CB2C0,0x003AFD18,0x003AF108,0x003AF4F8,0x003AFEC8,0x0044DDF8,0x0044D920,0x0044E088,0x004AA1D8,0x004AA168,0x0044DE80,0x0044D988,0x003A5C68 --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(pit_controller_oracle tests/ui/pit_controller_oracle.c ${gen_pit_controller})
  target_link_libraries(pit_controller_oracle PRIVATE moon_ee ${MOON_CORE_STACK} moon_physics)
  target_compile_options(pit_controller_oracle PRIVATE $<$<C_COMPILER_ID:MSVC>:/fp:strict> $<$<NOT:$<C_COMPILER_ID:MSVC>>:-ffp-contract=off;-frounding-math>)
  set(MOON_TEST_PIT_STATE "" CACHE FILEPATH "User EE snapshot in a pit lane; local only")
  if(MOON_TEST_DISC AND MOON_TEST_PIT_STATE)
    add_test(NAME pit_controller COMMAND pit_controller_oracle ${MOON_TEST_DISC} ${CMAKE_SOURCE_DIR}/extracted/core.bin ${MOON_TEST_PIT_STATE})
    add_test(NAME pit_controller_negative COMMAND pit_controller_oracle ${MOON_TEST_DISC} ${CMAKE_SOURCE_DIR}/extracted/core.bin ${MOON_TEST_PIT_STATE} --negative)
    add_test(NAME pit_controller_camera_negative COMMAND pit_controller_oracle ${MOON_TEST_DISC} ${CMAKE_SOURCE_DIR}/extracted/core.bin ${MOON_TEST_PIT_STATE} --negative-camera)
    add_test(NAME pit_controller_owned_negative COMMAND pit_controller_oracle ${MOON_TEST_DISC} ${CMAKE_SOURCE_DIR}/extracted/core.bin ${MOON_TEST_PIT_STATE} --negative-owned)
    add_test(NAME pit_controller_model_negative COMMAND pit_controller_oracle ${MOON_TEST_DISC} ${CMAKE_SOURCE_DIR}/extracted/core.bin ${MOON_TEST_PIT_STATE} --negative-model)
    add_test(NAME pit_controller_refuel_negative COMMAND pit_controller_oracle ${MOON_TEST_DISC} ${CMAKE_SOURCE_DIR}/extracted/core.bin ${MOON_TEST_PIT_STATE} --negative-refuel)
    set_tests_properties(pit_controller_refuel_negative PROPERTIES WILL_FAIL TRUE)
    set_tests_properties(pit_controller_negative pit_controller_camera_negative pit_controller_owned_negative pit_controller_model_negative PROPERTIES WILL_FAIL TRUE)
  endif()
  if(MOON_TEST_DISC)
    add_test(NAME pit_motion COMMAND pit_motion_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin ${MOON_TEST_DISC})
    add_test(NAME pit_motion_negative COMMAND pit_motion_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin ${MOON_TEST_DISC} --negative)
    set_tests_properties(pit_motion_negative PROPERTIES WILL_FAIL TRUE)
    add_test(NAME pit_wheel_negative COMMAND pit_motion_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin ${MOON_TEST_DISC} --negative-wheel)
    set_tests_properties(pit_wheel_negative PROPERTIES WILL_FAIL TRUE)
  endif()
  if(MOON_TEST_DISC)
    add_test(NAME gtmode_license_menu_motion COMMAND gtmode_license_menu_motion_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin ${MOON_TEST_DISC})
    add_test(NAME gtmode_license_menu_motion_negative COMMAND gtmode_license_menu_motion_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin ${MOON_TEST_DISC})
    set_tests_properties(gtmode_license_menu_motion_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_MENU_MOTION_NEGATIVE=1")
    add_test(NAME gtmode_license_menu_curve_negative COMMAND gtmode_license_menu_motion_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin ${MOON_TEST_DISC})
    set_tests_properties(gtmode_license_menu_curve_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_MENU_CURVE_NEGATIVE=1")
  endif()
  if(MOON_TEST_DISC)
    add_test(NAME gtmode_license_motion COMMAND gtmode_license_motion_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin ${MOON_TEST_DISC})
    add_test(NAME gtmode_license_motion_negative COMMAND gtmode_license_motion_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin ${MOON_TEST_DISC})
    set_tests_properties(gtmode_license_motion_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_MOTION_NEGATIVE=1")
    add_test(NAME gtmode_license_curve_negative COMMAND gtmode_license_motion_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin ${MOON_TEST_DISC})
    set_tests_properties(gtmode_license_curve_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_CURVE_NEGATIVE=1")
  endif()
  set(gen_license_concourse "${CMAKE_BINARY_DIR}/gen/gtmode_license_concourse.c")
  set(gen_license_people "${CMAKE_BINARY_DIR}/gen/gtmode_license_people.c")
  add_custom_command(OUTPUT ${gen_license_people}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_license_people} --root 0x00408B08
      --exclude 0x0038DB30,0x0038DC18,0x0038DD08,0x0038DD68,0x0038E008,0x00344AA8,0x00344C08,0x0057D118,0x004081B8 --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_license_people_oracle tests/gtmode/license_people_run.c ${gen_license_people})
  target_link_libraries(gtmode_license_people_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  add_test(NAME gtmode_license_people COMMAND gtmode_license_people_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin)
  add_test(NAME gtmode_license_people_negative COMMAND gtmode_license_people_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin)
  set_tests_properties(gtmode_license_people_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_PEOPLE_NEGATIVE=1")
  set(gen_license_pose "${CMAKE_BINARY_DIR}/gen/gtmode_license_pose.c")
  add_custom_command(OUTPUT ${gen_license_pose}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_license_pose} --root 0x00427698
      --extra-roots 0x0042B440,0x0048A970
      --exclude 0x0057D8A0,0x0057F260 --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_license_pose_oracle tests/gtmode/license_pose_run.c ${gen_license_pose})
  target_link_libraries(gtmode_license_pose_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  add_executable(pit_pose_oracle tests/ui/pit_pose_oracle.c ${gen_license_pose})
  target_link_libraries(pit_pose_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  target_compile_options(pit_pose_oracle PRIVATE $<$<C_COMPILER_ID:MSVC>:/fp:strict> $<$<NOT:$<C_COMPILER_ID:MSVC>>:-ffp-contract=off;-frounding-math>)
  if(MOON_TEST_DISC)
    add_test(NAME pit_pose COMMAND pit_pose_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin ${MOON_TEST_DISC})
    add_test(NAME pit_pose_negative COMMAND pit_pose_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin ${MOON_TEST_DISC})
    set_tests_properties(pit_pose_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_POSE_NEGATIVE=1")
    add_test(NAME pit_scalar_negative COMMAND pit_pose_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin ${MOON_TEST_DISC})
    set_tests_properties(pit_scalar_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_PIT_SCALAR_NEGATIVE=1")
  endif()
  if(MSVC)
    target_compile_options(gtmode_license_pose_oracle PRIVATE /fp:strict)
  else()
    target_compile_options(gtmode_license_pose_oracle PRIVATE -ffp-contract=off -frounding-math)
  endif()
  if(MOON_TEST_DISC)
    add_test(NAME gtmode_license_pose COMMAND gtmode_license_pose_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin ${MOON_TEST_DISC})
    add_test(NAME gtmode_license_pose_negative COMMAND gtmode_license_pose_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin ${MOON_TEST_DISC})
    add_test(NAME gtmode_license_camera_negative COMMAND gtmode_license_pose_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin ${MOON_TEST_DISC})
    set_tests_properties(gtmode_license_pose_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_POSE_NEGATIVE=1")
    set_tests_properties(gtmode_license_camera_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_CAMERA_NEGATIVE=1")
  endif()
  add_custom_command(OUTPUT ${gen_license_concourse}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_license_concourse} --root 0x00409258
      --extra-roots 0x004096F8,0x00409860
      --exclude 0x00408268,0x00408A78,0x00408DB8,0x0034C190,0x00408EE0 --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_license_concourse_oracle tests/gtmode/license_concourse_run.c ${gen_license_concourse})
  target_link_libraries(gtmode_license_concourse_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  add_test(NAME gtmode_license_concourse COMMAND gtmode_license_concourse_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin)
  add_test(NAME gtmode_license_concourse_negative COMMAND gtmode_license_concourse_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin)
  set_tests_properties(gtmode_license_concourse_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_CONCOURSE_NEGATIVE=1")
  set(gen_license_image "${CMAKE_BINARY_DIR}/gen/gtmode_license_image.c")
  add_custom_command(OUTPUT ${gen_license_image}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_license_image} --root 0x003E0EE0:0x003E0EFC --stop 0x003E0EF4
      --extra-roots 0x004A8CB8,0x004A7D90,0x003EFDF0,0x00397FF8,0x00407FA0,0x003DFE20,0x003DF978,0x003E1C28,0x003E1C80,0x003E1E10,0x003E1EA0,0x004085C4,0x00409B68
      --exclude 0x00409A48,0x00396800,0x004A5ED8,0x0049FE28,0x004ABC90,0x004AA558,0x004AA6B8,0x004A81A8,0x004A8F78,0x0057D6C0,0x005F5040,0x00472BE0,0x0033DD88,0x003EFEA8,0x0033DE30,0x003E05A0,0x003E0708,0x004751D0,0x00475488,0x003B77E0,0x003EBF40,0x003DFAE0,0x00391DC8,0x00398758,0x003B76F0,0x003E18D0,0x003C0B88,0x003E1FB0,0x00407D28,0x004080B8,0x0040A1D0,0x00427820,0x00427830,0x00428AD8,0x00428C00,0x0042E478,0x004567E0,0x0049AB58,0x004A29A8,0x004A5348,0x004A53F8,0x004A5400,0x004A7698,0x004A7844,0x004A7890,0x004A79B0 --data-refs --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_license_image_oracle tests/gtmode/license_image_run.c ${gen_license_image})
  set(gen_race_lights "${CMAKE_BINARY_DIR}/gen/race_lights.c")
  add_custom_command(OUTPUT ${gen_race_lights}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_race_lights} --root 0x0038E7F8 --extra-roots 0x0038EFF8,0x0038F168
      --exclude 0x004523B0,0x004523E8,0x00452408,0x00396800,0x004A5ED8,0x0057D6C0,0x00397D48,0x00397E38,0x004A5CC0,0x004A5BF8,0x004A1638,0x004AB040 --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(race_lights_oracle tests/ui/race_lights_oracle.c ${gen_race_lights})
  target_link_libraries(race_lights_oracle PRIVATE moon_ee ${MOON_SDL_STACK})
  add_test(NAME race_lights_oracle COMMAND race_lights_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin)
  add_test(NAME race_lights_negative COMMAND race_lights_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin --negative)
  set_tests_properties(race_lights_negative PROPERTIES WILL_FAIL TRUE)
  if(MOON_TEST_DISC)
    add_test(NAME race_lights_disc COMMAND race_lights_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin --audit "${MOON_TEST_DISC}")
  endif()
  target_link_libraries(gtmode_license_image_oracle PRIVATE moon_ee moon_physics ${MOON_SDL_STACK} SDL2::SDL2main)
  add_test(NAME gtmode_license_image COMMAND gtmode_license_image_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin)
  foreach(part LUT MIX DECAY LIGHT CLOCK RESET SHADOW POINT)
    add_test(NAME gtmode_license_image_${part}_negative COMMAND gtmode_license_image_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin)
    set_tests_properties(gtmode_license_image_${part}_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_${part}_NEGATIVE=1")
  endforeach()
  set(gen_course_gadget "${CMAKE_BINARY_DIR}/gen/course_gadget.c")
  add_custom_command(OUTPUT ${gen_course_gadget}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_course_gadget} --root 0x003E24F0 --exclude 0x0057D8A0 --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(course_gadget_oracle tests/ui/course_gadget_run.c ${gen_course_gadget})
  target_link_libraries(course_gadget_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  if(MOON_TEST_DISC)
    add_test(NAME course_gadget_disc COMMAND course_gadget_oracle "${MOON_TEST_DISC}" ${CMAKE_SOURCE_DIR}/extracted/core.bin)
    foreach(part MATRIX SELECTION)
      add_test(NAME course_gadget_${part}_negative COMMAND course_gadget_oracle "${MOON_TEST_DISC}" ${CMAKE_SOURCE_DIR}/extracted/core.bin)
      set_tests_properties(course_gadget_${part}_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_GADGET_${part}_NEGATIVE=1")
    endforeach()
  endif()
  set(gen_license_camera "${CMAKE_BINARY_DIR}/gen/gtmode_license_camera.c")
  add_custom_command(OUTPUT ${gen_license_camera}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_license_camera} --root 0x0037E018:0x0037E054 --stop 0x0037E04C
      --extra-roots 0x003794A8,0x0033C7CC,0x0033FA40,0x003BC7DC,0x0037D5B0
      --exclude 0x005F5040,0x0057D380,0x00396AD0,0x003425E0 --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_license_camera_oracle tests/gtmode/license_camera_run.c ${gen_license_camera})
  target_link_libraries(gtmode_license_camera_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  add_test(NAME gtmode_license_lens COMMAND gtmode_license_camera_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin)
  add_test(NAME gtmode_license_lens_negative COMMAND gtmode_license_camera_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin)
  add_test(NAME gtmode_license_opening_control_negative COMMAND gtmode_license_camera_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin)
  set_tests_properties(gtmode_license_opening_control_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_OPENING_CONTROL_NEGATIVE=1")
  set_tests_properties(gtmode_license_lens_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_CAMERA_NEGATIVE=1")
  add_test(NAME gtmode_license_fade_negative COMMAND gtmode_license_camera_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin)
  set_tests_properties(gtmode_license_fade_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_FADE_NEGATIVE=1")
  add_test(NAME gtmode_license_fade_phase_negative COMMAND gtmode_license_camera_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin)
  set_tests_properties(gtmode_license_fade_phase_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_FADE_PHASE_NEGATIVE=1")
  add_test(NAME gtmode_license_fade_factor_negative COMMAND gtmode_license_camera_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin)
  set_tests_properties(gtmode_license_fade_factor_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_FADE_FACTOR_NEGATIVE=1")
  set(gen_license_presentation "${CMAKE_BINARY_DIR}/gen/gtmode_license_presentation.c")
  set(gen_license_menu_cycle "${CMAKE_BINARY_DIR}/gen/gtmode_license_menu_cycle.c")
  add_custom_command(OUTPUT ${gen_license_menu_cycle}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_license_menu_cycle} --root 0x0012B928 --extra-roots 0x0012BC18,0x0012B888
      --exclude 0x002638D8,0x005750C0,0x005788B8,0x00548228,0x00426A08,0x005C2560,0x0057F260,0x005C2630,0x00323D08,0x003166B8,0x002EFBF0,0x0023A538,0x002319A0,0x00323B60,0x005C11A8,0x00326798,0x002638E0,0x004A5418,0x004A5348,0x004A53F8,0x00576100,0x00576140,0x0012BFE0,0x0012C0C0,0x001056A0,0x004AB040,0x004A1638,0x004A2808,0x004AA1D8,0x00105818,0x004A6140,0x004A5688,0x004A5400,0x00387EF0 --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_license_menu_cycle_oracle tests/gtmode/license_menu_cycle_run.c ${gen_license_menu_cycle})
  target_link_libraries(gtmode_license_menu_cycle_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  add_test(NAME gtmode_license_menu_cycle COMMAND gtmode_license_menu_cycle_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin)
  add_test(NAME gtmode_license_menu_cycle_negative COMMAND gtmode_license_menu_cycle_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin)
  set_tests_properties(gtmode_license_menu_cycle_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_MENU_CYCLE_NEGATIVE=1")
  add_custom_command(OUTPUT ${gen_license_presentation}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_license_presentation} --root 0x003BD808
      --extra-roots 0x003E18D0,0x003E1A80,0x003AEC80,0x003AEC90,0x003AECA0,0x00498CA0
      --exclude 0x003A2360,0x00454B90 --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_license_presentation_oracle tests/gtmode/license_presentation_run.c ${gen_license_presentation})
  target_link_libraries(gtmode_license_presentation_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  add_test(NAME gtmode_license_presentation COMMAND gtmode_license_presentation_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin)
  add_test(NAME gtmode_license_presentation_negative COMMAND gtmode_license_presentation_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin)
  set_tests_properties(gtmode_license_presentation_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_PRESENTATION_NEGATIVE=1")
  add_test(NAME gtmode_license_clut_negative COMMAND gtmode_license_presentation_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin)
  set_tests_properties(gtmode_license_clut_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_CLUT_NEGATIVE=1")
  set(gen_training "${CMAKE_BINARY_DIR}/gen/gtmode_training.c")
  add_custom_command(OUTPUT ${gen_training}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_training} --root 0x0010FBD8
      --extra-roots 0x003D5AB8,0x003BB478,0x003ECC70,0x003F7050,0x003F7090,0x003F70A8,0x003F70D0,0x003F70D8,0x003F70E8,0x003F7078,0x003F7128,0x003F7058
      --exclude 0x004470E8,0x0038A3B8,0x0038A3D0,0x00114E38,0x0011CB38,0x003BB340,0x003D6268,0x003F1760,0x003C0E78,0x0035E8E8,0x0034CA20 --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_training_oracle tests/gtmode/training_run.c ${gen_training})
  target_link_libraries(gtmode_training_oracle PRIVATE moon_ee moon_physics ${MOON_CORE_STACK})
  if(EXISTS "${MOON_TEST_DISC}")
    add_test(NAME gtmode_training COMMAND gtmode_training_oracle "${MOON_TEST_DISC}")
    add_test(NAME gtmode_training_negative COMMAND gtmode_training_oracle "${MOON_TEST_DISC}")
    set_tests_properties(gtmode_training_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_TRAINING_NEGATIVE=1")
    add_test(NAME gtmode_training_getter_negative COMMAND gtmode_training_oracle "${MOON_TEST_DISC}")
    set_tests_properties(gtmode_training_getter_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_TRAINING_GETTER_NEGATIVE=1")
    add_test(NAME gtmode_training_start_negative COMMAND gtmode_training_oracle "${MOON_TEST_DISC}")
    set_tests_properties(gtmode_training_start_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_TRAINING_START_NEGATIVE=1")
  endif()
  set(gen_license_result "${CMAKE_BINARY_DIR}/gen/gtmode_license_result.c")
  set(gen_license_session "${CMAKE_BINARY_DIR}/gen/gtmode_license_session.c")
  add_custom_command(OUTPUT ${gen_license_session}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            --out ${gen_license_session} --root 0x003F8818
            --extra-roots 0x003F91F0,0x003FA248,0x003FA2B0,0x0035FE80,0x003F8AB0,0x003F8980,0x003F8D70,0x003F5FA0,0x0046B0A8,0x0046B9F0
            --exclude 0x00343D78,0x003454A8,0x0035DF80,0x0035E550,0x0035E078,0x0034C190,0x00359C10 --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  set(gen_mission_session "${CMAKE_BINARY_DIR}/gen/gtmode_mission_session.c")
  set(gen_mission_flow "${CMAKE_BINARY_DIR}/gen/gtmode_mission_flow.c")
  add_custom_command(OUTPUT ${gen_mission_flow}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_mission_flow} --root 0x003E05A0
      --extra-roots 0x00389638,0x003DF1B8,0x003DFE20,0x003E06F8
      --exclude 0x003DFCC8,0x004751D0,0x003DFE88 --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_mission_flow_oracle tests/gtmode/mission_flow_oracle.c ${gen_mission_flow})
  target_link_libraries(gtmode_mission_flow_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  add_test(NAME gtmode_mission_flow COMMAND gtmode_mission_flow_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  add_test(NAME gtmode_mission_flow_negative COMMAND gtmode_mission_flow_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  set_tests_properties(gtmode_mission_flow_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_MISSION_FLOW_NEGATIVE=1")
  set(gen_mission_exit "${CMAKE_BINARY_DIR}/gen/gtmode_mission_exit.c")
  add_custom_command(OUTPUT ${gen_mission_exit}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_mission_exit} --root 0x003ED188
      --extra-roots 0x0033C490,0x0033CCF0,0x00389E10,0x0038AAB0,0x00389ED8,0x0038A098
      --exclude 0x00389D98,0x003B70F0,0x00389758,0x003D8A60,0x00574F90,0x001012C8 --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_mission_exit_oracle tests/gtmode/mission_exit_oracle.c ${gen_mission_exit})
  target_link_libraries(gtmode_mission_exit_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  add_test(NAME gtmode_mission_exit COMMAND gtmode_mission_exit_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  add_test(NAME gtmode_mission_exit_negative COMMAND gtmode_mission_exit_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  set_tests_properties(gtmode_mission_exit_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_MISSION_EXIT_NEGATIVE=1")
  add_custom_command(OUTPUT ${gen_mission_session}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            --out ${gen_mission_session} --root 0x003F6E30
            --extra-roots 0x003F6E50,0x003F6EE8,0x003F6ED0,0x003F7010,0x003F7018,0x003F7020,0x003F7028,0x003F7030,0x003F7038,0x003F6E00,0x0034C250
            --exclude 0x0035DF80,0x0035E078,0x00345228,0x0034C190 --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_mission_session_oracle tests/gtmode/mission_session_run.c ${gen_mission_session})
  target_link_libraries(gtmode_mission_session_oracle PRIVATE moon_ee moon_physics ${MOON_CORE_STACK})
  if(EXISTS "${MOON_TEST_DISC}")
    add_test(NAME gtmode_mission_session COMMAND gtmode_mission_session_oracle "${MOON_TEST_DISC}")
    add_test(NAME gtmode_mission_session_negative COMMAND gtmode_mission_session_oracle "${MOON_TEST_DISC}")
    set_tests_properties(gtmode_mission_session_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_MISSION_SESSION_NEGATIVE=1")
    add_test(NAME gtmode_mission_disc COMMAND gtmode_mission_disc "${MOON_TEST_DISC}")
  endif()
  add_executable(gtmode_license_session_oracle tests/gtmode/license_session_run.c ${gen_license_session})
  target_link_libraries(gtmode_license_session_oracle PRIVATE moon_ee moon_physics ${MOON_CORE_STACK})
  add_executable(gtmode_license_lines_oracle tests/gtmode/license_lines_run.c ${gen_license_session})
  target_link_libraries(gtmode_license_lines_oracle PRIVATE moon_ee moon_physics ${MOON_CORE_STACK})
  if(EXISTS "${MOON_TEST_DISC}")
    add_test(NAME gtmode_license_lines COMMAND gtmode_license_lines_oracle "${MOON_TEST_DISC}")
    add_test(NAME gtmode_license_lines_negative COMMAND gtmode_license_lines_oracle "${MOON_TEST_DISC}")
    set_tests_properties(gtmode_license_lines_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_LINES_NEGATIVE=1")
    add_test(NAME gtmode_license_tape_evaluation COMMAND gtmode_license_session_oracle "${MOON_TEST_DISC}" --catalog-tapes)
    add_test(NAME gtmode_license_tape_evaluation_negative COMMAND gtmode_license_session_oracle "${MOON_TEST_DISC}" --catalog-tapes)
    set_tests_properties(gtmode_license_tape_evaluation_negative PROPERTIES
      WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_ACTUAL_NEGATIVE=1")
    add_test(NAME gtmode_license_actual_catalog COMMAND gtmode_license_session_oracle "${MOON_TEST_DISC}" --catalog-frames)
    add_test(NAME gtmode_license_actual_catalog_negative COMMAND gtmode_license_session_oracle "${MOON_TEST_DISC}" --catalog-frames)
    set_tests_properties(gtmode_license_actual_catalog_negative PROPERTIES
      WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_ACTUAL_NEGATIVE=1")
    add_test(NAME gtmode_license_session COMMAND gtmode_license_session_oracle "${MOON_TEST_DISC}")
    add_test(NAME gtmode_license_session_negative COMMAND gtmode_license_session_oracle "${MOON_TEST_DISC}")
    set_tests_properties(gtmode_license_session_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_SESSION_NEGATIVE=1")
  endif()
  add_custom_command(OUTPUT ${gen_license_result}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            --out ${gen_license_result} --root 0x001105A8 --extra-roots 0x00110800
            --exclude 0x00447280,0x004472A0,0x00430D20,0x00364760,0x00364868,0x0044AC20,0x004454C0,0x004482C0,0x00448398,0x00364E90,0x00364FA8,0x0010F718,0x004323D0,0x004320B8,0x003647A0,0x0042F2E0,0x0042F198 --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_license_result_oracle tests/gtmode/license_result_run.c ${gen_license_result})
  target_link_libraries(gtmode_license_result_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  add_test(NAME gtmode_license_result COMMAND gtmode_license_result_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  add_test(NAME gtmode_license_result_negative COMMAND gtmode_license_result_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  set_tests_properties(gtmode_license_result_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_RESULT_NEGATIVE=1")
  add_test(NAME gtmode_license_history_negative COMMAND gtmode_license_result_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  set_tests_properties(gtmode_license_history_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_HISTORY_NEGATIVE=1")
  set(gen_arcade_dirt "${CMAKE_BINARY_DIR}/gen/arcade_dirt.c")
  add_custom_command(OUTPUT ${gen_arcade_dirt}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            --out ${gen_arcade_dirt} --root 0x0012E498 --extra-roots 0x00448AD0
            --exclude 0x00312370,0x00314AD8,0x00312318,0x002FE278,0x003285A8,0x003285F8,0x002FC870,0x00447BB8,0x00447C28,0x00449298 --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(arcade_dirt_oracle tests/ui/arcade_dirt_oracle.c ${gen_arcade_dirt})
  target_link_libraries(arcade_dirt_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  set(gen_race_sound "${CMAKE_BINARY_DIR}/gen/race_sound.c")
  add_custom_command(OUTPUT ${gen_race_sound}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            --out ${gen_race_sound} --root 0x00558FD8
            --extra-roots 0x00558DB0,0x00558E28,0x00437B10,0x00437E98,0x00438170,0x004379F0,0x00438150,0x0033C180,0x0033C278,0x003C0F40,0x003C0FB0,0x003C04E8,0x00548060,0x00389B08,0x0033D510
            --exclude 0x0055D688,0x0055CFE8,0x00576100,0x00576140,0x0055D0B0,0x0055D6D0,0x00461E50,0x003B76F0,0x00578500,0x00578168,0x00373DA0,0x00391E50,0x003C0208,0x00391ED0,0x00394D80,0x00397CD8,0x0034DB30,0x003C0310,0x00426BC8,0x00387E88 --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(race_sound_oracle tests/ui/race_sound_oracle.c ${gen_race_sound})
  target_link_libraries(race_sound_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  set(gen_pregrid_paint "${CMAKE_BINARY_DIR}/gen/pregrid_paint.c")
  add_custom_command(OUTPUT ${gen_pregrid_paint}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            --out ${gen_pregrid_paint} --root 0x00445880 --extra-roots 0x00445AE8
            --exclude 0x00445808,0x00449210,0x004492D8 --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(pregrid_paint_oracle tests/ui/pregrid_paint_oracle.c ${gen_pregrid_paint})
  target_link_libraries(pregrid_paint_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  set(gen_race_car_sound "${CMAKE_BINARY_DIR}/gen/race_car_sound.c")
  add_custom_command(OUTPUT ${gen_race_car_sound}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            --out ${gen_race_car_sound} --root 0x003B5E80
            --extra-roots 0x00392640,0x00391CD8,0x00457F60,0x00373700,0x00373718,0x00373780,0x003737E8,0x00373850,0x003738C0,0x00373930,0x003739F0,0x00373AA0,0x00373C00,0x00373DD8,0x00379448,0x00379460,0x00379478,0x0037BA90,0x0037BB20,0x0037BB28,0x0037C9A0,0x0037C9B0,0x0037C9B8,0x00379320,0x00379338
            --exclude 0x00462A88,0x00392EB8,0x00392FC8,0x003924D0,0x004A7AA8,0x004A5348,0x004A7454,0x00372080,0x00579358 --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(race_car_sound_oracle tests/ui/race_car_sound_oracle.c ${gen_race_car_sound})
  target_link_libraries(race_car_sound_oracle PRIVATE moon_ee moon_race_sound ${MOON_CORE_STACK})
  set(gen_backfire "${CMAKE_BINARY_DIR}/gen/race_backfire.c")
  add_custom_command(OUTPUT ${gen_backfire}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py --out ${gen_backfire} --root 0x003D6D30 --extra-roots 0x00390898
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(race_backfire_oracle tests/ui/race_backfire_oracle.c ${gen_backfire})
  target_link_libraries(race_backfire_oracle PRIVATE moon_ee moon_race_sound ${MOON_CORE_STACK})
  add_test(NAME race_backfire COMMAND race_backfire_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin)
  add_test(NAME race_backfire_negative COMMAND race_backfire_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin --negative)
  set_tests_properties(race_backfire_negative PROPERTIES WILL_FAIL TRUE)
  set(gen_pregrid_text "${CMAKE_BINARY_DIR}/gen/pregrid_text.c")
  add_custom_command(OUTPUT ${gen_pregrid_text}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            --out ${gen_pregrid_text} --root 0x0048F790 --extra-roots 0x00492978,0x00579330,0x00246F58
            --exclude 0x00490B98,0x0057F260,0x005A609C,0x004924C0,0x00491F98,0x00491FC0,0x00492198,0x005C2560,0x005C2630 --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  set_source_files_properties(${gen_pregrid_text} PROPERTIES COMPILE_DEFINITIONS "ee_unrecompiled=pregrid_text_boundary")
  add_executable(pregrid_text_oracle tests/ui/pregrid_text_oracle.c ${gen_pregrid_text})
  target_link_libraries(pregrid_text_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  if(MOON_TEST_DISC)
    add_test(NAME hud_font_original COMMAND pregrid_text_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin "${MOON_TEST_DISC}")
    add_test(NAME hud_font_original_negative COMMAND pregrid_text_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin "${MOON_TEST_DISC}" --negative)
    set_tests_properties(hud_font_original hud_font_original_negative PROPERTIES ENVIRONMENT "GT4_HUD_FONT_ORACLE=1")
    set_tests_properties(hud_font_original_negative PROPERTIES WILL_FAIL TRUE)
  endif()
  set(gen_pregrid "${CMAKE_BINARY_DIR}/gen/pregrid.c")
  set(gen_tvcam "${CMAKE_BINARY_DIR}/gen/tvcam.c")
  add_custom_command(OUTPUT ${gen_tvcam}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_tvcam} --root 0x0037C138 --extra-roots 0x0037BF00,0x00578598,0x003852A0,0x00385370
      --exclude 0x00378CB8,0x00395400,0x005F5040,0x005F5230,0x005F5268,0x005F5010,0x0057D1F0,0x0057D008,0x0057D240,0x004A0690,0x004A53F8,0x004A7454,0x004A79D8,0x004A7988,0x004A79B0,0x004A7844,0x004A7550,0x004A5400,0x004A06F0,0x003FD260,0x004AC118 --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(tvcam_oracle tests/ui/tvcam_oracle.c ${gen_tvcam})
  target_link_libraries(tvcam_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  if(MSVC)
    target_compile_options(tvcam_oracle PRIVATE /fp:strict)
  else()
    target_compile_options(tvcam_oracle PRIVATE -ffp-contract=off -frounding-math)
  endif()
  add_test(NAME tvcam_original COMMAND tvcam_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin)
  add_test(NAME tvcam_original_negative COMMAND tvcam_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin)
  set_tests_properties(tvcam_original_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_TVCAM_ORACLE_NEGATIVE=1")
  add_test(NAME tvcam_operator_negative COMMAND tvcam_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin)
  set_tests_properties(tvcam_operator_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_TVCAM_OPERATOR_NEGATIVE=1")
  add_custom_command(OUTPUT ${gen_pregrid}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            --out ${gen_pregrid} --root 0x003DDBC8 --extra-roots 0x003FC9A0,0x003794A8,0x00385098
            --exclude 0x0057D380 --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s
    COMMENT "Preparing original pregrid camera scalar oracle")
  add_executable(pregrid_oracle tests/ui/pregrid_oracle.c ${gen_pregrid})
  target_link_libraries(pregrid_oracle PRIVATE moon_ee)
  # Original human constructor/matrix comparison is an independent diagnostic.
  set(gen_driver_pose "${CMAKE_BINARY_DIR}/gen/driver_pose.c")
  add_custom_command(OUTPUT ${gen_driver_pose}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            --out ${gen_driver_pose} --root 0x00419F80 --extra-roots 0x0041A988,0x00418A58,0x003B1DB0,0x00420510,0x00420AA8,0x0041B898,0x0041B9A8,0x0041E678,0x0041CB20,0x0041C6F8,0x0041D140,0x0041EFE0,0x0041F560,0x0041C1C8,0x00418F88,0x004218E0,0x00415D38,0x00415510,0x004154A0,0x00415648,0x00415240,0x00416B58,0x004173B0,0x0040FC78,0x00411628,0x0044F5A0,0x00412018,0x0040F850,0x0040F888,0x00413FC8,0x00414130,0x004104B8 --exclude 0x0040CA58 --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            ${CMAKE_SOURCE_DIR}/extracted/core.bin
            ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s
    COMMENT "Preparing standalone original driver pose oracle")
  # Supply only the declared oracle fixture boundaries; unknown calls still fail.
  set_source_files_properties(${gen_driver_pose} PROPERTIES COMPILE_DEFINITIONS "ee_unrecompiled=driver_oracle_boundary")
  add_executable(driver_pose_oracle tests/ui/driver_pose_oracle.c ${gen_driver_pose})
  target_link_libraries(driver_pose_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  set(gen_wheel_motion "${CMAKE_BINARY_DIR}/gen/wheel_motion.c")
  add_custom_command(OUTPUT ${gen_wheel_motion}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            --out ${gen_wheel_motion} --root 0x003D73A0 --extra-roots 0x00453E68,0x00458AB8
            --exclude 0x00454EF8,0x00107BE8,0x004568F0,0x00453BB0,0x00105930,0x0057D9C0,0x004552E8,0x00494F80,0x00495560,0x00494FC8,0x004550B0 --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s
    COMMENT "Preparing standalone original wheel motion oracle")
  add_executable(wheel_motion_oracle tests/ui/wheel_motion_oracle.c ${gen_wheel_motion})
  target_link_libraries(wheel_motion_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  if(EXISTS "${CMAKE_SOURCE_DIR}/extracted/core.bin")
    add_test(NAME wheel_motion_reference COMMAND wheel_motion_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin")
    add_test(NAME wheel_motion_reference_negative COMMAND wheel_motion_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin" --negative)
    set_tests_properties(wheel_motion_reference wheel_motion_reference_negative PROPERTIES RUN_SERIAL TRUE)
    set_tests_properties(wheel_motion_reference_negative PROPERTIES WILL_FAIL TRUE)
  endif()

  set(gen_wheel_order "${CMAKE_BINARY_DIR}/gen/wheel_order.c")
  add_custom_command(OUTPUT ${gen_wheel_order}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            --out ${gen_wheel_order} --root 0x00453028
            --exclude 0x004A53F8,0x00105250,0x00107AE8,0x00453A28,0x004519A8,0x0049B448,0x0049B478,0x004A79B0,0x004A7844,0x004A1638,0x004A7900,0x00451078,0x00455598,0x004568F0,0x00451020,0x004AB040,0x004519D8,0x00453A48,0x004A5400,0x00107B00,0x00105280 --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s
    COMMENT "Preparing standalone original wheel order oracle")
  add_executable(wheel_order_oracle tests/ui/wheel_order_oracle.c src/platform/sdl/car_model.c ${gen_wheel_order})
  target_include_directories(wheel_order_oracle PRIVATE src/platform/sdl)
  target_link_libraries(wheel_order_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  set(gen_wheel_accum "${CMAKE_BINARY_DIR}/gen/wheel_accum.c")
  add_custom_command(OUTPUT ${gen_wheel_accum}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            --out ${gen_wheel_accum} --root 0x00453BB0
            --exclude 0x004A5418,0x001055C0,0x00105440,0x00105450,0x00105528,0x001056A0,0x00499728,0x0049CC48,0x004AB040,0x004A29A8,0x004AA1D8,0x004A4550,0x004A5FE0,0x00105728,0x004A6668,0x004A4910,0x004A5688,0x004A1638,0x0057D8A0,0x0049B540 --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s
    COMMENT "Preparing standalone original wheel accumulation oracle")
  add_executable(wheel_accum_oracle tests/ui/wheel_accum_oracle.c ${gen_wheel_accum})
  target_link_libraries(wheel_accum_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  if(EXISTS "${CMAKE_SOURCE_DIR}/extracted/core.bin")
    add_test(NAME wheel_accum_reference COMMAND wheel_accum_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin")
    add_test(NAME wheel_accum_reference_negative COMMAND wheel_accum_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin" --negative)
    set_tests_properties(wheel_accum_reference wheel_accum_reference_negative PROPERTIES RUN_SERIAL TRUE)
    set_tests_properties(wheel_accum_reference_negative PROPERTIES WILL_FAIL TRUE)
  endif()

  # GT Mode oracle is an independent test executable; never linked into the app.
  set(gen_career "${CMAKE_BINARY_DIR}/gen/gtmode.c")
  add_custom_command(OUTPUT ${gen_career}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            --out ${gen_career} --root 0x0043afb0
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            ${CMAKE_SOURCE_DIR}/extracted/core.bin
            ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s
    COMMENT "Recompiling GT Mode credit oracle from the user's disc")
  add_executable(gtmode_oracle tests/gtmode/oracle_run.c ${gen_career})
  target_link_libraries(gtmode_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  set(gen_career_grade "${CMAKE_BINARY_DIR}/gen/gtmode_grade.c")
  add_custom_command(OUTPUT ${gen_career_grade}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            --out ${gen_career_grade} --root 0x00430c28
            --extra-roots 0x00430958,0x00430a10,0x00430a20,0x00430a58,0x00430bc0,0x00430c50,0x00430b58,0x00435348,0x00435920,0x00435860
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            ${CMAKE_SOURCE_DIR}/extracted/core.bin
            ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s
    COMMENT "Recompiling GT Mode record grade oracle from the user's disc")
  add_executable(gtmode_grade_oracle tests/gtmode/grade_run.c ${gen_career_grade})
  target_link_libraries(gtmode_grade_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  set(gen_career_calendar "${CMAKE_BINARY_DIR}/gen/gtmode_calendar.c")
  add_custom_command(OUTPUT ${gen_career_calendar}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            --out ${gen_career_calendar} --root 0x0042F530 --extra-roots 0x0042F0E0,0x00579898,0x00435148
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            ${CMAKE_SOURCE_DIR}/extracted/core.bin
            ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_calendar_oracle tests/gtmode/calendar_run.c ${gen_career_calendar})
  set(gen_weekday "${CMAKE_BINARY_DIR}/gen/gtmode_weekday.c")
  set(gen_home "${CMAKE_BINARY_DIR}/gen/gtmode_home.c")
  add_custom_command(OUTPUT ${gen_home}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --root 0x00468B58 --extra-roots 0x0043B018,0x0042F060,0x0042EA38,0x0042EAE8,0x0042EA58,0x0057F480,0x0057F360,0x00472C28
      --hooks --exclude 0x004683A8 --out ${gen_home}
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_home_oracle tests/gtmode/home_run.c ${gen_home})
  target_link_libraries(gtmode_home_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  add_test(NAME gtmode_home COMMAND gtmode_home_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  add_test(NAME gtmode_home_negative COMMAND gtmode_home_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  set_tests_properties(gtmode_home_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_HOME_NEGATIVE=1")
  set(gen_bspec_progress "${CMAKE_BINARY_DIR}/gen/bspec_progress.c")
  add_custom_command(OUTPUT ${gen_bspec_progress}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --root 0x00468710 --extra-roots 0x00468860
      --hooks --exclude 0x004683A8,0x00468690,0x00433C10,0x004336A0 --out ${gen_bspec_progress}
    DEPENDS tools/recomp/ee_recomp.py extracted/core.bin splat/asm/core/text.s)
  add_executable(bspec_progress_oracle tests/gtmode/bspec_progress.c ${gen_bspec_progress})
  target_link_libraries(bspec_progress_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  add_test(NAME bspec_progress COMMAND bspec_progress_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  add_test(NAME bspec_progress_negative COMMAND bspec_progress_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin" --negative)
  set_tests_properties(bspec_progress_negative PROPERTIES WILL_FAIL TRUE)
  set(gen_setting_defaults "${CMAKE_BINARY_DIR}/gen/setting_defaults.c")
  add_custom_command(OUTPUT ${gen_setting_defaults}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --root 0x00440268 --extra-roots 0x004404B8,0x004407B0,0x00440820,0x004403C0,0x004402F0,0x004406C8,0x00440430,0x004408D8
      --hooks --exclude 0x00443ED0 --out ${gen_setting_defaults}
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(setting_defaults_oracle tests/gtmode/setting_defaults_run.c ${gen_setting_defaults})
  target_link_libraries(setting_defaults_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  add_test(NAME setting_defaults_rows COMMAND setting_defaults_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  add_test(NAME setting_defaults_rows_negative COMMAND setting_defaults_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  set_tests_properties(setting_defaults_rows_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_SETTING_DEFAULT_ROW_NEGATIVE=1")
  add_custom_command(OUTPUT ${gen_weekday}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --root 0x00136270 --hooks --exclude 0x002FE278,0x002FC870,0x00309348,0x003285A8,0x003285F8 --out ${gen_weekday}
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_weekday_oracle tests/gtmode/weekday_run.c ${gen_weekday})
  target_link_libraries(gtmode_weekday_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  add_test(NAME gtmode_weekday COMMAND gtmode_weekday_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  add_test(NAME gtmode_weekday_negative COMMAND gtmode_weekday_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  set_tests_properties(gtmode_weekday_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_WEEKDAY_NEGATIVE=1")
  set(gen_ui_insensitive "${CMAKE_BINARY_DIR}/gen/ui_insensitive.c")
  add_custom_command(OUTPUT ${gen_ui_insensitive}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --root 0x002C9B68 --extra-roots 0x002C9740 --hooks
      --exclude 0x0057CE60,0x00206868,0x0025C300,0x002C9898 --out ${gen_ui_insensitive}
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(ui_insensitive_oracle tests/ui/option_insensitive_oracle.c ${gen_ui_insensitive})
  target_link_libraries(ui_insensitive_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  set(gen_ui_tooltip "${CMAKE_BINARY_DIR}/gen/ui_tooltip.c")
  add_custom_command(OUTPUT ${gen_ui_tooltip}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --root 0x0024A2F8 --exclude 0x002471E8,0x00490B38,0x0025B498 --out ${gen_ui_tooltip}
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(ui_tooltip_oracle tests/ui/tooltip_oracle.c ${gen_ui_tooltip})
  target_link_libraries(ui_tooltip_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  set(gen_ui_cursor "${CMAKE_BINARY_DIR}/gen/ui_cursor.c")
  set(gen_menu_repeat "${CMAKE_BINARY_DIR}/gen/menu_repeat.c")
  add_custom_command(OUTPUT ${gen_menu_repeat}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --root 0x002517E0 --out ${gen_menu_repeat}
    DEPENDS tools/recomp/ee_recomp.py extracted/core.bin splat/asm/core/text.s)
  add_executable(menu_repeat_oracle tests/ui/menu_repeat_oracle.c ${gen_menu_repeat})
  target_link_libraries(menu_repeat_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  add_test(NAME menu_repeat_oracle COMMAND menu_repeat_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  add_test(NAME menu_repeat_oracle_negative COMMAND menu_repeat_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin" --negative)
  set_tests_properties(menu_repeat_oracle_negative PROPERTIES WILL_FAIL TRUE)
  add_custom_command(OUTPUT ${gen_ui_cursor}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --root 0x00230384:0x0023055C --stop 0x00230558 --exclude 0x002A3A18 --out ${gen_ui_cursor}
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(ui_cursor_oracle tests/ui/cursor_oracle.c ${gen_ui_cursor})
  target_link_libraries(ui_cursor_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  set(gen_ui_transition "${CMAKE_BINARY_DIR}/gen/ui_transition.c")
  add_custom_command(OUTPUT ${gen_ui_transition}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --root 0x0028A61C:0x0028A654 --extra-roots 0x0027A1B8,0x0027AEE0,0x0028A6B0,0x00287348,0x004A2808,0x0024D328,0x0010AE40,0x00461068,0x00460FE0
      --hooks --stop 0x0028A650 --exclude 0x0055BEC0,0x0055BF18,0x0055C020 --out ${gen_ui_transition}
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(ui_transition_oracle tests/ui/transition_oracle.c ${gen_ui_transition})
  target_link_libraries(ui_transition_oracle PRIVATE moon_ee ${MOON_CORE_STACK})

  foreach(actor IN ITEMS move fade)
    if(actor STREQUAL "move")
      set(actor_root "0x002C1BB0:0x002C1C1C")
      set(actor_stop "0x002C1C18")
    else()
      set(actor_root "0x0020C568:0x0020C5CC")
      set(actor_stop "0x0020C5C8")
    endif()
    set(gen_actor "${CMAKE_BINARY_DIR}/gen/ui_${actor}.c")
    add_custom_command(OUTPUT ${gen_actor}
      COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
        --root ${actor_root} --stop ${actor_stop} --out ${gen_actor}
      DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
        ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
    add_executable(ui_${actor}_oracle tests/ui/popup_actor_oracle.c ${gen_actor})
    if(actor STREQUAL "fade")
      target_compile_definitions(ui_${actor}_oracle PRIVATE TEST_FADE=1)
    endif()
    target_link_libraries(ui_${actor}_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  endforeach()
  target_link_libraries(gtmode_calendar_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  add_executable(gtmode_record_oracle tests/gtmode/record_run.c ${gen_career_grade})
  target_link_libraries(gtmode_record_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  add_executable(gtmode_garage_oracle tests/gtmode/garage_run.c ${gen_career_grade})
  target_link_libraries(gtmode_garage_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  set(gen_career_spec "${CMAKE_BINARY_DIR}/gen/gtmode_spec.c")
  set(gen_career_spec_parts)
  foreach(part RANGE 0 7)
    list(APPEND gen_career_spec_parts "${CMAKE_BINARY_DIR}/gen/gtmode_spec_p${part}.c")
  endforeach()
  add_custom_command(OUTPUT ${gen_career_spec} ${gen_career_spec_parts}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            --out ${gen_career_spec} --parts 8 --root 0x00441128
            --extra-roots 0x004411f0,0x00431260,0x004312B0,0x004312d8,0x00431300,0x00447670,0x004476B0,0x004476F0,0x00447188,0x00431328,0x00431C60,0x005A2F88,0x00447598,0x00430990,0x004309B0,0x00431C38,0x00430A58,0x00430A20,0x00430A10,0x0044A048,0x0044A2E0,0x004488A0,0x00448968,0x00448908,0x00448928,0x0042F218,0x0042F1B8,0x004458C8,0x00435860,0x0043AFB0 --data-refs
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            ${CMAKE_SOURCE_DIR}/extracted/core.bin
            ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s
    COMMENT "Recompiling standalone GT Mode SpecDB diagnostic oracle")
  add_executable(gtmode_spec_probe tests/gtmode/spec_probe.c ${gen_career_spec} ${gen_career_spec_parts})
  target_link_libraries(gtmode_spec_probe PRIVATE moon_ee)
  add_executable(gtmode_price_oracle tests/gtmode/prices_run.c ${gen_career_spec} ${gen_career_spec_parts})
  target_link_libraries(gtmode_price_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  add_executable(gtmode_race_data_oracle tests/gtmode/race_data_run.c ${gen_career_spec} ${gen_career_spec_parts})
  target_link_libraries(gtmode_race_data_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  add_executable(gtmode_lookup_oracle tests/gtmode/lookup_run.c ${gen_career_spec} ${gen_career_spec_parts})
  target_link_libraries(gtmode_lookup_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  add_executable(gtmode_parts_oracle tests/gtmode/parts_run.c ${gen_career_spec} ${gen_career_spec_parts})
  target_link_libraries(gtmode_parts_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  add_executable(gtmode_sale_oracle tests/gtmode/sale_run.c ${gen_career_spec} ${gen_career_spec_parts})
  target_link_libraries(gtmode_sale_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  # Test-only purchase closure; never linked into the application.
  set(gen_career_setup "${CMAKE_BINARY_DIR}/gen/gtmode_active_setup.c")
  add_custom_command(OUTPUT ${gen_career_setup}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_career_setup} --root 0x00441248 --extra-roots 0x004452F8,0x0035E108,0x00367AF8,0x00367918,0x003BB5A8,0x00448C10 --data-refs --hooks
      --exclude 0x004473B0,0x00443ED0,0x004474B0,0x0040AA48
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_active_setup_oracle tests/gtmode/active_setup_run.c ${gen_career_setup})
  target_link_libraries(gtmode_active_setup_oracle PRIVATE moon_ee moon_physics ${MOON_CORE_STACK})
  add_executable(gtmode_enemy_indices_oracle tests/gtmode/enemy_indices_run.c ${gen_career_setup})
  target_link_libraries(gtmode_enemy_indices_oracle PRIVATE moon_ee moon_physics ${MOON_CORE_STACK})
  set(gen_career_paint "${CMAKE_BINARY_DIR}/gen/gtmode_paint.c")
  add_custom_command(OUTPUT ${gen_career_paint}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_career_paint} --root 0x0040AA48 --extra-roots 0x0040A6F0,0x00445828,0x00445AB8,0x004474B0,0x003BB5A8,0x003EE3C8,0x003BB820 --data-refs --hooks
      --exclude 0x004458C8,0x00445808,0x00448820,0x00449210,0x004492D8,0x00444690,0x00447400,0x00447458,0x004473B0,0x00448C10,0x00449D58,0x00449D78,0x00449E10,0x0044A048,0x004468F8,0x004462E0,0x00446320,0x004454C0,0x00443F00
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_paint_oracle tests/gtmode/paint_run.c ${gen_career_paint})
  target_link_libraries(gtmode_paint_oracle PRIVATE moon_ee moon_physics ${MOON_CORE_STACK})
  add_executable(gtmode_grid_paint_oracle tests/gtmode/grid_paint_run.c ${gen_career_paint})
  target_link_libraries(gtmode_grid_paint_oracle PRIVATE moon_ee moon_physics ${MOON_CORE_STACK})
  add_executable(gtmode_event_tyre_oracle tests/gtmode/event_tyre_run.c ${gen_career_paint})
  target_link_libraries(gtmode_event_tyre_oracle PRIVATE moon_ee moon_physics ${MOON_CORE_STACK})
  set(gen_career_event_result "${CMAKE_BINARY_DIR}/gen/gtmode_event_result.c")
  set(gen_career_start "${CMAKE_BINARY_DIR}/gen/gtmode_start.c")
  add_custom_command(OUTPUT ${gen_career_start}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_career_start} --root 0x003C0208:0x003C0310 --extra-roots 0x003C0190
      --exclude 0x003973D0 --data-refs --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_start_oracle tests/gtmode/start_run.c ${gen_career_start})
  target_link_libraries(gtmode_start_oracle PRIVATE moon_ee moon_physics ${MOON_CORE_STACK})
  set(gen_start_select "${CMAKE_BINARY_DIR}/gen/gtmode_start_select.c")
  add_custom_command(OUTPUT ${gen_start_select}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_start_select} --root 0x003883EC:0x0038843C --stop 0x00388438
      --extra-roots 0x003475B8,0x003F5528 --data-refs --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_start_select_oracle tests/gtmode/start_select_run.c ${gen_start_select})
  target_link_libraries(gtmode_start_select_oracle PRIVATE moon_ee moon_physics ${MOON_CORE_STACK})
  add_custom_command(OUTPUT ${gen_career_event_result}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_career_event_result} --root 0x00114498 --extra-roots 0x0043AFB0 --data-refs --hooks
      --exclude 0x00114390,0x003B6FD0,0x00447280,0x00430D20,0x0035E3F0,0x00110570,0x004475A8,0x003C0FF0,0x0042F448,0x0042F270,0x00110898,0x0057F238
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_event_result_oracle tests/gtmode/event_result_run.c ${gen_career_event_result})
  add_executable(gtmode_event_cycle_oracle tests/gtmode/event_cycle_run.c ${gen_career_event_result})
  target_link_libraries(gtmode_event_cycle_oracle PRIVATE moon_ee ${MOON_CORE_STACK} moon_physics)
  target_link_libraries(gtmode_event_result_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  add_executable(gtmode_event_payment_oracle tests/gtmode/event_payment_run.c ${gen_career_event_result})
  target_include_directories(gtmode_event_payment_oracle PRIVATE src/core/physics)
  target_link_libraries(gtmode_event_payment_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  set(gen_championship "${CMAKE_BINARY_DIR}/gen/gtmode_championship.c")
  set(gen_endurance_queries "${CMAKE_BINARY_DIR}/gen/gtmode_endurance_queries.c")
  add_custom_command(OUTPUT ${gen_endurance_queries}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_endurance_queries} --root 0x0035AF10 --extra-roots 0x0038B980
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_endurance_queries tests/gtmode/endurance_queries.c ${gen_endurance_queries})
  target_link_libraries(gtmode_endurance_queries PRIVATE moon_ee moon_physics ${MOON_CORE_STACK})
  add_test(NAME gtmode_endurance_queries COMMAND gtmode_endurance_queries "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  add_test(NAME gtmode_endurance_queries_negative COMMAND gtmode_endurance_queries "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  set_tests_properties(gtmode_endurance_queries_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_ENDURANCE_QUERY_NEGATIVE=1")
  add_custom_command(OUTPUT ${gen_championship}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_championship} --root 0x00119CB8 --extra-roots 0x003EF0E8,0x003EF088,0x00119BD8,0x0042FB48,0x0042FD00,0x0042FD78,0x0042FDF0,0x0042FE88,0x0042FF48,0x00119918,0x00114390,0x00127DF8,0x0043E2E8,0x00441350,0x004401C0,0x00449D58,0x00146628,0x0043F4C8,0x0044A250,0x0044A288,0x00446AE0,0x004455B0,0x00440B50,0x004404B8,0x00440820,0x004407B0,0x00440430,0x004406C8,0x0043B638,0x0043B690 --data-refs --hooks
      --exclude 0x00443F40,0x00443ED0,0x003B6FD0,0x00447280,0x00430D20,0x0035E3F0,0x00110570,0x004475A8,0x003C0FF0,0x0042F270,0x00110898,0x0011A168,0x0011A2D8,0x003BB440,0x004472C0,0x00445450,0x00445AE8,0x00447CA0,0x004472A0,0x00110498,0x00443240
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_championship_oracle tests/gtmode/championship_run.c ${gen_championship})
  target_link_libraries(gtmode_championship_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  set(gen_career_purchase "${CMAKE_BINARY_DIR}/gen/gtmode_purchase.c")
  set(gen_event_wear "${CMAKE_BINARY_DIR}/gen/gtmode_event_wear.c")
  add_custom_command(OUTPUT ${gen_event_wear}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_event_wear} --root 0x0010FA58 --hooks --exclude 0x00441248
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_event_wear_oracle tests/gtmode/event_wear_run.c ${gen_event_wear})
  target_link_libraries(gtmode_event_wear_oracle PRIVATE moon_ee ${MOON_CORE_STACK} moon_physics)
  if(EXISTS "${MOON_TEST_DISC}")
    add_test(NAME gtmode_event_wear COMMAND gtmode_event_wear_oracle "${MOON_TEST_DISC}" "${CMAKE_SOURCE_DIR}/extracted/core.bin")
    add_test(NAME gtmode_event_wear_negative COMMAND gtmode_event_wear_oracle "${MOON_TEST_DISC}" "${CMAKE_SOURCE_DIR}/extracted/core.bin")
    set_tests_properties(gtmode_event_wear_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_EVENT_WEAR_NEGATIVE=1")
    add_test(NAME gtmode_live_mileage_negative COMMAND gtmode_event_wear_oracle "${MOON_TEST_DISC}" "${CMAKE_SOURCE_DIR}/extracted/core.bin")
    set_tests_properties(gtmode_live_mileage_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LIVE_MILEAGE_NEGATIVE=1")
  endif()
  set(gen_event_distance "${CMAKE_BINARY_DIR}/gen/gtmode_event_distance.c")
  add_custom_command(OUTPUT ${gen_event_distance}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_event_distance} --root 0x00114044:0x0011407C --stop 0x00114078
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_event_distance_oracle tests/gtmode/event_distance_run.c ${gen_event_distance})
  target_link_libraries(gtmode_event_distance_oracle PRIVATE moon_ee ${MOON_CORE_STACK} moon_physics)
  set(gen_career_purchase_parts)
  foreach(part RANGE 0 7)
    list(APPEND gen_career_purchase_parts "${CMAKE_BINARY_DIR}/gen/gtmode_purchase_p${part}.c")
  endforeach()
  add_custom_command(OUTPUT ${gen_career_purchase} ${gen_career_purchase_parts}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            --out ${gen_career_purchase} --parts 8 --root 0x00435800 --extra-roots 0x0042F1B8,0x0043AFB0,0x00600B88,0x00435388,0x00435418,0x0042F270,0x0042F2E0,0x00439DE0,0x00439E00,0x00439E18,0x00431CB8 --data-refs --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            ${CMAKE_SOURCE_DIR}/extracted/core.bin
            ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_purchase_probe tests/gtmode/spec_probe.c ${gen_career_purchase} ${gen_career_purchase_parts})
  target_link_libraries(gtmode_purchase_probe PRIVATE moon_ee)
  add_executable(gtmode_purchase_menu tests/gtmode/purchase_menu.c)
  add_executable(gtmode_map_navigation tests/gtmode/map_navigation.c)
  target_link_libraries(gtmode_map_navigation PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
  add_test(NAME gtmode_map_navigation COMMAND gtmode_map_navigation "${MOON_TEST_DISC}")
  add_test(NAME gtmode_map_navigation_negative COMMAND gtmode_map_navigation "${MOON_TEST_DISC}" --negative)
  set_tests_properties(gtmode_map_navigation_negative PROPERTIES WILL_FAIL TRUE)
  add_executable(gtmode_gtauto_menu tests/gtmode/gtauto_menu.c)
  target_link_libraries(gtmode_gtauto_menu PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
  add_test(NAME gtmode_gtauto_menu COMMAND gtmode_gtauto_menu "${MOON_TEST_DISC}")
  add_test(NAME gtmode_gtauto_menu_negative COMMAND gtmode_gtauto_menu "${MOON_TEST_DISC}")
  set_tests_properties(gtmode_gtauto_menu_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_GTAUTO_TRANSACTION_NEGATIVE=1")
  add_test(NAME gtmode_gtauto_camera_negative COMMAND gtmode_gtauto_menu "${MOON_TEST_DISC}")
  set_tests_properties(gtmode_gtauto_camera_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_GTAUTO_CAMERA_NEGATIVE=1")
  target_link_libraries(gtmode_purchase_menu PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
  add_test(NAME gtmode_home_empty_car COMMAND gtmode_purchase_menu "${MOON_TEST_DISC}" --home-garage)
  add_test(NAME gtmode_home_empty_car_negative COMMAND gtmode_purchase_menu "${MOON_TEST_DISC}" --home-garage)
  set_tests_properties(gtmode_home_empty_car_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_EMPTY_HOME_NEGATIVE=1")
  add_executable(gtmode_purchase_oracle tests/gtmode/purchase_run.c ${gen_career_purchase} ${gen_career_purchase_parts})
  target_link_libraries(gtmode_purchase_oracle PRIVATE moon_career_vehicle moon_ee moon_physics ${MOON_CORE_STACK})
  add_executable(gtmode_tuned_oracle tests/gtmode/tuned_run.c ${gen_career_purchase} ${gen_career_purchase_parts})
  target_link_libraries(gtmode_tuned_oracle PRIVATE moon_career_vehicle moon_ee moon_physics ${MOON_CORE_STACK})
  # Statistics callback instruction spans; Adhoc boxing/cleanup is excluded.
  set(award_stat_spans "00163BFC:0x00163C14" "00163EC4:0x00163EDC")
  set(gen_award_stats)
  set(stat_index 0)
  foreach(span IN LISTS award_stat_spans)
    set(generated "${CMAKE_BINARY_DIR}/gen/gtmode_award_stat_${stat_index}.c")
    add_custom_command(OUTPUT ${generated}
      COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
        --out ${generated} --root "0x${span}" --exclude 0x0015F388
      DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
        ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
    set_source_files_properties(${generated} PROPERTIES COMPILE_DEFINITIONS "recomp_lookup=award_stat_lookup_${stat_index};recomp_entry=award_stat_entry_${stat_index};recomp_code_ranges=award_stat_ranges_${stat_index};recomp_n_code_ranges=award_stat_count_${stat_index}")
    list(APPEND gen_award_stats ${generated})
    math(EXPR stat_index "${stat_index}+1")
  endforeach()
  add_executable(gtmode_award_menu tests/gtmode/award_menu.c)
  target_link_libraries(gtmode_award_menu PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
  add_executable(gtmode_award_oracle tests/gtmode/award_run.c ${gen_career_purchase} ${gen_career_purchase_parts} ${gen_award_stats})
  target_link_libraries(gtmode_award_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  set(gen_service_values "${CMAKE_BINARY_DIR}/gen/gtmode_service_values.c")
  set(gen_service_sound_clock "${CMAKE_BINARY_DIR}/gen/service_sound_clock.c")
  add_custom_command(OUTPUT ${gen_service_sound_clock}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_service_sound_clock} --root 0x0055A858 --extra-roots 0x0055D420,0x0055D470,0x0055AB98
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(service_sound_clock tests/ui/service_sound_clock.c ${gen_service_sound_clock})
  target_link_libraries(service_sound_clock PRIVATE moon_ee ${MOON_CORE_STACK})
  add_test(NAME service_sound_clock COMMAND service_sound_clock "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  foreach(part RATE RAMP SCHEDULE)
    add_test(NAME service_sound_clock_${part}_negative COMMAND service_sound_clock "${CMAKE_SOURCE_DIR}/extracted/core.bin")
    set_tests_properties(service_sound_clock_${part}_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_SE_${part}_NEGATIVE=1")
  endforeach()
  set(gen_service_placement "${CMAKE_BINARY_DIR}/gen/gtmode_service_placement.c")
  add_custom_command(OUTPUT ${gen_service_placement}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_service_placement} --root 0x00156AA0 --extra-roots 0x0021B4F8,0x0021B528,0x00156950,0x00153BB8,0x004A2808,0x001060F8,0x00107328,0x004A2C58 --hooks
      --exclude 0x00364C40,0x00364D78,0x00364C60,0x00364F70,0x00364F80,0x004A53F8,0x004A5400,0x00156820,0x00156920,0x004A7844,0x004A79B0,0x004A7890,0x004A7550,0x004A7454,0x004A7A54,0x004A7774,0x0048EDD0,0x0057AF30,0x0057AFF0,0x0057AED8,0x004A22C0,0x004A0808,0x004A0910
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_service_placement tests/gtmode/service_placement.c ${gen_service_placement})
  target_link_libraries(gtmode_service_placement PRIVATE moon_ee ${MOON_CORE_STACK})
  add_test(NAME gtmode_service_placement COMMAND gtmode_service_placement "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  add_test(NAME gtmode_service_position_negative COMMAND gtmode_service_placement "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  set_tests_properties(gtmode_service_position_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_SERVICE_POSITION_NEGATIVE=1")
  add_test(NAME gtmode_service_placement_negative COMMAND gtmode_service_placement "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  set_tests_properties(gtmode_service_placement_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_SERVICE_PLACEMENT_NEGATIVE=1")
  add_test(NAME gtmode_service_lift_negative COMMAND gtmode_service_placement "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  set_tests_properties(gtmode_service_lift_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_SERVICE_LIFT_NEGATIVE=1")
  add_test(NAME gtmode_service_controller_negative COMMAND gtmode_service_placement "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  set_tests_properties(gtmode_service_controller_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_SERVICE_CONTROLLER_NEGATIVE=1")
  foreach(part POSE DRAW_CLOCK DROP BLEND PROJECTION JITTER)
    add_test(NAME gtmode_service_${part}_negative COMMAND gtmode_service_placement "${CMAKE_SOURCE_DIR}/extracted/core.bin")
    set_tests_properties(gtmode_service_${part}_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_SERVICE_${part}_NEGATIVE=1")
  endforeach()
  add_executable(gtmode_service_motion tests/gtmode/service_motion.c ${gen_license_pose})
  target_link_libraries(gtmode_service_motion PRIVATE moon_ee ${MOON_CORE_STACK})
  if(MSVC)
    target_compile_options(gtmode_service_motion PRIVATE /fp:strict)
  else()
    target_compile_options(gtmode_service_motion PRIVATE -ffp-contract=off -frounding-math)
  endif()
  if(MOON_TEST_DISC)
    add_test(NAME gtmode_service_motion COMMAND gtmode_service_motion ${CMAKE_SOURCE_DIR}/extracted/core.bin ${MOON_TEST_DISC})
    add_test(NAME gtmode_service_motion_negative COMMAND gtmode_service_motion ${CMAKE_SOURCE_DIR}/extracted/core.bin ${MOON_TEST_DISC})
    set_tests_properties(gtmode_service_motion_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_SERVICE_MOTION_NEGATIVE=1")
    add_test(NAME gtmode_service_channels_negative COMMAND gtmode_service_motion ${CMAKE_SOURCE_DIR}/extracted/core.bin ${MOON_TEST_DISC})
    set_tests_properties(gtmode_service_channels_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_SERVICE_CHANNELS_NEGATIVE=1")
  endif()
  add_custom_command(OUTPUT ${gen_service_values}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_service_values} --root 0x00343A80 --extra-roots 0x003438A0,0x003437F0,0x003660D8,0x003437B0
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_service_values tests/gtmode/service_values.c ${gen_service_values})
  target_link_libraries(gtmode_service_values PRIVATE moon_ee ${MOON_CORE_STACK})
  add_test(NAME gtmode_service_values COMMAND gtmode_service_values "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  foreach(part dirt oil setter)
    add_test(NAME gtmode_service_${part}_negative COMMAND gtmode_service_values "${CMAKE_SOURCE_DIR}/extracted/core.bin")
    set_tests_properties(gtmode_service_${part}_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_SERVICE_${part}_NEGATIVE=1")
  endforeach()
  set(gen_career_oil "${CMAKE_BINARY_DIR}/gen/gtmode_oil.c")
  add_custom_command(OUTPUT ${gen_career_oil}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            --out ${gen_career_oil} --root 0x00343928
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            ${CMAKE_SOURCE_DIR}/extracted/core.bin
            ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s
    COMMENT "Recompiling standalone GT Mode oil maintenance oracle")
  add_executable(gtmode_oil_oracle tests/gtmode/oil_run.c ${gen_career_oil})
  target_link_libraries(gtmode_oil_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  set(gen_career_rules "${CMAKE_BINARY_DIR}/gen/gtmode_rules.c")
  add_custom_command(OUTPUT ${gen_career_rules}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            --out ${gen_career_rules} --root 0x004475A8 --extra-roots 0x003434A8,0x00449190
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_rules_oracle tests/gtmode/rules_run.c ${gen_career_rules})
  target_link_libraries(gtmode_rules_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  # Original instruction spans for scalar MRaceData getters. VM boxing is
  # outside the comparison boundary; no generated code enters the app.
  set(race_field_spans
    "001A16F0:001A16F4" "001A1788:001A178C" "001A1820:001A1824"
    "001A18B8:001A18BC" "001A1950:001A1954" "001A19E4:001A19F8"
    "001A1A84:001A1A98" "001A1B34:001A1B48" "001A1BDC:001A1BF8"
    "001A2668:001A266C" "001A2E60:001A2E64" "001A2DC8:001A2DCC"
    "001A293C:001A2958" "001A2720:001A2724" "001A2784:001A2788" "001A139C:001A13A4")
  set(gen_race_fields)
  set(field_index 0)
  foreach(span IN LISTS race_field_spans)
    string(REPLACE ":" ";" bounds "${span}")
    list(GET bounds 0 begin)
    list(GET bounds 1 end)
    set(generated "${CMAKE_BINARY_DIR}/gen/gtmode_field_${field_index}.c")
    add_custom_command(OUTPUT ${generated}
      COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
        --out ${generated} --root "0x${begin}:0x${end}" --exclude 0x002FE278
      DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
        ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
    set_source_files_properties(${generated} PROPERTIES
      COMPILE_DEFINITIONS "recomp_lookup=gtmode_field_${field_index}_lookup;recomp_entry=gtmode_field_${field_index}_entry;recomp_code_ranges=gtmode_field_${field_index}_ranges;recomp_n_code_ranges=gtmode_field_${field_index}_n_ranges")
    list(APPEND gen_race_fields ${generated})
    math(EXPR field_index "${field_index}+1")
  endforeach()
  set(gen_money_compare "${CMAKE_BINARY_DIR}/gen/gtmode_money_compare.c")
  add_custom_command(OUTPUT ${gen_money_compare}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_money_compare} --root 0x001639F8:0x00163A08
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  set_source_files_properties(${gen_money_compare} PROPERTIES
    COMPILE_DEFINITIONS "recomp_lookup=gtmode_money_compare_lookup;recomp_entry=gtmode_money_compare_entry;recomp_code_ranges=gtmode_money_compare_ranges;recomp_n_code_ranges=gtmode_money_compare_n_ranges")
  add_executable(gtmode_field_oracle tests/gtmode/fields_run.c ${gen_race_fields} ${gen_money_compare})
  target_link_libraries(gtmode_field_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  set(unit_attribute_spans "004309E8:00430A04" "001A4D88:001A4DA4" "001A4FA0:001A4FBC" "001A51B8:001A51D8")
  set(gen_unit_attributes)
  set(attribute_index 0)
  foreach(span IN LISTS unit_attribute_spans)
    string(REPLACE ":" ";" bounds "${span}")
    list(GET bounds 0 begin)
    list(GET bounds 1 end)
    set(generated "${CMAKE_BINARY_DIR}/gen/gtmode_attribute_${attribute_index}.c")
    add_custom_command(OUTPUT ${generated}
      COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
        --out ${generated} --root "0x${begin}:0x${end}"
      DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
        ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
    set_source_files_properties(${generated} PROPERTIES
      COMPILE_DEFINITIONS "recomp_lookup=gtmode_attribute_${attribute_index}_lookup;recomp_entry=gtmode_attribute_${attribute_index}_entry;recomp_code_ranges=gtmode_attribute_${attribute_index}_ranges;recomp_n_code_ranges=gtmode_attribute_${attribute_index}_n_ranges")
    list(APPEND gen_unit_attributes ${generated})
    math(EXPR attribute_index "${attribute_index}+1")
  endforeach()
  add_executable(gtmode_unit_oracle tests/gtmode/unit_run.c ${gen_unit_attributes} ${gen_career_spec} ${gen_career_spec_parts})
  target_link_libraries(gtmode_unit_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  set(gen_career_service "${CMAKE_BINARY_DIR}/gen/gtmode_service.c")
  add_custom_command(OUTPUT ${gen_career_service}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            --out ${gen_career_service} --root 0x00343928 --extra-roots 0x0043AFB0,0x00435860
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_service_oracle tests/gtmode/service_run.c ${gen_career_service})
  target_link_libraries(gtmode_service_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  set(gen_synthetic "${CMAKE_BINARY_DIR}/gen/recomp_synthetic.c")
  add_custom_command(OUTPUT ${gen_synthetic}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tests/recomp/generate_test.py ${gen_synthetic}
    DEPENDS ${CMAKE_SOURCE_DIR}/tests/recomp/generate_test.py
            ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py)
  add_executable(ee_recomp_tests ${gen_synthetic})
  target_link_libraries(ee_recomp_tests PRIVATE moon_ee)
  add_test(NAME ee_recomp COMMAND ee_recomp_tests)
  set(core_bin "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  set(gen_initsys "${CMAKE_BINARY_DIR}/gen/initsys.c")
  add_custom_command(
    OUTPUT ${gen_initsys}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            --out ${gen_initsys}
            --root 0x00100008:0x00100220 --stop 0x001001f0
            --exclude 0x0048ef90,0x00107f08,0x005a3140,0x005b7960
            --data-refs
            # SetSyscall handlers listed in the data table at 0x00658368
            # and in the kernel patch tables at 0x006592b0 and 0x00659698
            --extra-roots 0x005b73c8,0x005b7390,0x005b79f8,0x005b98d0
            # libkernl copies these into kernel memory (func_005B7A40, func_005B9928)
            --blob 0x00658b48:0x80076000:0x740
            --blob 0x00659318:0x80075000:0x330
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${core_bin}
            ${CMAKE_SOURCE_DIR}/splat/symbol_addrs.txt
            ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s
    COMMENT "Recompiling crt0 + _InitSys from extracted/core.bin")
  add_executable(initsys_run tests/initsys/main.c ${gen_initsys})
  target_link_libraries(initsys_run PRIVATE moon_ee)
  if(MSVC)
    # Generated code is large and flat; keep warnings from drowning the rest.
    set_source_files_properties(${gen_initsys} PROPERTIES COMPILE_OPTIONS "/W1")
  endif()
  set(gen_init "${CMAKE_BINARY_DIR}/gen/init.c")
  add_custom_command(
    OUTPUT ${gen_init}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            --out ${gen_init}
            --root 0x00100008:0x00100220 --stop 0x00100204
            --exclude 0x00107f08,0x005a3140,0x005b7960
            --data-refs
            # SetSyscall handlers listed in the data table at 0x00658368
            # and in the kernel patch tables at 0x006592b0 and 0x00659698
            --extra-roots 0x005b73c8,0x005b7390,0x005b79f8,0x005b98d0
            # libkernl copies these into kernel memory (func_005B7A40, func_005B9928)
            --blob 0x00658b48:0x80076000:0x740
            --blob 0x00659318:0x80075000:0x330
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${core_bin}
            ${CMAKE_SOURCE_DIR}/splat/symbol_addrs.txt
            ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s
    COMMENT "Recompiling crt0 + _InitSys + _init from extracted/core.bin")
  add_executable(init_run tests/initsys/main.c ${gen_init})
  target_link_libraries(init_run PRIVATE moon_ee)
  target_compile_definitions(init_run PRIVATE MOON_INIT_CHUNK=1)
  if(MSVC)
    # Generated code is large and flat; keep warnings from drowning the rest.
    set_source_files_properties(${gen_init} PROPERTIES COMPILE_OPTIONS "/W1")
  endif()
  # From crt0 through main with no stop: runs until the first missing piece.
  # Roots also come from every function pointer in the data record (constructor
  # lists, vtables, callback tables). Split into 16 parts to compile in parallel.
  set(gen_main "${CMAKE_BINARY_DIR}/gen/main.c")
  set(gen_main_parts "")
  foreach(k RANGE 15)
    list(APPEND gen_main_parts "${CMAKE_BINARY_DIR}/gen/main_p${k}.c")
  endforeach()
  add_custom_command(
    OUTPUT ${gen_main} ${gen_main_parts}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            --out ${gen_main} --parts 16
            --root 0x00100008:0x00100220
            --exclude 0x005a3140,0x005b7960
            --data-refs
            --extra-roots 0x005b73c8,0x005b7390,0x005b79f8,0x005b98d0
            --blob 0x00658b48:0x80076000:0x740
            --blob 0x00659318:0x80075000:0x330
            --table-roots 0x00616f24:0x006d5dfc
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${core_bin}
            ${CMAKE_SOURCE_DIR}/splat/symbol_addrs.txt
            ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s
    COMMENT "Recompiling crt0 through main from extracted/core.bin")
  add_executable(main_run tests/initsys/main.c ${gen_main} ${gen_main_parts})
  target_link_libraries(main_run PRIVATE moon_ee)
  target_compile_definitions(main_run PRIVATE MOON_INIT_CHUNK=1 MOON_MAIN_CHUNK=1)
  if(MSVC)
    set_source_files_properties(${gen_main} ${gen_main_parts} PROPERTIES COMPILE_OPTIONS "/W1;/bigobj")
    target_compile_options(main_run PRIVATE /MP)
  endif()
  # The original vehicle simulation step run on a RAM snapshot: the oracle
  # for the physics rewrite (docs/analysis/PHYSICS.md). The step's closure
  # and what the data tables point at are recompiled with hooks, so that
  # rewritten functions can replace the original ones one at a time.
  set(gen_phys "${CMAKE_BINARY_DIR}/gen/physics.c")
  set(gen_phys_parts "")
  foreach(k RANGE 15)
    list(APPEND gen_phys_parts "${CMAKE_BINARY_DIR}/gen/physics_p${k}.c")
  endforeach()
  add_custom_command(
    OUTPUT ${gen_phys} ${gen_phys_parts}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            --out ${gen_phys} --parts 16 --hooks --root 0x0034db30 --data-refs
            # the car setup (tests/physics/setup_run.c)
            --extra-roots 0x003f3b68,0x00444480,0x0046b0a8,0x0046b9f0,0x003e5818,0x0034d490
            # virtual calls: every function pointer in the data record
            --table-roots 0x00616f24:0x006d5dfc
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${core_bin}
            ${CMAKE_SOURCE_DIR}/splat/symbol_addrs.txt
            ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s
    COMMENT "Recompiling the vehicle simulation step from extracted/core.bin")
  add_library(moon_recomp_physics STATIC ${gen_phys} ${gen_phys_parts} tests/physics/natives.c tests/physics/replay_arithmetic.c)
  target_link_libraries(moon_recomp_physics PUBLIC moon_ee moon_physics ${MOON_CORE_STACK})
  target_include_directories(moon_recomp_physics PUBLIC tests/physics)
  if(MSVC)
    set_source_files_properties(${gen_phys} ${gen_phys_parts} PROPERTIES COMPILE_OPTIONS "/W1;/bigobj")
    target_compile_options(moon_recomp_physics PRIVATE /MP)
  endif()
  if(MOON_TEST_TRAINING_STATE)
    get_filename_component(training_state_dir "${MOON_TEST_TRAINING_STATE}" DIRECTORY)
    add_test(NAME gtmode_training_step COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tests/gtmode/training_step.py
      $<TARGET_FILE:physics_run> ${MOON_TEST_TRAINING_STATE} ${training_state_dir}/Scratchpad.bin ${CMAKE_BINARY_DIR}/training-step)
    add_test(NAME gtmode_training_step_negative COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tests/gtmode/training_step.py
      $<TARGET_FILE:physics_run> ${MOON_TEST_TRAINING_STATE} ${training_state_dir}/Scratchpad.bin ${CMAKE_BINARY_DIR}/training-step-negative)
    set_tests_properties(gtmode_training_step_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_TRAINING_STEP_NEGATIVE=1")
  endif()
  add_executable(physics_run tests/physics/step_run.c tests/physics/race_setup.c tests/physics/race_build.c)
  target_link_libraries(physics_run PRIVATE moon_recomp_physics)
  add_executable(surface_bumps_reference tests/physics/surface_bumps_reference.c)
  target_link_libraries(surface_bumps_reference PRIVATE moon_recomp_physics)
  file(GLOB surface_reference_candidates "${CMAKE_SOURCE_DIR}/captures/physics/*.eeMemory.bin")
  set(surface_references "")
  foreach(reference IN LISTS surface_reference_candidates)
    get_filename_component(reference_name "${reference}" NAME)
    if(reference_name MATCHES "^(ar_|rp_|rv_|nur_)")
      list(APPEND surface_references "${reference}")
    endif()
  endforeach()
  if(surface_references)
    add_test(NAME physics_surface_waves COMMAND surface_bumps_reference ${surface_references})
    add_test(NAME physics_surface_waves_negative COMMAND surface_bumps_reference --negative ${surface_references})
    set_tests_properties(physics_surface_waves_negative PROPERTIES WILL_FAIL TRUE)
  endif()

  add_executable(gtmode_license_trajectory_oracle tests/gtmode/license_trajectory_run.c)
  target_link_libraries(gtmode_license_trajectory_oracle PRIVATE moon_recomp_physics)
  add_executable(gtmode_license_constructor_oracle tests/gtmode/license_constructor_run.c)
  target_link_libraries(gtmode_license_constructor_oracle PRIVATE moon_recomp_physics)
  add_executable(gtmode_mission_search tests/gtmode/mission_search_run.c)
  target_link_libraries(gtmode_mission_search PRIVATE moon_physics moon_core moon_platform_common)
  if(MOON_TEST_DISC)
    add_test(NAME gtmode_license_trajectory_b1 COMMAND gtmode_license_trajectory_oracle "${MOON_TEST_DISC}" l0b0001)
    add_test(NAME gtmode_license_trajectory_negative COMMAND gtmode_license_trajectory_oracle "${MOON_TEST_DISC}" l0b0001)
    set_tests_properties(gtmode_license_trajectory_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_TRAJECTORY_NEGATIVE=1")
    add_test(NAME gtmode_license_trajectory_catalog COMMAND gtmode_license_trajectory_oracle "${MOON_TEST_DISC}" --all)
    set_tests_properties(gtmode_license_trajectory_catalog PROPERTIES TIMEOUT 3600)
    add_test(NAME gtmode_mission_search_cycle_02 COMMAND ${MOON_PYTHON}
      ${CMAKE_SOURCE_DIR}/tests/gtmode/mission_search_cycle.py "${MOON_TEST_DISC}"
      $<TARGET_FILE:gtmode_mission_search> $<TARGET_FILE:gtmode_mission_cycle> l0m0002)
    add_test(NAME gtmode_mission_search_negative COMMAND ${MOON_PYTHON}
      ${CMAKE_SOURCE_DIR}/tests/gtmode/mission_search_cycle.py "${MOON_TEST_DISC}"
      $<TARGET_FILE:gtmode_mission_search> $<TARGET_FILE:gtmode_mission_cycle> l0m0002 --negative)
    set_tests_properties(gtmode_mission_search_cycle_02 gtmode_mission_search_negative PROPERTIES
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 2400)
    add_test(NAME gtmode_mission_search_cycle_06 COMMAND ${MOON_PYTHON}
      ${CMAKE_SOURCE_DIR}/tests/gtmode/mission_search_cycle.py "${MOON_TEST_DISC}"
      $<TARGET_FILE:gtmode_mission_search> $<TARGET_FILE:gtmode_mission_cycle> l0m0006)
    add_test(NAME gtmode_mission_search_negative_06 COMMAND ${MOON_PYTHON}
      ${CMAKE_SOURCE_DIR}/tests/gtmode/mission_search_cycle.py "${MOON_TEST_DISC}"
      $<TARGET_FILE:gtmode_mission_search> $<TARGET_FILE:gtmode_mission_cycle> l0m0006 --negative)
    set_tests_properties(gtmode_mission_search_cycle_06 gtmode_mission_search_negative_06 PROPERTIES
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 2400)
  endif()
  add_executable(physics_controller_event tests/physics/controller_event.c tests/physics/race_setup.c tests/physics/race_build.c)
  target_link_libraries(physics_controller_event PRIVATE moon_recomp_physics)
  if(WIN32)
    target_link_libraries(physics_run PRIVATE bcrypt)
    target_link_libraries(physics_controller_event PRIVATE bcrypt)
  endif()
  set(MOON_TEST_ENDURANCE_STALL_STATE "" CACHE FILEPATH "Local native GT40 endurance trace; never committed")
  set(MOON_TEST_ENDURANCE_APPROACH_STATE "" CACHE FILEPATH "Local moving GT40 checkpoint before the barrier; never committed")
  if(MOON_TEST_ENDURANCE_APPROACH_STATE AND MOON_TEST_DISC)
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/controller-event ${CMAKE_BINARY_DIR}/controller-event-negative)
    add_test(NAME physics_controller_event COMMAND physics_controller_event ${MOON_TEST_ENDURANCE_APPROACH_STATE}
      - 0x1cf8ed0 1 ${CMAKE_BINARY_DIR}/controller-event all)
    add_test(NAME physics_controller_event_negative COMMAND physics_controller_event ${MOON_TEST_ENDURANCE_APPROACH_STATE}
      - 0x1cf8ed0 1 ${CMAKE_BINARY_DIR}/controller-event-negative all)
    set_tests_properties(physics_controller_event PROPERTIES ENVIRONMENT "PHYS_AB=event:200")
    set_tests_properties(physics_controller_event_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "PHYS_AB=event:200;GT4_CONTROLLER_PACE_NEGATIVE=1")
    add_test(NAME physics_controller_kind COMMAND physics_controller_event ${MOON_TEST_ENDURANCE_APPROACH_STATE}
      - 0x1cf8ed0 1 ${CMAKE_BINARY_DIR}/controller-event all)
    add_test(NAME physics_controller_kind_negative COMMAND physics_controller_event ${MOON_TEST_ENDURANCE_APPROACH_STATE}
      - 0x1cf8ed0 1 ${CMAKE_BINARY_DIR}/controller-event-negative all)
    set_tests_properties(physics_controller_kind PROPERTIES ENVIRONMENT "PHYS_AB=kind:256")
    set_tests_properties(physics_controller_kind_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "PHYS_AB=kind:256;GT4_CONTROLLER_PACE_NEGATIVE=1")
    add_test(NAME gtmode_endurance_trajectory COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tests/gtmode/endurance_trajectory.py
      $<TARGET_FILE:physics_run> ${MOON_TEST_ENDURANCE_APPROACH_STATE} ${CMAKE_BINARY_DIR}/endurance-trajectory
      --production $<TARGET_FILE:gtmode_trajectory_native> --disc ${MOON_TEST_DISC})
    add_test(NAME gtmode_endurance_trajectory_negative COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tests/gtmode/endurance_trajectory.py
      $<TARGET_FILE:physics_run> ${MOON_TEST_ENDURANCE_APPROACH_STATE} ${CMAKE_BINARY_DIR}/endurance-trajectory-negative)
    set_tests_properties(gtmode_endurance_trajectory_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_ENDURANCE_TRAJECTORY_NEGATIVE=1")
    if(WIN32)
      add_test(NAME gtmode_endurance_digest COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tests/gtmode/endurance_trajectory.py
        $<TARGET_FILE:physics_run> ${MOON_TEST_ENDURANCE_APPROACH_STATE} ${CMAKE_BINARY_DIR}/endurance-digest --digest
        --production $<TARGET_FILE:gtmode_trajectory_native> --disc ${MOON_TEST_DISC})
      add_test(NAME gtmode_endurance_digest_bytes COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tests/gtmode/trajectory_digest.py
        ${CMAKE_BINARY_DIR}/endurance-trajectory/original/cars.bin ${CMAKE_BINARY_DIR}/endurance-digest/original/cars.bin)
      add_test(NAME gtmode_endurance_digest_bytes_negative COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tests/gtmode/trajectory_digest.py
        ${CMAKE_BINARY_DIR}/endurance-trajectory/original/cars.bin ${CMAKE_BINARY_DIR}/endurance-digest/original/cars.bin)
      set_tests_properties(gtmode_endurance_digest_bytes gtmode_endurance_digest_bytes_negative PROPERTIES DEPENDS "gtmode_endurance_trajectory;gtmode_endurance_digest")
      set_tests_properties(gtmode_endurance_digest_bytes_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_STATE_DIGEST_NEGATIVE=1")
    endif()
  endif()
  if(MOON_TEST_ENDURANCE_STALL_STATE)
    add_test(NAME gtmode_endurance_stall_step COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tests/gtmode/endurance_stall_step.py
      $<TARGET_FILE:physics_run> ${MOON_TEST_ENDURANCE_STALL_STATE} ${CMAKE_BINARY_DIR}/endurance-stall-step)
    add_test(NAME gtmode_endurance_stall_step_negative COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tests/gtmode/endurance_stall_step.py
      $<TARGET_FILE:physics_run> ${MOON_TEST_ENDURANCE_STALL_STATE} ${CMAKE_BINARY_DIR}/endurance-stall-step-negative)
    set_tests_properties(gtmode_endurance_stall_step_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_ENDURANCE_STALL_NEGATIVE=1")
  endif()
  add_executable(setup_run tests/physics/setup_run.c)
  target_link_libraries(setup_run PRIVATE moon_recomp_physics)
  # Transitional: drive the car of a savestate with the game's physics.
  add_executable(moon_drive tests/physics/drive.c tests/physics/race_setup.c tests/physics/race_build.c)
  target_link_libraries(moon_drive PRIVATE moon_recomp_physics ${MOON_SDL_STACK} miniz SDL2::SDL2main)
  # a race run natively, no recompiled code (src/core/physics/race_run.c)
  add_executable(race_run_test tests/physics/race_run_test.c)
  target_link_libraries(race_run_test PRIVATE moon_physics ${MOON_CORE_STACK})
  target_include_directories(race_run_test PRIVATE src/core/physics tests/physics)
endif()

# The Logger's graph and cursor on synthetic tracks (src/core/logger_view.c)
add_executable(logger_view_tests tests/ui/logger_view_test.c)
target_link_libraries(logger_view_tests PRIVATE ${MOON_CORE_STACK})
add_test(NAME logger_view COMMAND logger_view_tests)
add_test(NAME logger_view_negative COMMAND logger_view_tests --negative)
set_tests_properties(logger_view_negative PROPERTIES WILL_FAIL TRUE)

add_executable(menu_music_transport_test tests/ui/menu_music_transport.c)
target_include_directories(menu_music_transport_test PRIVATE src/app src/platform/sdl)
target_link_libraries(menu_music_transport_test PRIVATE ${MOON_CORE_STACK} SDL2::SDL2)
if(MOON_TEST_DISC)
  add_test(NAME menu_music_transport COMMAND menu_music_transport_test "${MOON_TEST_DISC}")
  add_test(NAME menu_music_transport_negative COMMAND menu_music_transport_test "${MOON_TEST_DISC}" --defer-midi)
  add_test(NAME menu_music_preroll_negative COMMAND menu_music_transport_test "${MOON_TEST_DISC}" --short-prefix)
  set_tests_properties(menu_music_transport_negative PROPERTIES WILL_FAIL TRUE)
  set_tests_properties(menu_music_preroll_negative PROPERTIES WILL_FAIL TRUE)
endif()

add_executable(audio_backend_tests tests/ui/test_audio_backend.c)
target_link_libraries(audio_backend_tests PRIVATE ${MOON_SDL_STACK})
add_test(NAME audio_backend COMMAND audio_backend_tests)
add_test(NAME audio_backend_fade_negative COMMAND audio_backend_tests)
set_tests_properties(audio_backend_fade_negative PROPERTIES WILL_FAIL TRUE
  ENVIRONMENT "GT4_AUDIO_FADE_NEGATIVE=1")
add_executable(native_audio_capture_test tests/ui/native_audio_capture.c)
add_executable(native_video_capture_test tests/ui/native_video_capture.c)
target_link_libraries(native_video_capture_test PRIVATE ${MOON_SDL_STACK})
add_test(NAME native_video_capture COMMAND native_video_capture_test "${CMAKE_BINARY_DIR}/native-video-capture.rgba")
add_test(NAME native_video_capture_negative COMMAND native_video_capture_test "${CMAKE_BINARY_DIR}/native-video-negative.rgba" --negative)
set_tests_properties(native_video_capture_negative PROPERTIES WILL_FAIL TRUE)
target_link_libraries(native_audio_capture_test PRIVATE ${MOON_SDL_STACK})
add_test(NAME native_audio_capture COMMAND native_audio_capture_test "${CMAKE_BINARY_DIR}/native-audio-capture.bin")
add_test(NAME native_audio_capture_negative COMMAND native_audio_capture_test "${CMAKE_BINARY_DIR}/native-audio-negative.bin" --negative)
set_tests_properties(native_audio_capture_negative PROPERTIES WILL_FAIL TRUE)

add_executable(gtmode_game_tests tests/ui/test_gtmode_game.c)
target_link_libraries(gtmode_game_tests PRIVATE ${MOON_CORE_STACK})
add_test(NAME gtmode_game COMMAND gtmode_game_tests)

# Native Setting UI integration, using the user's authored project and car.
add_executable(gtmode_license_teacher tests/gtmode/license_teacher_run.c)
target_link_libraries(gtmode_license_teacher PRIVATE moon_physics ${MOON_CORE_STACK})
add_executable(gtmode_training_capture tests/gtmode/training_capture_run.c)
target_link_libraries(gtmode_training_capture PRIVATE moon_physics ${MOON_CORE_STACK})
set(MOON_TEST_TRAINING_STATE "" CACHE FILEPATH "User EE snapshot at B-5 start; local only")
if(MOON_TEST_DISC AND MOON_TEST_TRAINING_STATE)
  add_test(NAME gtmode_training_capture COMMAND gtmode_training_capture ${MOON_TEST_DISC} l0b0005 ${MOON_TEST_TRAINING_STATE})
  add_test(NAME gtmode_training_capture_negative COMMAND gtmode_training_capture ${MOON_TEST_DISC} l0b0005 ${MOON_TEST_TRAINING_STATE})
  set_tests_properties(gtmode_training_capture_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_TRAINING_CAPTURE_NEGATIVE=1")
endif()
add_executable(gtmode_setting_race tests/gtmode/setting_race_run.c)
target_link_libraries(gtmode_setting_race PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
if(MOON_RECOMP_TESTS)
  set(gen_setting_curve "${CMAKE_BINARY_DIR}/gen/setting_curve.c")
  add_custom_command(OUTPUT ${gen_setting_curve}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_setting_curve} --root 0x0034B960 --extra-roots 0x0034B8E8,0x00148B60 --data-refs --hooks
      --exclude 0x0013BDC0,0x00147D80,0x00441248,0x004413C8,0x002EFC48,0x002EFCE0,0x00575DC8,0x00200140,0x002F9360,0x0013BD68,0x003285A8,0x003285F8,0x001FFF30,0x002F7B68,0x002ED5C0
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_tune_power_probe tests/gtmode/tune_power_probe.c)
  target_link_libraries(gtmode_tune_power_probe PRIVATE moon_career_vehicle moon_ee moon_physics ${MOON_CORE_STACK})
  target_compile_options(gtmode_tune_power_probe PRIVATE $<$<C_COMPILER_ID:MSVC>:/fp:strict>)
  add_executable(gtmode_setting_curve tests/gtmode/setting_curve_run.c ${gen_setting_curve})
  target_link_libraries(gtmode_setting_curve PRIVATE moon_ee moon_physics ${MOON_CORE_STACK})
  target_compile_options(gtmode_setting_curve PRIVATE $<$<C_COMPILER_ID:MSVC>:/fp:strict>)
  set(gen_setting_auto "${CMAKE_BINARY_DIR}/gen/setting_auto.c")
  add_custom_command(OUTPUT ${gen_setting_auto}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_setting_auto} --root 0x00149538 --extra-roots 0x0014A2D0,0x0044A1E0 --data-refs --hooks
      --exclude 0x0013BDC0,0x00147D80,0x004413C8,0x00443ED0,0x002EFC48,0x002EFCE0,0x002FE278,0x0013BD68,0x003285A8,0x003285F8,0x002FC870,0x002ED5C0
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_setting_auto tests/gtmode/setting_auto_run.c ${gen_setting_auto})
  target_link_libraries(gtmode_setting_auto PRIVATE moon_ee moon_physics ${MOON_CORE_STACK})
  target_compile_options(gtmode_setting_auto PRIVATE $<$<C_COMPILER_ID:MSVC>:/fp:strict>)
  set(gen_setting_motion "${CMAKE_BINARY_DIR}/gen/setting_motion.c")
  add_custom_command(OUTPUT ${gen_setting_motion}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_setting_motion} --root 0x002C1BB0:0x002C1C1C --stop 0x002C1C18
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  set(gen_setting_units "${CMAKE_BINARY_DIR}/gen/setting_units.c")
  add_custom_command(OUTPUT ${gen_setting_units}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_setting_units} --root 0x001AF760 --extra-roots 0x00472758,0x001AF598,0x004726B8 --hooks --data-refs
      --exclude 0x002FE278,0x003285A8,0x003285F8,0x002FC870
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_setting_units tests/gtmode/setting_units_run.c ${gen_setting_units})
  target_link_libraries(gtmode_setting_units PRIVATE moon_ee moon_career_vehicle ${MOON_CORE_STACK})
  target_compile_options(gtmode_setting_units PRIVATE $<$<C_COMPILER_ID:MSVC>:/fp:strict>)
  add_test(NAME gtmode_setting_units COMMAND gtmode_setting_units "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  add_test(NAME gtmode_setting_units_negative COMMAND gtmode_setting_units "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  add_test(NAME gtmode_setting_power_negative COMMAND gtmode_setting_units "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  set_tests_properties(gtmode_setting_power_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_SETTING_POWER_NEGATIVE=1")
  set_tests_properties(gtmode_setting_units_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_SETTING_UNITS_NEGATIVE=1")
  add_executable(gtmode_setting_motion tests/gtmode/setting_motion_run.c ${gen_setting_motion})
  target_link_libraries(gtmode_setting_motion PRIVATE moon_ee moon_career_vehicle ${MOON_CORE_STACK})
  target_compile_options(gtmode_setting_motion PRIVATE $<$<C_COMPILER_ID:MSVC>:/fp:strict>)
  if(MOON_TEST_DISC)
    add_test(NAME gtmode_setting_motion COMMAND gtmode_setting_motion "${CMAKE_SOURCE_DIR}/extracted/core.bin" "${MOON_TEST_DISC}")
    add_test(NAME gtmode_setting_motion_negative COMMAND gtmode_setting_motion "${CMAKE_SOURCE_DIR}/extracted/core.bin" "${MOON_TEST_DISC}")
    set_tests_properties(gtmode_setting_motion_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_SETTING_MOTION_NEGATIVE=1")
    add_test(NAME gtmode_setting_auto COMMAND gtmode_setting_auto "${CMAKE_SOURCE_DIR}/extracted/core.bin" "${MOON_TEST_DISC}")
    foreach(negative IN ITEMS RANGE BODY STAGE)
      add_test(NAME gtmode_setting_auto_${negative}_negative COMMAND gtmode_setting_auto "${CMAKE_SOURCE_DIR}/extracted/core.bin" "${MOON_TEST_DISC}")
      set_tests_properties(gtmode_setting_auto_${negative}_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_SETTING_AUTO_${negative}_NEGATIVE=1;GT4_SETTING_CAR_LIMIT=1")
    endforeach()
    add_test(NAME gtmode_setting_curve COMMAND gtmode_setting_curve "${CMAKE_SOURCE_DIR}/extracted/core.bin" "${MOON_TEST_DISC}")
    add_test(NAME gtmode_setting_curve_upgrades COMMAND gtmode_setting_curve "${CMAKE_SOURCE_DIR}/extracted/core.bin" "${MOON_TEST_DISC}")
    set_tests_properties(gtmode_setting_curve_upgrades PROPERTIES ENVIRONMENT "GT4_SETTING_CURVE_UPGRADES=1")
    add_test(NAME gtmode_setting_curve_negative COMMAND gtmode_setting_curve "${CMAKE_SOURCE_DIR}/extracted/core.bin" "${MOON_TEST_DISC}")
    set_tests_properties(gtmode_setting_curve_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_SETTING_CURVE_NEGATIVE=1")
    add_test(NAME gtmode_setting_curve_labels_negative COMMAND gtmode_setting_curve "${CMAKE_SOURCE_DIR}/extracted/core.bin" "${MOON_TEST_DISC}")
    set_tests_properties(gtmode_setting_curve_labels_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_SETTING_CURVE_LABEL_NEGATIVE=1;GT4_SETTING_CAR_LIMIT=1")
  endif()
endif()
if(MOON_TEST_DISC)
  add_test(NAME gtmode_setting_race COMMAND gtmode_setting_race "${MOON_TEST_DISC}")
  add_test(NAME gtmode_setting_race_negative COMMAND gtmode_setting_race "${MOON_TEST_DISC}")
  set_tests_properties(gtmode_setting_race_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_SETTING_RACE_NEGATIVE=1")
  add_test(NAME gtmode_license_teacher_b1 COMMAND gtmode_license_teacher "${MOON_TEST_DISC}" l0b0001)
  add_test(NAME gtmode_license_teacher_b1_negative COMMAND gtmode_license_teacher "${MOON_TEST_DISC}" l0b0001)
  set_tests_properties(gtmode_license_teacher_b1_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_TEACHER_NEGATIVE=1")
endif()
add_executable(gtmode_setting_menu tests/gtmode/setting_menu.c)
target_link_libraries(gtmode_setting_menu PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
add_executable(options_lifecycle tests/ui/options_lifecycle.c)
target_link_libraries(options_lifecycle PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
if(MOON_TEST_DISC)
  add_test(NAME options_lifecycle COMMAND options_lifecycle "${MOON_TEST_DISC}")
  add_test(NAME options_lifecycle_profile COMMAND "${MOON_PYTHON}" "${CMAKE_SOURCE_DIR}/tests/ui/options_profile.py" "$<TARGET_FILE:options_lifecycle>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}")
  add_test(NAME options_lifecycle_route_negative COMMAND options_lifecycle "${MOON_TEST_DISC}")
  add_test(NAME options_lifecycle_units_negative COMMAND options_lifecycle "${MOON_TEST_DISC}")
  set_tests_properties(options_lifecycle_route_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_OPTIONS_ROUTE_NEGATIVE=1")
  set_tests_properties(options_lifecycle_units_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_OPTIONS_UNITS_NEGATIVE=1")
endif()
add_executable(gtmode_setting_catalog_menu tests/gtmode/setting_catalog_menu.c)
target_link_libraries(gtmode_setting_catalog_menu PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
add_executable(gtmode_setting_reference tests/gtmode/setting_reference.c)
target_link_libraries(gtmode_setting_reference PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
if(MOON_RECOMP_TESTS)
  set(gen_setting_draw "${CMAKE_BINARY_DIR}/gen/setting_draw.c")
  add_custom_command(OUTPUT ${gen_setting_draw}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_setting_draw} --root 0x002E88F8 --hooks --data-refs
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  target_sources(gtmode_setting_reference PRIVATE ${gen_setting_draw})
  target_compile_definitions(gtmode_setting_reference PRIVATE SETTING_DRAW_ORACLE=1)
  target_link_libraries(gtmode_setting_reference PRIVATE moon_ee)
endif()
set(MOON_SETTING_REFERENCE "" CACHE FILEPATH "Local PCSX2 Setting GearRoot eeMemory.bin")
if(MOON_TEST_DISC AND EXISTS "${MOON_SETTING_REFERENCE}")
  add_test(NAME gtmode_setting_reference COMMAND gtmode_setting_reference "${MOON_TEST_DISC}" "${MOON_SETTING_REFERENCE}")
  add_test(NAME gtmode_setting_reference_negative COMMAND gtmode_setting_reference "${MOON_TEST_DISC}" "${MOON_SETTING_REFERENCE}")
  set_tests_properties(gtmode_setting_reference_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_SETTING_REFERENCE_NEGATIVE=1")
  add_test(NAME gtmode_setting_closed_negative COMMAND gtmode_setting_reference "${MOON_TEST_DISC}" "${MOON_SETTING_REFERENCE}")
  set_tests_properties(gtmode_setting_closed_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_SETTING_CLOSED_NEGATIVE=1")
  if(MOON_RECOMP_TESTS)
    add_test(NAME gtmode_setting_draw_negative COMMAND gtmode_setting_reference "${MOON_TEST_DISC}" "${MOON_SETTING_REFERENCE}")
    set_tests_properties(gtmode_setting_draw_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_SETTING_DRAW_GEOMETRY_NEGATIVE=1")
  endif()
endif()
if(MOON_TEST_DISC)
  add_test(NAME gtmode_setting_catalog_menu COMMAND gtmode_setting_catalog_menu "${MOON_TEST_DISC}")
  add_test(NAME gtmode_setting_catalog_menu_negative COMMAND gtmode_setting_catalog_menu "${MOON_TEST_DISC}")
  set_tests_properties(gtmode_setting_catalog_menu_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_SETTING_CATALOG_NEGATIVE=1;GT4_SETTING_CAR_LIMIT=1")
endif()
if(MOON_TEST_DISC)
  add_test(NAME gtmode_setting_menu COMMAND gtmode_setting_menu "${MOON_TEST_DISC}")
  add_test(NAME gtmode_setting_menu_negative COMMAND gtmode_setting_menu "${MOON_TEST_DISC}")
  add_test(NAME gtmode_setting_controls_negative COMMAND gtmode_setting_menu "${MOON_TEST_DISC}")
  set_tests_properties(gtmode_setting_controls_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_SETTING_CONTROL_NEGATIVE=1")
  add_test(NAME gtmode_setting_save COMMAND gtmode_setting_menu "${MOON_TEST_DISC}")
  set_tests_properties(gtmode_setting_save PROPERTIES ENVIRONMENT "GT4_SETTING_SAVE_DIR=${CMAKE_BINARY_DIR}/setting-native/ctest-save")
  set_tests_properties(gtmode_setting_menu_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_SETTING_FLOW_NEGATIVE=1")
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  set(gen_setting "${CMAKE_BINARY_DIR}/gen/gtmode_setting.c")
  add_custom_command(OUTPUT ${gen_setting}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_setting} --root 0x0043EC30 --extra-roots 0x00355278,0x001490b0,0x0014a3d0,0x0014a6a0,0x0014aad8,0x0014b478,0x0014c898,0x0014cbc8,0x0014d278,0x0014d638,0x0043e2f0,0x0043e320,0x0043e348,0x0043e370,0x0043e398,0x0043e3c0,0x0043e3e8,0x0043e410,0x0043e438,0x0043e598,0x0043e5c0,0x0043e630,0x0043e658,0x0043e6c8,0x0043e6f0,0x0043e760,0x0043e788,0x0043e7b0,0x0043e7d8,0x0043e800,0x0043e828,0x0043e850,0x0043e878,0x0043e8a0,0x0043e8c8,0x0043e8f0,0x0043e918,0x0043e940,0x0043e968,0x0043e990,0x0043e9b8,0x0043e9e0,0x0043ea08,0x0043ea30,0x0043ea58,0x0043eaa0,0x0043eae8,0x0043eb10,0x0043eb38,0x0043eb60,0x0043eb90,0x0043ebb8,0x0043ebe0,0x0043ec08,0x0043ec30,0x0043ec58,0x0043ec80,0x0043eca8,0x0043edc0,0x0043ede8,0x0043ee10,0x0043ee38,0x0043ee60,0x0043ee88,0x0043eeb0,0x0043eed8,0x0043ef00,0x0043ef28,0x0043ef50,0x0043ef78,0x0043efa0,0x0043efc8,0x0043eff0,0x0043f018,0x0043f040,0x0043f068,0x0043f090,0x0043f0b8,0x0043f0e0,0x0043f108,0x0043f130,0x0043f158,0x0043f180,0x0043f1a8,0x0043f1d0,0x0043f1f8,0x0043f220,0x0043f248 --data-refs --hooks
      --exclude 0x0013BDC0,0x0013BD68,0x002EFC48,0x002EFCE0,0x002FE278,0x003285A8,0x003285F8,0x002FC870,0x002ED5C0
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(gtmode_setting_oracle tests/gtmode/setting_run.c ${gen_setting})
  target_link_libraries(gtmode_setting_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  add_test(NAME gtmode_setting_oracle COMMAND gtmode_setting_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  add_test(NAME gtmode_setting_oracle_negative COMMAND gtmode_setting_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  set_tests_properties(gtmode_setting_oracle_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_SETTING_ORACLE_NEGATIVE=1")
  foreach(part IN ITEMS RANGE RATIO)
    add_test(NAME gtmode_setting_${part}_negative COMMAND gtmode_setting_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin")
    set_tests_properties(gtmode_setting_${part}_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_SETTING_${part}_NEGATIVE=1")
  endforeach()
endif()

# Local two-player battle integration (original Battle2P session).
add_executable(split_battle_test tests/physics/split_battle.c)
target_link_libraries(split_battle_test PRIVATE moon_physics ${MOON_CORE_STACK})
target_include_directories(split_battle_test PRIVATE src/core/physics)

add_executable(split_input_test tests/ui/split_input.c src/app/race_audio.c src/app/video_settings.c src/app/options_store.c src/app/desktop_input.c src/app/discord_presence.c)
target_link_libraries(split_input_test PRIVATE moon_replay_state moon_race_sound moon_career_vehicle ${MOON_SDL_STACK} moon_physics miniz SDL2::SDL2main)
add_test(NAME split_input COMMAND split_input_test)
add_test(NAME split_input_negative COMMAND split_input_test --negative)
set_tests_properties(split_input_negative PROPERTIES WILL_FAIL TRUE)
if(MOON_RECOMP_TESTS AND EXISTS "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  set(gen_split_battle "${CMAKE_BINARY_DIR}/gen/split_battle.c")
  add_custom_command(OUTPUT ${gen_split_battle}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
            --out ${gen_split_battle} --root 0x003471A8
            --extra-roots 0x00347188,0x00347108,0x0034D048 --hooks
    DEPENDS tools/recomp/ee_recomp.py extracted/core.bin splat/asm/core/text.s)
  add_executable(split_battle_oracle tests/physics/split_oracle.c ${gen_split_battle})
  target_link_libraries(split_battle_oracle PRIVATE moon_ee moon_physics ${MOON_CORE_STACK})
  add_test(NAME split_battle_oracle COMMAND split_battle_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  add_test(NAME split_battle_oracle_negative COMMAND split_battle_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin" --negative)
  set_tests_properties(split_battle_oracle_negative PROPERTIES WILL_FAIL TRUE)
endif()

add_executable(split_menu_test tests/ui/split_menu.c)
target_link_libraries(split_menu_test PRIVATE ${MOON_CORE_STACK})
add_executable(storage_audit_test tests/ui/storage_audit.c)
target_link_libraries(storage_audit_test PRIVATE ${MOON_CORE_STACK} miniz)
add_executable(profile_storage_test tests/ui/profile_storage.c)
target_link_libraries(profile_storage_test PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
add_executable(split_select_test tests/ui/split_select.c)
target_link_libraries(split_select_test PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
add_executable(split_owned_test tests/ui/split_owned.c src/app/video_settings.c src/app/options_store.c src/app/desktop_input.c src/app/discord_presence.c)
target_include_directories(split_owned_test PRIVATE src/app)
target_link_libraries(split_owned_test PRIVATE moon_career_vehicle moon_race_sound ${MOON_SDL_STACK} moon_replay_state miniz SDL2::SDL2main)
add_executable(split_replay_test tests/ui/split_replay.c src/app/video_settings.c src/app/options_store.c src/app/desktop_input.c src/app/discord_presence.c)
target_include_directories(split_replay_test PRIVATE src/app)
target_link_libraries(split_replay_test PRIVATE moon_career_vehicle moon_race_sound ${MOON_SDL_STACK} moon_replay_state miniz SDL2::SDL2main)
if(MOON_TEST_DISC)
  add_test(NAME split_replay COMMAND split_replay_test "${MOON_TEST_DISC}" --capture "${CMAKE_BINARY_DIR}/split-mirror.png")
  add_test(NAME split_mirror_negative COMMAND split_replay_test "${MOON_TEST_DISC}" --mirror-negative)
  set_tests_properties(split_mirror_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME split_replay_negative COMMAND split_replay_test "${MOON_TEST_DISC}" --negative)
  set_tests_properties(split_replay_negative PROPERTIES WILL_FAIL TRUE)
endif()
add_executable(split_viewport_test tests/ui/split_viewport.c)
target_include_directories(split_viewport_test PRIVATE src/platform/sdl)
target_link_libraries(split_viewport_test PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
add_test(NAME split_viewport COMMAND split_viewport_test)
add_test(NAME split_viewport_negative COMMAND split_viewport_test --negative)
set_tests_properties(split_viewport_negative PROPERTIES WILL_FAIL TRUE)
if(MOON_TEST_DISC)
  add_test(NAME profile_storage COMMAND profile_storage_test "${MOON_TEST_DISC}")
  add_test(NAME storage_audit COMMAND storage_audit_test "${MOON_TEST_DISC}")
  add_test(NAME storage_audit_negative COMMAND storage_audit_test "${MOON_TEST_DISC}" --negative)
  set_tests_properties(storage_audit_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME profile_storage_negative COMMAND profile_storage_test "${MOON_TEST_DISC}" --negative)
  set_tests_properties(profile_storage_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME split_select_cursor_negative COMMAND split_select_test "${MOON_TEST_DISC}" --cursor-negative)
  set_tests_properties(split_select_cursor_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME split_select COMMAND split_select_test "${MOON_TEST_DISC}")
  add_test(NAME split_owned COMMAND split_owned_test "${MOON_TEST_DISC}")
  add_test(NAME split_owned_negative COMMAND split_owned_test "${MOON_TEST_DISC}" --negative)
  set_tests_properties(split_owned_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME split_owned_odometer_negative COMMAND split_owned_test "${MOON_TEST_DISC}" --negative-odometer)
  set_tests_properties(split_owned_odometer_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME split_select_negative COMMAND split_select_test "${MOON_TEST_DISC}" --negative)
  set_tests_properties(split_select_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME split_select_paint_negative COMMAND split_select_test "${MOON_TEST_DISC}" --paint-negative)
  set_tests_properties(split_select_paint_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME split_select_availability_negative COMMAND split_select_test "${MOON_TEST_DISC}" --availability-negative)
  set_tests_properties(split_select_availability_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME split_select_owned_negative COMMAND split_select_test "${MOON_TEST_DISC}" --owned-negative)
  set_tests_properties(split_select_owned_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME split_menu COMMAND split_menu_test "${MOON_TEST_DISC}")
  add_test(NAME split_menu_negative COMMAND split_menu_test "${MOON_TEST_DISC}" --negative)
  set_tests_properties(split_menu_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME split_battle COMMAND split_battle_test "${MOON_TEST_DISC}")
  add_test(NAME split_battle_negative COMMAND split_battle_test "${MOON_TEST_DISC}" --negative)
  set_tests_properties(split_battle_negative PROPERTIES WILL_FAIL TRUE)
endif()

if(MOON_TEST_DISC)
  add_test(NAME split_pause_negative COMMAND split_menu_test "${MOON_TEST_DISC}" --pause-negative)
  set_tests_properties(split_pause_negative PROPERTIES WILL_FAIL TRUE)
endif()

add_executable(split_layout_test tests/ui/split_layout.c)
target_include_directories(split_layout_test PRIVATE src/core)
if(EXISTS "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  add_test(NAME split_layout COMMAND split_layout_test "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  add_test(NAME split_layout_negative COMMAND split_layout_test "${CMAKE_SOURCE_DIR}/extracted/core.bin" --negative)
  set_tests_properties(split_layout_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME split_mirror_layout_negative COMMAND split_layout_test "${CMAKE_SOURCE_DIR}/extracted/core.bin" --mirror-negative)
  set_tests_properties(split_mirror_layout_negative PROPERTIES WILL_FAIL TRUE)
endif()

# Photo scalar passes compared directly with the original EE routines.
add_executable(race_resources_test tests/ui/race_resources.c src/app/photo_travel.c src/app/video_settings.c src/app/options_store.c src/app/desktop_input.c src/app/discord_presence.c)
target_include_directories(race_resources_test PRIVATE src/app)
target_link_libraries(race_resources_test PRIVATE moon_career_vehicle moon_race_sound ${MOON_SDL_STACK} moon_replay_state miniz SDL2::SDL2main)
if(MOON_TEST_DISC)
  add_test(NAME race_resources COMMAND race_resources_test "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/race-resources-save" "${CMAKE_BINARY_DIR}/race-resources.png")
  add_test(NAME race_resources_negative COMMAND race_resources_test "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/race-resources-save" "${CMAKE_BINARY_DIR}/race-resources.png" --negative)
  set_tests_properties(race_resources_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME photo_drive_negative COMMAND race_resources_test "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/race-resources-negative-save" "${CMAKE_BINARY_DIR}/race-resources-negative.png" --photo-negative)
  set_tests_properties(photo_drive_negative PROPERTIES WILL_FAIL TRUE)
  foreach(part camera film)
    add_test(NAME photo_drive_${part}_negative COMMAND race_resources_test "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/photo-${part}-negative-save" "${CMAKE_BINARY_DIR}/photo-${part}-negative.png" --${part}-negative)
    set_tests_properties(photo_drive_${part}_negative PROPERTIES WILL_FAIL TRUE)
  endforeach()
endif()
set_source_files_properties(src/core/photo_mode.c src/core/ps2_media.c PROPERTIES COMPILE_OPTIONS "$<$<C_COMPILER_ID:MSVC>:/fp:strict>;$<$<NOT:$<C_COMPILER_ID:MSVC>>:-ffp-contract=off;-frounding-math>")
if(MOON_RECOMP_TESTS)
  set(gen_ranking_code "${CMAKE_BINARY_DIR}/gen/ranking_code.c")
  add_custom_command(OUTPUT ${gen_ranking_code}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_ranking_code} --root 0x0044B258 --hooks --exclude 0x00548918
    DEPENDS tools/recomp/ee_recomp.py extracted/core.bin splat/asm/core/text.s)
  add_executable(ranking_code_oracle tests/gtmode/ranking_code_run.c ${gen_ranking_code})
  target_link_libraries(ranking_code_oracle PRIVATE moon_ee ${MOON_CORE_STACK} miniz)
  add_test(NAME ranking_code COMMAND ranking_code_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin "${MOON_TEST_DISC}")
  add_test(NAME ranking_code_negative COMMAND ranking_code_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin "${MOON_TEST_DISC}" --negative)
  set_tests_properties(ranking_code_negative PROPERTIES WILL_FAIL TRUE)
  set(gen_photo_optics "${CMAKE_BINARY_DIR}/gen/photo_optics.c")
  add_custom_command(OUTPUT ${gen_photo_optics}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_photo_optics} --root 0x003EFC10
      --extra-roots 0x003EFD80,0x00579438,0x00579548,0x00332550,0x00333680,0x0033B358,0x0036F628,0x0036FA20,0x00334730
      --exclude 0x0057D6C0,0x003326E8,0x004A7550,0x004A74B4,0x004A7844,0x004A7454,0x00489FD8,0x004A7774,0x00343170,0x0033E368,0x00334C98 --hooks
    DEPENDS tools/recomp/ee_recomp.py extracted/core.bin splat/asm/core/text.s)
  add_executable(photo_optics_oracle tests/gtmode/photo_optics_run.c ${gen_photo_optics})
  target_link_libraries(photo_optics_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  add_test(NAME photo_optics COMMAND photo_optics_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin)
  foreach(part COLOR SAMPLE LENS TIME)
    add_test(NAME photo_optics_${part}_negative COMMAND photo_optics_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin)
    set_tests_properties(photo_optics_${part}_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_PHOTO_${part}_NEGATIVE=1")
  endforeach()
endif()

add_executable(gtmode_extras_menu tests/gtmode/extras_menu.c)
target_link_libraries(gtmode_extras_menu PRIVATE moon_career_vehicle ${MOON_CORE_STACK})

add_executable(photo_motion_test tests/ui/photo_motion.c)
add_executable(wheel_input_test tests/ui/wheel_input.c)
add_executable(playlist_cache_test tests/ui/playlist_cache.c)
target_link_libraries(playlist_cache_test PRIVATE ${MOON_CORE_STACK})
add_test(NAME playlist_cache COMMAND playlist_cache_test)
add_test(NAME playlist_cache_negative COMMAND playlist_cache_test --negative)
set_tests_properties(playlist_cache_negative PROPERTIES WILL_FAIL TRUE)
target_link_libraries(wheel_input_test PRIVATE moon_platform_sdl SDL2::SDL2main)
target_include_directories(wheel_input_test PRIVATE src/core src/platform/sdl)
add_test(NAME wheel_input COMMAND wheel_input_test)
add_test(NAME wheel_input_negative COMMAND wheel_input_test --negative)
set_tests_properties(wheel_input_negative PROPERTIES WILL_FAIL TRUE)
target_link_libraries(photo_motion_test PRIVATE ${MOON_SDL_STACK} miniz SDL2::SDL2main)
add_test(NAME photo_motion COMMAND photo_motion_test)
add_test(NAME photo_motion_negative COMMAND photo_motion_test --negative)
set_tests_properties(photo_motion_negative PROPERTIES WILL_FAIL TRUE)

add_executable(photo_drive_menu_test tests/ui/photo_drive_menu.c)
target_link_libraries(photo_drive_menu_test PRIVATE moon_career_vehicle ${MOON_CORE_STACK})

if(MOON_TEST_DISC)
  add_test(NAME gtmode_extras_menu COMMAND gtmode_extras_menu "${MOON_TEST_DISC}")
  foreach(part stock wheel secret inherit)
    add_test(NAME gtmode_extras_${part}_negative COMMAND gtmode_extras_menu "${MOON_TEST_DISC}" --${part}-negative)
    set_tests_properties(gtmode_extras_${part}_negative PROPERTIES WILL_FAIL TRUE)
  endforeach()
endif()
if(MOON_TEST_DISC)
  add_test(NAME photo_drive_menu COMMAND photo_drive_menu_test "${MOON_TEST_DISC}")
  add_test(NAME photo_drive_menu_negative COMMAND photo_drive_menu_test "${MOON_TEST_DISC}" --negative)
  set_tests_properties(photo_drive_menu_negative PROPERTIES WILL_FAIL TRUE)
endif()

if(MOON_TEST_DISC AND TARGET outside_menu)
 add_test(NAME slideshow_playlist_negative COMMAND outside_menu "${MOON_TEST_DISC}" "${CMAKE_SOURCE_DIR}/extracted/core.bin" "${CMAKE_BINARY_DIR}/outside-playlist-fixture" --playlist-negative)
 set_tests_properties(slideshow_playlist_negative PROPERTIES WILL_FAIL TRUE RUN_SERIAL TRUE)
endif()

# Original media archives; fixtures supplied by the test driver, never game data.
add_executable(ps2_media_test tests/ui/ps2_media.c)
target_link_libraries(ps2_media_test PRIVATE ${MOON_CORE_STACK} miniz)

add_executable(film_develop_test tests/ui/film_develop.c)
target_include_directories(film_develop_test PRIVATE src/app)
target_link_libraries(film_develop_test PRIVATE moon_race_sound ${MOON_SDL_STACK} miniz SDL2::SDL2main)
add_executable(film_menu_test tests/ui/film_menu.c)
target_link_libraries(film_menu_test PRIVATE moon_career_vehicle ${MOON_SDL_STACK} miniz SDL2::SDL2main)
if(MOON_TEST_DISC)
 add_test(NAME film_develop COMMAND film_develop_test "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/film-develop")
 add_test(NAME film_develop_negative COMMAND film_develop_test "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/film-develop-negative" --negative)
 add_test(NAME film_develop_details_negative COMMAND film_develop_test "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/film-details-negative" --details-negative)
 set_tests_properties(film_develop film_develop_negative film_develop_details_negative PROPERTIES RUN_SERIAL TRUE)
 set_tests_properties(film_develop_negative film_develop_details_negative PROPERTIES WILL_FAIL TRUE)
 add_test(NAME film_menu COMMAND film_menu_test "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/film-menu")
 add_test(NAME film_menu_negative COMMAND film_menu_test "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/film-menu-negative" --negative)
 set_tests_properties(film_menu_negative PROPERTIES WILL_FAIL TRUE)
endif()

if(MOON_RECOMP_TESTS)
 set(gen_photo_film "${CMAKE_BINARY_DIR}/gen/photo_film.c")
 add_custom_command(OUTPUT ${gen_photo_film}
   COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
     --out ${gen_photo_film} --root 0x00334FC8 --extra-roots 0x0033ADE8,0x0045A4A0,0x00334B28,0x0041BCC0,0x0041C0B8,0x003E93D0,0x0040B690,0x004594D0,0x004514A0,0x0041B048
     --hooks --exclude 0x0033A2A0,0x0033A018,0x0033DE30,0x0057D6C0
   DEPENDS tools/recomp/ee_recomp.py extracted/core.bin splat/asm/core/text.s)
 add_executable(photo_film_oracle tests/ui/photo_film_oracle.c ${gen_photo_film})
 target_link_libraries(photo_film_oracle PRIVATE moon_ee ${MOON_CORE_STACK} miniz)
 add_test(NAME photo_film_format COMMAND photo_film_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin")
 add_test(NAME photo_film_format_negative COMMAND photo_film_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin" --negative)
 add_test(NAME photo_film_driver_negative COMMAND photo_film_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin" --driver-negative)
 add_test(NAME photo_film_driver_blend_negative COMMAND photo_film_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin" --driver-blend-negative)
 add_test(NAME photo_film_wheel_negative COMMAND photo_film_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin" --wheel-negative)
 add_test(NAME photo_film_suspension_negative COMMAND photo_film_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin" --suspension-negative)
 add_test(NAME photo_film_particle_negative COMMAND photo_film_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin" --particle-negative)
 set_tests_properties(photo_film_format_negative photo_film_driver_negative photo_film_particle_negative photo_film_wheel_negative photo_film_suspension_negative photo_film_driver_blend_negative PROPERTIES WILL_FAIL TRUE)
endif()

if(MOON_TEST_DISC AND TARGET race_car_sound_oracle)
 add_executable(photo_ambient_oracle tests/ui/photo_ambient_oracle.c ${gen_race_car_sound})
 target_link_libraries(photo_ambient_oracle PRIVATE moon_ee ${MOON_CORE_STACK} moon_race_sound)
 add_test(NAME photo_ambient COMMAND photo_ambient_oracle "${MOON_TEST_DISC}" "${CMAKE_SOURCE_DIR}/extracted/core.bin")
 add_test(NAME photo_ambient_negative COMMAND photo_ambient_oracle "${MOON_TEST_DISC}" "${CMAKE_SOURCE_DIR}/extracted/core.bin" --negative)
 set_tests_properties(photo_ambient_negative PROPERTIES WILL_FAIL TRUE)
endif()

add_executable(photo_pause_test tests/ui/photo_pause.c)
target_link_libraries(photo_pause_test PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
if(MOON_TEST_DISC)
 add_test(NAME photo_pause_movie COMMAND photo_pause_test "${MOON_TEST_DISC}")
 add_test(NAME photo_pause_movie_negative COMMAND photo_pause_test "${MOON_TEST_DISC}" --negative)
 set_tests_properties(photo_pause_movie_negative PROPERTIES WILL_FAIL TRUE)
endif()

if(MOON_TEST_DISC AND MOON_PYTHON)
 add_test(NAME ps2_media_import COMMAND "${MOON_PYTHON}" "${CMAKE_SOURCE_DIR}/tests/ui/ps2_media_fixture.py" $<TARGET_FILE:ps2_media_test> "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/ps2-media-fixtures")
 set_tests_properties(ps2_media_import PROPERTIES RUN_SERIAL TRUE)
endif()

# Regression: course movie must not survive behind the returned mode menu.
add_test(NAME menu_movie_layer_negative COMMAND menu_tests)
set_tests_properties(menu_movie_layer_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_MOVIE_LAYER_NEGATIVE=1")
add_test(NAME menu_buttons_negative COMMAND menu_tests)
set_tests_properties(menu_buttons_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_MENU_BUTTON_NEGATIVE=1")
add_test(NAME ui_menu_projection COMMAND ui_blend_probe)
add_test(NAME ui_menu_projection_negative COMMAND ui_blend_probe)
set_tests_properties(ui_menu_projection_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_CAPTURE_ASPECT_NEGATIVE=1")
add_test(NAME ui_menu_scissor_negative COMMAND ui_blend_probe)
set_tests_properties(ui_menu_scissor_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_MENU_SCISSOR_NEGATIVE=1")

# Licence aids and result controller integration.
add_executable(license_information tests/ui/license_information.c)
target_link_libraries(license_information PRIVATE ${MOON_CORE_STACK})
add_executable(license_load tests/ui/license_load.c src/app/race_audio.c src/app/video_settings.c src/app/options_store.c src/app/desktop_input.c src/app/discord_presence.c)
target_link_libraries(license_load PRIVATE moon_replay_state moon_race_sound moon_physics ${MOON_SDL_STACK} SDL2::SDL2main)
add_executable(license_result_input tests/ui/license_result_input.c src/app/race_audio.c src/app/video_settings.c src/app/options_store.c src/app/desktop_input.c src/app/discord_presence.c)
target_link_libraries(license_result_input PRIVATE moon_replay_state moon_race_sound moon_physics ${MOON_SDL_STACK} SDL2::SDL2main)
add_executable(license_aids tests/gtmode/license_aids.c)
target_link_libraries(license_aids PRIVATE moon_physics ${MOON_CORE_STACK})
set(MOON_TEST_LICENSE_AIDS_STATE "${CMAKE_SOURCE_DIR}/extracted/handoff/license-user-states-20261006/slot02/eeMemory.bin" CACHE FILEPATH "PCSX2 B-1 EE memory for licence aids comparison")
if(EXISTS "${MOON_TEST_DISC}")
  add_test(NAME license_information COMMAND license_information "${MOON_TEST_DISC}")
  add_test(NAME license_information_negative COMMAND license_information "${MOON_TEST_DISC}" --negative)
  set_tests_properties(license_information_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME license_information_reentry_negative COMMAND license_information "${MOON_TEST_DISC}" --reentry-negative)
  set_tests_properties(license_information_reentry_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME license_load_b5 COMMAND license_load "${MOON_TEST_DISC}" l0b0005)
  add_test(NAME license_load_coffee_b COMMAND license_load "${MOON_TEST_DISC}" l0c0001)
  foreach(exam l0b0010 l0b0015 l0a0005 l0a0010 l0a0015 lib0005 lib0010 lib0015 lia0005 lia0010 lia0015)
    add_test(NAME license_load_${exam} COMMAND license_load "${MOON_TEST_DISC}" ${exam})
  endforeach()
  add_test(NAME license_load_negative COMMAND license_load "${MOON_TEST_DISC}" l0b0005 --negative)
  set_tests_properties(license_load_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME license_result_input COMMAND license_result_input "${MOON_TEST_DISC}")
  add_test(NAME license_result_input_negative COMMAND license_result_input "${MOON_TEST_DISC}" --negative)
  set_tests_properties(license_result_input_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME license_aids COMMAND license_aids "${MOON_TEST_DISC}")
  add_test(NAME license_aids_negative COMMAND license_aids "${MOON_TEST_DISC}" --negative)
  set_tests_properties(license_aids_negative PROPERTIES WILL_FAIL TRUE)
  if(EXISTS "${MOON_TEST_LICENSE_AIDS_STATE}")
    add_test(NAME license_aids_reference COMMAND license_aids "${MOON_TEST_DISC}" --reference "${MOON_TEST_LICENSE_AIDS_STATE}")
    add_test(NAME license_aids_reference_negative COMMAND license_aids "${MOON_TEST_DISC}" --reference "${MOON_TEST_LICENSE_AIDS_STATE}" --negative)
    set_tests_properties(license_aids_reference_negative PROPERTIES WILL_FAIL TRUE)
  endif()
endif()

add_executable(replay_types_test tests/ui/replay_types.c)
target_link_libraries(replay_types_test PRIVATE ${MOON_CORE_STACK} miniz)
if(MOON_TEST_DISC)
 add_test(NAME replay_types COMMAND replay_types_test "${MOON_TEST_DISC}")
 add_test(NAME replay_types_negative COMMAND replay_types_test "${MOON_TEST_DISC}" --negative)
 set_tests_properties(replay_types_negative PROPERTIES WILL_FAIL TRUE)
endif()

set_source_files_properties(src/core/physics/course_objects.c PROPERTIES COMPILE_OPTIONS "$<$<C_COMPILER_ID:MSVC>:/fp:strict>;$<$<NOT:$<C_COMPILER_ID:MSVC>>:-ffp-contract=off;-frounding-math>")

if(EXISTS "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  set(gen_prop_physics "${CMAKE_BINARY_DIR}/gen/prop_physics.c")
  add_custom_command(OUTPUT ${gen_prop_physics}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_prop_physics} --root 0x003E3ED0 --extra-roots 0x003E3840,0x003E2B28,0x003E20A8,0x003E44E8
      --exclude 0x0057D8A0,0x003E21D8,0x003E2220,0x003E3290,0x003E3458,0x003E29F8,0x003E3C48,0x005FE3A0,0x00395A38,0x00343BF0,0x00350530 --hooks
    DEPENDS tools/recomp/ee_recomp.py extracted/core.bin splat/asm/core/text.s)
  set_source_files_properties(${gen_prop_physics} PROPERTIES COMPILE_DEFINITIONS "ee_unrecompiled=prop_oracle_boundary;ee_unsupported=prop_oracle_instruction")
  add_executable(course_objects_oracle tests/ui/course_objects_oracle.c ${gen_prop_physics})
  target_link_libraries(course_objects_oracle PRIVATE moon_physics moon_ee ${MOON_CORE_STACK})
  add_test(NAME course_objects_original COMMAND course_objects_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin)
  foreach(part CONTACT GROUND MOTION BLINK WALL)
    add_test(NAME course_objects_${part}_negative COMMAND course_objects_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin)
    set_tests_properties(course_objects_${part}_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_PROP_${part}_NEGATIVE=1")
  endforeach()
endif()
add_test(NAME driver_windshield COMMAND driver_skin_probe)
add_test(NAME steering_windshield_negative COMMAND driver_skin_probe)
set_tests_properties(steering_windshield_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_STEERING_GLASS_NEGATIVE=1")
add_test(NAME driver_windshield_negative COMMAND driver_skin_probe)
set_tests_properties(driver_windshield_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_DRIVER_GLASS_NEGATIVE=1")
add_executable(coffee_objects tests/ui/coffee_objects.c)
target_link_libraries(coffee_objects PRIVATE moon_physics ${MOON_CORE_STACK})
if(EXISTS "${MOON_TEST_DISC}")
  add_test(NAME coffee_objects COMMAND coffee_objects "${MOON_TEST_DISC}")
  add_test(NAME coffee_objects_count COMMAND coffee_objects "${MOON_TEST_DISC}" l0c0002)
  foreach(exam l0c0003 l0c0004 l0c0005)
    add_test(NAME coffee_objects_${exam} COMMAND coffee_objects "${MOON_TEST_DISC}" ${exam})
  endforeach()
  add_test(NAME coffee_objects_negative COMMAND coffee_objects "${MOON_TEST_DISC}")
  set_tests_properties(coffee_objects_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_COFFEE_CONTACT_NEGATIVE=1")
endif()

# Authored disc texture levels, including the deliberate comparison failure.
add_executable(tex1_mips_test tests/ui/tex1_mips.c)
target_link_libraries(tex1_mips_test PRIVATE ${MOON_CORE_STACK})
add_test(NAME tex1_mips COMMAND tex1_mips_test)
add_test(NAME tex1_mips_negative COMMAND tex1_mips_test --negative)
set_tests_properties(tex1_mips_negative PROPERTIES WILL_FAIL TRUE)

add_executable(render3d_mips_test tests/ui/render3d_mips.c)
target_link_libraries(render3d_mips_test PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
add_test(NAME render3d_mips COMMAND render3d_mips_test)
add_test(NAME render3d_mips_negative COMMAND render3d_mips_test --negative)
set_tests_properties(render3d_mips_negative PROPERTIES WILL_FAIL TRUE)

add_executable(render3d_depth_test tests/ui/render3d_depth.c)
target_link_libraries(render3d_depth_test PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)

if(EXISTS "${CMAKE_SOURCE_DIR}/extracted/core.bin")
 add_test(NAME render3d_depth COMMAND render3d_depth_test "${CMAKE_SOURCE_DIR}/extracted/core.bin")
 add_test(NAME render3d_depth_negative COMMAND render3d_depth_test "${CMAKE_SOURCE_DIR}/extracted/core.bin" --negative)
 set_tests_properties(render3d_depth_negative PROPERTIES WILL_FAIL TRUE)
endif()

if(MOON_RECOMP_TESTS)
 set(gen_course_vision "${CMAKE_BINARY_DIR}/gen/course_vision.c")
 add_custom_command(OUTPUT ${gen_course_vision}
  COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py --out ${gen_course_vision} --root 0x004580F0 --extra-roots 0x00458008
  DEPENDS tools/recomp/ee_recomp.py extracted/core.bin splat/asm/core/text.s)
 add_executable(course_vision_oracle tests/ui/course_vision_oracle.c ${gen_course_vision})
 target_link_libraries(course_vision_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
endif()

add_executable(render3d_vision_test tests/ui/render3d_vision.c)
target_link_libraries(render3d_vision_test PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
add_test(NAME render3d_vision COMMAND render3d_vision_test)
add_test(NAME render3d_vision_negative COMMAND render3d_vision_test --negative)
set_tests_properties(render3d_vision_negative PROPERTIES WILL_FAIL TRUE)

add_executable(model_attach_upload_test tests/ui/model_attach_upload.c)
target_link_libraries(model_attach_upload_test PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
add_test(NAME model_attach_upload COMMAND model_attach_upload_test)
add_test(NAME model_attach_upload_negative COMMAND model_attach_upload_test --negative)
set_tests_properties(model_attach_upload_negative PROPERTIES WILL_FAIL TRUE)

add_executable(gtmode_award_presentation tests/gtmode/award_presentation.c)
target_link_libraries(gtmode_award_presentation PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
if(MOON_TEST_DISC)
 add_test(NAME gtmode_award_presentation COMMAND gtmode_award_presentation "${MOON_TEST_DISC}")
 add_test(NAME gtmode_award_animation_negative COMMAND gtmode_award_presentation "${MOON_TEST_DISC}")
 set_tests_properties(gtmode_award_animation_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_AWARD_ANIMATION_NEGATIVE=1")
 add_test(NAME gtmode_award_ok_negative COMMAND gtmode_award_presentation "${MOON_TEST_DISC}")
 set_tests_properties(gtmode_award_ok_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_AWARD_OK_NEGATIVE=1")
endif()

add_test(NAME menu_dialog_background_negative COMMAND menu_tests)
set_tests_properties(menu_dialog_background_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_DIALOG_BACKGROUND_NEGATIVE=1")
if(MOON_TEST_DISC AND TARGET course_vision_oracle)
 add_test(NAME course_vision COMMAND course_vision_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin" "${MOON_TEST_DISC}")
 add_test(NAME course_vision_negative COMMAND course_vision_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin" "${MOON_TEST_DISC}" --negative)
 set_tests_properties(course_vision_negative PROPERTIES WILL_FAIL TRUE)
endif()

add_test(NAME menu_dialog_background COMMAND menu_tests)
set_tests_properties(menu_dialog_background PROPERTIES ENVIRONMENT "GT4_DIALOG_BACKGROUND_ONLY=1")
set_tests_properties(menu_dialog_background_negative PROPERTIES ENVIRONMENT "GT4_DIALOG_BACKGROUND_ONLY=1;GT4_DIALOG_BACKGROUND_NEGATIVE=1")

add_executable(storage_access tests/ui/storage_access.c src/app/save_package.c)
target_include_directories(storage_access PRIVATE src/app)
target_link_libraries(storage_access PRIVATE moon_career_vehicle ${MOON_CORE_STACK} SDL2::SDL2 miniz)
if(MOON_TEST_DISC)
 add_test(NAME storage_access COMMAND storage_access "${MOON_TEST_DISC}")
 add_test(NAME storage_access_negative COMMAND storage_access "${MOON_TEST_DISC}" --negative)
 set_tests_properties(storage_access_negative PROPERTIES WILL_FAIL TRUE)
endif()

add_executable(gtmode_home_progress tests/gtmode/home_progress.c)
target_link_libraries(gtmode_home_progress PRIVATE moon_career_vehicle ${MOON_CORE_STACK})

if(MOON_TEST_DISC AND MOON_PYTHON)
 add_test(NAME gtmode_award_app COMMAND "${MOON_PYTHON}" "${CMAKE_SOURCE_DIR}/tests/ui/award_presentation_app.py" "$<TARGET_FILE:moonengine>" "$<TARGET_FILE:gtmode_award_presentation>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/award-app-test")
 add_test(NAME gtmode_award_app_negative COMMAND "${MOON_PYTHON}" "${CMAKE_SOURCE_DIR}/tests/ui/award_presentation_app.py" "$<TARGET_FILE:moonengine>" "$<TARGET_FILE:gtmode_award_presentation>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/award-app-negative" --negative)
 set_tests_properties(gtmode_award_app gtmode_award_app_negative PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 90)
 set_tests_properties(gtmode_award_app_negative PROPERTIES WILL_FAIL TRUE)
endif()

if(MOON_TEST_DISC)
 add_test(NAME gtmode_home_progress COMMAND gtmode_home_progress "${MOON_TEST_DISC}")
 add_test(NAME gtmode_home_progress_negative COMMAND gtmode_home_progress "${MOON_TEST_DISC}" --negative)
 set_tests_properties(gtmode_home_progress_negative PROPERTIES WILL_FAIL TRUE)
endif()

add_executable(gtmode_status_start tests/gtmode/status_start.c)
target_link_libraries(gtmode_status_start PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
if(MOON_TEST_DISC)
 add_test(NAME gtmode_status_start COMMAND gtmode_status_start "${MOON_TEST_DISC}")
 add_test(NAME gtmode_status_start_negative COMMAND gtmode_status_start "${MOON_TEST_DISC}" --negative)
 set_tests_properties(gtmode_status_start_negative PROPERTIES WILL_FAIL TRUE)
endif()

add_executable(fitted_parts_test tests/ui/fitted_parts.c src/app/photo_travel.c src/app/video_settings.c src/app/options_store.c src/app/desktop_input.c src/app/discord_presence.c)
add_executable(championship_grid_points tests/ui/championship_grid_points.c src/app/photo_travel.c src/app/video_settings.c src/app/options_store.c src/app/desktop_input.c src/app/discord_presence.c)
target_include_directories(championship_grid_points PRIVATE src/app)
target_link_libraries(championship_grid_points PRIVATE moon_career_vehicle moon_race_sound ${MOON_SDL_STACK} moon_replay_state miniz SDL2::SDL2main)
if(MOON_TEST_DISC)
  add_test(NAME championship_grid_points COMMAND championship_grid_points "${MOON_TEST_DISC}")
  add_test(NAME championship_grid_points_negative COMMAND championship_grid_points "${MOON_TEST_DISC}" --negative)
  set_tests_properties(championship_grid_points_negative PROPERTIES WILL_FAIL TRUE)
endif()

add_executable(replay_rumble tests/ui/replay_rumble.c src/app/photo_travel.c src/app/video_settings.c src/app/options_store.c src/app/desktop_input.c src/app/discord_presence.c)
target_include_directories(replay_rumble PRIVATE src/app)
target_link_libraries(replay_rumble PRIVATE moon_career_vehicle moon_race_sound ${MOON_SDL_STACK} moon_replay_state miniz SDL2::SDL2main)
add_test(NAME replay_rumble COMMAND replay_rumble)
add_test(NAME replay_rumble_negative COMMAND replay_rumble --negative)
set_tests_properties(replay_rumble_negative PROPERTIES WILL_FAIL TRUE)

add_executable(race_options_units tests/ui/race_options_units.c src/app/photo_travel.c src/app/video_settings.c src/app/options_store.c src/app/desktop_input.c src/app/discord_presence.c)
target_include_directories(race_options_units PRIVATE src/app)
target_link_libraries(race_options_units PRIVATE moon_career_vehicle moon_race_sound ${MOON_SDL_STACK} moon_replay_state miniz SDL2::SDL2main)
if(MOON_TEST_DISC)
  add_test(NAME race_options_units COMMAND race_options_units "${MOON_TEST_DISC}")
  add_test(NAME race_options_units_negative COMMAND race_options_units "${MOON_TEST_DISC}" --negative)
  set_tests_properties(race_options_units_negative PROPERTIES WILL_FAIL TRUE)
endif()
target_include_directories(fitted_parts_test PRIVATE src/app)
target_link_libraries(fitted_parts_test PRIVATE moon_career_vehicle moon_race_sound ${MOON_SDL_STACK} moon_replay_state miniz SDL2::SDL2main)
if(MOON_TEST_DISC)
  add_test(NAME fitted_parts COMMAND fitted_parts_test "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/fitted-parts-save")
  add_test(NAME fitted_parts_negative COMMAND fitted_parts_test "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/fitted-parts-negative-save" --negative)
  add_test(NAME fitted_parts_photo_negative COMMAND fitted_parts_test "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/fitted-parts-photo-negative-save" --photo-negative)
  set_tests_properties(fitted_parts_negative fitted_parts_photo_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME fitted_parts_replay_negative COMMAND fitted_parts_test "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/fitted-parts-replay-negative-save" --replay-negative)
  set_tests_properties(fitted_parts_replay_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME fitted_purchase COMMAND gtmode_purchase_menu "${MOON_TEST_DISC}" --customization-purchase)
  add_test(NAME fitted_purchase_negative COMMAND gtmode_purchase_menu "${MOON_TEST_DISC}" --customization-purchase)
  set_tests_properties(fitted_purchase_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_PURCHASE_PART_LABEL_NEGATIVE=1")
endif()

if(MOON_TEST_DISC)
 add_test(NAME car_selection_count_layout COMMAND gtmode_purchase_menu "${MOON_TEST_DISC}" --selection-layout)
 add_test(NAME car_selection_count_layout_negative COMMAND gtmode_purchase_menu "${MOON_TEST_DISC}" --selection-layout)
 set_tests_properties(car_selection_count_layout_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_SELECTION_COUNT_NEGATIVE=1")
endif()

add_test(NAME transition_retained_hold_negative COMMAND menu_tests)
set_tests_properties(transition_retained_hold_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_TRANSITION_HOLD_NEGATIVE=1")

add_test(NAME attract_outgoing_black_negative COMMAND menu_tests)
set_tests_properties(attract_outgoing_black_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_ATTRACT_BLACK_NEGATIVE=1")

if(MOON_TEST_DISC)
 add_test(NAME title_exit_fade COMMAND gtmode_purchase_menu "${MOON_TEST_DISC}" --title-exit)
 add_test(NAME title_exit_fade_negative COMMAND gtmode_purchase_menu "${MOON_TEST_DISC}" --title-exit)
 set_tests_properties(title_exit_fade_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_TITLE_EXIT_NEGATIVE=1")
endif()

if(MOON_TEST_DISC)
  add_test(NAME race_audio_listener_negative COMMAND race_audio_clock "${MOON_TEST_DISC}" --listener-negative)
  set_tests_properties(race_audio_listener_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME arcade_driving_transfer_negative COMMAND split_select_test "${MOON_TEST_DISC}" --driving-negative)
  set_tests_properties(arcade_driving_transfer_negative PROPERTIES WILL_FAIL TRUE)
endif()

if(MOON_TEST_DISC)
  add_test(NAME arcade_manual_driver COMMAND race_audio_clock "${MOON_TEST_DISC}" --manual)
  add_test(NAME arcade_manual_driver_negative COMMAND race_audio_clock "${MOON_TEST_DISC}" --manual-negative)
  set_tests_properties(arcade_manual_driver_negative PROPERTIES WILL_FAIL TRUE)
endif()

add_executable(car_time_zone tests/render/car_time_zone.c)
target_link_libraries(car_time_zone PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
if(MOON_TEST_DISC)
  add_test(NAME car_time_zone COMMAND car_time_zone "${MOON_TEST_DISC}")
  add_test(NAME car_time_zone_negative COMMAND car_time_zone "${MOON_TEST_DISC}" --negative)
  set_tests_properties(car_time_zone_negative PROPERTIES WILL_FAIL TRUE)
endif()
add_executable(car_brake_branch tests/ui/car_brake_branch.c)
target_link_libraries(car_brake_branch PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)

if(MOON_TEST_DISC)
 add_test(NAME car_brake_branch COMMAND car_brake_branch "${MOON_TEST_DISC}")
 add_test(NAME car_brake_branch_negative COMMAND car_brake_branch "${MOON_TEST_DISC}" --negative)
 set_tests_properties(car_brake_branch_negative PROPERTIES WILL_FAIL TRUE)
endif()

add_executable(bspec_career_test tests/ui/bspec_career.c src/app/photo_travel.c src/app/video_settings.c src/app/options_store.c src/app/desktop_input.c src/app/discord_presence.c)
target_include_directories(bspec_career_test PRIVATE src/app)
target_link_libraries(bspec_career_test PRIVATE moon_career_vehicle moon_race_sound ${MOON_SDL_STACK} moon_replay_state miniz SDL2::SDL2main)
if(MOON_TEST_DISC)
  add_test(NAME bspec_career COMMAND bspec_career_test "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/bspec-career-save")
  add_test(NAME bspec_career_negative COMMAND bspec_career_test "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/bspec-career-negative-save" --negative)
  set_tests_properties(bspec_career bspec_career_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 180)
  set_tests_properties(bspec_career_negative PROPERTIES WILL_FAIL TRUE)
endif()
add_executable(gtmode_transmission_test tests/ui/gtmode_transmission.c src/app/photo_travel.c src/app/video_settings.c src/app/options_store.c src/app/desktop_input.c src/app/discord_presence.c)
target_include_directories(gtmode_transmission_test PRIVATE src/app)
target_link_libraries(gtmode_transmission_test PRIVATE moon_career_vehicle moon_race_sound ${MOON_SDL_STACK} moon_replay_state miniz SDL2::SDL2main)
if(MOON_TEST_DISC)
  add_test(NAME gtmode_transmission COMMAND gtmode_transmission_test "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/gtmode-transmission-save")
  add_test(NAME gtmode_transmission_negative COMMAND gtmode_transmission_test "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/gtmode-transmission-negative-save" --negative)
  set_tests_properties(gtmode_transmission_negative PROPERTIES WILL_FAIL TRUE)
endif()

add_executable(arcade_dealer_hover tests/ui/arcade_dealer_hover.c)
target_link_libraries(arcade_dealer_hover PRIVATE ${MOON_CORE_STACK})
if(MOON_TEST_DISC)
  add_test(NAME arcade_dealer_hover COMMAND arcade_dealer_hover "${MOON_TEST_DISC}")
  add_test(NAME arcade_dealer_hover_negative COMMAND arcade_dealer_hover "${MOON_TEST_DISC}" --negative)
  set_tests_properties(arcade_dealer_hover_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME arcade_car_hover_negative COMMAND arcade_dealer_hover "${MOON_TEST_DISC}" --negative --car)
  set_tests_properties(arcade_car_hover_negative PROPERTIES WILL_FAIL TRUE)
endif()
add_executable(arcade_return tests/ui/arcade_return.c)
target_link_libraries(arcade_return PRIVATE ${MOON_CORE_STACK})
if(MOON_TEST_DISC)
  add_test(NAME arcade_return COMMAND arcade_return "${MOON_TEST_DISC}")
  add_test(NAME arcade_return_negative COMMAND arcade_return "${MOON_TEST_DISC}" --negative)
  set_tests_properties(arcade_return_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME arcade_return_cursor_negative COMMAND arcade_return "${MOON_TEST_DISC}" --cursor-negative)
  add_test(NAME arcade_return_list_negative COMMAND arcade_return "${MOON_TEST_DISC}" --list-negative)
  set_tests_properties(arcade_return_cursor_negative arcade_return_list_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME arcade_return_info_negative COMMAND arcade_return "${MOON_TEST_DISC}" --info-negative)
  add_test(NAME arcade_return_index_negative COMMAND arcade_return "${MOON_TEST_DISC}" --index-negative)
  set_tests_properties(arcade_return_info_negative arcade_return_index_negative PROPERTIES WILL_FAIL TRUE)
endif()

if(MOON_TEST_DISC AND EXISTS "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/pregrid_shoulders_orig.c
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --root 0x00235680 --hooks --exclude 0x00233E08,0x00234810,0x00235C00,0x0023DDF0,0x0023F8C8,0x0023FB80,0x00263D28,0x00277438,0x00277850,0x00282968,0x00282D80
      --out ${CMAKE_BINARY_DIR}/pregrid_shoulders_orig.c
    DEPENDS tools/recomp/ee_recomp.py extracted/core.bin splat/asm/core/text.s VERBATIM)
  add_executable(pregrid_shoulders_oracle tests/ui/pregrid_shoulders_oracle.c ${CMAKE_BINARY_DIR}/pregrid_shoulders_orig.c)
  target_link_libraries(pregrid_shoulders_oracle PRIVATE ${MOON_CORE_STACK} moon_ee)
  add_test(NAME pregrid_shoulders_oracle COMMAND pregrid_shoulders_oracle "${MOON_TEST_DISC}" ${CMAKE_SOURCE_DIR}/extracted/core.bin)
  add_test(NAME pregrid_shoulders_oracle_negative COMMAND pregrid_shoulders_oracle "${MOON_TEST_DISC}" ${CMAKE_SOURCE_DIR}/extracted/core.bin --negative)
  set_tests_properties(pregrid_shoulders_oracle_negative PROPERTIES WILL_FAIL TRUE)
endif()

# PC desktop menu input and shared preferences only; no physics suite.
add_test(NAME switch_profile_slow_negative COMMAND switch_profile_test --slow-negative)
set_tests_properties(switch_profile_slow_negative PROPERTIES WILL_FAIL TRUE)
if(MOON_TEST_DISC)
  add_executable(race_afterfire_warmup tests/platform/race_afterfire_warmup.c)
  target_link_libraries(race_afterfire_warmup PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
  add_test(NAME race_afterfire_warmup COMMAND race_afterfire_warmup "${MOON_TEST_DISC}")
  add_test(NAME race_afterfire_warmup_negative COMMAND race_afterfire_warmup "${MOON_TEST_DISC}" --negative)
  set_tests_properties(race_afterfire_warmup_negative PROPERTIES WILL_FAIL TRUE)
endif()
add_executable(switch_scene_options tests/ui/switch_scene_options.c src/app/options_store.c)
target_include_directories(switch_scene_options PRIVATE src/app include)
target_compile_definitions(switch_scene_options PRIVATE MOON_SWITCH_OPTIONS_TEST=1)
add_test(NAME switch_scene_options COMMAND switch_scene_options "${CMAKE_BINARY_DIR}/switch-scene-options")
add_test(NAME switch_scene_options_negative COMMAND switch_scene_options "${CMAKE_BINARY_DIR}/switch-scene-options-negative" --negative)
set_tests_properties(switch_scene_options_negative PROPERTIES WILL_FAIL TRUE)
add_executable(desktop_options_store tests/ui/options_store.c src/app/options_store.c)
target_include_directories(desktop_options_store PRIVATE src/app include)
add_test(NAME desktop_options_store COMMAND desktop_options_store "${CMAKE_BINARY_DIR}/desktop-options-test.ini")
add_test(NAME desktop_options_store_negative COMMAND desktop_options_store "${CMAKE_BINARY_DIR}/desktop-options-negative.ini" --negative)
set_tests_properties(desktop_options_store_negative PROPERTIES WILL_FAIL TRUE)
add_test(NAME scene_options_store_negative COMMAND desktop_options_store "${CMAKE_BINARY_DIR}/scene-options-negative.ini" --scene-negative)
set_tests_properties(scene_options_store_negative PROPERTIES WILL_FAIL TRUE)
add_executable(desktop_pointer tests/ui/desktop_pointer.c src/app/desktop_input.c)
target_include_directories(desktop_pointer PRIVATE src/app src/platform/sdl)
target_compile_definitions(desktop_pointer PRIVATE SDL_MAIN_HANDLED)
target_link_libraries(desktop_pointer PRIVATE SDL2::SDL2-static)
target_link_libraries(desktop_pointer PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
if(MOON_TEST_DISC)
  add_test(NAME desktop_pointer COMMAND desktop_pointer "${MOON_TEST_DISC}")
  add_test(NAME desktop_pointer_negative COMMAND desktop_pointer "${MOON_TEST_DISC}" --negative)
  set_tests_properties(desktop_pointer_negative PROPERTIES WILL_FAIL TRUE)
endif()

if(MOON_TEST_DISC AND MOON_PYTHON)
  add_test(NAME desktop_settings_app COMMAND "${MOON_PYTHON}" "${CMAKE_SOURCE_DIR}/tests/ui/desktop_settings.py"
    "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/desktop-settings-test")
  set_tests_properties(desktop_settings_app PROPERTIES TIMEOUT 240 RUN_SERIAL TRUE)
  add_test(NAME pc_quality_settings COMMAND "${MOON_PYTHON}" "${CMAKE_SOURCE_DIR}/tests/ui/quality_settings.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/pc-quality-settings")
  add_test(NAME pc_quality_settings_negative COMMAND "${MOON_PYTHON}" "${CMAKE_SOURCE_DIR}/tests/ui/quality_settings.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/pc-quality-settings-negative" --negative)
  set_tests_properties(pc_quality_settings pc_quality_settings_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 360)
  set_tests_properties(pc_quality_settings_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME pc_edge_aa_settings COMMAND "${MOON_PYTHON}" "${CMAKE_SOURCE_DIR}/tests/ui/quality_settings.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/pc-edge-aa-settings" --edge-only)
  add_test(NAME pc_edge_aa_settings_negative COMMAND "${MOON_PYTHON}" "${CMAKE_SOURCE_DIR}/tests/ui/quality_settings.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/pc-edge-aa-settings-negative" --edge-only --negative)
  set_tests_properties(pc_edge_aa_settings pc_edge_aa_settings_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 180)
  set_tests_properties(pc_edge_aa_settings_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME pc_volume_settings COMMAND "${MOON_PYTHON}" "${CMAKE_SOURCE_DIR}/tests/ui/quality_settings.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/pc-volume-settings" --volume-only)
  add_test(NAME pc_volume_settings_negative COMMAND "${MOON_PYTHON}" "${CMAKE_SOURCE_DIR}/tests/ui/quality_settings.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/pc-volume-settings-negative" --volume-only --negative)
  set_tests_properties(pc_volume_settings pc_volume_settings_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 180)
  set_tests_properties(pc_volume_settings_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME scene_settings_app COMMAND "${MOON_PYTHON}" "${CMAKE_SOURCE_DIR}/tests/ui/scene_settings.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/scene-settings")
  add_test(NAME scene_settings_app_negative COMMAND "${MOON_PYTHON}" "${CMAKE_SOURCE_DIR}/tests/ui/scene_settings.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/scene-settings-negative" --negative)
  set_tests_properties(scene_settings_app scene_settings_app_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 300)
  set_tests_properties(scene_settings_app_negative PROPERTIES WILL_FAIL TRUE)
endif()

add_executable(desktop_events tests/ui/desktop_events.c)
target_link_libraries(desktop_events PRIVATE ${MOON_SDL_STACK})
add_test(NAME desktop_events COMMAND desktop_events)
add_test(NAME desktop_events_negative COMMAND desktop_events --negative)
set_tests_properties(desktop_events_negative PROPERTIES WILL_FAIL TRUE)

add_executable(desktop_navigation tests/ui/desktop_navigation.c)
add_executable(project_entry_fades tests/ui/project_entry.c)
target_link_libraries(project_entry_fades PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
if(MOON_TEST_DISC)
  add_test(NAME project_entry_fades COMMAND "${MOON_PYTHON}" "${CMAKE_SOURCE_DIR}/tests/ui/project_entry.py" "$<TARGET_FILE:project_entry_fades>" "${MOON_TEST_DISC}")
  add_test(NAME project_entry_fades_negative COMMAND "${MOON_PYTHON}" "${CMAKE_SOURCE_DIR}/tests/ui/project_entry.py" "$<TARGET_FILE:project_entry_fades>" "${MOON_TEST_DISC}")
  set_tests_properties(project_entry_fades_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "MOON_PROJECT_FADE_NEGATIVE=1")
endif()
add_executable(photo_travel_focus tests/ui/photo_travel_focus.c)
target_link_libraries(photo_travel_focus PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
if(MOON_TEST_DISC)
  add_test(NAME photo_travel_focus COMMAND photo_travel_focus "${MOON_TEST_DISC}")
  add_test(NAME photo_travel_focus_negative COMMAND photo_travel_focus "${MOON_TEST_DISC}" --negative)
  set_tests_properties(photo_travel_focus_negative PROPERTIES WILL_FAIL TRUE)
endif()
add_executable(save_keyboard_case tests/ui/save_keyboard_case.c src/app/save_package.c)
target_include_directories(save_keyboard_case PRIVATE src/app src/platform/sdl)
target_link_libraries(save_keyboard_case PRIVATE moon_career_vehicle ${MOON_CORE_STACK} SDL2::SDL2-static miniz)
if(MOON_TEST_DISC)
  add_test(NAME save_keyboard_case COMMAND save_keyboard_case "${MOON_TEST_DISC}")
  add_test(NAME save_keyboard_case_negative COMMAND save_keyboard_case "${MOON_TEST_DISC}" --negative)
  set_tests_properties(save_keyboard_case_negative PROPERTIES WILL_FAIL TRUE)
endif()
add_executable(used_car_stamp tests/ui/used_car_stamp.c)
target_link_libraries(used_car_stamp PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
if(MOON_TEST_DISC)
  add_test(NAME used_car_stamp COMMAND used_car_stamp "${MOON_TEST_DISC}")
  add_test(NAME switch_touch_chip_negative COMMAND used_car_stamp "${MOON_TEST_DISC}")
  set_tests_properties(switch_touch_chip_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "MOON_TOUCH_CHIP_NEGATIVE=1")
  add_test(NAME used_car_stamp_negative COMMAND used_car_stamp "${MOON_TEST_DISC}" --negative)
  set_tests_properties(used_car_stamp_negative PROPERTIES WILL_FAIL TRUE)
endif()
target_link_libraries(desktop_navigation PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
if(MOON_TEST_DISC)
  add_test(NAME desktop_navigation COMMAND desktop_navigation "${MOON_TEST_DISC}")
  add_test(NAME desktop_navigation_negative COMMAND desktop_navigation "${MOON_TEST_DISC}" --negative)
  set_tests_properties(desktop_navigation_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME switch_touch_hover_negative COMMAND desktop_pointer "${MOON_TEST_DISC}")
  set_tests_properties(switch_touch_hover_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "MOON_TOUCH_HOVER_NEGATIVE=1")
  add_test(NAME switch_touch_back_negative COMMAND desktop_pointer "${MOON_TEST_DISC}")
  set_tests_properties(switch_touch_back_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "MOON_TOUCH_BACK_NEGATIVE=1")
  add_test(NAME switch_touch_cursor_negative COMMAND desktop_pointer "${MOON_TEST_DISC}")
  set_tests_properties(switch_touch_cursor_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "MOON_TOUCH_CURSOR_NEGATIVE=1")
  add_test(NAME android_pause_touch_negative COMMAND desktop_navigation "${MOON_TEST_DISC}")
  set_tests_properties(android_pause_touch_negative PROPERTIES WILL_FAIL TRUE RUN_SERIAL TRUE ENVIRONMENT "MOON_PAUSE_TOUCH_NEGATIVE=1")
  add_test(NAME switch_touch_negative COMMAND desktop_navigation "${MOON_TEST_DISC}")
  set_tests_properties(switch_touch_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "MOON_TOUCH_NEGATIVE=1")
endif()

add_test(NAME desktop_map_option_negative COMMAND desktop_options_store "${CMAKE_BINARY_DIR}/desktop-map-negative.ini" --map-negative)
add_test(NAME desktop_hints_option_negative COMMAND desktop_options_store "${CMAKE_BINARY_DIR}/desktop-hints-negative.ini" --hints-negative)
set_tests_properties(desktop_map_option_negative desktop_hints_option_negative PROPERTIES WILL_FAIL TRUE)

if(TARGET moon_recomp_physics AND MOON_TEST_DISC)
  add_executable(disc_arithmetic_oracle tests/ui/disc_arithmetic_oracle.c tests/physics/replay_arithmetic.c)
  target_include_directories(disc_arithmetic_oracle PRIVATE src/ee src/app)
  target_link_libraries(disc_arithmetic_oracle PRIVATE moon_recomp_physics moon_ee moon_replay_state moon_physics)
  if(MSVC)
    target_compile_options(disc_arithmetic_oracle PRIVATE /fp:strict)
  else()
    target_compile_options(disc_arithmetic_oracle PRIVATE -ffp-contract=off -frounding-math)
  endif()
  add_test(NAME disc_replay_arithmetic COMMAND disc_arithmetic_oracle "${MOON_TEST_DISC}")
  add_test(NAME disc_replay_arithmetic_negative COMMAND disc_arithmetic_oracle "${MOON_TEST_DISC}")
  set_tests_properties(disc_replay_arithmetic_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "MOON_REPLAY_ARITHMETIC_NEGATIVE=1")
  add_executable(disc_lap_oracle tests/ui/disc_arithmetic_oracle.c tests/physics/replay_arithmetic.c)
  target_include_directories(disc_lap_oracle PRIVATE src/ee src/app)
  target_link_libraries(disc_lap_oracle PRIVATE moon_recomp_physics moon_ee moon_replay_state moon_physics)
  add_test(NAME disc_replay_lap_event COMMAND disc_lap_oracle "${MOON_TEST_DISC}" --lap-event)
  add_test(NAME disc_replay_lap_event_negative COMMAND disc_lap_oracle "${MOON_TEST_DISC}" --lap-event)
  set_tests_properties(disc_replay_lap_event_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "MOON_REPLAY_ARITHMETIC_NEGATIVE=1")
endif()

if(MOON_TEST_DISC)
  add_test(NAME disc_theater_complete COMMAND disc_demo_probe "${MOON_TEST_DISC}" --theater-complete)
  add_test(NAME disc_theater_complete_negative COMMAND disc_demo_probe "${MOON_TEST_DISC}" --theater-complete-negative)
  set_tests_properties(disc_theater_complete disc_theater_complete_negative PROPERTIES TIMEOUT 600)
  set_tests_properties(disc_theater_complete_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME gtmode_license_gold_catalog COMMAND gtmode_license_teacher "${MOON_TEST_DISC}" --gold-catalog)
  add_test(NAME gtmode_license_gold_catalog_negative COMMAND gtmode_license_teacher "${MOON_TEST_DISC}" --gold-catalog)
  set_tests_properties(gtmode_license_gold_catalog gtmode_license_gold_catalog_negative PROPERTIES TIMEOUT 600)
  set_tests_properties(gtmode_license_gold_catalog_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_LICENSE_TEACHER_NEGATIVE=1")
endif()

add_test(NAME replay_codegen COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/physics/replay_codegen.py")
add_test(NAME replay_codegen_negative COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/physics/replay_codegen.py" --negative)
set_tests_properties(replay_codegen_negative PROPERTIES WILL_FAIL TRUE)

add_executable(physics_math_modes tests/physics/float_modes.c)
target_link_libraries(physics_math_modes PRIVATE moon_physics)
if(MOON_TEST_DISC)
  add_test(NAME physics_math_modes COMMAND physics_math_modes "${MOON_TEST_DISC}")
  add_test(NAME physics_math_modes_negative COMMAND physics_math_modes "${MOON_TEST_DISC}" --negative)
  set_tests_properties(physics_math_modes_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME physics_saved_ghost COMMAND tt_game_probe "${MOON_TEST_DISC}" tsukuba ka_01 --saved)
  add_test(NAME physics_saved_ghost_negative COMMAND tt_game_probe "${MOON_TEST_DISC}" tsukuba ka_01 --saved --negative)
  set_tests_properties(physics_saved_ghost_negative PROPERTIES WILL_FAIL TRUE)
endif()

# Every original-code comparison of vehicle state uses the same EE arithmetic.
# Presentation-only tests (pit poses, image LUTs and Setting panel motion) retain
# their separate host reference; they do not compare the vehicle solver.
if(MOON_RECOMP_TESTS)
  add_library(moon_ee_physics_math OBJECT tests/physics/replay_arithmetic.c)
  target_include_directories(moon_ee_physics_math PRIVATE src/ee)
  set(physics_ee_oracles
    disc_setup_oracle gtmode_logger_oracle gtmode_logger_drive
    gtmode_aspiration_oracle gtmode_training_oracle
    gtmode_mission_session_oracle gtmode_license_session_oracle gtmode_license_lines_oracle
    gtmode_active_setup_oracle gtmode_enemy_indices_oracle gtmode_paint_oracle
    gtmode_grid_paint_oracle gtmode_event_tyre_oracle gtmode_start_oracle
    gtmode_start_select_oracle gtmode_event_cycle_oracle gtmode_endurance_queries
    gtmode_event_wear_oracle gtmode_event_distance_oracle gtmode_purchase_oracle
    gtmode_tuned_oracle gtmode_tune_power_probe gtmode_setting_curve
    gtmode_setting_auto gtmode_setting_units split_battle_oracle course_objects_oracle)
  foreach(oracle IN LISTS physics_ee_oracles)
    if(TARGET ${oracle})
      target_sources(${oracle} PRIVATE $<TARGET_OBJECTS:moon_ee_physics_math>)
    endif()
  endforeach()
endif()

if(TARGET disc_arithmetic_oracle)
  add_test(NAME physics_bspec_choices COMMAND disc_arithmetic_oracle "${MOON_TEST_DISC}" --bspec-choices)
  add_test(NAME physics_bspec_choices_negative COMMAND disc_arithmetic_oracle "${MOON_TEST_DISC}" --bspec-choices)
  set_tests_properties(physics_bspec_choices_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "MOON_REPLAY_ARITHMETIC_NEGATIVE=1")
endif()
add_executable(physics_step_cost tools/physics_step_cost.c)
target_link_libraries(physics_step_cost PRIVATE moon_physics)

# External pad-driver policies, not vehicle parameters or softened success gates.
# The old commands lose identically in native/original EE simulation.
if(TEST gtmode_mission_cycle AND TEST gtmode_mission_cycle_22)
  set_tests_properties(gtmode_mission_cycle PROPERTIES ENVIRONMENT "GT4_MISSION_GAIN=0.8")
  set_tests_properties(gtmode_mission_cycle_negative PROPERTIES ENVIRONMENT "GT4_MISSION_GAIN=0.8;GT4_MISSION_CYCLE_NEGATIVE=1")
  set_tests_properties(gtmode_mission_cycle_22 PROPERTIES ENVIRONMENT "GT4_MISSION_DRAFT=1;GT4_MISSION_PASSING_GAP=12;GT4_MISSION_GAIN=1.2")
  set_tests_properties(gtmode_mission_cycle_22_negative PROPERTIES ENVIRONMENT "GT4_MISSION_DRAFT=1;GT4_MISSION_PASSING_GAP=12;GT4_MISSION_GAIN=1.2;GT4_MISSION_CYCLE_NEGATIVE=1")
endif()

add_executable(physics_float_fast tests/physics/float_fast.c tests/physics/float_reference.c)
target_link_libraries(physics_float_fast PRIVATE ${MOON_CORE_STACK})
add_test(NAME physics_float_fast COMMAND physics_float_fast)
add_test(NAME physics_float_fast_negative COMMAND physics_float_fast --negative)
set_tests_properties(physics_float_fast_negative PROPERTIES WILL_FAIL TRUE)

if(MOON_TEST_DISC)
  foreach(mode teacher-catalog attract imported logger)
    string(REPLACE "-" "_" test_mode "${mode}")
    add_test(NAME replay_${test_mode}_e2e COMMAND disc_demo_probe "${MOON_TEST_DISC}" --${mode})
    add_test(NAME replay_${test_mode}_e2e_negative COMMAND disc_demo_probe "${MOON_TEST_DISC}" --${mode}-negative)
    set_tests_properties(replay_${test_mode}_e2e replay_${test_mode}_e2e_negative PROPERTIES TIMEOUT 900 RUN_SERIAL TRUE)
    set_tests_properties(replay_${test_mode}_e2e_negative PROPERTIES WILL_FAIL TRUE)
  endforeach()
endif()

# Direct object linkage also covers tools inheriting the full physics oracle;
# archive order must never silently select moon_ee's historical host helpers.
if(TARGET moon_ee_physics_math)
  get_property(ee_audit_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
  foreach(oracle IN LISTS ee_audit_targets)
    get_target_property(oracle_type ${oracle} TYPE)
    get_target_property(oracle_links ${oracle} LINK_LIBRARIES)
    if(oracle_type STREQUAL "EXECUTABLE" AND "moon_recomp_physics" IN_LIST oracle_links)
      get_target_property(oracle_sources ${oracle} SOURCES)
      if(NOT "tests/physics/replay_arithmetic.c" IN_LIST oracle_sources)
        target_sources(${oracle} PRIVATE $<TARGET_OBJECTS:moon_ee_physics_math>)
      endif()
    endif()
  endforeach()
endif()

if(TARGET disc_arithmetic_oracle)
  foreach(mission 9 22)
    if(mission EQUAL 9)
      set(mission_code l0m0009)
      set(mission_input "GT4_MISSION_GAIN=0.8")
    else()
      set(mission_code l0m0022)
      set(mission_input "GT4_MISSION_DRAFT=1;GT4_MISSION_PASSING_GAP=12;GT4_MISSION_GAIN=1.2")
    endif()
    add_test(NAME physics_mission_${mission}_original COMMAND disc_arithmetic_oracle "${MOON_TEST_DISC}" --mission=${mission_code})
    add_test(NAME physics_mission_${mission}_original_negative COMMAND disc_arithmetic_oracle "${MOON_TEST_DISC}" --mission=${mission_code})
    set_tests_properties(physics_mission_${mission}_original PROPERTIES TIMEOUT 600 ENVIRONMENT "${mission_input};MOON_MISSION_EXPECT_RANK=1")
    set_tests_properties(physics_mission_${mission}_original_negative PROPERTIES TIMEOUT 600 WILL_FAIL TRUE ENVIRONMENT "${mission_input};MOON_REPLAY_ARITHMETIC_NEGATIVE=1")
  endforeach()
endif()

# Source-derived regulation background geometry; never commit generated EE code.
if(EXISTS "${CMAKE_SOURCE_DIR}/extracted/core.bin" AND MOON_TEST_DISC)
  set(gen_regulation_geometry "${CMAKE_BINARY_DIR}/gen/regulation_geometry.c")
  add_custom_command(OUTPUT ${gen_regulation_geometry}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_regulation_geometry} --root 0x002854F8 --extra-roots 0x002B8778,0x00215FE0,0x004A2DF8
      --exclude 0x0025B498,0x002030A0,0x002030E8,0x00203118,0x00263C40,0x002073D8,0x0025BF60,0x00206868,0x0025C300,0x0025B500,0x0025B538,0x00207378,0x00106298,0x001062B8,0x004A2E98 --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(regulation_geometry_oracle tests/ui/regulation_geometry_oracle.c ${gen_regulation_geometry})
  target_link_libraries(regulation_geometry_oracle PRIVATE moon_ee moon_career_vehicle ${MOON_CORE_STACK})
  add_test(NAME regulation_geometry_original COMMAND regulation_geometry_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin" "${MOON_TEST_DISC}")
  add_test(NAME regulation_geometry_original_negative COMMAND regulation_geometry_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin" "${MOON_TEST_DISC}")
  set_tests_properties(regulation_geometry_original_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_REGULATION_DIVIDERS_NEGATIVE=1")
  add_test(NAME regulation_scissor_original_negative COMMAND regulation_geometry_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin" "${MOON_TEST_DISC}")
  set_tests_properties(regulation_scissor_original_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_REGULATION_SCISSOR_NEGATIVE=1")
endif()


add_executable(switch_touch_contact tests/platform/touch_contact.c)
target_include_directories(switch_touch_contact PRIVATE src/platform/sdl)
target_link_libraries(switch_touch_contact PRIVATE SDL2::SDL2)
add_test(NAME switch_touch_contact COMMAND switch_touch_contact)
add_test(NAME switch_touch_contact_negative COMMAND switch_touch_contact --negative)
set_tests_properties(switch_touch_contact_negative PROPERTIES WILL_FAIL TRUE)

add_executable(first_run_dialog tests/ui/first_run_dialog.c src/app/save_package.c)
target_include_directories(first_run_dialog PRIVATE src/app src/platform/sdl)
target_link_libraries(first_run_dialog PRIVATE moon_career_vehicle ${MOON_CORE_STACK} SDL2::SDL2-static miniz)
if(MOON_TEST_DISC)
  add_test(NAME first_run_dialog COMMAND first_run_dialog "${MOON_TEST_DISC}")
  add_test(NAME first_run_dialog_negative COMMAND first_run_dialog "${MOON_TEST_DISC}" --negative)
  set_tests_properties(first_run_dialog_negative PROPERTIES WILL_FAIL TRUE)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  set(gen_menu_navigation "${CMAKE_BINARY_DIR}/gen/menu_navigation.c")
  add_custom_command(OUTPUT ${gen_menu_navigation}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --root 0x0026F2B8 --extra-roots 0x0026F630,0x0026F868 --exclude 0x005DF950 --out ${gen_menu_navigation}
    DEPENDS tools/recomp/ee_recomp.py extracted/core.bin splat/asm/core/text.s)
  add_executable(menu_navigation_oracle tests/ui/menu_navigation_oracle.c ${gen_menu_navigation})
  target_link_libraries(menu_navigation_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  add_test(NAME menu_navigation_oracle COMMAND menu_navigation_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  add_test(NAME menu_navigation_oracle_negative COMMAND menu_navigation_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin" --negative)
  set_tests_properties(menu_navigation_oracle_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy")
  set_tests_properties(menu_navigation_oracle PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy")
endif()

add_executable(menu_directional_navigation tests/ui/menu_navigation.c)
target_link_libraries(menu_directional_navigation PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
if(MOON_TEST_DISC)
  add_test(NAME menu_directional_navigation COMMAND menu_directional_navigation "${MOON_TEST_DISC}")
  add_test(NAME menu_directional_navigation_negative COMMAND menu_directional_navigation "${MOON_TEST_DISC}" --negative)
  set_tests_properties(menu_directional_navigation_negative PROPERTIES WILL_FAIL TRUE)
  set_tests_properties(menu_directional_navigation menu_directional_navigation_negative PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy")
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  set(gen_menu_collect "${CMAKE_BINARY_DIR}/gen/menu_collect.c")
  add_custom_command(OUTPUT ${gen_menu_collect}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --root 0x0026F9E0 --hooks
      --exclude 0x00265EA8,0x0025B498,0x00267F70,0x00268228,0x005DFB88,0x005DF950,0x00265DC8,0x002661C8,0x00206880,0x00206868,0x0025B2B0,0x0025B310,0x0025C300
      --out ${gen_menu_collect}
    DEPENDS tools/recomp/ee_recomp.py extracted/core.bin splat/asm/core/text.s)
  add_executable(menu_navigation_collect tests/ui/menu_navigation_collect.c ${gen_menu_collect})
  target_link_libraries(menu_navigation_collect PRIVATE moon_ee moon_career_vehicle ${MOON_CORE_STACK})
  add_test(NAME menu_navigation_collect COMMAND menu_navigation_collect "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  add_test(NAME menu_navigation_collect_negative COMMAND menu_navigation_collect "${CMAKE_SOURCE_DIR}/extracted/core.bin" --negative)
  set_tests_properties(menu_navigation_collect_negative PROPERTIES WILL_FAIL TRUE)
  set_tests_properties(menu_navigation_collect menu_navigation_collect_negative PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy")
endif()

# Portable personal-preset interchange, isolated from user preferences.
add_executable(personal_presets tests/ui/personal_presets.c src/app/options_store.c)
target_include_directories(personal_presets PRIVATE src/app include)
add_test(NAME personal_presets COMMAND personal_presets "${CMAKE_BINARY_DIR}/personal-presets-test.moonpreset")
add_test(NAME personal_presets_negative COMMAND personal_presets "${CMAKE_BINARY_DIR}/personal-presets-negative.moonpreset" --negative)
set_tests_properties(personal_presets_negative PROPERTIES WILL_FAIL TRUE)

add_executable(race_display_options tests/ui/race_display_options.c)
target_link_libraries(race_display_options PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
if(MOON_TEST_DISC)
  add_test(NAME race_display_options COMMAND race_display_options "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/race-display.options")
  add_test(NAME race_display_options_negative COMMAND race_display_options "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/race-display-negative.options" --negative)
  set_tests_properties(race_display_options_negative PROPERTIES WILL_FAIL TRUE)
  set_tests_properties(race_display_options race_display_options_negative PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy")
endif()

if(MOON_TEST_DISC)
  add_test(NAME custom_music_mixed COMMAND custom_music_tests "${CMAKE_BINARY_DIR}/custom-music-mixed" --mixed "${MOON_TEST_DISC}")
  if(WIN32)
    add_test(NAME custom_music_formats COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/custom_music_formats.py" "$<TARGET_FILE:custom_music_tests>" "${CMAKE_BINARY_DIR}/custom-music-formats")
  endif()
  add_test(NAME custom_music_ui COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/custom_music_ui.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/custom-music-ui")
  add_test(NAME custom_music_folders COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/custom_music_folders.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/custom-music-folders")
  add_test(NAME custom_music_folders_negative COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/custom_music_folders.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/custom-music-folders-negative" --negative)
  set_tests_properties(custom_music_folders_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME launcher_guide_ui COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/launcher_guide.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/launcher-guide-ui")
  add_test(NAME launcher_options_guide COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/launcher_options_guide.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/launcher-options-guide")
  add_test(NAME launcher_options_guide_negative COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/launcher_options_guide.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/launcher-options-guide-negative" --negative)
  set_tests_properties(launcher_options_guide_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME launcher_scene COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/launcher_scene.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/launcher-scene")
  add_test(NAME launcher_scene_negative COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/launcher_scene.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/launcher-scene-negative" --negative)
  set_tests_properties(launcher_scene_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME launcher_prints COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/launcher_prints.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/launcher-prints")
  add_test(NAME launcher_prints_negative COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/launcher_prints.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/launcher-prints-negative" --negative)
  set_tests_properties(launcher_prints_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME pc_settings_section COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/pc_settings_section.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/pc-settings-section")
  add_test(NAME pc_settings_section_negative COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/pc_settings_section.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/pc-settings-section-negative" --negative)
  set_tests_properties(pc_settings_section_negative PROPERTIES WILL_FAIL TRUE)
  add_test(NAME personal_presets_ui COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/personal_presets_ui.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/personal-presets-ui")
  add_test(NAME personal_presets_ui_negative COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/personal_presets_ui.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/personal-presets-ui-negative" --negative)
  set_tests_properties(personal_presets_ui_negative PROPERTIES WILL_FAIL TRUE)
endif()

if(MOON_TEST_DISC)
  add_test(NAME save_transfer_negative COMMAND profile_storage_test "${MOON_TEST_DISC}" --transfer-negative)
  set_tests_properties(save_transfer_negative PROPERTIES WILL_FAIL TRUE)
endif()

add_executable(hud_detail tests/ui/hud_detail.c)
target_link_libraries(hud_detail PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
if(MOON_TEST_DISC)
  add_test(NAME hud_detail COMMAND hud_detail "${MOON_TEST_DISC}")
  add_test(NAME hud_detail_negative COMMAND hud_detail "${MOON_TEST_DISC}" --negative)
  set_tests_properties(hud_detail_negative PROPERTIES WILL_FAIL TRUE)
  set_tests_properties(hud_detail hud_detail_negative PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy")
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  set(gen_hud_steering "${CMAKE_BINARY_DIR}/gen/hud_steering.c")
  add_custom_command(OUTPUT ${gen_hud_steering}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --root 0x003A5D30 --extra-roots 0x003A1EE0 --hooks
      --exclude 0x003A1E10,0x004A53F8,0x004A5400,0x004A7844,0x004A2808,0x004AA1D8,0x004A7890,0x004A79D8,0x004A2C58,0x004A2B10,0x00499728,0x0049CC48,0x003AF6E0,0x003AFD78
      --out ${gen_hud_steering}
    DEPENDS tools/recomp/ee_recomp.py extracted/core.bin splat/asm/core/text.s)
  add_executable(hud_steering_oracle tests/ui/hud_steering_oracle.c ${gen_hud_steering})
  target_link_libraries(hud_steering_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  add_test(NAME hud_steering_oracle COMMAND hud_steering_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  add_test(NAME hud_steering_oracle_negative COMMAND hud_steering_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin" --negative)
  set_tests_properties(hud_steering_oracle_negative PROPERTIES WILL_FAIL TRUE)
  set_tests_properties(hud_steering_oracle hud_steering_oracle_negative PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy")
endif()

add_executable(save_package_tests tests/ui/save_package.c src/app/save_package.c)
target_include_directories(save_package_tests PRIVATE src/app src/core)
target_link_libraries(save_package_tests PRIVATE ${MOON_CORE_STACK} miniz)
add_test(NAME save_package COMMAND save_package_tests "${CMAKE_BINARY_DIR}/save-package-tests")
add_test(NAME save_package_negative COMMAND save_package_tests "${CMAKE_BINARY_DIR}/save-package-negative-tests" --negative)
set_tests_properties(save_package_negative PROPERTIES WILL_FAIL TRUE)

if(EXISTS "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  set(gen_pack_reverse "${CMAKE_BINARY_DIR}/gen/pack_reverse.c")
  add_custom_command(OUTPUT ${gen_pack_reverse}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --root 0x002E99E0 --hooks
      --exclude 0x002E9600,0x002E96C0,0x002E9790,0x002E97B8,0x002E94E0,0x00206868,0x00263C40,0x002073D8,0x002E97E0,0x002E9540,0x002E95A0,0x002E9570,0x002E95D0,0x002E9830,0x00207378,0x0025C300
      --out ${gen_pack_reverse}
    DEPENDS tools/recomp/ee_recomp.py extracted/core.bin splat/asm/core/text.s)
  add_executable(pack_reverse_oracle tests/ui/pack_reverse_oracle.c ${gen_pack_reverse})
  target_link_libraries(pack_reverse_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  add_test(NAME pack_reverse_oracle COMMAND pack_reverse_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  add_test(NAME pack_reverse_oracle_negative COMMAND pack_reverse_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin" --negative)
  set_tests_properties(pack_reverse_oracle_negative PROPERTIES WILL_FAIL TRUE)
  set_tests_properties(pack_reverse_oracle pack_reverse_oracle_negative PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy")
endif()

add_executable(pack_reverse tests/ui/pack_reverse.c)
target_link_libraries(pack_reverse PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
if(MOON_TEST_DISC)
  add_test(NAME pack_reverse COMMAND pack_reverse "${MOON_TEST_DISC}")
  add_test(NAME pack_reverse_negative COMMAND pack_reverse "${MOON_TEST_DISC}" --negative)
  set_tests_properties(pack_reverse_negative PROPERTIES WILL_FAIL TRUE)
  set_tests_properties(pack_reverse pack_reverse_negative PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy")
endif()

if(EXISTS "${MOON_TEST_DISC}")
  add_test(NAME save_package_ui COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/save_package_ui.py" "$<TARGET_FILE:moonengine>" "$<TARGET_FILE:profile_storage_test>" "$<TARGET_FILE:save_package_tests>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/save-package-ui-test")
  add_test(NAME save_package_ui_negative COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/save_package_ui.py" "$<TARGET_FILE:moonengine>" "$<TARGET_FILE:profile_storage_test>" "$<TARGET_FILE:save_package_tests>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/save-package-ui-negative-test" --negative)
  set_tests_properties(save_package_ui_negative PROPERTIES WILL_FAIL TRUE)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  set(gen_hud_name "${CMAKE_BINARY_DIR}/gen/hud_name.c")
  add_custom_command(OUTPUT ${gen_hud_name}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --root 0x003A9780 --extra-roots 0x003A4E00,0x003A99A8,0x0039CCF0,0x003A1970 --hooks
      --exclude 0x003A18A8,0x005A48D8,0x003AEC80,0x003AEC90 --out ${gen_hud_name}
    DEPENDS tools/recomp/ee_recomp.py extracted/core.bin splat/asm/core/text.s)
  add_executable(hud_name_oracle tests/ui/hud_name_oracle.c ${gen_hud_name})
  target_link_libraries(hud_name_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  add_test(NAME hud_name_oracle COMMAND hud_name_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  add_test(NAME hud_name_oracle_negative COMMAND hud_name_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin" --negative)
  set_tests_properties(hud_name_oracle_negative PROPERTIES WILL_FAIL TRUE)
  set_tests_properties(hud_name_oracle hud_name_oracle_negative PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy")
endif()

add_executable(hud_name tests/ui/hud_name.c)
target_link_libraries(hud_name PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
if(MOON_TEST_DISC)
  add_test(NAME hud_name COMMAND hud_name "${MOON_TEST_DISC}")
  add_test(NAME hud_name_negative COMMAND hud_name "${MOON_TEST_DISC}" --negative)
  set_tests_properties(hud_name_negative PROPERTIES WILL_FAIL TRUE)
  set_tests_properties(hud_name hud_name_negative PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy")
endif()

add_executable(native_ui_queries tests/ui/native_ui_queries.c)
target_link_libraries(native_ui_queries PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
if(MOON_TEST_DISC)
  add_test(NAME native_ui_queries COMMAND native_ui_queries "${MOON_TEST_DISC}")
  add_test(NAME native_ui_queries_negative COMMAND native_ui_queries "${MOON_TEST_DISC}" --negative)
  set_tests_properties(native_ui_queries_negative PROPERTIES WILL_FAIL TRUE)
  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/ui-audit-no-mods")
  set_tests_properties(native_ui_queries native_ui_queries_negative PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_OVERLAY=${CMAKE_BINARY_DIR}/ui-audit-no-mods")
endif()

add_test(NAME ui_tour_report COMMAND "${MOON_PYTHON}" "${CMAKE_SOURCE_DIR}/tests/ui/ui_tour_report.py")
add_test(NAME ui_tour_report_negative COMMAND "${MOON_PYTHON}" "${CMAKE_SOURCE_DIR}/tests/ui/ui_tour_report.py" --negative)
set_tests_properties(ui_tour_report_negative PROPERTIES WILL_FAIL TRUE)
set_tests_properties(ui_tour_report ui_tour_report_negative PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy")

add_executable(option_items tests/ui/option_items.c)
target_link_libraries(option_items PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
if(MOON_TEST_DISC)
  add_test(NAME option_items COMMAND option_items "${MOON_TEST_DISC}")
  add_test(NAME option_items_negative COMMAND option_items "${MOON_TEST_DISC}" --negative)
  set_tests_properties(option_items_negative PROPERTIES WILL_FAIL TRUE)
  set_tests_properties(option_items option_items_negative PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_OVERLAY=${CMAKE_BINARY_DIR}/ui-audit-no-mods")
endif()

add_executable(ui_tour tests/ui/ui_tour.c)
target_link_libraries(ui_tour PRIVATE moon_race_sound moon_career_vehicle ${MOON_SDL_STACK} moon_physics miniz)
if(MOON_TEST_DISC)
  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/ui-tour-proof")
  add_test(NAME ui_tour_cursor COMMAND ui_tour "${MOON_TEST_DISC}" cursor ConfirmDialog --shot "${CMAKE_BINARY_DIR}/ui-tour-proof/dialog.png")
  add_test(NAME ui_tour_root_negative COMMAND ui_tour "${MOON_TEST_DISC}" cursor UnknownRoot --shot "${CMAKE_BINARY_DIR}/ui-tour-proof/missing.png")
  set_tests_properties(ui_tour_root_negative PROPERTIES WILL_FAIL TRUE)
  set_tests_properties(ui_tour_cursor PROPERTIES PASS_REGULAR_EXPRESSION "UI_TOUR requested=cursor/ConfirmDialog active=cursor/ConfirmDialog")
  set_tests_properties(ui_tour_cursor ui_tour_root_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 30 ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;MOON_GRAPHICS_API=opengl;MOON_OVERLAY=${CMAKE_BINARY_DIR}/ui-audit-no-mods")
endif()


add_executable(save_export_visibility tests/ui/save_export_visibility.c src/app/save_package.c)
target_include_directories(save_export_visibility PRIVATE src/app src/platform/sdl)
target_link_libraries(save_export_visibility PRIVATE moon_career_vehicle ${MOON_CORE_STACK} SDL2::SDL2-static miniz)
if(MOON_TEST_DISC)
  add_test(NAME save_export_visibility COMMAND save_export_visibility "${MOON_TEST_DISC}")
  add_test(NAME save_export_visibility_negative COMMAND save_export_visibility "${MOON_TEST_DISC}" --negative)
  set_tests_properties(save_export_visibility_negative PROPERTIES WILL_FAIL TRUE)
  set_tests_properties(save_export_visibility save_export_visibility_negative PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy")
endif()

add_executable(scroll_takeover tests/ui/scroll_takeover.c)
target_link_libraries(scroll_takeover PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
if(MOON_TEST_DISC)
  add_test(NAME scroll_takeover COMMAND scroll_takeover "${MOON_TEST_DISC}")
  add_test(NAME scroll_takeover_negative COMMAND scroll_takeover "${MOON_TEST_DISC}" --negative)
  set_tests_properties(scroll_takeover_negative PROPERTIES WILL_FAIL TRUE)
  set_tests_properties(scroll_takeover scroll_takeover_negative PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy")
endif()

add_executable(result_name tests/ui/result_name.c src/app/race_audio.c src/app/video_settings.c src/app/options_store.c src/app/desktop_input.c src/app/discord_presence.c)
target_include_directories(result_name PRIVATE src/app)
target_link_libraries(result_name PRIVATE moon_replay_state moon_race_sound moon_physics ${MOON_SDL_STACK} miniz SDL2::SDL2main)
if(MOON_TEST_DISC)
  add_test(NAME result_name COMMAND result_name "${MOON_TEST_DISC}")
  add_test(NAME result_name_negative COMMAND result_name "${MOON_TEST_DISC}" --negative)
  set_tests_properties(result_name_negative PROPERTIES WILL_FAIL TRUE)
  set_tests_properties(result_name result_name_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 90 ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;MOON_GRAPHICS_API=opengl;MOON_OVERLAY=${CMAKE_BINARY_DIR}/ui-audit-no-mods")
endif()

if(MOON_TEST_DISC)
  add_test(NAME launcher_layout COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/launcher_layout.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/launcher-layout")
  add_test(NAME launcher_layout_negative COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/launcher_layout.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/launcher-layout-negative" --negative)
  set_tests_properties(launcher_layout_negative PROPERTIES WILL_FAIL TRUE)
  set_tests_properties(launcher_layout launcher_layout_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 300)
endif()

add_executable(daylight_baked tests/render/daylight_baked.c)
target_include_directories(daylight_baked PRIVATE src/platform/sdl)
target_link_libraries(daylight_baked PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
foreach(api IN ITEMS opengl vulkan)
  add_test(NAME daylight_baked_${api} COMMAND daylight_baked)
  add_test(NAME daylight_baked_${api}_negative COMMAND daylight_baked --negative)
  set_tests_properties(daylight_baked_${api}_negative PROPERTIES WILL_FAIL TRUE)
  set_tests_properties(daylight_baked_${api} daylight_baked_${api}_negative PROPERTIES RUN_SERIAL TRUE ENVIRONMENT "MOON_GRAPHICS_API=${api};SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1")
endforeach()


add_executable(daylight_layers tests/render/daylight_layers.c)
target_include_directories(daylight_layers PRIVATE src/platform/sdl)
target_link_libraries(daylight_layers PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
foreach(api IN ITEMS opengl vulkan)
  add_test(NAME daylight_layers_${api} COMMAND daylight_layers)
  add_test(NAME daylight_layers_${api}_negative COMMAND daylight_layers --negative)
  set_tests_properties(daylight_layers_${api}_negative PROPERTIES WILL_FAIL TRUE)
  set_tests_properties(daylight_layers_${api} daylight_layers_${api}_negative PROPERTIES RUN_SERIAL TRUE ENVIRONMENT "MOON_GRAPHICS_API=${api};SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1")
endforeach()

if(MOON_TEST_DISC)
  add_test(NAME launcher_panel_fade COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/launcher_panel_fade.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/launcher-panel-fade")
  add_test(NAME launcher_panel_fade_negative COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/launcher_panel_fade.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/launcher-panel-fade-negative" --negative)
  set_tests_properties(launcher_panel_fade_negative PROPERTIES WILL_FAIL TRUE)
  set_tests_properties(launcher_panel_fade launcher_panel_fade_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 300)
endif()

add_executable(music_folder_tests tests/ui/music_folder.c)
target_include_directories(music_folder_tests PRIVATE src/app)
target_link_libraries(music_folder_tests PRIVATE SDL2::SDL2 ${MOON_CORE_STACK})
add_test(NAME music_folder COMMAND music_folder_tests "${CMAKE_BINARY_DIR}/music-folder")
add_test(NAME music_folder_negative COMMAND music_folder_tests "${CMAKE_BINARY_DIR}/music-folder-negative" --negative)
set_tests_properties(music_folder_negative PROPERTIES WILL_FAIL TRUE)

add_test(NAME custom_music_sequence COMMAND custom_music_tests "${CMAKE_BINARY_DIR}/music-sequence" --sequence)
add_test(NAME custom_music_sequence_shuffle COMMAND custom_music_tests "${CMAKE_BINARY_DIR}/music-sequence-shuffle" --sequence-shuffle)
add_test(NAME custom_music_sequence_negative COMMAND custom_music_tests "${CMAKE_BINARY_DIR}/music-sequence-negative" --sequence-negative)
set_tests_properties(custom_music_sequence_negative PROPERTIES WILL_FAIL TRUE)
set_tests_properties(custom_music_sequence custom_music_sequence_shuffle custom_music_sequence_negative PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy")


if(TARGET race_lights_oracle)
  add_test(NAME race_lamp_shadows_negative COMMAND race_lights_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin --shadow-negative)
  set_tests_properties(race_lamp_shadows_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy")
endif()

add_executable(vulkan_blit_clip_tests tests/render/vulkan_blit_clip.cpp)
target_include_directories(vulkan_blit_clip_tests PRIVATE src/platform/sdl)
add_test(NAME vulkan_blit_clip COMMAND vulkan_blit_clip_tests)
add_test(NAME vulkan_blit_clip_negative COMMAND vulkan_blit_clip_tests --negative)
set_tests_properties(vulkan_blit_clip_negative PROPERTIES WILL_FAIL TRUE)
if(MOON_TEST_DISC)
  add_test(NAME launcher_guide_resize COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/launcher_guide_resize.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/guide-resize-test")
  add_test(NAME launcher_guide_resize_negative COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/launcher_guide_resize.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/guide-resize-negative" --negative)
  set_tests_properties(launcher_guide_resize_negative PROPERTIES WILL_FAIL TRUE)
  set_tests_properties(launcher_guide_resize launcher_guide_resize_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 300)
endif()

add_executable(night_courses tests/render/night_courses.c)
target_include_directories(night_courses PRIVATE src/platform/sdl)
target_link_libraries(night_courses PRIVATE ${MOON_SDL_STACK} miniz SDL2::SDL2main)
if(MOON_TEST_DISC)
  foreach(api opengl vulkan)
    add_test(NAME night_course_${api} COMMAND night_courses "${MOON_TEST_DISC}" route5 "${CMAKE_BINARY_DIR}/night-${api}")
    add_test(NAME night_course_${api}_negative COMMAND night_courses "${MOON_TEST_DISC}" route5 "${CMAKE_BINARY_DIR}/night-${api}-negative" --negative)
    set_tests_properties(night_course_${api}_negative PROPERTIES WILL_FAIL TRUE)
    set_tests_properties(night_course_${api} night_course_${api}_negative PROPERTIES RUN_SERIAL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;MOON_GRAPHICS_API=${api}")
  endforeach()
endif()

if(MOON_TEST_DISC)
  add_test(NAME race_brake_lamps COMMAND race_inspect_probe "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/race-brake")
  add_test(NAME race_brake_lamps_negative COMMAND race_inspect_probe "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/race-brake-negative")
  set_tests_properties(race_brake_lamps PROPERTIES RUN_SERIAL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;PROBE_BRAKE_LAMPS=1;PROBE_CAR=focus_rs_02")
  set_tests_properties(race_brake_lamps_negative PROPERTIES RUN_SERIAL TRUE WILL_FAIL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;PROBE_BRAKE_LAMPS=1;PROBE_BRAKE_NEGATIVE=1;PROBE_CAR=focus_rs_02")
endif()

add_executable(prize_materials tests/ui/prize_materials.c)
target_link_libraries(prize_materials PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
if(MOON_TEST_DISC)
  add_test(NAME prize_materials COMMAND prize_materials "${MOON_TEST_DISC}")
  add_test(NAME prize_materials_negative COMMAND prize_materials "${MOON_TEST_DISC}" --negative)
  set_tests_properties(prize_materials_negative PROPERTIES WILL_FAIL TRUE)
  set_tests_properties(prize_materials prize_materials_negative PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_OVERLAY=${CMAKE_BINARY_DIR}/ui-audit-no-mods")
endif()

add_executable(car_lamps_test tests/render/car_lamps.c)
target_include_directories(car_lamps_test PRIVATE src/platform/sdl)
target_link_libraries(car_lamps_test PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
foreach(api opengl vulkan)
  add_test(NAME car_lamps_${api} COMMAND car_lamps_test)
  add_test(NAME car_lamps_${api}_negative COMMAND car_lamps_test --negative)
  set_tests_properties(car_lamps_${api}_negative PROPERTIES WILL_FAIL TRUE)
  set_tests_properties(car_lamps_${api} car_lamps_${api}_negative PROPERTIES RUN_SERIAL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;MOON_GRAPHICS_API=${api}")
endforeach()

if(MOON_TEST_DISC)
  add_test(NAME gtauto_oil_background COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/gtauto_oil_background.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/gtauto-oil-background")
  add_test(NAME gtauto_oil_background_negative COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/gtauto_oil_background.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/gtauto-oil-background-negative" --negative)
  set_tests_properties(gtauto_oil_background_negative PROPERTIES WILL_FAIL TRUE)
  set_tests_properties(gtauto_oil_background gtauto_oil_background_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 300)
endif()

add_executable(night_sky_test tests/render/night_sky.c)
target_include_directories(night_sky_test PRIVATE src/platform/sdl)
target_link_libraries(night_sky_test PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
foreach(api opengl vulkan)
  add_test(NAME night_sky_${api} COMMAND night_sky_test)
  add_test(NAME night_sky_${api}_negative COMMAND night_sky_test --negative)
  set_tests_properties(night_sky_${api}_negative PROPERTIES WILL_FAIL TRUE)
  set_tests_properties(night_sky_${api} night_sky_${api}_negative PROPERTIES RUN_SERIAL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;MOON_GRAPHICS_API=${api}")
endforeach()

# Original GDGT shadow presentation; no physics changes.
if(EXISTS "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  set(gen_gadget_shadow "${CMAKE_BINARY_DIR}/gen/gadget_shadow.c")
  add_custom_command(OUTPUT ${gen_gadget_shadow}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py
      --out ${gen_gadget_shadow} --root 0x003E4968 --extra-roots 0x003983E0,0x003E53B8
      --exclude 0x00455708 --hooks
    DEPENDS tools/recomp/ee_recomp.py extracted/core.bin splat/asm/core/text.s)
  add_executable(gadget_shadow_oracle tests/ui/gadget_shadow_oracle.c ${gen_gadget_shadow})
  target_link_libraries(gadget_shadow_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  add_test(NAME gadget_shadow_original COMMAND gadget_shadow_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin")
  foreach(part matrix alpha model)
    add_test(NAME gadget_shadow_${part}_negative COMMAND gadget_shadow_oracle "${CMAKE_SOURCE_DIR}/extracted/core.bin" --${part}-negative)
    set_tests_properties(gadget_shadow_${part}_negative PROPERTIES WILL_FAIL TRUE)
  endforeach()
endif()

add_executable(gadget_shadow_gpu tests/render/gadget_shadow.c)
target_include_directories(gadget_shadow_gpu PRIVATE src/platform/sdl)
target_link_libraries(gadget_shadow_gpu PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main miniz)
if(MOON_TEST_DISC)
  foreach(api opengl vulkan)
    add_test(NAME gadget_shadow_${api} COMMAND gadget_shadow_gpu "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/gadget-shadow-${api}")
    add_test(NAME gadget_shadow_${api}_negative COMMAND gadget_shadow_gpu "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/gadget-shadow-${api}-negative" --negative)
    set_tests_properties(gadget_shadow_${api}_negative PROPERTIES WILL_FAIL TRUE)
    set_tests_properties(gadget_shadow_${api} gadget_shadow_${api}_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 120 ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;MOON_GRAPHICS_API=${api}")
  endforeach()
endif()

add_executable(night_baked tests/render/night_baked.c)
target_include_directories(night_baked PRIVATE src/platform/sdl)
target_link_libraries(night_baked PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
foreach(api opengl vulkan)
  add_test(NAME night_baked_${api} COMMAND night_baked)
  add_test(NAME night_baked_${api}_negative COMMAND night_baked --negative)
  set_tests_properties(night_baked_${api}_negative PROPERTIES WILL_FAIL TRUE)
  set_tests_properties(night_baked_${api} night_baked_${api}_negative PROPERTIES RUN_SERIAL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;MOON_GRAPHICS_API=${api}")
endforeach()

add_executable(celestial_cycle tests/render/celestial_cycle.c)
target_include_directories(celestial_cycle PRIVATE src/platform/sdl)
target_link_libraries(celestial_cycle PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
foreach(api opengl vulkan)
  add_test(NAME celestial_cycle_${api} COMMAND celestial_cycle)
  foreach(part moon clock)
    add_test(NAME celestial_cycle_${api}_${part}_negative COMMAND celestial_cycle --${part}-negative)
    set_tests_properties(celestial_cycle_${api}_${part}_negative PROPERTIES WILL_FAIL TRUE)
  endforeach()
  set_tests_properties(celestial_cycle_${api} celestial_cycle_${api}_moon_negative celestial_cycle_${api}_clock_negative PROPERTIES RUN_SERIAL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;MOON_GRAPHICS_API=${api}")
endforeach()

if(MOON_TEST_DISC)
  foreach(api opengl vulkan)
    add_test(NAME celestial_clock_${api} COMMAND race_inspect_probe "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/celestial-clock-${api}")
    add_test(NAME celestial_clock_${api}_negative COMMAND race_inspect_probe "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/celestial-clock-${api}-negative")
    set_tests_properties(celestial_clock_${api} celestial_clock_${api}_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 120 ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;MOON_GRAPHICS_API=${api};PROBE_CELESTIAL_CLOCK=1;PROBE_COURSE=route5;PROBE_CAR=focus_rs_02;PROBE_WIDTH=640;PROBE_HEIGHT=448")
    set_property(TEST celestial_clock_${api}_negative APPEND PROPERTY ENVIRONMENT "PROBE_CELESTIAL_NEGATIVE=1")
    set_tests_properties(celestial_clock_${api}_negative PROPERTIES WILL_FAIL TRUE)
  endforeach()
endif()

# Optional car-lamp world occlusion; deliberate negative omits the depth atlas.
add_executable(lamp_occlusion_test tests/render/lamp_occlusion.c)
target_include_directories(lamp_occlusion_test PRIVATE src/platform/sdl)
target_link_libraries(lamp_occlusion_test PRIVATE ${MOON_SDL_STACK} miniz SDL2::SDL2main)
foreach(api opengl vulkan)
  add_test(NAME lamp_occlusion_${api} COMMAND lamp_occlusion_test)
  add_test(NAME lamp_occlusion_${api}_negative COMMAND lamp_occlusion_test --negative)
  set_tests_properties(lamp_occlusion_${api}_negative PROPERTIES WILL_FAIL TRUE)
  set_tests_properties(lamp_occlusion_${api} lamp_occlusion_${api}_negative PROPERTIES RUN_SERIAL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;MOON_GRAPHICS_API=${api}")
endforeach()
add_executable(lamp_occlusion_course tests/render/lamp_occlusion_course.c)
target_include_directories(lamp_occlusion_course PRIVATE src/platform/sdl)
target_link_libraries(lamp_occlusion_course PRIVATE ${MOON_SDL_STACK} miniz SDL2::SDL2main)
if(MOON_TEST_DISC)
  foreach(api opengl vulkan)
    add_test(NAME lamp_occlusion_course_${api} COMMAND lamp_occlusion_course "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/lamp-occlusion-${api}")
    add_test(NAME lamp_occlusion_course_${api}_negative COMMAND lamp_occlusion_course "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/lamp-occlusion-${api}-negative" --negative)
    set_tests_properties(lamp_occlusion_course_${api}_negative PROPERTIES WILL_FAIL TRUE)
    set_tests_properties(lamp_occlusion_course_${api} lamp_occlusion_course_${api}_negative PROPERTIES RUN_SERIAL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;MOON_GRAPHICS_API=${api}")
  endforeach()
endif()

# Bounded survey of the geometry-derived optional light sources.
add_executable(lamp_survey tests/render/lamp_survey.c)
target_include_directories(lamp_survey PRIVATE src/platform/sdl)
target_link_libraries(lamp_survey PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
if(MOON_TEST_DISC)
  add_test(NAME lamp_survey COMMAND lamp_survey "${MOON_TEST_DISC}")
  add_test(NAME lamp_survey_negative COMMAND lamp_survey "${MOON_TEST_DISC}" --negative)
  set_tests_properties(lamp_survey_negative PROPERTIES WILL_FAIL TRUE)
  set_tests_properties(lamp_survey lamp_survey_negative PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_OVERLAY=${CMAKE_BINARY_DIR}/ui-audit-no-mods")
endif()
add_executable(lamp_grid tests/render/lamp_grid.c)
target_include_directories(lamp_grid PRIVATE src/platform/sdl)
target_link_libraries(lamp_grid PRIVATE ${MOON_SDL_STACK} miniz SDL2::SDL2main)
if(MOON_TEST_DISC)
  foreach(api opengl vulkan)
    add_test(NAME lamp_grid_${api} COMMAND lamp_grid "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/lamp-grid-${api}")
    add_test(NAME lamp_grid_${api}_negative COMMAND lamp_grid "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/lamp-grid-${api}-negative" --negative)
    set_tests_properties(lamp_grid_${api}_negative PROPERTIES WILL_FAIL TRUE)
    set_tests_properties(lamp_grid_${api} lamp_grid_${api}_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 90 ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;MOON_GRAPHICS_API=${api}")
  endforeach()
endif()
add_executable(lamp_bounds tests/render/lamp_bounds.c)
target_include_directories(lamp_bounds PRIVATE src/platform/sdl src/core)
add_test(NAME lamp_bounds COMMAND lamp_bounds)
add_test(NAME lamp_bounds_negative COMMAND lamp_bounds --negative)
set_tests_properties(lamp_bounds_negative PROPERTIES WILL_FAIL TRUE)

# Direct light atlas against retained per-tile copy transport, on both APIs.
add_executable(lamp_atlas tests/render/lamp_atlas.c)
target_include_directories(lamp_atlas PRIVATE src/platform/sdl)
target_link_libraries(lamp_atlas PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
foreach(api opengl vulkan)
  add_test(NAME lamp_atlas_${api} COMMAND lamp_atlas)
  add_test(NAME lamp_atlas_${api}_negative COMMAND lamp_atlas --negative)
  set_tests_properties(lamp_atlas_${api}_negative PROPERTIES WILL_FAIL TRUE)
  set_tests_properties(lamp_atlas_${api} lamp_atlas_${api}_negative PROPERTIES RUN_SERIAL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;MOON_GRAPHICS_API=${api}")
endforeach()

add_executable(lamp_emission tests/render/lamp_emission.c)
target_include_directories(lamp_emission PRIVATE src/platform/sdl)
target_link_libraries(lamp_emission PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
foreach(api opengl vulkan)
  add_test(NAME lamp_emission_${api} COMMAND lamp_emission)
  add_test(NAME lamp_emission_${api}_negative COMMAND lamp_emission --negative)
  set_tests_properties(lamp_emission_${api}_negative PROPERTIES WILL_FAIL TRUE)
  set_tests_properties(lamp_emission_${api} lamp_emission_${api}_negative PROPERTIES RUN_SERIAL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;MOON_GRAPHICS_API=${api}")
endforeach()

# Original backfire endpoint interpolation and authored ground model.
add_executable(backfire_tween tests/render/backfire_tween.c)
target_include_directories(backfire_tween PRIVATE src/platform/sdl)
target_link_libraries(backfire_tween PRIVATE ${MOON_SDL_STACK} miniz SDL2::SDL2main)
if(MOON_TEST_DISC)
  foreach(api opengl vulkan)
    add_test(NAME backfire_tween_${api} COMMAND backfire_tween "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/backfire-${api}")
    add_test(NAME backfire_tween_${api}_negative COMMAND backfire_tween "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/backfire-${api}-negative" --negative)
    set_tests_properties(backfire_tween_${api}_negative PROPERTIES WILL_FAIL TRUE)
    set_tests_properties(backfire_tween_${api} backfire_tween_${api}_negative PROPERTIES RUN_SERIAL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;MOON_GRAPHICS_API=${api}")
  endforeach()
endif()

if(TARGET moon_ee AND EXISTS "${CMAKE_SOURCE_DIR}/extracted/core.bin" AND EXISTS "${CMAKE_SOURCE_DIR}/splat/asm/core/text.s")
  set(gen_backfire_visual "${CMAKE_BINARY_DIR}/gen/backfire_visual.c")
  add_custom_command(OUTPUT ${gen_backfire_visual}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py --out ${gen_backfire_visual} --root 0x00452F28
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(backfire_visual tests/platform/backfire_visual.c ${gen_backfire_visual})
  target_link_libraries(backfire_visual PRIVATE moon_ee ${MOON_SDL_STACK} SDL2::SDL2main)
  add_test(NAME backfire_visual COMMAND backfire_visual ${CMAKE_SOURCE_DIR}/extracted/core.bin)
  add_test(NAME backfire_visual_negative COMMAND backfire_visual ${CMAKE_SOURCE_DIR}/extracted/core.bin --negative)
  set_tests_properties(backfire_visual_negative PROPERTIES WILL_FAIL TRUE)
endif()

add_executable(backfire_scene tests/render/backfire_scene.c)
target_include_directories(backfire_scene PRIVATE src/platform/sdl)
target_link_libraries(backfire_scene PRIVATE ${MOON_SDL_STACK} miniz SDL2::SDL2main)
if(MOON_TEST_DISC)
  foreach(api opengl vulkan)
    add_test(NAME backfire_scene_${api} COMMAND backfire_scene "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/backfire-scene-${api}")
    set_tests_properties(backfire_scene_${api} PROPERTIES RUN_SERIAL TRUE TIMEOUT 60 ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;MOON_GRAPHICS_API=${api}")
  endforeach()
endif()

if(TARGET backfire_visual)
  add_test(NAME backfire_visual_ground_negative COMMAND backfire_visual ${CMAKE_SOURCE_DIR}/extracted/core.bin --negative-ground)
  set_tests_properties(backfire_visual_ground_negative PROPERTIES WILL_FAIL TRUE)
endif()

if(TARGET moon_ee AND MOON_TEST_DISC AND EXISTS "${CMAKE_SOURCE_DIR}/extracted/core.bin" AND EXISTS "${CMAKE_SOURCE_DIR}/splat/asm/core/text.s")
  set(gen_photo_regions "${CMAKE_BINARY_DIR}/gen/photo_regions.c")
  add_custom_command(OUTPUT ${gen_photo_regions}
    COMMAND ${MOON_PYTHON} ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py --out ${gen_photo_regions} --root 0x003FECB0 --extra-roots 0x003FFC68,0x003FEE08,0x00370EF8,0x00380C50,0x003DE268 --exclude 0x005A48D8,0x0057D008,0x0059D2A0,0x0057D380,0x00344798,0x003447F8,0x00344828,0x003448B8,0x00344858,0x00344888,0x003F3708,0x003F3720,0x003F3738,0x003F3748,0x004A0690,0x004A53F8,0x004A7454,0x004A7844,0x004A79B0,0x004A7988,0x004A79D8,0x004A5400,0x004A06F0,0x004A7AA8 --hooks
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/recomp/ee_recomp.py ${CMAKE_SOURCE_DIR}/extracted/core.bin ${CMAKE_SOURCE_DIR}/splat/asm/core/text.s)
  add_executable(photo_regions_oracle tests/ui/photo_regions_oracle.c ${gen_photo_regions})
  target_link_libraries(photo_regions_oracle PRIVATE moon_ee ${MOON_CORE_STACK})
  add_test(NAME photo_regions COMMAND photo_regions_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin "${MOON_TEST_DISC}")
  add_test(NAME photo_regions_negative COMMAND photo_regions_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin "${MOON_TEST_DISC}" --negative)
  set_tests_properties(photo_regions_negative PROPERTIES WILL_FAIL TRUE)
endif()

if(TARGET photo_regions_oracle)
  foreach(part aim movement points zoom)
    add_test(NAME photo_regions_${part}_negative COMMAND photo_regions_oracle ${CMAKE_SOURCE_DIR}/extracted/core.bin "${MOON_TEST_DISC}" --negative-${part})
    set_tests_properties(photo_regions_${part}_negative PROPERTIES WILL_FAIL TRUE)
  endforeach()
  add_test(NAME photo_can_shoot_negative COMMAND photo_travel_probe "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/photo-shoot-negative" "${CMAKE_BINARY_DIR}/photo-shoot-negative.png")
  set_tests_properties(photo_can_shoot_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;MOON_PHOTO_SHOOT_NEGATIVE=1")
endif()

# Authored PhotoPause viewfinder and instruction panel.
add_executable(photo_hud tests/render/photo_hud.c)
target_include_directories(photo_hud PRIVATE src/platform/sdl)
target_link_libraries(photo_hud PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main miniz)

# Standalone save editor: native writer/reader interoperability, no game assets.
add_executable(save_editor_bridge tests/ui/save_editor_bridge.c)
target_link_libraries(save_editor_bridge PRIVATE ${MOON_CORE_STACK})
set(MOON_SAVE_EDITOR "${CMAKE_BINARY_DIR}/MoonSaveEditor.exe" CACHE FILEPATH "Locally built standalone save editor")
if(EXISTS "${MOON_SAVE_EDITOR}")
  add_test(NAME save_editor_interop COMMAND ${MOON_PYTHON} "${CMAKE_SOURCE_DIR}/tests/ui/save_editor_interop.py" "$<TARGET_FILE:save_editor_bridge>" "${MOON_SAVE_EDITOR}")
  add_test(NAME save_editor_interop_negative COMMAND ${MOON_PYTHON} "${CMAKE_SOURCE_DIR}/tests/ui/save_editor_interop.py" "$<TARGET_FILE:save_editor_bridge>" "${MOON_SAVE_EDITOR}" --negative)
  set_tests_properties(save_editor_interop_negative PROPERTIES WILL_FAIL TRUE)
  set_tests_properties(save_editor_interop save_editor_interop_negative PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy")
endif()
if(MOON_TEST_DISC)
 foreach(api opengl vulkan)
  add_test(NAME photo_hud_${api} COMMAND photo_hud "${MOON_TEST_DISC}")
  foreach(part marker digits viewport)
   add_test(NAME photo_hud_${api}_${part}_negative COMMAND photo_hud "${MOON_TEST_DISC}" --${part}-negative)
   set_tests_properties(photo_hud_${api}_${part}_negative PROPERTIES WILL_FAIL TRUE)
  endforeach()
  set_tests_properties(photo_hud_${api} photo_hud_${api}_marker_negative photo_hud_${api}_digits_negative photo_hud_${api}_viewport_negative PROPERTIES RUN_SERIAL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;MOON_GRAPHICS_API=${api}")
 endforeach()
endif()

# Read-only, disc-backed names and record mapping for the standalone editor.
add_executable(moon_save_catalog tools/c/moon_save_catalog.c)
target_link_libraries(moon_save_catalog PRIVATE ${MOON_CORE_STACK})

if(MOON_TEST_DISC)
  add_test(NAME launcher_initial_paint COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/launcher_initial_paint.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/launcher-initial-paint")
  add_test(NAME launcher_initial_paint_negative COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/launcher_initial_paint.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/launcher-initial-paint-negative" --negative)
  set_tests_properties(launcher_initial_paint_negative PROPERTIES WILL_FAIL TRUE)
  set_tests_properties(launcher_initial_paint launcher_initial_paint_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 300)
endif()
add_executable(save_editor_guided tests/ui/save_editor_guided.c)
target_link_libraries(save_editor_guided PRIVATE moon_career_vehicle ${MOON_CORE_STACK})

# P: complete skybox cloud volume, deterministic replay-time motion.
add_executable(realistic_sky tests/render/realistic_sky.c)
target_include_directories(realistic_sky PRIVATE src/platform/sdl)
target_link_libraries(realistic_sky PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main miniz)
foreach(api opengl vulkan)
 add_test(NAME realistic_sky_${api} COMMAND realistic_sky)
 add_test(NAME realistic_sky_${api}_negative COMMAND realistic_sky --negative)
 set_tests_properties(realistic_sky_${api}_negative PROPERTIES WILL_FAIL TRUE)
 set_tests_properties(realistic_sky_${api} realistic_sky_${api}_negative PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;MOON_GRAPHICS_API=${api}" RUN_SERIAL TRUE)
endforeach()

add_executable(controller_tiles_live tests/ui/controller_tiles_live.c)
target_link_libraries(controller_tiles_live PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
if(MOON_TEST_DISC)
  add_test(NAME controller_tiles_live COMMAND controller_tiles_live "${MOON_TEST_DISC}")
  add_test(NAME controller_tiles_live_negative COMMAND controller_tiles_live "${MOON_TEST_DISC}" --negative)
  set_tests_properties(controller_tiles_live_negative PROPERTIES WILL_FAIL TRUE)
  set_tests_properties(controller_tiles_live controller_tiles_live_negative PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy")
endif()

foreach(api opengl vulkan)
 add_test(NAME realistic_lighting_${api}_contrast_negative COMMAND modern_lighting_tests --contrast-negative)
 set_tests_properties(realistic_lighting_${api}_contrast_negative PROPERTIES WILL_FAIL TRUE RUN_SERIAL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;MOON_GRAPHICS_API=${api}")
endforeach()

add_executable(mission_pointer tests/ui/mission_pointer.c)
target_link_libraries(mission_pointer PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
if(MOON_TEST_DISC)
 add_test(NAME mission_pointer COMMAND mission_pointer "${MOON_TEST_DISC}")
 add_test(NAME mission_pointer_negative COMMAND mission_pointer "${MOON_TEST_DISC}" --negative)
 set_tests_properties(mission_pointer_negative PROPERTIES WILL_FAIL TRUE)
 set_tests_properties(mission_pointer mission_pointer_negative PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy")
endif()
option(MOON_SAVE_EDITOR_UI_TESTS "Run local Playwright guided save-editor checks" OFF)
if(MOON_SAVE_EDITOR_UI_TESTS AND MOON_TEST_DISC AND EXISTS "${MOON_SAVE_EDITOR}")
 find_program(MOON_EDITOR_NODE NAMES node REQUIRED)
 foreach(suffix "" "_negative")
  set(extra)
  if(suffix)
   set(extra --negative)
  endif()
  add_test(NAME save_editor_guided${suffix} COMMAND ${MOON_PYTHON} "${CMAKE_SOURCE_DIR}/tests/ui/save_editor_guided.py" "$<TARGET_FILE:save_editor_guided>" "$<TARGET_FILE:moon_save_catalog>" "${MOON_SAVE_EDITOR}" "${MOON_TEST_DISC}" "${MOON_EDITOR_NODE}" "${CMAKE_BINARY_DIR}/save-editor-guided" ${extra})
  set_tests_properties(save_editor_guided${suffix} PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" RUN_SERIAL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy")
 endforeach()
 set_tests_properties(save_editor_guided_negative PROPERTIES WILL_FAIL TRUE)
endif()

if(TARGET gtmode_service_values)
 add_test(NAME gtmode_oil_warning COMMAND gtmode_service_values "${CMAKE_SOURCE_DIR}/extracted/core.bin")
 add_test(NAME gtmode_oil_warning_negative COMMAND gtmode_service_values "${CMAKE_SOURCE_DIR}/extracted/core.bin")
 set_tests_properties(gtmode_oil_warning_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_OIL_WARNING_NEGATIVE=1;SDL_AUDIODRIVER=dummy")
endif()

add_executable(hud_oil_warning tests/ui/hud_oil_warning.c)
target_link_libraries(hud_oil_warning PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
if(MOON_TEST_DISC)
 add_test(NAME hud_oil_warning COMMAND hud_oil_warning "${MOON_TEST_DISC}")
 add_test(NAME hud_oil_warning_negative COMMAND hud_oil_warning "${MOON_TEST_DISC}" --negative)
 set_tests_properties(hud_oil_warning_negative PROPERTIES WILL_FAIL TRUE)
 set_tests_properties(hud_oil_warning hud_oil_warning_negative PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy")
endif()

add_executable(gtauto_oil_color tests/ui/gtauto_oil_color.c)
target_link_libraries(gtauto_oil_color PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
if(MOON_TEST_DISC)
 add_test(NAME gtauto_oil_color COMMAND gtauto_oil_color "${MOON_TEST_DISC}")
 add_test(NAME gtauto_oil_color_negative COMMAND gtauto_oil_color "${MOON_TEST_DISC}" --negative)
 set_tests_properties(gtauto_oil_color_negative PROPERTIES WILL_FAIL TRUE)
 set_tests_properties(gtauto_oil_color gtauto_oil_color_negative PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy")
endif()

add_executable(dialog_style tests/ui/dialog_style.c)
target_link_libraries(dialog_style PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
if(MOON_TEST_DISC)
 add_test(NAME dialog_style COMMAND dialog_style "${MOON_TEST_DISC}")
 add_test(NAME dialog_style_negative COMMAND dialog_style "${MOON_TEST_DISC}" --negative)
 set_tests_properties(dialog_style_negative PROPERTIES WILL_FAIL TRUE)
 set_tests_properties(dialog_style dialog_style_negative PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy")
endif()

# P: roughness must broaden the environment reflection, on both GPU backends.
foreach(api opengl vulkan)
 add_test(NAME realistic_lighting_${api}_environment_negative COMMAND modern_lighting_tests --environment-negative)
 set_tests_properties(realistic_lighting_${api}_environment_negative PROPERTIES WILL_FAIL TRUE RUN_SERIAL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;MOON_GRAPHICS_API=${api}")
endforeach()

add_executable(tuning_effects tests/ui/tuning_effects.c)
target_link_libraries(tuning_effects PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
if(MOON_TEST_DISC)
 add_test(NAME tuning_effects COMMAND tuning_effects "${MOON_TEST_DISC}")
 add_test(NAME tuning_effects_negative COMMAND tuning_effects "${MOON_TEST_DISC}" --negative)
 set_tests_properties(tuning_effects_negative PROPERTIES WILL_FAIL TRUE)
 set_tests_properties(tuning_effects tuning_effects_negative PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy")
endif()

# Offline native garage operations for the standalone save editor.
add_executable(moon_save_vehicle tools/c/moon_save_vehicle.c)
target_link_libraries(moon_save_vehicle PRIVATE moon_career_vehicle ${MOON_CORE_STACK})

foreach(api opengl vulkan)
 add_test(NAME realistic_lighting_${api}_warm_negative COMMAND modern_lighting_tests --warm-negative)
 set_tests_properties(realistic_lighting_${api}_warm_negative PROPERTIES WILL_FAIL TRUE RUN_SERIAL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;MOON_GRAPHICS_API=${api}")
endforeach()

add_executable(jukebox_takeover tests/ui/jukebox_takeover.c)
target_link_libraries(jukebox_takeover PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
if(MOON_TEST_DISC)
 add_test(NAME jukebox_takeover COMMAND jukebox_takeover "${MOON_TEST_DISC}")
 add_test(NAME jukebox_takeover_negative COMMAND jukebox_takeover "${MOON_TEST_DISC}" --negative)
 set_tests_properties(jukebox_takeover_negative PROPERTIES WILL_FAIL TRUE)
 set_tests_properties(jukebox_takeover jukebox_takeover_negative PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy")
endif()


# Standalone editor native vehicle and advanced parameter checks (area only).
if(MOON_SAVE_EDITOR_UI_TESTS AND EXISTS "${MOON_SAVE_EDITOR}" AND MOON_TEST_DISC)
 foreach(suffix IN ITEMS "" "_negative")
  set(extra)
  if(suffix)
   set(extra --negative)
  endif()
  add_test(NAME save_editor_vehicle${suffix} COMMAND ${MOON_PYTHON} "${CMAKE_SOURCE_DIR}/tests/ui/save_editor_vehicle.py" "$<TARGET_FILE:save_editor_guided>" "$<TARGET_FILE:moon_save_catalog>" "${MOON_SAVE_EDITOR}" "$<TARGET_FILE:moon_save_vehicle>" "${MOON_TEST_DISC}" "${MOON_EDITOR_NODE}" "${CMAKE_BINARY_DIR}/save-editor-vehicle.png" ${extra})
  set_tests_properties(save_editor_vehicle${suffix} PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" RUN_SERIAL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy")
 endforeach()
 set_tests_properties(save_editor_vehicle_negative PROPERTIES WILL_FAIL TRUE)
endif()

if(TARGET mission_pointer AND MOON_TEST_DISC)
 add_test(NAME mission_edge_negative COMMAND mission_pointer "${MOON_TEST_DISC}")
 set_tests_properties(mission_edge_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_MISSION_EDGE_NEGATIVE=1;SDL_AUDIODRIVER=dummy")
endif()

if(TARGET scroll_takeover AND MOON_TEST_DISC)
 add_test(NAME scroll_strip_takeover_negative COMMAND scroll_takeover "${MOON_TEST_DISC}")
 set_tests_properties(scroll_strip_takeover_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "GT4_SCROLL_STRIP_NEGATIVE=1;SDL_AUDIODRIVER=dummy")
endif()

# Custom Menu songs replace the GT Mode map only.
if(MOON_TEST_DISC)
 add_test(NAME menu_custom_scope_negative COMMAND menu_music_transport_test "${MOON_TEST_DISC}" --custom-scope)
 add_test(NAME menu_custom_pcm_scope_negative COMMAND pregrid_audio_probe "${MOON_TEST_DISC}")
 set_tests_properties(menu_custom_scope_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy")
 set_tests_properties(menu_custom_pcm_scope_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy;GT4_MENU_PCM_SCOPE_NEGATIVE=1")
endif()

# All launcher option captions share their panel's fading text boundary.
if(MOON_TEST_DISC AND TARGET moonengine)
 add_test(NAME launcher_options_ink COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/launcher_options_ink.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/launcher-options-ink")
 add_test(NAME launcher_options_ink_negative COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/launcher_options_ink.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/launcher-options-ink-negative" --negative)
 set_tests_properties(launcher_options_ink_negative PROPERTIES WILL_FAIL TRUE)
 set_tests_properties(launcher_options_ink launcher_options_ink_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 300 ENVIRONMENT "SDL_AUDIODRIVER=dummy")
endif()

add_executable(course_entry_buttons tests/ui/course_entry_buttons.c)
target_link_libraries(course_entry_buttons PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
if(MOON_TEST_DISC)
 add_test(NAME course_entry_buttons COMMAND course_entry_buttons "${MOON_TEST_DISC}")
 add_test(NAME course_entry_buttons_mouse COMMAND course_entry_buttons "${MOON_TEST_DISC}" --mouse)
 add_test(NAME course_entry_buttons_negative COMMAND course_entry_buttons "${MOON_TEST_DISC}" --negative)
 set_tests_properties(course_entry_buttons_negative PROPERTIES WILL_FAIL TRUE)
 set_tests_properties(course_entry_buttons course_entry_buttons_mouse course_entry_buttons_negative PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy")
endif()

# Regression: native page-close flags and current-player garage selection.
add_test(NAME page_close_visible_negative COMMAND menu_tests)
set_tests_properties(page_close_visible_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy;GT4_PAGE_CLOSE_VISIBLE_NEGATIVE=1")
if(MOON_TEST_DISC)
 add_test(NAME owned_car_picker_negative COMMAND profile_storage_test "${MOON_TEST_DISC}" --owned-picker-negative)
 set_tests_properties(owned_car_picker_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy")
 foreach(suffix IN ITEMS "" "_negative")
  set(extra)
  if(suffix)
   set(extra --negative)
  endif()
  add_test(NAME save_editor_cappuccino${suffix} COMMAND ${MOON_PYTHON} "${CMAKE_SOURCE_DIR}/tests/ui/save_editor_cappuccino.py" "$<TARGET_FILE:save_editor_guided>" "$<TARGET_FILE:moon_save_vehicle>" "${MOON_TEST_DISC}" ${extra})
  set_tests_properties(save_editor_cappuccino${suffix} PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" ENVIRONMENT "SDL_AUDIODRIVER=dummy" RUN_SERIAL TRUE)
 endforeach()
 set_tests_properties(save_editor_cappuccino_negative PROPERTIES WILL_FAIL TRUE)
endif()

# Bounded GPU equivalence to full-resolution cloud / per-tap linear decode references.
foreach(api opengl vulkan)
 add_test(NAME lighting_cache_${api}_cloud_negative COMMAND realistic_sky --cache-negative)
 add_test(NAME lighting_cache_${api}_environment_negative COMMAND modern_lighting_tests --cache-negative)
 set_tests_properties(lighting_cache_${api}_cloud_negative lighting_cache_${api}_environment_negative PROPERTIES WILL_FAIL TRUE RUN_SERIAL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;MOON_GRAPHICS_API=${api}")
endforeach()

if(MOON_TEST_DISC)
 add_test(NAME photo_drive_select_negative COMMAND race_resources_test "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/photo-select-negative-save" "${CMAKE_BINARY_DIR}/photo-select-negative.png" --select-negative)
 set_tests_properties(photo_drive_select_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1")
endif()

add_executable(authored_sky tests/render/authored_sky.c)
target_include_directories(authored_sky PRIVATE src/platform/sdl)
target_link_libraries(authored_sky PRIVATE ${MOON_SDL_STACK} miniz SDL2::SDL2main)
if(MOON_TEST_DISC)
 foreach(api opengl vulkan)
  foreach(course tsukuba laguna route5)
   add_test(NAME authored_sky_${api}_${course} COMMAND authored_sky "${MOON_TEST_DISC}" ${course} "${CMAKE_BINARY_DIR}/authored-${api}-${course}")
   set_tests_properties(authored_sky_${api}_${course} PROPERTIES RUN_SERIAL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;MOON_GRAPHICS_API=${api}")
  endforeach()
  add_test(NAME authored_sky_${api}_negative COMMAND authored_sky "${MOON_TEST_DISC}" tsukuba "${CMAKE_BINARY_DIR}/authored-${api}-negative" --negative)
  set_tests_properties(authored_sky_${api}_negative PROPERTIES WILL_FAIL TRUE RUN_SERIAL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;MOON_GRAPHICS_API=${api}")
 endforeach()
endif()

add_executable(setting_display tests/ui/setting_display.c)
target_link_libraries(setting_display PRIVATE moon_career_vehicle ${MOON_CORE_STACK})
if(MOON_TEST_DISC)
 add_test(NAME setting_display COMMAND setting_display "${MOON_TEST_DISC}")
 add_test(NAME setting_display_negative COMMAND setting_display "${MOON_TEST_DISC}" --missing-label)
 set_tests_properties(setting_display setting_display_negative PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy" RUN_SERIAL TRUE)
 set_tests_properties(setting_display_negative PROPERTIES WILL_FAIL TRUE)
 add_test(NAME setting_cursor_negative COMMAND setting_display "${MOON_TEST_DISC}" --arrow-negative)
 set_tests_properties(setting_cursor_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy" RUN_SERIAL TRUE)
endif()

foreach(api opengl vulkan)
 add_test(NAME authored_sky_${api}_source_negative COMMAND realistic_sky --authored-negative)
 set_tests_properties(authored_sky_${api}_source_negative PROPERTIES WILL_FAIL TRUE RUN_SERIAL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;MOON_GRAPHICS_API=${api}")
endforeach()

# Implicit card checks in ordinary operations always use the active player.
if(MOON_TEST_DISC)
 add_test(NAME operation_current_profile COMMAND profile_storage_test "${MOON_TEST_DISC}")
 add_test(NAME operation_current_profile_negative COMMAND profile_storage_test "${MOON_TEST_DISC}" --operation-picker-negative)
 set_tests_properties(operation_current_profile PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy")
 set_tests_properties(operation_current_profile_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy")
endif()


if(MOON_TEST_DISC)
 add_test(NAME save_authored_handler_negative COMMAND profile_storage_test "${MOON_TEST_DISC}" --save-handler-negative)
 set_tests_properties(save_authored_handler_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endif()

if(MOON_TEST_DISC)
 add_test(NAME setting_field_negative COMMAND setting_display "${MOON_TEST_DISC}" --field-negative)
 set_tests_properties(setting_field_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy" RUN_SERIAL TRUE)
endif()

add_executable(car_wheel_body tests/render/car_wheel_body.c)
target_link_libraries(car_wheel_body PRIVATE ${MOON_SDL_STACK} SDL2::SDL2main)
if(MOON_TEST_DISC)
  add_test(NAME car_wheel_body COMMAND car_wheel_body "${MOON_TEST_DISC}")
  add_test(NAME car_wheel_body_negative COMMAND car_wheel_body "${MOON_TEST_DISC}" --negative)
  set_tests_properties(car_wheel_body car_wheel_body_negative PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy" RUN_SERIAL TRUE TIMEOUT 300)
  set_tests_properties(car_wheel_body_negative PROPERTIES WILL_FAIL TRUE)
endif()

if(MOON_TEST_DISC)
  add_test(NAME diary_wheel_labels COMMAND gtmode_purchase_menu "${MOON_TEST_DISC}" --home-status)
  add_test(NAME diary_wheel_labels_negative COMMAND gtmode_purchase_menu "${MOON_TEST_DISC}" --home-status)
  set_tests_properties(diary_wheel_labels_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "MOON_DIARY_WHEEL_NEGATIVE=1")
endif()

if(MOON_TEST_DISC)
  add_test(NAME photo_pointer_zoom_negative COMMAND photo_travel_probe "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/photo-pointer-negative" "${CMAKE_BINARY_DIR}/photo-pointer-negative.png")
  set_tests_properties(photo_pointer_zoom_negative PROPERTIES WILL_FAIL TRUE RUN_SERIAL TRUE TIMEOUT 90 ENVIRONMENT "MOON_PHOTO_POINTER_NEGATIVE=1")
  add_test(NAME photo_preview_return_negative COMMAND photo_travel_probe "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/photo-return-negative" "${CMAKE_BINARY_DIR}/photo-return-negative.png")
  set_tests_properties(photo_preview_return_negative PROPERTIES WILL_FAIL TRUE RUN_SERIAL TRUE TIMEOUT 90 ENVIRONMENT "MOON_PHOTO_RETURN_NEGATIVE=1")
endif()

add_executable(wheel_swap_pixels tests/render/wheel_swap_pixels.c)
target_link_libraries(wheel_swap_pixels PRIVATE moon_career_vehicle ${MOON_SDL_STACK} miniz SDL2::SDL2main)

add_executable(launcher_bindings_test tests/ui/launcher_bindings.c)
target_include_directories(launcher_bindings_test PRIVATE src/core include)
add_test(NAME launcher_bindings COMMAND launcher_bindings_test)
add_test(NAME launcher_bindings_negative COMMAND launcher_bindings_test --live-cycling)
set_tests_properties(launcher_bindings launcher_bindings_negative PROPERTIES ENVIRONMENT "SDL_AUDIODRIVER=dummy")
set_tests_properties(launcher_bindings_negative PROPERTIES WILL_FAIL TRUE)

if(MOON_TEST_DISC AND Python3_EXECUTABLE)
  add_test(NAME settings_pages COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/settings_pages.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/settings-pages")
  add_test(NAME settings_pages_negative COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/ui/settings_pages.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" "${CMAKE_BINARY_DIR}/settings-pages-negative" --negative)
  set_tests_properties(settings_pages_negative PROPERTIES WILL_FAIL TRUE)
  set_tests_properties(settings_pages settings_pages_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 180)
endif()

if(MOON_TEST_DISC)
 add_test(NAME launcher_binding_input COMMAND "${MOON_PYTHON}" "${CMAKE_SOURCE_DIR}/tests/ui/launcher_binding_input.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}")
 add_test(NAME launcher_binding_input_negative COMMAND "${MOON_PYTHON}" "${CMAKE_SOURCE_DIR}/tests/ui/launcher_binding_input.py" "$<TARGET_FILE:moonengine>" "${MOON_TEST_DISC}" --negative)
 set_tests_properties(launcher_binding_input launcher_binding_input_negative PROPERTIES RUN_SERIAL TRUE TIMEOUT 100 ENVIRONMENT "SDL_AUDIODRIVER=dummy")
 set_tests_properties(launcher_binding_input_negative PROPERTIES WILL_FAIL TRUE)
endif()

# Loaded Supra turbo pressure reaches the HUD under load and on lift.
if(MOON_TEST_DISC)
  foreach(car castrol_supra_00 castrol_supra_01 denso_supra_00 au_supra_01 woodone_supra_03)
    add_test(NAME hud_boost_${car} COMMAND suggested_gear_hud "${MOON_TEST_DISC}")
    add_test(NAME hud_boost_${car}_negative COMMAND suggested_gear_hud "${MOON_TEST_DISC}" --negative)
    set_tests_properties(hud_boost_${car} hud_boost_${car}_negative PROPERTIES
      ENVIRONMENT "MOON_BOOST_CAR=${car};SDL_AUDIODRIVER=dummy")
    set_tests_properties(hud_boost_${car}_negative PROPERTIES WILL_FAIL TRUE)
  endforeach()
endif()

add_test(NAME menu_loading_fade_negative COMMAND menu_tests)
set_tests_properties(menu_loading_fade_negative PROPERTIES WILL_FAIL TRUE ENVIRONMENT "MOON_LOADING_FADE_NEGATIVE=1")

foreach(api opengl vulkan)
 foreach(part normal cutoff)
  add_test(NAME car_lamps_${api}_${part}_negative COMMAND car_lamps_test --${part}-negative)
  set_tests_properties(car_lamps_${api}_${part}_negative PROPERTIES WILL_FAIL TRUE RUN_SERIAL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;MOON_GRAPHICS_API=${api}")
 endforeach()
endforeach()

foreach(api opengl vulkan)
 add_test(NAME realistic_sky_${api}_night_negative COMMAND realistic_sky --night-negative)
 set_tests_properties(realistic_sky_${api}_night_negative PROPERTIES WILL_FAIL TRUE RUN_SERIAL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;MOON_GRAPHICS_API=${api}")
endforeach()

add_executable(course_surface_probe tests/render/course_surface_probe.c)
target_link_libraries(course_surface_probe PRIVATE ${MOON_SDL_STACK} miniz SDL2::SDL2main)

foreach(api opengl vulkan)
 add_test(NAME modern_lighting_${api}_local_negative COMMAND modern_lighting_tests --local-negative)
 set_tests_properties(modern_lighting_${api}_local_negative PROPERTIES WILL_FAIL TRUE RUN_SERIAL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;MOON_GRAPHICS_API=${api}")
endforeach()

foreach(api opengl vulkan)
 add_test(NAME daylight_baked_${api}_authored_negative COMMAND daylight_baked --authored-negative)
 set_tests_properties(daylight_baked_${api}_authored_negative PROPERTIES WILL_FAIL TRUE RUN_SERIAL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;MOON_GRAPHICS_API=${api}")
endforeach()

foreach(api opengl vulkan)
 add_test(NAME night_baked_${api}_authored_negative COMMAND night_baked --authored-negative)
 set_tests_properties(night_baked_${api}_authored_negative PROPERTIES WILL_FAIL TRUE RUN_SERIAL TRUE ENVIRONMENT "SDL_AUDIODRIVER=dummy;MOON_WINDOW_HIDDEN=1;MOON_GRAPHICS_API=${api}")
endforeach()

if(MOON_TEST_DISC)
 add_test(NAME options_navigation_negative COMMAND options_lifecycle "${MOON_TEST_DISC}")
 set_tests_properties(options_navigation_negative PROPERTIES WILL_FAIL TRUE RUN_SERIAL TRUE ENVIRONMENT "GT4_OPTIONS_NAV_NEGATIVE=1;SDL_AUDIODRIVER=dummy")
endif()
