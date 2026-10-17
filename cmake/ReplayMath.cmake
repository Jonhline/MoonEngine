# Lower our native C scalar operations to the existing EE arithmetic model.
# Original executable/disassembly/generated game code are never build inputs.
get_target_property(physics_math_sources moon_physics SOURCES)
list(REMOVE_ITEM physics_math_sources src/core/physics/fpu_env.c)
file(GLOB physics_math_headers CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/core/physics/*.h")
execute_process(COMMAND "${Python3_EXECUTABLE}" -c "import pycparser"
  RESULT_VARIABLE physics_math_parser_status OUTPUT_QUIET ERROR_QUIET)
if(NOT physics_math_parser_status EQUAL 0)
  message(FATAL_ERROR "PS2 physics arithmetic requires pycparser: python -m pip install pycparser==3.0")
endif()
set(physics_math_compiler_args)
if(MSVC)
  list(APPEND physics_math_compiler_args --msvc)
endif()
foreach(kind IN ITEMS ps2 legacy)
  set(outputs)
  foreach(source IN LISTS physics_math_sources)
    get_filename_component(name "${source}" NAME)
    list(APPEND outputs "${CMAKE_CURRENT_BINARY_DIR}/physics_math/${kind}/${name}")
  endforeach()
  set(kind_args)
  if(kind STREQUAL "legacy")
    list(APPEND kind_args --legacy)
  endif()
  add_custom_command(
    OUTPUT ${outputs} "${CMAKE_CURRENT_BINARY_DIR}/physics_math/${kind}/prefix.h"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/ps2_replay_float.py"
      --compiler "${CMAKE_C_COMPILER}" ${physics_math_compiler_args} ${kind_args}
      --root "${CMAKE_CURRENT_SOURCE_DIR}" --out "${CMAKE_CURRENT_BINARY_DIR}/physics_math/${kind}"
      --generated-dir "${CMAKE_CURRENT_BINARY_DIR}/generated"
      ${physics_math_sources}
    DEPENDS ${physics_math_sources} ${physics_math_headers}
      "${CMAKE_CURRENT_SOURCE_DIR}/tools/ps2_replay_float.py"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" VERBATIM)
  set(physics_math_${kind}_outputs ${outputs})
endforeach()
add_library(moon_legacy_math OBJECT ${physics_math_legacy_outputs})
target_include_directories(moon_legacy_math PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/src/core"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/core/physics" "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ps2float"
  "${CMAKE_CURRENT_BINARY_DIR}/generated")
if(MSVC)
  target_compile_options(moon_legacy_math PRIVATE /fp:strict /wd4305)
  target_compile_definitions(moon_legacy_math PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(moon_legacy_math PRIVATE -ffp-contract=off -frounding-math)
endif()
set_property(TARGET moon_physics PROPERTY SOURCES ${physics_math_ps2_outputs}
  "${CMAKE_CURRENT_SOURCE_DIR}/src/core/physics/fpu_env.c"
  $<TARGET_OBJECTS:moon_legacy_math>)
target_include_directories(moon_physics PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ps2float")
if(NOT MSVC)
  target_compile_options(moon_physics PRIVATE -ffp-contract=off -frounding-math)
endif()

# Logger re-simulates/samples original commands too; its scalar math follows EE.
set(logger_math "${CMAKE_CURRENT_BINARY_DIR}/physics_math/logger/logger.c")
add_custom_command(OUTPUT "${logger_math}" "${CMAKE_CURRENT_BINARY_DIR}/physics_math/logger/prefix.h"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/ps2_replay_float.py"
    --compiler "${CMAKE_C_COMPILER}" ${physics_math_compiler_args}
    --root "${CMAKE_CURRENT_SOURCE_DIR}" --out "${CMAKE_CURRENT_BINARY_DIR}/physics_math/logger"
    --generated-dir "${CMAKE_CURRENT_BINARY_DIR}/generated" src/core/logger.c
  DEPENDS src/core/logger.c src/core/logger.h tools/ps2_replay_float.py ${physics_math_headers}
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" VERBATIM)
get_target_property(core_math_sources moon_core SOURCES)
list(REMOVE_ITEM core_math_sources src/core/logger.c)
set_property(TARGET moon_core PROPERTY SOURCES ${core_math_sources} "${logger_math}")
target_include_directories(moon_core PRIVATE src/core/physics third_party/ps2float)

if(MSVC)
  set_source_files_properties("${logger_math}" PROPERTIES COMPILE_OPTIONS "/fp:strict")
else()
  set_source_files_properties("${logger_math}" PROPERTIES COMPILE_OPTIONS "-ffp-contract=off;-frounding-math")
endif()
