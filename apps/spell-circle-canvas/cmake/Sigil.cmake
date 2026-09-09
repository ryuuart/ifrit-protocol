# The calls every library under src/ is built from:
#
#   sigil_library_root()     opens a library — its include root, its test
#                            and bench directories, its documentation site
#   sigil_library()          one feature target: its sources, its public
#                            headers and the links the call names
#   sigil_test()             this directory's cases, in its library's one
#                            GoogleTest binary, one ctest entry per case
#   sigil_bench()            this directory's benchmarks, in its library's
#                            one Google Benchmark binary, never a test
#   sigil_finalize_tests()   the ctest entries, once every directory has
#                            contributed
#   sigil_qt_target()        turns Qt's source scanning on for one target
#   sigil_header_self_test() compiles every public header first and alone
#   sigil_doc_probes()       compiles the names a library's README spells
#                            against the headers that own them
#   sigil_shader_sources()   compiles a directory of shader text into a
#                            target, reachable through a generated accessor
#   sigil_frameworks()       resolves Apple frameworks by name, once for
#                            the whole tree
#
# Link visibility is the caller's: nothing here decides PUBLIC against
# PRIVATE, adds a link a call did not name, or globs a source. Each
# function's contract is stated above it. A build module only ONE library
# needs lives in that library's own cmake/ directory instead.

# Where gtest_discover_tests() comes from.
include(GoogleTest)

# The documentation sites, whose registration sigil_library_root() calls
# into: everything the whole tree shares is reached through this one file.
# Not in SCRIPT MODE — the shader embedding at the foot of this file runs
# `cmake -P` over it, where finding a package that declares an executable
# is an error and no target exists to register anyway.
if(NOT CMAKE_SCRIPT_MODE_FILE)
  include(${CMAKE_CURRENT_LIST_DIR}/Docs.cmake)
endif()

# sigil_frameworks(<out> <name>...)
#   Sets <out> in the caller's scope to the paths of the named Apple
#   frameworks, and to nothing at all off Apple, so a directory names what
#   it links the same way on every platform. Each name is searched for once
#   per build tree: the result is a cache entry keyed by the name, which
#   every later call in every other directory reads instead of searching
#   again.
function(sigil_frameworks out)
  set(paths)
  if(APPLE)
    foreach(name IN LISTS ARGN)
      string(TOUPPER ${name} upper)
      find_library(SIGIL_FRAMEWORK_${upper} ${name} REQUIRED)
      list(APPEND paths ${SIGIL_FRAMEWORK_${upper}})
    endforeach()
  endif()
  set(${out} ${paths} PARENT_SCOPE)
endfunction()

# Compiles every Objective-C++ source among SOURCES with automatic
# reference counting. Named per source rather than per target because a
# target can also carry C++ translation units that manage their own
# CoreFoundation references.
function(_sigil_arc_sources target)
  foreach(source IN LISTS ARGN)
    if(source MATCHES "\\.mm$")
      get_filename_component(source ${source} ABSOLUTE)
      set_source_files_properties(${source} TARGET_DIRECTORY ${target}
        PROPERTIES COMPILE_OPTIONS -fobjc-arc)
    endif()
  endforeach()
endfunction()

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

