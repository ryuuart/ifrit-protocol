# Python sketches compiled into a host's registry.
#
# A Python entry shares the native registry: only its name, description
# and declared package requirements are read at build time, into one
# generated translation unit of lazy entries, and opening a session
# imports the current source. The generated unit is an object library of
# its own, so Python stays out of the C++ sketch compilation and its
# hot-reload flags.
#
#   sigil_python_sketches(<target> [FILES <module.py>...])
#     makes the OBJECT library <target> registering every module in FILES
#     that declares a `@sketch` class; a host links it beside
#     SigilSketchPython.

set(SIGIL_PYTHON_SKETCHES_STEP
    "${CMAKE_CURRENT_LIST_DIR}/register_sketches.py"
    CACHE INTERNAL "the step that writes Python sketches' registry entries")

function(sigil_python_sketches target)
  cmake_parse_arguments(ARG "" "" "FILES" ${ARGN})
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  set(registry ${CMAKE_CURRENT_BINARY_DIR}/${target}.cpp)
  add_custom_command(
    OUTPUT ${registry}
    COMMAND ${Python3_EXECUTABLE} ${SIGIL_PYTHON_SKETCHES_STEP}
            --output ${registry} ${ARG_FILES}
    DEPENDS ${SIGIL_PYTHON_SKETCHES_STEP} ${ARG_FILES}
    COMMENT "Registering ${target}'s Python sketches"
    VERBATIM)
  add_library(${target} OBJECT ${registry})
  target_link_libraries(${target} PRIVATE SigilSketchPython)
endfunction()
