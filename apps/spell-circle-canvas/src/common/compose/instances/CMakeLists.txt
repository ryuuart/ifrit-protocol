# SigilComposeInstances — thousands of repeated sprites as one leaf.
#
# The atlas baked once from element trees, the user-owned struct-of-arrays
# pool, the single stamping draw with its arithmetic cull, the pick that
# inverts it, and the kit's placers (kit/Placers.h), which are shipped here
# because every signature is spelled in the pool. Links Core publicly: an
# atlas cell is an Element and the leaf is one. A TRANSITIONAL target — the
# leaf is the one component that keeps its data on the caller's side of the
# seam, and it is linked only by what stamps with it.
add_library(SigilComposeInstances STATIC
  Instances.cpp
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/instances/Instances.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/kit/Placers.h
)
target_compile_features(SigilComposeInstances PUBLIC cxx_std_20)
target_link_libraries(SigilComposeInstances PUBLIC SigilComposeCore)

# The pool's lanes, the atlas and its variants, both stamp modes, the
# per-sprite blend, the pick and the placers. Links this tier alone over the
# kernel; the support header reaches the shared harness through the
# library's test/support directory.
add_executable(compose_instances_test test/InstancesTest.cpp)
target_include_directories(compose_instances_test PRIVATE
  ${SIGIL_COMPOSE_TEST_SUPPORT_DIR}/support)
target_link_libraries(compose_instances_test PRIVATE
  SigilComposeInstances SigilComposeTesting SigilWeavePorts GTest::gtest_main)
add_test(NAME compose_instances_test COMMAND compose_instances_test)
