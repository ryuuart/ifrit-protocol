# The style vocabulary every other feature speaks, one header per subject:
# a TextStyle's shaping half (the shape-cache key) and paint half (resolved
# at draw time), paint layers, decorations, the named-style registry, the
# OpenType feature presets, and the designated-init `Type` a call site
# names a style's numbers in, with Style.h the umbrella over them.
# Header-only over Skia's paint and typeface types; a paint layer names a
# SigilMaterial instance by pointer and links nothing for it.
find_package(unofficial-skia CONFIG REQUIRED)
sigil_library(SigilWeaveStyle
  HEADERS Decoration.h PaintLayer.h PaintStyle.h ShapingStyle.h
          Style.h StyleSet.h Type.h TextStyle.h
  INTERFACE unofficial::skia::skia)

# Styles are plain values, so their test shapes nothing, links no fonts and
# needs nothing of the machine it runs on.
sigil_test(weave_test
  SOURCES test/StyleTest.cpp
  LIBRARIES SigilWeaveStyle SigilWeaveKit)
