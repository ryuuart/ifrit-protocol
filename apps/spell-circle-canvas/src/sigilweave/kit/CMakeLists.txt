# Companion utilities distilled from the gallery, demo and app consumers:
# keyed rebuild guards, paragraph-aware layout memoization, glyph
# bucketing, label and style shorthand, and shared sample content. They
# make the invisible parts of fast usage — cache invalidation keys,
# draw-call batching — explicit and reusable.
# Qt-free; a label draws, so the kit rests on the paint feature, and it
# asks the Unicode leaf the character questions its tables are derived from
# rather than holding a copy of the answers.
sigil_library(SigilWeaveKit
  SOURCES Hyphenation.cpp Labels.cpp LineTables.cpp SampleText.cpp
  HEADERS SigilWeaveKit.h RebuildGuard.h CachedValue.h LayoutGuard.h
          Quantize.h GlyphBuckets.h Hyphenation.h Labels.h LineTables.h
          Palette.h SampleText.h
  PUBLIC SigilWeavePaint unofficial::skia::skia
  PRIVATE SigilWeaveUnicode Boost::unordered)

# The invalidation semantics the kit exists to make explicit.
sigil_test(weave_kit_test
  SOURCES test/KitTest.cpp
  LIBRARIES SigilWeaveKit SigilWeavePorts)
