# The element feature: one node of a 3D scene as a comparable
# copy-on-write value — the verbs, the transform lanes, the geometry
# slot, tags, selectors and the generator seam. Nothing here cooks, draws
# or retains: it links the vocabularies it is written in and the
# reconciler's kernels, and nothing that owns a device.
add_library(SigilWorldElement STATIC
  Element.cpp
  Equal.cpp
  Geometry.cpp
  Lanes.cpp
  Selector.cpp
  Transform.cpp
  ${SIGIL_WORLD_INCLUDE_DIR}/sigilworld/element/Element.h
  ${SIGIL_WORLD_INCLUDE_DIR}/sigilworld/element/Environment.h
  ${SIGIL_WORLD_INCLUDE_DIR}/sigilworld/element/Geometry.h
  ${SIGIL_WORLD_INCLUDE_DIR}/sigilworld/element/Lanes.h
  ${SIGIL_WORLD_INCLUDE_DIR}/sigilworld/element/Node.h
  ${SIGIL_WORLD_INCLUDE_DIR}/sigilworld/element/Selector.h
  ${SIGIL_WORLD_INCLUDE_DIR}/sigilworld/element/Transform.h
)
target_compile_features(SigilWorldElement PUBLIC cxx_std_20)
target_include_directories(SigilWorldElement PUBLIC ${SIGIL_WORLD_INCLUDE_DIR})
target_link_libraries(SigilWorldElement PUBLIC
  SigilCoreComparable
  SigilCoreCompute
  SigilCoreReconcile
  SigilCoreCache
  SigilGeometryMesh
  SigilGeometryMeshCamera
  SigilGeometryMeshCurve
  SigilGeometryMeshPop
  SigilMaterialCore
  SigilMaterialTexture
  SigilMotionValues SigilMotionSchedule
  SigilWorldLight)

# Links only the library it exercises, so a link edge that pulled the
# retained side or a device in would show up as a build failure here.
add_executable(world_element_test
  test/ElementTest.cpp
  test/SelectorTest.cpp
)
target_link_libraries(world_element_test PRIVATE SigilWorldElement
  GTest::gtest_main)
add_test(NAME world_element_test COMMAND world_element_test)

# Google Benchmark (use a Release build). Not a test: it builds through
# the `benches` target and runs through scripts/bench_ledger.py.
add_executable(world_element_bench bench/ElementBench.cpp)
target_link_libraries(world_element_bench PRIVATE SigilWorldElement
  benchmark::benchmark)
add_dependencies(benches world_element_bench)
