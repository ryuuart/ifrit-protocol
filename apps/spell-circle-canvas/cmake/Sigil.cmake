# The calls every library under src/ is built from:
#
#   sigil_library_root()     opens a library — its include root, its test
#                            and bench directories, its documentation site
#   sigil_library()          one feature target: its sources, its public
#                            headers and the links the call names
#   sigil_test()             a GoogleTest binary registered with ctest
#   sigil_bench()            a Google Benchmark binary, never a test
#   sigil_qt_target()        turns Qt's source scanning on for one target
#   sigil_header_self_test() compiles every public header first and alone
#   sigil_shader_sources()   compiles a directory of shader text into a
#                            target, reachable through a generated accessor
#
# Link visibility is the caller's: nothing here decides PUBLIC against
# PRIVATE, adds a link a call did not name, or globs a source. Each
# function's contract is stated above it. A build module only ONE library
# needs lives in that library's own cmake/ directory instead.

# sigil_library_root(<Name> BRIEF "<one line>" [DOCS <file>...])
#   Once, in a library's root CMakeLists.txt. For that directory and every
#   one added beneath it, sets
#     SIGIL_LIBRARY_ROOT      the root directory
#     SIGIL_INCLUDE_DIR       <root>/include, the one public include root
#     SIGIL_HEADER_NAMESPACE  the one directory under include/
#     SIGIL_TEST_DIR          <root>/test, the fixtures every test shares
#     SIGIL_BENCH_DIR         <root>/bench, the corpus every bench shares
#   and registers the documentation site, with README.md as its front page
#   and DOCS as further pages.
function(sigil_library_root name)
  cmake_parse_arguments(ARG "" "BRIEF" "DOCS" ${ARGN})
  set(root ${CMAKE_CURRENT_LIST_DIR})
  file(GLOB entries RELATIVE ${root}/include LIST_DIRECTORIES true
       ${root}/include/*)
  set(namespaces)
  foreach(entry IN LISTS entries)
    if(IS_DIRECTORY ${root}/include/${entry})
      list(APPEND namespaces ${entry})
    endif()
  endforeach()
  list(LENGTH namespaces count)
  if(NOT count EQUAL 1)
    message(FATAL_ERROR
      "sigil_library_root(${name}): include/ must hold one directory, "
      "found '${namespaces}'")
  endif()
  set(SIGIL_LIBRARY_ROOT ${root} PARENT_SCOPE)
  set(SIGIL_INCLUDE_DIR ${root}/include PARENT_SCOPE)
  set(SIGIL_HEADER_NAMESPACE ${namespaces} PARENT_SCOPE)
  set(SIGIL_TEST_DIR ${root}/test PARENT_SCOPE)
  set(SIGIL_BENCH_DIR ${root}/bench PARENT_SCOPE)
  sigil_add_docs(
    NAME ${name}
    BRIEF "${ARG_BRIEF}"
    INPUT ${root}/include ${root}/README.md ${ARG_DOCS}
    MAINPAGE ${root}/README.md
    STRIP ${root}/include)
endfunction()

# sigil_library(<Target> [PIC] [SOURCES <file>...] [HEADERS <file>...]
#               [PUBLIC <item>...] [PRIVATE <item>...] [INTERFACE <item>...]
#               [INCLUDE_PRIVATE <dir>...])
#   A STATIC archive, or an INTERFACE target when there are no SOURCES,
#   carrying SIGIL_INCLUDE_DIR on its public include path and the links the
#   call names. SOURCES are relative to the calling directory; HEADERS to
#   include/<namespace>/<calling directory relative to the root>/, so a
#   feature names its own headers bare and a sibling's through `..`. PIC
#   marks the archive position-independent, for a host that force-loads it
#   into a dylib's flat namespace.
function(sigil_library target)
  cmake_parse_arguments(ARG "PIC" ""
    "SOURCES;HEADERS;PUBLIC;PRIVATE;INTERFACE;INCLUDE_PRIVATE" ${ARGN})
  if(NOT SIGIL_LIBRARY_ROOT)
    message(FATAL_ERROR
      "sigil_library(${target}): no sigil_library_root() above this directory")
  endif()
  file(RELATIVE_PATH feature ${SIGIL_LIBRARY_ROOT} ${CMAKE_CURRENT_LIST_DIR})
  set(header_dir ${SIGIL_INCLUDE_DIR}/${SIGIL_HEADER_NAMESPACE})
  if(feature)
    string(APPEND header_dir /${feature})
  endif()
  set(headers)
  foreach(header IN LISTS ARG_HEADERS)
    if(NOT IS_ABSOLUTE ${header})
      get_filename_component(header ${header_dir}/${header} ABSOLUTE)
    endif()
    list(APPEND headers ${header})
  endforeach()

  if(ARG_SOURCES)
    add_library(${target} STATIC ${ARG_SOURCES} ${headers})
    target_include_directories(${target} PUBLIC ${SIGIL_INCLUDE_DIR})
    if(ARG_INCLUDE_PRIVATE)
      target_include_directories(${target} PRIVATE ${ARG_INCLUDE_PRIVATE})
    endif()
    if(ARG_PIC)
      set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()
    if(ARG_PUBLIC)
      target_link_libraries(${target} PUBLIC ${ARG_PUBLIC})
    endif()
    if(ARG_PRIVATE)
      target_link_libraries(${target} PRIVATE ${ARG_PRIVATE})
    endif()
    if(ARG_INTERFACE)
      target_link_libraries(${target} INTERFACE ${ARG_INTERFACE})
    endif()
  else()
    if(ARG_PUBLIC OR ARG_PRIVATE OR ARG_INCLUDE_PRIVATE OR ARG_PIC)
      message(FATAL_ERROR
        "sigil_library(${target}): a target with no SOURCES is INTERFACE "
        "and takes INTERFACE links only")
    endif()
    add_library(${target} INTERFACE ${headers})
    target_include_directories(${target} INTERFACE ${SIGIL_INCLUDE_DIR})
    if(ARG_INTERFACE)
      target_link_libraries(${target} INTERFACE ${ARG_INTERFACE})
    endif()
  endif()
endfunction()

# What a test and a bench binary share: the support directories, the
# fixture directory, the caller's definitions, and ARC on the
# Objective-C++ sources.
function(_sigil_binary_support target kind)
  cmake_parse_arguments(ARG "ARC" "ASSET_DIR"
    "SOURCES;DEFINITIONS;SUPPORT_DIRS" ${ARGN})
  set(dirs)
  if(kind STREQUAL test)
    list(APPEND dirs ${SIGIL_TEST_DIR} ${SIGIL_TEST_SUPPORT_DIR})
  else()
    # A bench reads the same fixtures a test does, so the tree-wide support
    # directory is on its include path too.
    list(APPEND dirs ${SIGIL_BENCH_DIR} ${SIGIL_TEST_SUPPORT_DIR})
  endif()
  list(APPEND dirs ${ARG_SUPPORT_DIRS})
  if(dirs)
    target_include_directories(${target} PRIVATE ${dirs})
  endif()
  if(NOT ARG_ASSET_DIR AND SIGIL_TEST_DIR)
    set(ARG_ASSET_DIR ${SIGIL_TEST_DIR}/assets)
  endif()
  if(ARG_ASSET_DIR)
    target_compile_definitions(${target} PRIVATE
      SIGIL_TEST_ASSET_DIR="${ARG_ASSET_DIR}")
  endif()
  # The instrument faces are the whole tree's, not one library's, so they
  # are named separately from the fixtures a single library commits: a
  # binary reads both, and "Fonts.h" spells only this one.
  if(SIGIL_TEST_SUPPORT_DIR)
    target_compile_definitions(${target} PRIVATE
      SIGIL_TEST_INSTRUMENT_DIR="${SIGIL_TEST_SUPPORT_DIR}/assets")
  endif()
  if(ARG_DEFINITIONS)
    target_compile_definitions(${target} PRIVATE ${ARG_DEFINITIONS})
  endif()
  if(ARG_ARC)
    foreach(source IN LISTS ARG_SOURCES)
      if(source MATCHES "\\.mm$")
        set_source_files_properties(${source} PROPERTIES
          COMPILE_OPTIONS -fobjc-arc)
      endif()
    endforeach()
  endif()
endfunction()

# sigil_test(<name> SOURCES <file>... [LIBRARIES <item>...] [LABELS <label>...]
#            [DEFINITIONS <define>...] [SUPPORT_DIRS <dir>...]
#            [ASSET_DIR <dir>] [ARC])
#   A GoogleTest binary registered with ctest. It sees SIGIL_TEST_DIR, the
#   tree-wide test support directory and SUPPORT_DIRS, and reads its
#   committed fixtures through SIGIL_TEST_ASSET_DIR — <root>/test/assets
#   unless ASSET_DIR says otherwise — and the tree's instrument faces
#   through SIGIL_TEST_INSTRUMENT_DIR. ARC compiles the Objective-C++
#   sources with automatic reference counting.
function(sigil_test name)
  cmake_parse_arguments(ARG "ARC" "ASSET_DIR"
    "SOURCES;LIBRARIES;LABELS;DEFINITIONS;SUPPORT_DIRS" ${ARGN})
  # The framework every test binary links, found by the call that links it.
  if(NOT TARGET GTest::gtest_main)
    find_package(GTest CONFIG REQUIRED)
  endif()
  add_executable(${name} ${ARG_SOURCES})
  target_link_libraries(${name} PRIVATE ${ARG_LIBRARIES} GTest::gtest_main)
  set(arc)
  if(ARG_ARC)
    set(arc ARC)
  endif()
  _sigil_binary_support(${name} test ${arc}
    SOURCES ${ARG_SOURCES}
    DEFINITIONS ${ARG_DEFINITIONS}
    SUPPORT_DIRS ${ARG_SUPPORT_DIRS}
    ASSET_DIR ${ARG_ASSET_DIR})
  add_test(NAME ${name} COMMAND ${name})
  if(ARG_LABELS)
    set_tests_properties(${name} PROPERTIES LABELS "${ARG_LABELS}")
  endif()
endfunction()

# sigil_bench(<name> SOURCES <file>... [LIBRARIES <item>...]
#             [DEFINITIONS <define>...] [SUPPORT_DIRS <dir>...] [GPU] [ARC])
#   A Google Benchmark binary hung off the `benches` target and never a
#   test. It sees SIGIL_BENCH_DIR and SUPPORT_DIRS and reads fixtures
#   through SIGIL_TEST_ASSET_DIR and instrument faces through
#   SIGIL_TEST_INSTRUMENT_DIR. GPU adds the Graphite arm where a device
#   is guaranteed: on Apple the binary links SigilSkia and is compiled with
#   SIGIL_BENCH_GPU.
function(sigil_bench name)
  cmake_parse_arguments(ARG "GPU;ARC" ""
    "SOURCES;LIBRARIES;DEFINITIONS;SUPPORT_DIRS" ${ARGN})
  if(NOT TARGET benchmark::benchmark)
    find_package(benchmark CONFIG REQUIRED)
  endif()
  add_executable(${name} ${ARG_SOURCES})
  target_link_libraries(${name} PRIVATE ${ARG_LIBRARIES} benchmark::benchmark)
  if(ARG_GPU AND APPLE)
    target_link_libraries(${name} PRIVATE SigilSkia)
    target_compile_definitions(${name} PRIVATE SIGIL_BENCH_GPU)
  endif()
  set(arc)
  if(ARG_ARC)
    set(arc ARC)
  endif()
  _sigil_binary_support(${name} bench ${arc}
    SOURCES ${ARG_SOURCES}
    DEFINITIONS ${ARG_DEFINITIONS}
    SUPPORT_DIRS ${ARG_SUPPORT_DIRS})
  add_dependencies(benches ${name})
endfunction()

# sigil_qt_target(<target>...)
#   Turns moc on for targets that carry Qt types. The scan is a per-target
#   build step that reads every source whether or not a Q_OBJECT is there,
#   and almost nothing in this tree is a Qt target, so it is off by default
#   (CMAKE_AUTOMOC in the top-level file) and named here by the targets
#   that need it. Call it right after the target is created and BEFORE
#   qt_add_qml_module(), which registers its types out of moc's output.
function(sigil_qt_target)
  foreach(target IN LISTS ARGV)
    set_target_properties(${target} PROPERTIES AUTOMOC ON)
  endforeach()
endfunction()

# sigil_header_self_test(<target> HEADERS <file>... [LIBRARIES <item>...]
#                        [INCLUDE_ROOT <dir>])
#   One generated translation unit per header holding two `#include` lines
#   — the first proves the header stands alone, the second that it can be
#   included twice — compiled into one OBJECT library in the default build
#   and registered with ctest as <target>. LIBRARIES are every feature
#   target a header can belong to; HEADERS are absolute paths under
#   INCLUDE_ROOT, which defaults to the calling library's SIGIL_INCLUDE_DIR.
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

# sigil_shader_sources(<target> DIR <dir> NAMESPACE <ns> [NAME <stem>]
#                      [PATTERNS <glob>...])
#   The one lane a library's STOCK shader text reaches it by. Globs <dir>
#   (CONFIGURE_DEPENDS) and generates one header and one translation unit
#   carrying every file's bytes as a std::string_view table keyed by the
#   path relative to <dir>, so a binary carries every shader it can draw
#   with. The accessor is declared in <ns> and reached as
#   <sigilshaders/<stem>.h>, <stem> defaulting to the target's name without
#   its Sigil prefix; PATTERNS defaults to *.sksl and *.slang. One accessor
#   per target and no re-export: a consumer that needs another library's
#   shader text asks that library for it by name. A shader a USER authored
#   is a different thing and arrives by URI through SigilIO.
set(SIGIL_SHADER_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated/shaders"
    CACHE INTERNAL "generated shader-source tables")
set(SIGIL_CMAKE_MODULE "${CMAKE_CURRENT_LIST_FILE}"
    CACHE INTERNAL "this file, which is also the shader-embedding script")

function(sigil_shader_sources target)
  cmake_parse_arguments(ARG "" "DIR;NAMESPACE;NAME" "PATTERNS" ${ARGN})
  if(NOT ARG_DIR OR NOT ARG_NAMESPACE)
    message(FATAL_ERROR
      "sigil_shader_sources(${target}): DIR and NAMESPACE are required")
  endif()
  if(NOT ARG_NAME)
    string(REGEX REPLACE "^Sigil" "" ARG_NAME "${target}")
  endif()
  if(NOT ARG_PATTERNS)
    set(ARG_PATTERNS "*.sksl" "*.slang")
  endif()

  get_filename_component(_dir "${ARG_DIR}" ABSOLUTE)
  set(_globs)
  foreach(_pattern IN LISTS ARG_PATTERNS)
    list(APPEND _globs "${_dir}/${_pattern}" "${_dir}/*/${_pattern}")
  endforeach()
  file(GLOB_RECURSE _files CONFIGURE_DEPENDS ${_globs})
  list(SORT _files)
  if(NOT _files)
    message(FATAL_ERROR
      "sigil_shader_sources(${target}): ${_dir} holds no shader")
  endif()

  set(_header "${SIGIL_SHADER_GENERATED_DIR}/sigilshaders/${ARG_NAME}.h")
  set(_source "${SIGIL_SHADER_GENERATED_DIR}/${ARG_NAME}.cpp")
  add_custom_command(
    OUTPUT "${_header}" "${_source}"
    COMMAND ${CMAKE_COMMAND}
            -DNAME=${ARG_NAME}
            "-DSHADER_NAMESPACE=${ARG_NAMESPACE}"
            "-DDIRECTORY=${_dir}"
            "-DFILES=${_files}"
            "-DHEADER=${_header}"
            "-DSOURCE=${_source}"
            -P "${SIGIL_CMAKE_MODULE}"
    DEPENDS ${_files} "${SIGIL_CMAKE_MODULE}"
    COMMENT "embedding ${ARG_NAME} shader sources"
    VERBATIM)

  set_source_files_properties(${_files} PROPERTIES HEADER_FILE_ONLY TRUE)
  target_sources(${target} PRIVATE "${_header}" "${_source}" ${_files})
  target_include_directories(${target} PRIVATE "${SIGIL_SHADER_GENERATED_DIR}")
endfunction()

# ---------------------------------------------------------------------------
# Script mode. `cmake -DNAME=… -DSHADER_NAMESPACE=… -DDIRECTORY=… -DFILES=…
# -DHEADER=… -DSOURCE=… -P Sigil.cmake` writes the pair of files
# sigil_shader_sources() adds to a target — the command above is its only
# caller. Each shader's bytes become a raw string literal, so nothing in a
# shader has to be escaped; a text containing the terminator itself is a
# build failure here rather than a mangled body at run time. Nothing below
# runs during a configure.
if(CMAKE_SCRIPT_MODE_FILE)
  set(_terminator ")SHADER\"")

  set(_entries "")
  foreach(_file IN LISTS FILES)
    file(RELATIVE_PATH _key "${DIRECTORY}" "${_file}")
    file(READ "${_file}" _text)
    string(FIND "${_text}" "${_terminator}" _clash)
    if(NOT _clash EQUAL -1)
      message(FATAL_ERROR "${_file} contains the raw literal terminator")
    endif()
    string(APPEND _entries "    {\"${_key}\", R\"SHADER(\n${_text})SHADER\"},\n")
  endforeach()

  set(_out "#pragma once\n")
  string(APPEND _out "// Generated from the shader directory. Do not edit.\n\n")
  string(APPEND _out "#include <span>\n#include <string_view>\n\n")
  string(APPEND _out "namespace ${SHADER_NAMESPACE} {\n\n")
  string(APPEND _out
    "/** One stock shader: its path beneath this library's shader directory,\n"
    " *  and its text, compiled into the archive. */\n"
    "struct ShaderSource {\n"
    "  std::string_view name;\n"
    "  std::string_view text;\n"
    "};\n\n")
  string(APPEND _out
    "/** The text of the stock shader @p name, or an empty view when this\n"
    " *  library ships no such file. */\n"
    "std::string_view shaderSource(std::string_view name);\n\n")
  string(APPEND _out
    "/** Every stock shader this library ships, in name order. */\n"
    "std::span<const ShaderSource> shaderSources();\n\n")
  string(APPEND _out "}  // namespace ${SHADER_NAMESPACE}\n")

  file(WRITE "${HEADER}.tmp" "${_out}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
                  "${HEADER}.tmp" "${HEADER}")
  file(REMOVE "${HEADER}.tmp")

  set(_body "// Generated from the shader directory. Do not edit.\n\n")
  string(APPEND _body "#include <sigilshaders/${NAME}.h>\n\n")
  string(APPEND _body "namespace ${SHADER_NAMESPACE} {\n\n")
  string(APPEND _body "namespace {\n\n")
  string(APPEND _body "constexpr ShaderSource kSources[] = {\n")
  string(APPEND _body "${_entries}")
  string(APPEND _body "};\n\n}  // namespace\n\n")
  string(APPEND _body
    "std::string_view shaderSource(std::string_view name) {\n"
    "  for (const ShaderSource& source : kSources)\n"
    "    if (source.name == name) return source.text;\n"
    "  return {};\n"
    "}\n\n")
  string(APPEND _body
    "std::span<const ShaderSource> shaderSources() { return kSources; }\n\n")
  string(APPEND _body "}  // namespace ${SHADER_NAMESPACE}\n")

  file(WRITE "${SOURCE}.tmp" "${_body}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
                  "${SOURCE}.tmp" "${SOURCE}")
  file(REMOVE "${SOURCE}.tmp")
endif()