# sigil_library(<Target> [SOURCES <file>...] [HEADERS <file>...]
#               [PUBLIC <item>...] [PRIVATE <item>...] [INTERFACE <item>...]
#               [INCLUDE_PRIVATE <dir>...] [FRAMEWORKS <name>...] [ARC])
#   A STATIC archive, or an INTERFACE target when there are no SOURCES,
#   carrying SIGIL_INCLUDE_DIR on its public include path and the links the
#   call names. SOURCES are relative to the calling directory; HEADERS to
#   include/<namespace>/<calling directory relative to the root>/, so a
#   feature names its own headers bare and a sibling's through `..`.
#   FRAMEWORKS are Apple frameworks named bare, linked PRIVATE because
#   which system framework an implementation reaches for is nobody else's
#   business, and resolved once for the tree; off Apple they name nothing.
#   ARC compiles the Objective-C++ sources with automatic reference
#   counting.
function(sigil_library target)
  cmake_parse_arguments(ARG "ARC" ""
    "SOURCES;HEADERS;PUBLIC;PRIVATE;INTERFACE;INCLUDE_PRIVATE;FRAMEWORKS"
    ${ARGN})
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
    if(ARG_PUBLIC)
      target_link_libraries(${target} PUBLIC ${ARG_PUBLIC})
    endif()
    if(ARG_PRIVATE)
      target_link_libraries(${target} PRIVATE ${ARG_PRIVATE})
    endif()
    if(ARG_INTERFACE)
      target_link_libraries(${target} INTERFACE ${ARG_INTERFACE})
    endif()
    if(ARG_FRAMEWORKS)
      sigil_frameworks(frameworks ${ARG_FRAMEWORKS})
      if(frameworks)
        target_link_libraries(${target} PRIVATE ${frameworks})
      endif()
    endif()
    if(ARG_ARC)
      _sigil_arc_sources(${target} ${ARG_SOURCES})
    endif()
  else()
    if(ARG_PUBLIC OR ARG_PRIVATE OR ARG_INCLUDE_PRIVATE OR ARG_FRAMEWORKS
       OR ARG_ARC)
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
# Objective-C++ sources. Called once per contributing directory, so every
# value here is added to a target another directory may have created;
# CMake keeps one copy of a repeated include directory or definition.
function(_sigil_binary_support target kind)
  cmake_parse_arguments(ARG "ARC" "" "SOURCES;DEFINITIONS;SUPPORT_DIRS" ${ARGN})
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
  if(SIGIL_TEST_DIR)
    target_compile_definitions(${target} PRIVATE
      SIGIL_TEST_ASSET_DIR="${SIGIL_TEST_DIR}/assets")
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
    _sigil_arc_sources(${target} ${ARG_SOURCES})
  endif()
endfunction()

# One binary per library, in bin/<config>/tests and bin/<config>/benches.
# The first call creates it; every later call adds sources and links to the
# one that stands, which is how a directory keeps declaring its own cases
# beside the code they cover without a target per file.
function(_sigil_binary target kind)
  if(TARGET ${target})
    return()
  endif()
  add_executable(${target})
  set(home benches)
  if(kind STREQUAL test)
    set(home tests)
    if(NOT TARGET GTest::gtest_main)
      find_package(GTest CONFIG REQUIRED)
    endif()
    target_link_libraries(${target} PRIVATE GTest::gtest_main)
    set_property(TARGET ${target} PROPERTY SIGIL_TEST_LABEL_GROUPS "")
    set_property(TARGET ${target} PROPERTY SIGIL_TEST_LABELS "")
    # sigil_finalize_tests() writes the ctest entries, once every
    # directory has contributed its cases.
    set_property(GLOBAL APPEND PROPERTY SIGIL_TEST_BINARIES ${target})
  else()
    if(NOT TARGET benchmark::benchmark)
      find_package(benchmark CONFIG REQUIRED)
    endif()
    # benchmark_main supplies main() for the binary; a bench source that
    # needs its own defines it and the linker leaves this one out.
    target_link_libraries(${target} PRIVATE benchmark::benchmark_main)
    add_dependencies(benches ${target})
  endif()
  set_target_properties(${target} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/${home})
endfunction()

# sigil_finalize_tests()
#   Once, after every subdirectory has been added. One ctest entry per
#   CASE, discovered from each binary when ctest runs, so a suite or a
#   case is selected by name with no target behind it. A group of cases
#   that carries labels is discovered by itself with those labels on it;
#   everything the groups do not name is discovered with the binary's own.
function(sigil_finalize_tests)
  get_property(binaries GLOBAL PROPERTY SIGIL_TEST_BINARIES)
  foreach(target IN LISTS binaries)
    _sigil_discover_tests(${target})
  endforeach()
endfunction()

