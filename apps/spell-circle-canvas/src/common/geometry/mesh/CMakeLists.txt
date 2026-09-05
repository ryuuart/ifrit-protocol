# The mesh currency and its procedural generators — the 3D tier's root.
# The public headers expose only Skia and glm types.
sigil_library(SigilGeometryMesh
  SOURCES Generators.cpp Mesh.cpp
  HEADERS Mesh.h Vec.h
  PUBLIC SigilGeometryPath Boost::container)

sigil_test(geometry_mesh_test
  SOURCES test/MeshTest.cpp
  LIBRARIES SigilGeometryMesh)

sigil_bench(geometry_mesh_bench
  SOURCES bench/MeshBench.cpp
  LIBRARIES SigilGeometryMesh)

# The tier's features, in dependency order.
add_subdirectory(camera)
add_subdirectory(render)
add_subdirectory(curve)
add_subdirectory(pop)
add_subdirectory(codec)
