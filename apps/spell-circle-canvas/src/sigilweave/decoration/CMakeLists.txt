# Decoration bands resolved against placed runs: thickness, position and
# color filled in from the font's metrics, the paint a band draws with,
# skip-ink cut around glyph intercepts, and the walk that turns a layout's
# decorations into rectangles the paint feature puts on the canvas. The
# intercept cache's key is folded with Boost's stir: the table lives
# inside one run and nothing outside the process ever sees a bucket of
# it, so the fold does not have to be one whose answer is pinned.
find_package(boost_unordered CONFIG REQUIRED)
sigil_library(SigilWeaveDecoration
  SOURCES Decoration.cpp
  HEADERS Decoration.h DecorationRects.h
  PUBLIC SigilWeaveLayout
  PRIVATE Boost::unordered)

# Bands checked as geometry, without drawing them: where each kind and side
# anchors is one TEST_P over the anchor table. Whether the ink arrives is
# the paint feature's, which is the only other place these bands are asked
# about.
sigil_test(weave_test
  SOURCES test/DecorationTest.cpp
  LIBRARIES SigilWeaveDecoration SigilWeavePorts)
