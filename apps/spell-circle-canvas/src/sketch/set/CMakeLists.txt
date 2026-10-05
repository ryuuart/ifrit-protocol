# The set runtime: the 3D sketch surface and the session that drives it —
# a ticker, a retained Scene, and one set body describing a frame as a
# function of the scene time. The test walks through the door, so it
# links what a set doing the same would: the compose tree it paints and
# the leaf that keeps the scene.
#
# A set is drawn on a device and nowhere else, so the cases that open one
# link the device executor and the one device a test process brings up,
# and SKIP where the machine has no Vulkan runtime — which the `gpu` label
# on their suites makes visible from outside.
sigil_test(sketch_test
  SOURCES test/SetSessionTest.cpp
  SUPPORT_DIRS ${SIGIL_GEOMETRY_DEVICE_TEST_SUPPORT_DIR}
  LIBRARIES SigilSketch SigilWeavePorts SigilWeaveFonts SigilGeometryKit
            SigilCompose SigilComposeTexture SigilWorldDiligent
  SUITES SetSession SetDoors SetWindow SetBodies SetKinds TheSetSession/*
  LABELS gpu)

sigil_bench(sketch_bench
  SOURCES bench/SetSessionBench.cpp
  SUPPORT_DIRS ${CMAKE_CURRENT_LIST_DIR}/../test
               ${SIGIL_GEOMETRY_DEVICE_TEST_SUPPORT_DIR}
  LIBRARIES SigilSketch SigilWeavePorts SigilWeaveFonts SigilGeometryKit
            SigilWorldDiligent)
