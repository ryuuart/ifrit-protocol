# The flags a sketch outside the host is compiled with: the compile line
# of one anchor unit the host's build compiles, lifted out of the
# compilation database into a response file the live host hands its
# compiler (`Host::Options::flagsFile`).
#
# The anchor is the HOST'S: a unit that includes what the host's sketches
# may include, compiled inside the target that carries their include
# directories and definitions, so the line a hot-reloaded sketch compiles
# with is the line a compiled-in one compiled with. The framework names
# no anchor and no catalogue of its own.
#
# The response file is a tracked build output, not a post-build step:
# deleting it or reconfiguring — the compilation database is rewritten at
# generate time — regenerates it, whether or not the host relinks. The
# compilation database is the top-level tree's, so the tree must be
# configured with CMAKE_EXPORT_COMPILE_COMMANDS on.
#
#   sigil_sketch_flags(<host> ANCHOR <source> OUTPUT <file>)
#     writes <file> from <source>'s compile line and makes <host> wait for
#     it; the step is the target <host>_sketch_flags. <file> may carry
#     $<CONFIG>.

set(SIGIL_SKETCH_FLAGS_STEP "${CMAKE_CURRENT_LIST_DIR}/SketchFlags.py"
    CACHE INTERNAL "the step that lifts a sketch compile line")

function(sigil_sketch_flags host)
  cmake_parse_arguments(ARG "" "ANCHOR;OUTPUT" "" ${ARGN})
  if(NOT ARG_ANCHOR OR NOT ARG_OUTPUT)
    message(FATAL_ERROR
      "sigil_sketch_flags(${host}): ANCHOR and OUTPUT are required")
  endif()
  get_filename_component(anchor "${ARG_ANCHOR}" ABSOLUTE)
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  add_custom_command(OUTPUT "${ARG_OUTPUT}"
    COMMAND ${Python3_EXECUTABLE} "${SIGIL_SKETCH_FLAGS_STEP}"
      --compdb "${CMAKE_BINARY_DIR}/compile_commands.json"
      --anchor "${anchor}" --config "$<CONFIG>" --out "${ARG_OUTPUT}"
    DEPENDS "${CMAKE_BINARY_DIR}/compile_commands.json"
            "${SIGIL_SKETCH_FLAGS_STEP}"
    COMMENT "Extracting ${host}'s sketch compile flags from compile_commands.json"
    VERBATIM)
  add_custom_target(${host}_sketch_flags DEPENDS "${ARG_OUTPUT}")
  add_dependencies(${host} ${host}_sketch_flags)
endfunction()
