# The mesh currency and its procedural generators — the 3D tier's root.
# earcut.hpp is private plumbing (cap triangulation); the public headers
# expose only Skia and glm types.
add_library(SigilGeometryMesh STATIC
  Generators.cpp
  Mesh.cpp
  Spirv.cpp
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/mesh/Mesh.h
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/mesh/Spirv.h
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/mesh/Vec.h
)
target_compile_features(SigilGeometryMesh PUBLIC cxx_std_20)
target_include_directories(SigilGeometryMesh PUBLIC ${SIGIL_GEOMETRY_INCLUDE_DIR})
# workaround: the earcut-hpp port installs headers and exports no config.
find_path(EARCUT_INCLUDE_DIR mapbox/earcut.hpp REQUIRED)
target_include_directories(SigilGeometryMesh PRIVATE ${EARCUT_INCLUDE_DIR})
target_link_libraries(SigilGeometryMesh PUBLIC SigilGeometryPath)

add_executable(geometry_mesh_test test/MeshTest.cpp)
target_include_directories(geometry_mesh_test PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../test)
target_link_libraries(geometry_mesh_test PRIVATE SigilGeometryMesh
  GTest::gtest_main)
add_test(NAME geometry_mesh_test COMMAND geometry_mesh_test)

add_executable(geometry_mesh_bench bench/MeshBench.cpp)
target_link_libraries(geometry_mesh_bench PRIVATE SigilGeometryMesh
  benchmark::benchmark)
add_dependencies(benches geometry_mesh_bench)

# The tier's features, in dependency order.
add_subdirectory(camera)
add_subdirectory(render)
add_subdirectory(curve)
add_subdirectory(pop)
add_subdirectory(codec)
