# The frame feature: a frame as three declared things — the scene, the
# passes and the readbacks — the resources those passes write, and the
# Runtime/Executor seam a pass is performed through. The built-in
# executor is the CPU one; a feature that owns a device supplies its own
# as a value, so nothing here links a device and every dependency points
# down.
add_library(SigilWorldFrame STATIC
  Cpu.cpp
  CpuGeometry.cpp
  CpuPost.cpp
  Frame.cpp
  Pass.cpp
  Targets.cpp
  View.cpp
  Cpu.h
  ${SIGIL_WORLD_INCLUDE_DIR}/sigilworld/frame/Frame.h
  ${SIGIL_WORLD_INCLUDE_DIR}/sigilworld/frame/Pass.h
  ${SIGIL_WORLD_INCLUDE_DIR}/sigilworld/frame/Runtime.h
  ${SIGIL_WORLD_INCLUDE_DIR}/sigilworld/frame/Targets.h
  ${SIGIL_WORLD_INCLUDE_DIR}/sigilworld/frame/View.h
)
target_compile_features(SigilWorldFrame PUBLIC cxx_std_20)
target_include_directories(SigilWorldFrame PUBLIC ${SIGIL_WORLD_INCLUDE_DIR})
# SigilMaterialTexture because a body's view carries the map its surface
# is dressed with, which every executor samples; SigilCoreReconcile
# because a pass body and a generator are its erased value. The CPU
# executor cooks a body's chain itself, so the point operators are named
# privately.
target_link_libraries(SigilWorldFrame
  PUBLIC SigilWorldElement SigilMaterialTexture SigilGeometryMeshRender
         SigilCoreReconcile Boost::container
  PRIVATE SigilGeometryMeshPop)

# Links only the library it exercises, so a link edge that pulled the
# retained side or a device in would show up as a build failure here.
add_executable(world_frame_test test/FrameTest.cpp)
target_include_directories(world_frame_test PRIVATE ${SIGIL_WORLD_TEST_DIR})
target_link_libraries(world_frame_test PRIVATE SigilWorldFrame
  GTest::gtest_main)
add_test(NAME world_frame_test COMMAND world_frame_test)

# Google Benchmark (use a Release build). Not a test: it builds through
# the `benches` target and runs through scripts/bench_ledger.py.
add_executable(world_frame_bench bench/FrameBench.cpp)
target_link_libraries(world_frame_bench PRIVATE SigilWorldFrame
  benchmark::benchmark)
add_dependencies(benches world_frame_bench)