function(_sigil_discover_tests target)
  get_property(groups TARGET ${target} PROPERTY SIGIL_TEST_LABEL_GROUPS)
  set(labelled)
  foreach(group IN LISTS groups)
    string(REGEX MATCH "^([^|]*)\\|(.*)$" matched "${group}")
    string(REPLACE "," ";" labels "${CMAKE_MATCH_1}")
    string(REPLACE "," ";" patterns "${CMAKE_MATCH_2}")
    set(filter)
    foreach(pattern IN LISTS patterns)
      # A bare name is a whole suite; a name with a dot is one case.
      if(NOT pattern MATCHES "\\.")
        string(APPEND pattern ".*")
      endif()
      list(APPEND filter ${pattern})
      list(APPEND labelled ${pattern})
    endforeach()
    list(JOIN filter ":" filter)
    gtest_discover_tests(${target}
      TEST_FILTER "${filter}"
      PROPERTIES LABELS "${labels}"
      DISCOVERY_MODE PRE_TEST)
  endforeach()
  set(rest)
  if(labelled)
    list(JOIN labelled ":" rest)
    set(rest "-${rest}")
  endif()
  get_property(labels TARGET ${target} PROPERTY SIGIL_TEST_LABELS)
  list(REMOVE_DUPLICATES labels)
  gtest_discover_tests(${target}
    TEST_FILTER "${rest}"
    PROPERTIES LABELS "${labels}"
    DISCOVERY_MODE PRE_TEST)
endfunction()

# sigil_test(<library>_test SOURCES <file>... [LIBRARIES <item>...]
#            [SUITES <pattern>... LABELS <label>...]
#            [DEFINITIONS <define>...] [SUPPORT_DIRS <dir>...] [ARC])
#   Adds this directory's cases to its library's one GoogleTest binary. It
#   sees SIGIL_TEST_DIR, the tree-wide test support directory and
#   SUPPORT_DIRS, and reads the library's committed fixtures through
#   SIGIL_TEST_ASSET_DIR and the tree's instrument faces through
#   SIGIL_TEST_INSTRUMENT_DIR. LABELS are the ctest labels — what a runner
#   has to supply, so a run can leave those cases out; alone they go on
#   every case the binary holds that no SUITES group claims, and with
#   SUITES (GoogleTest suite names, or Suite.Case entries) on those cases
#   only. A call may carry SUITES and LABELS and no SOURCES, which labels
#   cases another call in the same library contributed. ARC compiles the
#   Objective-C++ sources with automatic reference counting.
function(sigil_test name)
  cmake_parse_arguments(ARG "ARC" ""
    "SOURCES;LIBRARIES;LABELS;SUITES;DEFINITIONS;SUPPORT_DIRS" ${ARGN})
  _sigil_binary(${name} test)
  if(ARG_SOURCES)
    target_sources(${name} PRIVATE ${ARG_SOURCES})
  endif()
  if(ARG_LIBRARIES)
    target_link_libraries(${name} PRIVATE ${ARG_LIBRARIES})
  endif()
  set(arc)
  if(ARG_ARC)
    set(arc ARC)
  endif()
  _sigil_binary_support(${name} test ${arc}
    SOURCES ${ARG_SOURCES}
    DEFINITIONS ${ARG_DEFINITIONS}
    SUPPORT_DIRS ${ARG_SUPPORT_DIRS})
  if(ARG_SUITES)
    if(NOT ARG_LABELS)
      message(FATAL_ERROR
        "sigil_test(${name}): SUITES names the cases a LABELS carries; "
        "without labels it says nothing")
    endif()
    list(JOIN ARG_LABELS "," labels)
    list(JOIN ARG_SUITES "," suites)
    set_property(TARGET ${name} APPEND PROPERTY
      SIGIL_TEST_LABEL_GROUPS "${labels}|${suites}")
  elseif(ARG_LABELS)
    set_property(TARGET ${name} APPEND PROPERTY SIGIL_TEST_LABELS ${ARG_LABELS})
  endif()
endfunction()

