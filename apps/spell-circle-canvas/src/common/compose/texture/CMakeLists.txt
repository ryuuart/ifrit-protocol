sigil_library(SigilComposeTexture
  SOURCES Texture.cpp SurfaceScene.cpp
  HEADERS Texture.h SurfaceScene.h SurfaceRange.h
  # The door compares descriptions through the kernel's own comparator.
  INCLUDE_PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../core
  PUBLIC SigilComposeCore SigilMaterialTexture SigilSkiaGraphite
         SigilCoreHardware
  PRIVATE SigilMotionClock SigilMaterialSkia)
# The suite includes device-backed surface cases that skip without a GPU.
sigil_test(compose_test
  SOURCES test/ComposeTextureTest.cpp test/ComposeRepaintTest.cpp
    test/ComposeSurfaceSceneMapsTest.cpp
    test/ComposeSurfaceSceneComposerTest.cpp
    test/ComposeSurfaceSceneNormalTest.cpp
    test/ComposeSurfaceSceneReliefTest.cpp
    test/ComposeSurfaceSceneBrushingTest.cpp
    test/ComposeSurfaceSceneEmissionTest.cpp
    test/ComposeSurfaceSceneTransmissionTest.cpp
  LIBRARIES SigilComposeTexture SigilComposeTesting SigilComposeBrush
            SigilWeavePorts
            SigilGeometryDevice
  SUPPORT_DIRS ${SIGIL_GEOMETRY_DEVICE_TEST_SUPPORT_DIR}
  SUITES ComposeTexture LABELS gpu)

sigil_bench(compose_bench
  SOURCES bench/TextureBench.cpp
  LIBRARIES SigilComposeTexture SigilWeavePorts)
