# Decoration bands resolved against placed runs: thickness, position and
# color filled in from the font's metrics, the paint a band draws with,
# skip-ink cut around glyph intercepts, and the walk that turns a layout's
# decorations into rectangles the paint feature puts on the canvas. The
# intercept cache's key is folded with the shared stir, so a hash is one
# body wherever it is accumulated.
sigil_library(SigilWeaveDecoration
  SOURCES Decoration.cpp
  HEADERS Decoration.h DecorationRects.h
  PUBLIC SigilWeaveLayout
  PRIVATE SigilCoreCompute Boost::unordered)

# Bands checked as geometry, without drawing them.
sigil_test(weave_decoration_test
  SOURCES test/DecorationTest.cpp
  LIBRARIES SigilWeaveDecoration SigilWeavePorts)
