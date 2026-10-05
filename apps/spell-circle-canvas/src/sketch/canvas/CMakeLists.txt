# The canvas runtime: the 2D sketch surface and the session that drives
# it — a clock, a ticker and a Composer wired together, with one sketch
# body describing into them, and the guest a body wears another
# application's picture as. The test walks through both doors, so it
# links what a sketch doing the same would: the compose leaf that keeps
# the scene, and the world it bakes — on the device, which the cases
# that bake a set bring up and skip without.
sigil_test(sketch_test
  SOURCES test/CanvasSessionTest.cpp test/GuestTest.cpp
  SUPPORT_DIRS ${SIGIL_GEOMETRY_DEVICE_TEST_SUPPORT_DIR}
  LIBRARIES SigilSketch SigilComposeTexture SigilComposeDraw SigilWorldNode
            SigilWorldFrame SigilGeometryKit SigilMaterialSurface
            SigilWeavePorts SigilWorldDiligent
  SUITES CanvasBakes
  LABELS gpu)

sigil_bench(sketch_bench
  SOURCES bench/CanvasSessionBench.cpp
  LIBRARIES SigilSketch SigilWeavePorts)
