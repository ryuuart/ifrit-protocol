# The draws of a finished layout — draw() one blob per run and pass,
# drawBatched() one drawGlyphs call per (font, paint) bucket and pass —
# with the decoration bands beneath and above the glyphs. The members are
# declared on ParagraphLayout in the layout feature's header and defined
# here, so a program that draws links this archive.
add_library(SigilWeavePaint STATIC
  Paint.cpp
)
target_compile_features(SigilWeavePaint PUBLIC cxx_std_20)
target_include_directories(SigilWeavePaint PUBLIC
  $<BUILD_INTERFACE:${SIGIL_WEAVE_INCLUDE_DIR}>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(SigilWeavePaint
  PUBLIC
    SigilWeaveDecoration
)

# Every test here puts pixels on a surface. The stress case paints through
# the preset runtime shaders, which is the one reason to link them.
add_executable(weave_paint_test
  test/PaintTest.cpp
  test/DecorationPaintTest.cpp
  test/StressTest.cpp
)
target_include_directories(weave_paint_test PRIVATE ${SIGIL_WEAVE_TEST_SUPPORT_DIR})
target_link_libraries(weave_paint_test PRIVATE
  SigilWeavePaint
  SigilWeaveShaders
  SigilWeavePorts
  GTest::gtest_main
)
add_test(NAME weave_paint_test COMMAND weave_paint_test)

# draw and drawBatched per glyph on a raster surface. Built through
# `benches`, run through scripts/bench_ledger.py from a Release build.
add_executable(weave_paint_bench bench/PaintBench.cpp)
target_include_directories(weave_paint_bench PRIVATE ${SIGIL_WEAVE_BENCH_SUPPORT_DIR})
target_link_libraries(weave_paint_bench PRIVATE
  SigilWeavePaint
  SigilWeaveShaders
  SigilWeavePorts
  benchmark::benchmark
)
add_dependencies(benches weave_paint_bench)
