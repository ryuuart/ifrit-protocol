# The draw feature: the immediate-mode sketch surface and the session
# that drives it — a clock, a pen and a surface that keeps what earlier
# frames drew, with one sketch body drawing into it every frame.
#
# ITS PUBLIC DEPENDENCIES ARE THE DRAW SKETCH API. What a draw sketch
# may #include is exactly this list, because the flags a sketch compiles
# with are lifted from a translation unit inside this target graph. The
# ticker is on the context a setup is handed, so its vocabulary —
# steppables, the fixed step and choreograph's Outputs — is part of the
# draw sketch API. The session's own clock and lap timer are read in its
# source alone, so they are no part of it; the image asset is what
# loadImage reads a frame off.
sigil_library(SigilSketchDraw PIC
  SOURCES Session.cpp
  HEADERS Draw.h
  PUBLIC SigilSketchCore SigilDraw SigilMotionClock
  PRIVATE SigilMeasure SigilImage)

sigil_test(sketch_test
  SOURCES test/DrawSessionTest.cpp
  LIBRARIES SigilSketchDraw SigilWeavePorts)

sigil_bench(sketch_bench
  SOURCES bench/DrawSessionBench.cpp
  LIBRARIES SigilSketchDraw SigilWeavePorts)
