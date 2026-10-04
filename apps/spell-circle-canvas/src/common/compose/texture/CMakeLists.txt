sigil_library(SigilComposeTexture
  SOURCES Texture.cpp SurfaceScene.cpp
  HEADERS Texture.h SurfaceScene.h
  PUBLIC SigilComposeCore SigilMaterialTexture SigilSkiaGraphite
         SigilCoreHardware
  PRIVATE SigilMotionClock)
# The suite includes device-backed surface cases that skip without a GPU.
sigil_test(compose_test
  SOURCES test/ComposeTextureTest.cpp test/ComposeSurfaceSceneTest.cpp
  LIBRARIES SigilComposeTexture SigilComposeTesting SigilComposeBrush
            SigilWeavePorts
            SigilGeometryDevice
  SUPPORT_DIRS ${SIGIL_GEOMETRY_DEVICE_TEST_SUPPORT_DIR}
  SUITES ComposeTexture LABELS gpu)

sigil_bench(compose_bench
  SOURCES bench/TextureBench.cpp
  LIBRARIES SigilComposeTexture SigilWeavePorts)
