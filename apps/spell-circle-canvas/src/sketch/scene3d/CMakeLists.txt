# The scene3d runtime: the 3D sketch surface and the session that drives it —
# a ticker, a retained Scene, and one scene3d body describing a frame as a
# function of the scene time. The test walks through the door, so it
# links what a scene3d sketch doing the same would: the compose tree it paints
# and the leaf that keeps the scene.
#
# A scene3d sketch is drawn on a device and nowhere else, so the cases that open
# one link the device executor and the one device a test process brings up,
# and SKIP where the machine has no Vulkan runtime — which the `gpu` label
# on their suites makes visible from outside.
sigil_test(sketch_test
  SOURCES test/Scene3DBodyTest.cpp test/Scene3DPageLightTest.cpp
          test/Scene3DPageLobeTest.cpp test/Scene3DPageTest.cpp
          test/Scene3DSessionTest.cpp test/Scene3DWindowTest.cpp
  SUPPORT_DIRS ${SIGIL_GEOMETRY_DEVICE_TEST_SUPPORT_DIR}
  LIBRARIES SigilSketch SigilWeavePorts SigilWeaveFonts SigilGeometryKit
            SigilCompose SigilComposeTexture SigilWorldDiligent
  SUITES Scene3DSession Scene3DDoors Scene3DPages Scene3DWindow Scene3DBodies Scene3DKinds TheScene3DSession/*
  LABELS gpu)

sigil_bench(sketch_bench
  SOURCES bench/Scene3DSessionBench.cpp
  SUPPORT_DIRS ${CMAKE_CURRENT_LIST_DIR}/../test
               ${SIGIL_GEOMETRY_DEVICE_TEST_SUPPORT_DIR}
  LIBRARIES SigilSketch SigilWeavePorts SigilWeaveFonts SigilGeometryKit
            SigilWorldDiligent)
