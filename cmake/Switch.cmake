# Wrapper around the official devkitPro toolchain; no SDK files are vendored.
if(NOT DEFINED ENV{DEVKITPRO})
  set(ENV{DEVKITPRO} "C:/devkitPro")
endif()
file(TO_CMAKE_PATH "$ENV{DEVKITPRO}" OPENGT4_DEVKITPRO)
if(NOT EXISTS "${OPENGT4_DEVKITPRO}/cmake/Switch.cmake")
  message(FATAL_ERROR "Install devkitPro switch-dev in DEVKITPRO first")
endif()
include("${OPENGT4_DEVKITPRO}/cmake/Switch.cmake")
