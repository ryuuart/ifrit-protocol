# Per-glyph choreography over a finished layout: the walk that hands every
# placed glyph to an effect with its rest pose, paint and identity, the
# dressing a glyph draws with, and the batches that collapse thousands of
# animated letters into a few drawGlyphsRSXform calls.
sigil_library(SigilWeaveChoreograph
  SOURCES Choreograph.cpp
  HEADERS PlacedGlyph.h GlyphDress.h GlyphBatches.h Choreograph.h
  PUBLIC SigilWeaveLayout
  PRIVATE Boost::container)

# The walk, the batches, and a glyph on a contour re-placed from its pen.
sigil_test(weave_choreograph_test
  SOURCES test/ChoreographTest.cpp
  LIBRARIES SigilWeaveChoreograph SigilWeavePorts)
