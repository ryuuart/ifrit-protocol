# The canvas feature: the 2D sketch surface and the session that drives
# it — a clock, a ticker and a Composer wired together, with one sketch
# body describing into them.
#
# ITS PUBLIC DEPENDENCIES ARE THE 2D SKETCH API. What a 2D sketch may
# #include is exactly this list, because the flags a sketch compiles with
# are lifted from a translation unit inside this target graph.
add_library(SigilSketchCanvas STATIC
  Session.cpp
  ${SIGIL_SKETCH_INCLUDE_DIR}/sigilsketch/canvas/Sketch.h
)
target_compile_features(SigilSketchCanvas PUBLIC cxx_std_20)
target_link_libraries(SigilSketchCanvas PUBLIC
  SigilSketchCore
  SigilComposeCore SigilComposeShape SigilComposeBrush
  SigilComposeTypography SigilComposeInstances SigilComposeKit
  SigilComposeTexture
  # Header-only, so it costs nothing at link time: a sketch that reports
  # its own checks reads the testing surface.
  SigilComposeTesting
  SigilGeometry SigilMaterial SigilWeavePorts EnTT::EnTT)
set_target_properties(SigilSketchCanvas PROPERTIES
  POSITION_INDEPENDENT_CODE ON)

add_executable(sketch_canvas_test test/CanvasSessionTest.cpp)
target_link_libraries(sketch_canvas_test PRIVATE SigilSketchCanvas
  GTest::gtest_main)
add_test(NAME sketch_canvas_test COMMAND sketch_canvas_test)

add_executable(sketch_canvas_bench bench/CanvasSessionBench.cpp)
target_link_libraries(sketch_canvas_bench PRIVATE SigilSketchCanvas
  benchmark::benchmark)
add_dependencies(benches sketch_canvas_bench)
