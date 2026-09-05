# The four calls a Sigil library is built from, and the one a library
# adopts to prove its headers. Link visibility is the caller's: nothing
# here decides PUBLIC against PRIVATE, adds a link a call did not name, or
# globs a source.
#
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
#
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
#
# sigil_test(<name> SOURCES <file>... [LIBRARIES <item>...] [LABELS <label>...]
#            [DEFINITIONS <define>...] [SUPPORT_DIRS <dir>...]
#            [ASSET_DIR <dir>] [ARC])
#   A GoogleTest binary registered with ctest. It sees SIGIL_TEST_DIR, the
#   tree-wide test support directory and SUPPORT_DIRS, and reads its
#   committed fixtures through SIGIL_TEST_ASSET_DIR — <root>/test/assets
#   unless ASSET_DIR says otherwise — and the tree's instrument faces
#   through SIGIL_TEST_INSTRUMENT_DIR. ARC compiles the Objective-C++
#   sources with automatic reference counting.
#
# sigil_bench(<name> SOURCES <file>... [LIBRARIES <item>...]
#             [DEFINITIONS <define>...] [SUPPORT_DIRS <dir>...] [GPU] [ARC])
#   A Google Benchmark binary hung off the `benches` target and never a
#   test. It sees SIGIL_BENCH_DIR and SUPPORT_DIRS and reads fixtures
#   through SIGIL_TEST_ASSET_DIR and instrument faces through
#   SIGIL_TEST_INSTRUMENT_DIR. GPU adds the Graphite arm where a device
#   is guaranteed: on Apple the binary links SigilSkia and is compiled with
#   SIGIL_BENCH_GPU.
#
# sigil_shader_sources(<target> DIR <dir> NAMESPACE <ns> [NAME <stem>]
#                      [PATTERNS <glob>...])
#   Defined in ShaderSources.cmake. A library whose stock shaders live in
#   one directory adopts it once per target: the bytes are compiled into
#   the archive and reached through the accessor the generated header
#   declares, so a binary carries every shader it can draw with.
#
# sigil_header_self_test(<target> HEADERS <file>... [LIBRARIES <item>...])
#   Defined in HeaderSelfTest.cmake. A library that claims every public
#   header stands alone adopts it once, in its root CMakeLists.txt after
#   its features: glob include/ with CONFIGURE_DEPENDS for HEADERS, name
#   every feature target a header can belong to as LIBRARIES — filtering
#   out the headers of an optional feature whose target is absent — and
#   the build compiles each header first, alone and twice.

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

include(HeaderSelfTest)
include(ShaderSources)
