# SigilSubstance — .sbsar procedural materials rendered to SkImages
# through the Adobe Substance 3D SDK's framework and CPU engine. Only
# configured when the SDK was found (see the top-level CMakeLists.txt);
# nothing here is vendored from the SDK.
#
# One static target over two subjects: graph/ (a graph described,
# changed, cooked and read) and package/ (the archive that owns its
# graphs and the engine renderer they share). They are one feature
# because neither exists without the other — a Graph is only ever
# constructed by its Package, and a Package is nothing but its graphs —
# so a test of one is a test of both. Every public header lives under
# include/sigilsubstance/<subject>/, and <sigilsubstance/Substance.h> is
# the umbrella over them.

set(SIGIL_SUBSTANCE_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include)

add_library(SigilSubstance STATIC
  graph/Describe.cpp
  graph/Graph.cpp
  graph/GraphImpl.h
  graph/Inputs.cpp
  graph/Numeric.h
  graph/Render.cpp
  package/Package.cpp
  ${SIGIL_SUBSTANCE_INCLUDE_DIR}/sigilsubstance/Substance.h
  ${SIGIL_SUBSTANCE_INCLUDE_DIR}/sigilsubstance/graph/Graph.h
  ${SIGIL_SUBSTANCE_INCLUDE_DIR}/sigilsubstance/graph/Output.h
  ${SIGIL_SUBSTANCE_INCLUDE_DIR}/sigilsubstance/graph/Parameter.h
  ${SIGIL_SUBSTANCE_INCLUDE_DIR}/sigilsubstance/package/Package.h
)

target_compile_features(SigilSubstance PUBLIC cxx_std_20)
target_include_directories(SigilSubstance
  PUBLIC ${SIGIL_SUBSTANCE_INCLUDE_DIR}
  # The package subject fills a Graph through graph/GraphImpl.h, an
  # internal header spelled by its subject directory.
  PRIVATE ${CMAKE_CURRENT_LIST_DIR})

# workaround: the Substance engine dylib's install name is @rpath-relative.
#
# The framework is a static archive over one engine dylib; the dylib's
# install name is @rpath-relative, so every executable linking this
# library carries the SDK's bin/release in its runtime search path.
target_link_libraries(SigilSubstance
  PUBLIC unofficial::skia::skia
  PRIVATE Substance::Framework
)
get_filename_component(_substance_bin "${SUBSTANCE_SDK_DIR}/bin/release" ABSOLUTE)
set_property(TARGET SigilSubstance PROPERTY INTERFACE_LINK_OPTIONS
  "LINKER:-rpath,${_substance_bin}")

# The test and the benchmark render the SDK's own sample archives, found
# through this definition; both skip, rather than fail, when the samples
# are not where the SDK was configured from.
add_executable(substance_test test/SubstanceTest.cpp)
target_link_libraries(substance_test PRIVATE SigilSubstance GTest::gtest_main)
target_compile_definitions(substance_test PRIVATE
  SIGIL_SUBSTANCE_SDK_DIR="${SUBSTANCE_SDK_DIR}")
add_test(NAME substance_test COMMAND substance_test)

# Google Benchmark (use a Release build). Not a test: it builds through the
# `benches` target and runs through scripts/bench_ledger.py.
add_executable(substance_bench bench/SubstanceBench.cpp)
target_link_libraries(substance_bench PRIVATE SigilSubstance benchmark::benchmark)
target_compile_definitions(substance_bench PRIVATE
  SIGIL_SUBSTANCE_SDK_DIR="${SUBSTANCE_SDK_DIR}")
add_dependencies(benches substance_bench)

# Registers this library with the `docs` target (see cmake/Docs.cmake).
# The README is the site's front page, so the generated pages open on the
# same document that is canon for the library.
sigil_add_docs(
  NAME SigilSubstance
  BRIEF "Adobe Substance 3D archives rendered to channel-tagged texture sets"
  INPUT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  MAINPAGE ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  STRIP
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
