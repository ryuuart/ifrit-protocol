# Splines, their parallel-transport frames and the geometry swept along
# them; projection to a 2D path needs the camera, so the painter is a
# dependency.
add_library(SigilGeometryCurves STATIC
  Curves.cpp
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/curves/Curves.h
)
target_compile_features(SigilGeometryCurves PUBLIC cxx_std_20)
target_include_directories(SigilGeometryCurves PUBLIC ${SIGIL_GEOMETRY_INCLUDE_DIR})
target_link_libraries(SigilGeometryCurves PUBLIC SigilGeometryMesh
  SigilGeometrySpace)

add_executable(geometry_curves_test test/CurvesTest.cpp)
target_link_libraries(geometry_curves_test PRIVATE SigilGeometryCurves
  GTest::gtest_main)
add_test(NAME geometry_curves_test COMMAND geometry_curves_test)
