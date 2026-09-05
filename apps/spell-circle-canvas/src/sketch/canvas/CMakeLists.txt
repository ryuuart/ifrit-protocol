# The canvas runtime: the 2D sketch surface and the session that drives
# it — a clock, a ticker and a Composer wired together, with one sketch
# body describing into them. The test walks through both doors, so it
# links what a sketch doing the same would: the compose leaf that keeps
# the scene, and the world it bakes.
sigil_test(sketch_test
  SOURCES test/CanvasSessionTest.cpp
  LIBRARIES SigilSketch SigilComposeTexture SigilWorldElement
            SigilWorldFrame SigilGeometryKit SigilMaterialKit
            SigilWeavePorts)

sigil_bench(sketch_bench
  SOURCES bench/CanvasSessionBench.cpp
  LIBRARIES SigilSketch)
