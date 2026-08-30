# The scene feature: the retained side of a 3D scene — the reconcile
# host, the entity store, the content-keyed resource store, the declared
# phases, the ordering and execution of a frame's passes, and the draw
# that presents what they wrote. EnTT is PRIVATE and appears in no
# public header: there is no registry accessor and no write path through
# one.
find_package(EnTT CONFIG REQUIRED)
add_library(SigilWorldScene STATIC
  Draw.cpp
  Execute.cpp
  Host.cpp
  Phases.cpp
  Resources.cpp
  Scene.cpp
  Resources.h
  SceneImpl.h
  ${SIGIL_WORLD_INCLUDE_DIR}/sigilworld/scene/Scene.h
  ${SIGIL_WORLD_INCLUDE_DIR}/sigilworld/scene/Stats.h
)
target_compile_features(SigilWorldScene PUBLIC cxx_std_20)
target_include_directories(SigilWorldScene PUBLIC ${SIGIL_WORLD_INCLUDE_DIR})
target_link_libraries(SigilWorldScene
  PUBLIC SigilWorldGraph SigilGeometryMeshRender SigilMotionClock
  PRIVATE EnTT::EnTT)

add_executable(world_scene_test test/SceneTest.cpp)
target_link_libraries(world_scene_test PRIVATE SigilWorldScene
  GTest::gtest_main)
add_test(NAME world_scene_test COMMAND world_scene_test)

# Google Benchmark (use a Release build). Not a test: it builds through
# the `benches` target and runs through scripts/bench_ledger.py.
add_executable(world_scene_bench bench/SceneBench.cpp)
target_link_libraries(world_scene_bench PRIVATE SigilWorldScene
  benchmark::benchmark)
add_dependencies(benches world_scene_bench)