# sigil_bench(<library>_bench SOURCES <file>... [LIBRARIES <item>...]
#             [DEFINITIONS <define>...] [SUPPORT_DIRS <dir>...]
#             [GPU_LIBRARIES <item>...] [ARC])
#   Adds this directory's benchmarks to its library's one Google Benchmark
#   binary, which hangs off the `benches` target and is never a test. It
#   sees SIGIL_BENCH_DIR and SUPPORT_DIRS and reads fixtures through
#   SIGIL_TEST_ASSET_DIR and instrument faces through
#   SIGIL_TEST_INSTRUMENT_DIR. GPU_LIBRARIES names the device arm's link
#   set, added where a device is guaranteed: on Apple the binary links
#   them and is compiled with SIGIL_BENCH_GPU.
function(sigil_bench name)
  cmake_parse_arguments(ARG "ARC" ""
    "SOURCES;LIBRARIES;DEFINITIONS;SUPPORT_DIRS;GPU_LIBRARIES" ${ARGN})
  _sigil_binary(${name} bench)
  target_sources(${name} PRIVATE ${ARG_SOURCES})
  if(ARG_LIBRARIES)
    target_link_libraries(${name} PRIVATE ${ARG_LIBRARIES})
  endif()
  if(ARG_GPU_LIBRARIES AND APPLE)
    target_link_libraries(${name} PRIVATE ${ARG_GPU_LIBRARIES})
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

