# The set feature: the 3D sketch surface and the session that drives it —
# a ticker, a retained Scene, and one set body describing a frame as a
# function of the scene time.
#
# ITS PUBLIC DEPENDENCIES ARE THE 3D SKETCH API. It links no device: the
# runtime a session draws through is a value the process installs, so a
# machine with no device runs every set on the CPU mesh executor. The
# session's lap timer is read in its source alone. So is the compose
# leaf a 2D scene is painted into a texture by: the context's door names
# the scene by forward declaration only, and a set walking through it
# includes SigilCompose's own headers.
sigil_library(SigilSketchSet PIC
  SOURCES Context.cpp Session.cpp
  HEADERS Set.h
  PUBLIC SigilSketchCore SigilWorldScene SigilWorldKit SigilWorldFrame
         SigilWorldElement SigilWorldLight SigilMaterialKit SigilMotionClock
  PRIVATE SigilMeasure SigilComposeTexture)

# The test walks through the door, so it includes and links what a set
# doing the same would: the compose tree it paints and the leaf that
# keeps the scene.
sigil_test(sketch_set_test
  SOURCES test/SetSessionTest.cpp
  LIBRARIES SigilSketchSet SigilWeavePorts SigilWeaveFonts SigilGeometryKit
            SigilCompose SigilComposeTexture)

sigil_bench(sketch_set_bench
  SOURCES bench/SetSessionBench.cpp
  LIBRARIES SigilSketchSet SigilWeavePorts SigilWeaveFonts SigilGeometryKit)
