sigil_library(SigilComposeTexture
  SOURCES Texture.cpp
  HEADERS Texture.h
  PUBLIC SigilComposeCore SigilMaterialTexture SigilSkiaGraphite
         SigilCoreHardware
  PRIVATE SigilMotionClock)
# The suite includes device-backed surface cases that skip without a GPU.
sigil_test(compose_test
  SOURCES test/ComposeTextureTest.cpp
  LIBRARIES SigilComposeTexture SigilComposeTesting SigilWeavePorts
            SigilGeometryDevice
  SUPPORT_DIRS ${SIGIL_GEOMETRY_DEVICE_TEST_SUPPORT_DIR}
  SUITES ComposeTexture LABELS gpu)

sigil_bench(compose_bench
  SOURCES bench/TextureBench.cpp
  LIBRARIES SigilComposeTexture SigilWeavePorts)
