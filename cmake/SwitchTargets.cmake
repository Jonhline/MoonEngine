# Included only after the native targets exist. Host tools/tests are not NROs.
set_target_properties(gt4vol gt4driver_survey PROPERTIES EXCLUDE_FROM_ALL TRUE)
find_package(PkgConfig REQUIRED)
if(OPENGT4_SWITCH_RENDERER STREQUAL "deko3d")
  include(cmake/SwitchDeko.cmake)
else()
  pkg_check_modules(SWITCH_GL REQUIRED IMPORTED_TARGET egl glapi)
  target_link_libraries(opengt4_platform_sdl PUBLIC glad PkgConfig::SWITCH_GL)
  target_sources(opengt4_platform_sdl PRIVATE src/platform/sdl/switch_render.c)
endif()
# Mesa contains C++ code; let the C++ driver put its runtime after the archives.
set_target_properties(opengt4 PROPERTIES LINKER_LANGUAGE CXX)
target_sources(opengt4 PRIVATE src/app/disc_checkpoint.c)
target_compile_options(opengt4_physics PRIVATE -ffp-contract=off -frounding-math)
target_compile_definitions(opengt4_platform_common PRIVATE _FILE_OFFSET_BITS=64)
target_sources(opengt4_platform_common PRIVATE src/platform/common/switch_profile.c)
target_sources(opengt4_platform_common PRIVATE src/platform/common/switch_file.c)

target_compile_options(opengt4 PRIVATE -ffp-contract=off -frounding-math)

set(OPENGT4_SWITCH_ICON "" CACHE FILEPATH "Local 256x256 JPEG project icon")
if(NOT EXISTS "${OPENGT4_SWITCH_ICON}")
  message(FATAL_ERROR "Set OPENGT4_SWITCH_ICON to a local 256x256 JPEG")
endif()
nx_generate_nacp(OUTPUT opengt4.nacp NAME "OpenGT4" AUTHOR "OpenGT4" VERSION "0.1.0")
nx_create_nro(opengt4 NACP opengt4.nacp ICON "${OPENGT4_SWITCH_ICON}")

add_executable(switch_fpu_probe tests/platform/switch_fpu.c src/app/switch_fpu.c)
target_include_directories(switch_fpu_probe PRIVATE src/app)
target_compile_options(switch_fpu_probe PRIVATE -ffp-contract=off -frounding-math)
target_link_libraries(switch_fpu_probe PRIVATE opengt4_physics opengt4_platform_common)
nx_generate_nacp(OUTPUT switch_fpu_probe.nacp NAME "OpenGT4 FPU check" AUTHOR "OpenGT4")
nx_create_nro(switch_fpu_probe NACP switch_fpu_probe.nacp NOICON)

# Standalone same-console renderer diagnostic; never replaces the game NRO.
if(OPENGT4_SWITCH_RENDERER STREQUAL "deko3d")
  add_executable(switch_gpu_compare tests/platform/switch_gpu_compare.cpp)
  target_link_libraries(switch_gpu_compare PRIVATE opengt4_core opengt4_platform_sdl
    opengt4_platform_common miniz SDL2::SDL2main)
  nx_generate_nacp(OUTPUT switch_gpu_compare.nacp NAME "OpenGT4 GPU Compare" AUTHOR "OpenGT4")
  nx_create_nro(switch_gpu_compare NACP switch_gpu_compare.nacp NOICON)
endif()
