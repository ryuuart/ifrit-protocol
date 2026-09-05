# The canvas feature: the 2D sketch surface and the session that drives
# it — a clock, a ticker and a Composer wired together, with one sketch
# body describing into them.
#
# ITS PUBLIC DEPENDENCIES ARE THE 2D SKETCH API. What a 2D sketch may
# #include is exactly this list, because the flags a sketch compiles with
# are lifted from a translation unit inside this target graph. The
# testing surface is header-only, so it costs nothing at link time: a
# sketch that reports its own checks reads it. The weave kit carries the
# tables a page of set text asks for — the hyphenation patterns among
# them — so a sketch may reach one.
#
# The session's own clock, ticker and lap timer are read in its source
# alone, so they are no part of the sketch API. So is the retained scene
# a set is baked to an image in: the context's door names a world Frame
# by forward declaration only, and a sketch walking through it includes
# SigilWorld's own headers.
sigil_library(SigilSketchCanvas PIC
  SOURCES Context.cpp Session.cpp
  HEADERS Sketch.h
  PUBLIC SigilSketchCore SigilComposeCore SigilComposeKit SigilComposeBrush
         SigilComposeTypography SigilComposeTexture SigilComposeTesting
         SigilGeometry SigilMaterial SigilWeaveKit SigilWeavePorts EnTT::EnTT
  PRIVATE SigilMotionClock SigilMeasure SigilWorldScene)

# The test walks through both doors, so it includes and links what a
# sketch doing the same would: the compose leaf that keeps the scene, and
# the world it bakes.
sigil_test(sketch_canvas_test
  SOURCES test/CanvasSessionTest.cpp
  LIBRARIES SigilSketchCanvas SigilComposeTexture SigilWorldElement
            SigilWorldFrame SigilGeometryKit SigilMaterialKit)

sigil_bench(sketch_canvas_bench
  SOURCES bench/CanvasSessionBench.cpp
  LIBRARIES SigilSketchCanvas)
