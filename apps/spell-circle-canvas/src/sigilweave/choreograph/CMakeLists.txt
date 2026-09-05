# Per-glyph choreography over a finished layout: the walk that hands every
# placed glyph to an effect with its rest pose, paint and identity, the
# dressing a glyph draws with, and the batches that collapse thousands of
# animated letters into a few drawGlyphsRSXform calls.
sigil_library(SigilWeaveChoreograph
  SOURCES Choreograph.cpp
  HEADERS PlacedGlyph.h GlyphDress.h GlyphBatches.h Choreograph.h
  PUBLIC SigilWeaveLayout
  PRIVATE Boost::container)

# Two subjects, one file each: the walk over a finished layout with the
# glyph on a contour it re-places from its pen, and the batches a paint-
# complete draw collapses into. The one case that needs a second face uses
# the committed instrument, so nothing here skips on what a machine has.
sigil_test(weave_choreograph_test
  SOURCES test/ChoreographTest.cpp test/GlyphBatchesTest.cpp
  LIBRARIES SigilWeaveChoreograph SigilWeavePorts)
