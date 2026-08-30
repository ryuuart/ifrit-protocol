# Splines, their parallel-transport frames and the geometry swept along
# them; projection to a 2D path needs the camera.
add_library(SigilGeometryMeshCurve STATIC
  Curve.cpp
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/mesh/curve/Curve.h
)
target_compile_features(SigilGeometryMeshCurve PUBLIC cxx_std_20)
target_include_directories(SigilGeometryMeshCurve PUBLIC ${SIGIL_GEOMETRY_INCLUDE_DIR})
target_link_libraries(SigilGeometryMeshCurve PUBLIC SigilGeometryMesh
  SigilGeometryMeshCamera)

add_executable(geometry_mesh_curve_test test/CurveTest.cpp)
target_link_libraries(geometry_mesh_curve_test PRIVATE SigilGeometryMeshCurve
  GTest::gtest_main)
add_test(NAME geometry_mesh_curve_test COMMAND geometry_mesh_curve_test)

add_executable(geometry_mesh_curve_bench bench/CurveBench.cpp)
target_link_libraries(geometry_mesh_curve_bench PRIVATE SigilGeometryMeshCurve
  benchmark::benchmark)
add_dependencies(benches geometry_mesh_curve_bench)
