# The layout stage: the flow geometries text is broken into, the greedy and
# Knuth-Plass breakers over one flattened interval sequence, placement as
# shared or baked blobs, the ellipsis, and the per-line and per-column
# metrics derived from the placed runs. SigilGeometryPath is public — a
# contour interval carries a geometry::path::Contour.
add_library(SigilWeaveLayout STATIC
  Beside.cpp
  Flow.cpp
  LineBreak.cpp
  KnuthPlass.cpp
  LayoutMetrics.cpp
  ParagraphLayoutInternal.h
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/layout/Beside.h
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/layout/Flow.h
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/layout/LayoutOptions.h
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/layout/PositionedRun.h
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/layout/ParagraphLayout.h
)
target_compile_features(SigilWeaveLayout PUBLIC cxx_std_20)
target_include_directories(SigilWeaveLayout PUBLIC
  $<BUILD_INTERFACE:${SIGIL_WEAVE_INCLUDE_DIR}>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(SigilWeaveLayout
  PUBLIC
    SigilWeaveParagraph
    SigilGeometryPath
  PRIVATE
    harfbuzz::harfbuzz
    ICU::uc
    absl::flat_hash_map
)

# Everything that places runs and reads where they landed: both breakers,
# the flows, overflow and clamp, vertical writing, placeholders, relayout
# locality, the typographic options the breakers read, text on a path, and
# the large-paragraph stress cases. The stress case that restyles through
# the query layer links it.
add_executable(weave_layout_test
  test/LayoutTest.cpp
  test/KnuthPlassTest.cpp
  test/OverflowTest.cpp
  test/VerticalTest.cpp
  test/FlowTest.cpp
  test/PathTextTest.cpp
  test/PlaceholderTest.cpp
  test/IncrementalTest.cpp
  test/TypographyTest.cpp
  test/ParagraphStyleTest.cpp
  test/StressTest.cpp
)
target_include_directories(weave_layout_test PRIVATE ${SIGIL_WEAVE_TEST_SUPPORT_DIR})
target_link_libraries(weave_layout_test PRIVATE
  SigilWeaveLayout
  SigilWeaveKit
  SigilWeaveQuery
  SigilWeavePorts
  absl::flat_hash_set
  GTest::gtest_main
)
add_test(NAME weave_layout_test COMMAND weave_layout_test)

# layoutParagraph per word over a warm shape cache. Built through
# `benches`, run through scripts/bench_ledger.py from a Release build.
add_executable(weave_layout_bench bench/LayoutBench.cpp)
target_include_directories(weave_layout_bench PRIVATE ${SIGIL_WEAVE_BENCH_SUPPORT_DIR})
target_link_libraries(weave_layout_bench PRIVATE
  SigilWeaveLayout
  SigilWeavePorts
  benchmark::benchmark
)
add_dependencies(benches weave_layout_bench)
