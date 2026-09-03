# The kit feature: presets that compose elements — a three-point rig, a
# turntable, and the set both make over a ground plane. Everything here
# returns an ordinary Element built out of the verbs a tree is already
# written in, so a caller reads, replaces or ignores any part of one.
#
# It links the description and SigilMaterial's kit, because a ground
# plane given no surface has to be made of something; it defines no
# shading model, no catalog and no material of its own. The turntable's
# phase along its rail is a motion value, read in the source alone.
add_library(SigilWorldKit STATIC
  Kit.cpp
  ${SIGIL_WORLD_INCLUDE_DIR}/sigilworld/kit/Kit.h
)
target_compile_features(SigilWorldKit PUBLIC cxx_std_20)
target_include_directories(SigilWorldKit PUBLIC ${SIGIL_WORLD_INCLUDE_DIR})
target_link_libraries(SigilWorldKit
  PUBLIC SigilWorldElement SigilMaterialKit SigilGeometryKit
  PRIVATE SigilMotionValues)

# The negative control for the include boundary above: a target that must
# FAIL to build, and a test that builds it and demands the one message that
# says the boundary held. Nothing puts a world source directory on the kit's
# include path, so the retained side's own header is unreachable from kit
# code — which is what makes "the kit sees public headers only" a property
# of the build rather than a convention.
include(BoundaryProbe)
sigil_boundary_probe(
  TARGET world_kit_boundary_probe
  SOURCES BoundaryProbe.cpp
  LIBRARIES SigilWorldKit
  EXPECT "'SceneImpl.h' file not found")

# Links only the library it exercises, so a link edge that pulled the
# retained side or a device in would show up as a build failure here.
add_executable(world_kit_test test/KitTest.cpp)
target_link_libraries(world_kit_test PRIVATE SigilWorldKit GTest::gtest_main)
add_test(NAME world_kit_test COMMAND world_kit_test)

# Google Benchmark (use a Release build). Not a test: it builds through
# the `benches` target and runs through scripts/bench_ledger.py.
add_executable(world_kit_bench bench/KitBench.cpp)
target_link_libraries(world_kit_bench PRIVATE SigilWorldKit
  benchmark::benchmark)
add_dependencies(benches world_kit_bench)
