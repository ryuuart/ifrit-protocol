# The core feature: what a sketch is, what it declares about its canvas,
# what it reaches for, and the seam that says which runtime draws it.
#
# It links the resource hub (a sketch reaches for files), the comparable
# erasure the kind seam is built on, and the instruments every runtime
# times itself with — and nothing that draws: a consumer that only wants
# to know what sketches exist links this alone.
add_library(SigilSketchCore STATIC
  Assets.cpp
  Registry.cpp
  ${SIGIL_SKETCH_INCLUDE_DIR}/sigilsketch/core/Assets.h
  ${SIGIL_SKETCH_INCLUDE_DIR}/sigilsketch/core/CanvasSpec.h
  ${SIGIL_SKETCH_INCLUDE_DIR}/sigilsketch/core/Kind.h
  ${SIGIL_SKETCH_INCLUDE_DIR}/sigilsketch/core/Registry.h
  ${SIGIL_SKETCH_INCLUDE_DIR}/sigilsketch/core/Session.h
)
target_compile_features(SigilSketchCore PUBLIC cxx_std_20)
target_include_directories(SigilSketchCore PUBLIC ${SIGIL_SKETCH_INCLUDE_DIR})
target_link_libraries(SigilSketchCore PUBLIC SigilLoader SigilCoreComparable)
set_target_properties(SigilSketchCore PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_executable(sketch_core_test test/RegistryTest.cpp)
target_link_libraries(sketch_core_test PRIVATE SigilSketchCore
  GTest::gtest_main)
add_test(NAME sketch_core_test COMMAND sketch_core_test)

# Google Benchmark (use a Release build). Not a test: it builds through
# the `benches` target and runs through scripts/bench_ledger.py.
add_executable(sketch_core_bench bench/RegistryBench.cpp)
target_link_libraries(sketch_core_bench PRIVATE SigilSketchCore
  benchmark::benchmark)
add_dependencies(benches sketch_core_bench)
