# The camera and the transforms that place geometry in front of it. Its
# view and projection are built with Skia's own matrix factories, so a
# point projected here lands where a canvas concat would put it; the
# billboard transform shares the orientation basis every stamp uses,
# which is why the mesh currency is a dependency.
sigil_library(SigilGeometryMeshCamera
  SOURCES Camera.cpp
  HEADERS Camera.h
  PUBLIC SigilGeometryMesh)

sigil_test(geometry_mesh_camera_test
  SOURCES test/CameraTest.cpp
  LIBRARIES SigilGeometryMeshCamera)

sigil_bench(geometry_mesh_camera_bench
  SOURCES bench/CameraBench.cpp
  LIBRARIES SigilGeometryMeshCamera)
