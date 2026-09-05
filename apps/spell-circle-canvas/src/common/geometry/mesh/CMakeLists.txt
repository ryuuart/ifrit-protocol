# The mesh currency and its procedural generators — the 3D tier's root.
# earcut.hpp is private plumbing (cap triangulation); the public headers
# expose only Skia and glm types.
#
# workaround: the earcut-hpp port installs headers and exports no config.
find_path(EARCUT_INCLUDE_DIR mapbox/earcut.hpp REQUIRED)
sigil_library(SigilGeometryMesh
  SOURCES Generators.cpp Mesh.cpp
  HEADERS Mesh.h Vec.h
  INCLUDE_PRIVATE ${EARCUT_INCLUDE_DIR}
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
