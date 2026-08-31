# The camera and the transforms that place geometry in front of it. Its
# view and projection are built with Skia's own matrix factories, so a
# point projected here lands where a canvas concat would put it; the
# billboard transform shares the orientation basis every stamp uses,
# which is why the mesh currency is a dependency.
add_library(SigilGeometryMeshCamera STATIC
  Camera.cpp
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/mesh/camera/Camera.h
)
target_compile_features(SigilGeometryMeshCamera PUBLIC cxx_std_20)
target_include_directories(SigilGeometryMeshCamera PUBLIC ${SIGIL_GEOMETRY_INCLUDE_DIR})
target_link_libraries(SigilGeometryMeshCamera PUBLIC SigilGeometryMesh)

add_executable(geometry_mesh_camera_test test/CameraTest.cpp)
target_link_libraries(geometry_mesh_camera_test PRIVATE SigilGeometryMeshCamera
  GTest::gtest_main)
add_test(NAME geometry_mesh_camera_test COMMAND geometry_mesh_camera_test)

add_executable(geometry_mesh_camera_bench bench/CameraBench.cpp)
target_link_libraries(geometry_mesh_camera_bench PRIVATE SigilGeometryMeshCamera
  benchmark::benchmark)
add_dependencies(benches geometry_mesh_camera_bench)
