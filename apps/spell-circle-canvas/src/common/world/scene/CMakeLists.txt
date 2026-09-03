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
# SigilMaterialKit is PRIVATE and is read for one thing: the slot name a
# surface carries its base-colour map in, so that extract can hand an
# execution the map the body is dressed with rather than a material tree
# to walk. Nothing here defines a shading model. The stats a scene
# publishes are the reconciler's counts under SigilMeasure's counters, so
# both are PUBLIC; the cache the phases prove against, the pose a body
# takes along a curve and the animated value a phase samples are read in
# the sources alone.
target_link_libraries(SigilWorldScene
  PUBLIC SigilWorldGraph SigilGeometryMeshRender SigilMotionClock
         SigilMotionSchedule SigilCoreReconcile SigilMeasure
  PRIVATE EnTT::EnTT SigilMaterialKit SigilCoreCache SigilGeometryMeshCurve
          SigilMotionValues)

add_executable(world_scene_test test/SceneTest.cpp)
target_include_directories(world_scene_test PRIVATE ${SIGIL_WORLD_TEST_DIR})
target_link_libraries(world_scene_test PRIVATE SigilWorldScene
  GTest::gtest_main)
add_test(NAME world_scene_test COMMAND world_scene_test)

# Google Benchmark (use a Release build). Not a test: it builds through
# the `benches` target and runs through scripts/bench_ledger.py.
add_executable(world_scene_bench bench/SceneBench.cpp)
target_link_libraries(world_scene_bench PRIVATE SigilWorldScene
  benchmark::benchmark)
add_dependencies(benches world_scene_bench)
