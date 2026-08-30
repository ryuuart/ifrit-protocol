# Drawing meshes and panels, and the Runtime seam a draw executes
# through. One executor ships here — everything on the CPU, emitting
# onto a Skia canvas — and a feature that owns a GPU device supplies its
# own as a value, so this target never learns about a device.
add_library(SigilGeometryMeshRender STATIC
  Painter.cpp
  Runtime.cpp
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/mesh/render/Painter.h
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/mesh/render/Runtime.h
)
target_compile_features(SigilGeometryMeshRender PUBLIC cxx_std_20)
target_include_directories(SigilGeometryMeshRender PUBLIC ${SIGIL_GEOMETRY_INCLUDE_DIR})
target_link_libraries(SigilGeometryMeshRender PUBLIC SigilGeometryMesh
  SigilGeometryMeshCamera)

add_executable(geometry_mesh_render_test
  test/PainterTest.cpp
  test/RuntimeTest.cpp
)
target_include_directories(geometry_mesh_render_test PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../test)
target_link_libraries(geometry_mesh_render_test PRIVATE SigilGeometryMeshRender
  GTest::gtest_main)
add_test(NAME geometry_mesh_render_test COMMAND geometry_mesh_render_test)

add_executable(geometry_mesh_render_bench bench/RenderBench.cpp)
target_link_libraries(geometry_mesh_render_bench PRIVATE SigilGeometryMeshRender
  benchmark::benchmark)
add_dependencies(benches geometry_mesh_render_bench)
