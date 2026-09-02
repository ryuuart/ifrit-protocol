# Shape interpolation over resampled contours — the Illustrator blend.
add_library(SigilGeometryPathBlend STATIC
  Blend.cpp
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/path/blend/Blend.h
)
target_compile_features(SigilGeometryPathBlend PUBLIC cxx_std_20)
target_include_directories(SigilGeometryPathBlend PUBLIC ${SIGIL_GEOMETRY_INCLUDE_DIR})
# The colour interpolation runs in OKLab, whose conversions are
# SigilMaterial's colour leaf; the link is private because no header here
# spells its types.
target_link_libraries(SigilGeometryPathBlend PUBLIC SigilGeometryPath
  PRIVATE SigilMaterialColor)

add_executable(geometry_path_blend_test test/BlendTest.cpp)
target_link_libraries(geometry_path_blend_test PRIVATE SigilGeometryPathBlend
  GTest::gtest_main)
add_test(NAME geometry_path_blend_test COMMAND geometry_path_blend_test)

add_executable(geometry_path_blend_bench bench/BlendBench.cpp)
target_link_libraries(geometry_path_blend_bench PRIVATE SigilGeometryPathBlend
  benchmark::benchmark)
add_dependencies(benches geometry_path_blend_bench)
