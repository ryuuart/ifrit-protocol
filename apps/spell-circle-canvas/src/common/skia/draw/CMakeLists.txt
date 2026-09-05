# The draw feature: the two SkCanvas ops Graphite leaves unimplemented,
# decomposed into ones every backend performs. Header-only over Skia and
# nothing else — no device, no context, no bring-up — so a consumer that
# never brings a Graphite context up still draws a nine-slice frame and a
# sprite sheet that survive being recorded on one canvas and replayed on
# another.
sigil_library(SigilSkiaDraw
  HEADERS Direct.h
  INTERFACE unofficial::skia::skia)

# The band split is arithmetic, so its test needs no GPU and no context —
# which is the point of the feature standing apart from the bring-up.
sigil_test(skia_draw_test
  SOURCES test/DirectTest.cpp
  LIBRARIES SigilSkiaDraw)
