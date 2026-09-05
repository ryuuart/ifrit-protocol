# The scene feature: the retained side of a 3D scene — the reconcile
# host, the entity store, the content-keyed resource store, the declared
# phases, the ordering and execution of a frame's passes, and the draw
# that presents what they wrote. EnTT is PRIVATE and appears in no
# public header: there is no registry accessor and no write path through
# one.
#
# SigilMaterialKit is PRIVATE and is read for one thing: the slot name a
# surface carries its base-colour map in, so that extract can hand an
# execution the map the body is dressed with rather than a material tree
# to walk. Nothing here defines a shading model. The stats a scene
# publishes are the reconciler's counts under SigilMeasure's counters, so
# both are PUBLIC; the cache the phases prove against, the pose a body
# takes along a curve and the animated value a phase samples are read in
# the sources alone.
find_package(EnTT CONFIG REQUIRED)
sigil_library(SigilWorldScene
  SOURCES Draw.cpp Execute.cpp Host.cpp Phases.cpp Resources.cpp Scene.cpp
          Resources.h SceneImpl.h
  HEADERS Scene.h Stats.h
  PUBLIC SigilWorldGraph SigilGeometryMeshRender SigilMotionClock
         SigilMotionSchedule SigilCoreReconcile SigilMeasure
  PRIVATE EnTT::EnTT SigilMaterialKit SigilCoreCache SigilGeometryMeshCurve
          SigilMotionValues)

sigil_test(world_scene_test
  SOURCES test/SceneTest.cpp
  LIBRARIES SigilWorldScene)

sigil_bench(world_scene_bench
  SOURCES bench/SceneBench.cpp
  LIBRARIES SigilWorldScene)
