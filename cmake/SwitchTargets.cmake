# Included only after the native targets exist. Host tools/tests are not NROs.
set_target_properties(gt4vol gt4driver_survey PROPERTIES EXCLUDE_FROM_ALL TRUE)
find_package(PkgConfig REQUIRED)
if(MOON_SWITCH_RENDERER STREQUAL "deko3d")
  include(cmake/SwitchDeko.cmake)
else()
  pkg_check_modules(SWITCH_GL REQUIRED IMPORTED_TARGET egl glapi)
  target_link_libraries(moon_platform_sdl PUBLIC glad PkgConfig::SWITCH_GL)
  target_sources(moon_platform_sdl PRIVATE src/platform/sdl/switch_render.c)
endif()
# Mesa contains C++ code; let the C++ driver put its runtime after the archives.
set_target_properties(moonengine PROPERTIES LINKER_LANGUAGE CXX)
target_sources(moonengine PRIVATE src/app/disc_checkpoint.c)
target_compile_options(moon_physics PRIVATE -ffp-contract=off -frounding-math)
target_compile_definitions(moon_platform_common PRIVATE _FILE_OFFSET_BITS=64)
target_sources(moon_platform_common PRIVATE src/platform/common/switch_profile.c)
target_sources(moon_platform_common PRIVATE src/platform/common/switch_file.c)

target_compile_options(moonengine PRIVATE -ffp-contract=off -frounding-math)

set(MOON_SWITCH_ICON "" CACHE FILEPATH "Local 256x256 JPEG project icon")
if(NOT EXISTS "${MOON_SWITCH_ICON}")
  message(FATAL_ERROR "Set MOON_SWITCH_ICON to a local 256x256 JPEG")
endif()
nx_generate_nacp(OUTPUT moonengine.nacp NAME "Moon Engine" AUTHOR "Moon Engine" VERSION "0.1.0")
nx_create_nro(moonengine NACP moonengine.nacp ICON "${MOON_SWITCH_ICON}")

add_executable(switch_fpu_probe tests/platform/switch_fpu.c src/app/switch_fpu.c)
target_include_directories(switch_fpu_probe PRIVATE src/app)
target_compile_options(switch_fpu_probe PRIVATE -ffp-contract=off -frounding-math)
target_link_libraries(switch_fpu_probe PRIVATE moon_physics moon_platform_common)
nx_generate_nacp(OUTPUT switch_fpu_probe.nacp NAME "Moon Engine FPU check" AUTHOR "Moon Engine")
nx_create_nro(switch_fpu_probe NACP switch_fpu_probe.nacp NOICON)

# Standalone same-console renderer diagnostic; never replaces the game NRO.
if(MOON_SWITCH_RENDERER STREQUAL "deko3d")
  add_executable(switch_gpu_compare tests/platform/switch_gpu_compare.cpp)
  target_link_libraries(switch_gpu_compare PRIVATE moon_core moon_platform_sdl
    moon_platform_common miniz SDL2::SDL2main)
  nx_generate_nacp(OUTPUT switch_gpu_compare.nacp NAME "Moon Engine GPU Compare" AUTHOR "Moon Engine")
  nx_create_nro(switch_gpu_compare NACP switch_gpu_compare.nacp NOICON)
endif()