# sigil_doc_probes(<library> READMES <file>... [LIBRARIES <target>...]
#                  [INCLUDES <dir>...] [PRELUDES <spelling>...]
#                  [ALIASES <name>=<namespace>...] [EXCLUDE <name>=<reason>...]
#                  [SKIP_HEADERS <file name>...]
#                  [FLOORS <usings,members,indexed,listed,designators>]
#                  [NAMESPACE <namespace>] [SUPPORT_DIRS <dir>...]
#                  [DEFINITIONS <define>...])
#   Compiles a library's own prose against the headers that own it. One
#   generated translation unit per library, extracted from READMES on
#   every build, compiled into the OBJECT library <library>_api_doc_probes
#   and linked into <library>_test, where the generator's counts also
#   stand as one visible case; the generator FAILS on a documented name no
#   header declares, so the guard cannot go quiet.
#
#   WHAT IS CHECKED, and nothing else: every BACKTICKED qualified name
#   (`shapes::polygon`, `PathFormat::effect`) and every designated
#   initialiser, in fenced cpp blocks and in prose alike; and the BARE
#   backticked names of a bullet that opens with a header path, which are
#   looked up in that header's own text. A bare name anywhere else is
#   invisible, so a script verb, a QML type or a file name passes unprobed
#   — which is why a document may spell those freely. A qualified name
#   that is not C++ at all (a CMake target, say) has to be named in
#   EXCLUDE with the reason it cannot resolve.
#
#   INCLUDES are the further include roots the scanner reads, beside the
#   library's own; a name whose type only another library declares needs
#   that library's root here. PRELUDES are headers the translation unit
#   opens with, after gtest's; ALIASES the namespace aliases the prose
#   writes names through; SKIP_HEADERS the library's own headers the
#   translation unit must not include, which is how a header behind an SDK
#   or a UI toolkit stays out of a probe that compiles everywhere.
#   FLOORS arms the count guard: the five minima the
#   visible case asserts, which catch an extractor that stopped matching
#   rather than an ordinary edit. LIBRARIES are linked into both the
#   object library and the test binary, because a document that spells a
#   name claims it exists wherever it lives.
function(sigil_doc_probes library)
  cmake_parse_arguments(ARG "" "NAMESPACE;FLOORS"
    "READMES;LIBRARIES;INCLUDES;PRELUDES;ALIASES;EXCLUDE;SKIP_HEADERS;SUPPORT_DIRS;DEFINITIONS"
    ${ARGN})
  if(NOT ARG_READMES)
    message(FATAL_ERROR "sigil_doc_probes(${library}): no READMES")
  endif()
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  if(NOT TARGET GTest::gtest)
    find_package(GTest CONFIG REQUIRED)
  endif()
  set(script ${SIGIL_TEST_SUPPORT_DIR}/docs/api_doc_probes.py)
  string(SUBSTRING ${library} 0 1 head)
  string(TOUPPER ${head} head)
  string(SUBSTRING ${library} 1 -1 rest)
  set(Library ${head}${rest})
  set(target ${library}_api_doc_probes)
  set(generated ${CMAKE_CURRENT_BINARY_DIR}/${Library}ApiDocProbes.cpp)
  if(NOT ARG_NAMESPACE)
    set(ARG_NAMESPACE sigil::${library})
  endif()
  if(NOT ARG_FLOORS)
    set(ARG_FLOORS 0,0,0,0,0)
  endif()

  # The library's own include root leads, so its headers are the ones the
  # probe TU includes wholesale. A library whose public surface is not a
  # C++ include root has none, and then the preludes are all it opens.
  set(includes ${ARG_INCLUDES})
  set(scanned)
  if(SIGIL_HEADER_NAMESPACE)
    list(INSERT includes 0 ${SIGIL_INCLUDE_DIR}/${SIGIL_HEADER_NAMESPACE})
    file(GLOB_RECURSE scanned CONFIGURE_DEPENDS
         ${SIGIL_INCLUDE_DIR}/${SIGIL_HEADER_NAMESPACE}/*.h)
  endif()

  set(args)
  foreach(doc IN LISTS ARG_READMES)
    list(APPEND args --md ${doc})
  endforeach()
  foreach(dir IN LISTS includes)
    list(APPEND args --include ${dir})
  endforeach()
  if(SIGIL_HEADER_NAMESPACE)
    list(APPEND args --library ${SIGIL_HEADER_NAMESPACE})
  endif()
  list(APPEND args --namespace ${ARG_NAMESPACE}
                   --suite ${Library}Docs
                   --floors ${ARG_FLOORS}
                   --prelude "<gtest/gtest.h>")
  foreach(prelude IN LISTS ARG_PRELUDES)
    list(APPEND args --prelude "${prelude}")
  endforeach()
  foreach(alias IN LISTS ARG_ALIASES)
    list(APPEND args --alias "${alias}")
  endforeach()
  foreach(exclusion IN LISTS ARG_EXCLUDE)
    list(APPEND args --exclude "${exclusion}")
  endforeach()
  foreach(header IN LISTS ARG_SKIP_HEADERS)
    list(APPEND args --skip-header "${header}")
  endforeach()

  add_custom_command(
    OUTPUT ${generated}
    COMMAND ${Python3_EXECUTABLE} ${script} ${args} --out ${generated}
    DEPENDS ${ARG_READMES} ${script} ${scanned}
    COMMENT "Extracting the ${library} docs' names into compile probes"
    VERBATIM)

  # The generator's own fixtures: a real name must yield a probe, an
  # unreal one must fail the run, an operator spelling must be exempted
  # and reported by name, a header listing's bare names must be checked
  # against the header they are listed under, and an EXTERNAL_CLASSES
  # member must take the class-scope probe path. One Python invocation for
  # the whole tree, since the fixtures are the generator's and not any
  # library's — and the only check that notices the EXTRACTOR narrowing,
  # since a generator that silently probes less still emits a translation
  # unit that compiles green.
  get_property(registered GLOBAL PROPERTY SIGIL_DOC_PROBES_SELF_TEST)
  if(NOT registered)
    set_property(GLOBAL PROPERTY SIGIL_DOC_PROBES_SELF_TEST ON)
    add_test(NAME api_doc_probes_self_test
      COMMAND ${Python3_EXECUTABLE} ${script} --self-test)
  endif()

  # The probes are their own object library, because a generated source is
  # compiled by a target in the directory that generates it, while the
  # library's one test binary is assembled in the feature directories.
  add_library(${target} OBJECT ${generated})
  target_link_libraries(${target} PRIVATE ${ARG_LIBRARIES} GTest::gtest)
  _sigil_binary_support(${target} test
    SUPPORT_DIRS ${ARG_SUPPORT_DIRS}
    DEFINITIONS ${ARG_DEFINITIONS})
  sigil_test(${library}_test LIBRARIES ${target} ${ARG_LIBRARIES})
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
