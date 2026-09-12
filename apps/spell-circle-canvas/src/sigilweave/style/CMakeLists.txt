# The style vocabulary every other feature speaks, one header per subject:
# a TextStyle's shaping half (the shape-cache key) and paint half (resolved
# at draw time), paint layers, decorations, a base style with the named
# partials over it, the length a size may be stated as, and the `Type` a
# call site names a style's numbers in — every field optional, so the merges
# that resolve one partial against another are the one translation unit
# here. Style.h is the umbrella over them. Over Skia's paint and typeface
# types; a paint layer names a SigilMaterial instance by pointer and links
# nothing for it.
find_package(unofficial-skia CONFIG REQUIRED)
sigil_library(SigilWeaveStyle
  SOURCES Type.cpp
  HEADERS Decoration.h Length.h PaintLayer.h PaintStyle.h ShapingStyle.h
          Style.h StyleSheet.h Type.h TextStyle.h
  PUBLIC unofficial::skia::skia)

# Styles are plain values, so their test shapes nothing, links no fonts and
# needs nothing of the machine it runs on.
sigil_test(weave_test
  SOURCES test/StyleTest.cpp
  LIBRARIES SigilWeaveStyle SigilWeaveKit)
