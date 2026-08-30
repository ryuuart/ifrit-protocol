# Point operators and the clouds they act on: cloud storage, generators,
# modifiers, instancing and the billboard rasterizer, plus the pop chain
# language's field table, its CPU reference executor and the
# mesh-forming sinks.
add_library(SigilGeometryMeshPop STATIC
  Billboards.cpp
  Cloud.cpp
  Cook.cpp
  Fields.cpp
  Generators.cpp
  Modifiers.cpp
  Sinks.cpp
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/mesh/pop/Points.h
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/mesh/pop/Pop.h
)
target_compile_features(SigilGeometryMeshPop PUBLIC cxx_std_20)
target_include_directories(SigilGeometryMeshPop PUBLIC ${SIGIL_GEOMETRY_INCLUDE_DIR})
target_link_libraries(SigilGeometryMeshPop PUBLIC SigilGeometryMeshCurve)

# The pop tests seed chains from an imported model and save a cooked
# result, so this binary reaches into the codec as well.
add_executable(geometry_mesh_pop_test
  test/PointsTest.cpp
  test/PopTest.cpp
)
target_include_directories(geometry_mesh_pop_test PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../test)
target_link_libraries(geometry_mesh_pop_test PRIVATE SigilGeometryMeshPop
  SigilGeometryMeshCodec GTest::gtest_main)
add_test(NAME geometry_mesh_pop_test COMMAND geometry_mesh_pop_test)

add_executable(geometry_mesh_pop_bench bench/PopBench.cpp)
target_link_libraries(geometry_mesh_pop_bench PRIVATE SigilGeometryMeshPop
  benchmark::benchmark)
add_dependencies(benches geometry_mesh_pop_bench)
