# SigilDrawKit — stock procedural tools over the pen's public verbs.
#
# A brush is style data, a stroke is pressure-bearing geometry and a direction
# field is a callable value. The kit links the pen and reaches no runtime or
# retained drawing system; a caller with its own tools links SigilDraw alone.
add_library(SigilDrawKit STATIC
  Box.cpp
  Brush.cpp
  Dab.cpp
  Engine.cpp
  Fields.cpp
  Geometry.cpp
  Path.cpp
  Shape.cpp
  ${SIGIL_DRAW_INCLUDE_DIR}/sigildraw/kit/Box.h
  ${SIGIL_DRAW_INCLUDE_DIR}/sigildraw/kit/Brush.h
  ${SIGIL_DRAW_INCLUDE_DIR}/sigildraw/kit/Brushwork.h
  ${SIGIL_DRAW_INCLUDE_DIR}/sigildraw/kit/Dab.h
  ${SIGIL_DRAW_INCLUDE_DIR}/sigildraw/kit/Engine.h
  ${SIGIL_DRAW_INCLUDE_DIR}/sigildraw/kit/Fields.h
  ${SIGIL_DRAW_INCLUDE_DIR}/sigildraw/kit/Geometry.h
  ${SIGIL_DRAW_INCLUDE_DIR}/sigildraw/kit/Path.h
  ${SIGIL_DRAW_INCLUDE_DIR}/sigildraw/kit/Shape.h
)
target_compile_features(SigilDrawKit PUBLIC cxx_std_20)
target_include_directories(SigilDrawKit PUBLIC ${SIGIL_DRAW_INCLUDE_DIR})
target_link_libraries(SigilDrawKit PUBLIC SigilDraw PRIVATE SigilSkiaDraw)
set_target_properties(SigilDrawKit PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_executable(draw_kit_test test/BrushTest.cpp)
target_link_libraries(draw_kit_test PRIVATE SigilDrawKit GTest::gtest_main)
add_test(NAME draw_kit_test COMMAND draw_kit_test)

add_executable(draw_kit_bench bench/BrushBench.cpp)
target_link_libraries(draw_kit_bench PRIVATE SigilDrawKit benchmark::benchmark)
add_dependencies(benches draw_kit_bench)
