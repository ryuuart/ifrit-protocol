# The draw feature: the immediate-mode sketch surface and the session
# that drives it — a clock, a pen and a surface that keeps what earlier
# frames drew, with one sketch body drawing into it every frame.
#
# ITS PUBLIC DEPENDENCIES ARE THE DRAW SKETCH API. What a draw sketch
# may #include is exactly this list, because the flags a sketch compiles
# with are lifted from a translation unit inside this target graph.
add_library(SigilSketchDraw STATIC
  Session.cpp
  ${SIGIL_SKETCH_INCLUDE_DIR}/sigilsketch/draw/Draw.h
)
target_compile_features(SigilSketchDraw PUBLIC cxx_std_20)
target_link_libraries(SigilSketchDraw PUBLIC SigilSketchCore SigilDraw)
# The session's own clock and lap timer are read in its source alone, so
# they are no part of the sketch API; the image asset is what loadImage
# reads a frame off.
target_link_libraries(SigilSketchDraw PRIVATE SigilMotionClock SigilMeasure
  SigilImage)
set_target_properties(SigilSketchDraw PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_executable(sketch_draw_test test/DrawSessionTest.cpp)
target_include_directories(sketch_draw_test PRIVATE
  ${SIGIL_SKETCH_TEST_SUPPORT_DIR})
target_link_libraries(sketch_draw_test PRIVATE SigilSketchDraw SigilWeavePorts
  GTest::gtest_main)
add_test(NAME sketch_draw_test COMMAND sketch_draw_test)

add_executable(sketch_draw_bench bench/DrawSessionBench.cpp)
target_link_libraries(sketch_draw_bench PRIVATE SigilSketchDraw SigilWeavePorts
  benchmark::benchmark)
add_dependencies(benches sketch_draw_bench)
