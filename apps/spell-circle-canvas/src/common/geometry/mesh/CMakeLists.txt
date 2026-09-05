# The mesh currency and its procedural generators — the 3D tier's root.
# The public headers expose only Skia and glm types.
find_package(boost_container CONFIG REQUIRED)
sigil_library(SigilGeometryMesh
  SOURCES Generators.cpp Mesh.cpp
  HEADERS Mesh.h Vec.h
  PUBLIC SigilGeometryPath Boost::container)

# ONE BINARY over the currency and the camera that places it. A camera is
# not a boundary anyone reads: it names only mesh types and matrices, and
# nothing links it without the currency underneath.
sigil_test(geometry_test
  SOURCES test/MeshTest.cpp test/CameraTest.cpp
  LIBRARIES SigilGeometryMeshCamera)

sigil_bench(geometry_bench
  SOURCES bench/MeshBench.cpp
  LIBRARIES SigilGeometryMesh)

# The tier's features, in dependency order.
add_subdirectory(camera)
add_subdirectory(render)
add_subdirectory(curve)
add_subdirectory(pop)
add_subdirectory(codec)
