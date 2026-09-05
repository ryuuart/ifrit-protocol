# The set runtime: the 3D sketch surface and the session that drives it —
# a ticker, a retained Scene, and one set body describing a frame as a
# function of the scene time. The test walks through the door, so it
# links what a set doing the same would: the compose tree it paints and
# the leaf that keeps the scene.
sigil_test(sketch_test
  SOURCES test/SetSessionTest.cpp
  LIBRARIES SigilSketch SigilWeavePorts SigilWeaveFonts SigilGeometryKit
            SigilCompose SigilComposeTexture)

sigil_bench(sketch_bench
  SOURCES bench/SetSessionBench.cpp
  LIBRARIES SigilSketch SigilWeavePorts SigilWeaveFonts SigilGeometryKit)
