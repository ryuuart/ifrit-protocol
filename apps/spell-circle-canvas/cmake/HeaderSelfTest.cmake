# Every public header of a library compiles first and alone.
#
# sigil_header_self_test(<target> HEADERS <file>... [LIBRARIES <item>...]
#                        [INCLUDE_ROOT <dir>])
#   One generated translation unit per header, holding nothing but two
#   `#include <path/from/the/include/root>` lines — the first proves the
#   header stands on its own (it declares or includes everything it
#   names), the second that it can be included twice — compiled into one
#   OBJECT library that is part of the default build, so a header that
#   leans on what an earlier include happened to bring in breaks the
#   build rather than the next consumer. The same target is registered
#   with ctest as <target>, a case that builds it, so the verdict has a
#   name in a test run. LIBRARIES are the targets whose include paths and
#   compile definitions the headers need — every feature target a
#   header can belong to, including the optional ones, which the caller
#   guards. HEADERS are absolute paths under INCLUDE_ROOT, which defaults
#   to the SIGIL_INCLUDE_DIR of the library the call is made in.

function(sigil_header_self_test target)
  cmake_parse_arguments(ARG "" "INCLUDE_ROOT" "HEADERS;LIBRARIES" ${ARGN})
  if(NOT ARG_INCLUDE_ROOT)
    set(ARG_INCLUDE_ROOT ${SIGIL_INCLUDE_DIR})
  endif()
  if(NOT ARG_INCLUDE_ROOT)
    message(FATAL_ERROR
      "sigil_header_self_test(${target}): no INCLUDE_ROOT and no "
      "sigil_library_root() above this directory")
  endif()
  if(NOT ARG_HEADERS)
    message(FATAL_ERROR "sigil_header_self_test(${target}): no HEADERS")
  endif()
  set(dir ${CMAKE_CURRENT_BINARY_DIR}/${target})
  set(sources)
  foreach(header IN LISTS ARG_HEADERS)
    file(RELATIVE_PATH spelled ${ARG_INCLUDE_ROOT} ${header})
    if(spelled MATCHES "^\\.\\.")
      message(FATAL_ERROR
        "sigil_header_self_test(${target}): ${header} is not under "
        "${ARG_INCLUDE_ROOT}")
    endif()
    string(REGEX REPLACE "\\.[^./]*$" ".cpp" source ${dir}/${spelled})
    set(content
      "// Generated: <${spelled}> must compile first and alone, and twice.\n"
      "#include <${spelled}>\n"
      "#include <${spelled}>\n")
    string(JOIN "" content ${content})
    # Written only when it differs, so a reconfigure does not touch every
    # probe and rebuild them all.
    set(existing)
    if(EXISTS ${source})
      file(READ ${source} existing)
    endif()
    if(NOT existing STREQUAL content)
      file(WRITE ${source} "${content}")
    endif()
    list(APPEND sources ${source})
  endforeach()
  add_library(${target} OBJECT ${sources})
  if(ARG_LIBRARIES)
    target_link_libraries(${target} PRIVATE ${ARG_LIBRARIES})
  endif()
  add_test(NAME ${target}
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
            --config $<CONFIG> --target ${target})
endfunction()
