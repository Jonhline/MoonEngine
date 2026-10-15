find_program(OPENGT4_UAM NAMES uam PATHS "$ENV{DEVKITPRO}/tools/bin" REQUIRED)
set(DEKO_SHADER_BINARIES)
foreach(STAGE ui.vert ui.frag mesh.vert mesh.frag mix.frag probe_first_mesh.vert probe_first_mesh.frag probe_broken_mesh.vert)
  if(STAGE MATCHES "vert$")
    set(KIND vert)
  else()
    set(KIND frag)
  endif()
  set(BINARY "${CMAKE_BINARY_DIR}/generated/${STAGE}.dksh")
  add_custom_command(OUTPUT "${BINARY}"
    COMMAND "${OPENGT4_UAM}" -s ${KIND} -o "${BINARY}"
      "${CMAKE_SOURCE_DIR}/src/platform/switch/shaders/${STAGE}"
    DEPENDS "src/platform/switch/shaders/${STAGE}"
    VERBATIM)
  list(APPEND DEKO_SHADER_BINARIES "${BINARY}")
endforeach()
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/generated/deko_shaders.h"
  COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_BINARY_DIR}/generated"
    -P "${CMAKE_SOURCE_DIR}/cmake/EmbedDeko.cmake"
  DEPENDS ${DEKO_SHADER_BINARIES} cmake/EmbedDeko.cmake VERBATIM)
target_sources(opengt4_platform_sdl PRIVATE "${CMAKE_BINARY_DIR}/generated/deko_shaders.h")
target_compile_definitions(opengt4_platform_sdl PUBLIC OPENGT4_DEKO3D=1)
target_link_options(opengt4_platform_sdl INTERFACE "-Wl,--wrap=nvGpuChannelKickoff")
option(OPENGT4_DEKO_DIAGNOSTICS "Log native SDK GPU faults and validate command inputs" OFF)
if(OPENGT4_DEKO_DIAGNOSTICS)
  target_compile_definitions(opengt4_platform_sdl PRIVATE OPENGT4_DEKO_DIAGNOSTICS=1)
  # The release SDK erases DK_WARNING/error messages, including GPU fault info.
  # Keep game Release optimization; only select the SDK archive with diagnostics.
  target_link_libraries(opengt4_platform_sdl PUBLIC deko3dd)
else()
  target_link_libraries(opengt4_platform_sdl PUBLIC deko3d)
endif()
set_source_files_properties(src/platform/switch/deko_device.cpp
  src/platform/switch/deko_video.cpp src/platform/switch/deko_render.cpp
  PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Werror=return-type")
