# Shape interpolation over resampled contours — the Illustrator blend.
add_library(SigilGeometryBlend STATIC
  Blend.cpp
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/blend/Blend.h
)
target_compile_features(SigilGeometryBlend PUBLIC cxx_std_20)
target_include_directories(SigilGeometryBlend PUBLIC ${SIGIL_GEOMETRY_INCLUDE_DIR})
target_link_libraries(SigilGeometryBlend PUBLIC SigilGeometryPath)

add_executable(geometry_blend_test test/BlendTest.cpp)
target_link_libraries(geometry_blend_test PRIVATE SigilGeometryBlend
  GTest::gtest_main)
add_test(NAME geometry_blend_test COMMAND geometry_blend_test)
