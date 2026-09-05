# The font service and the shaper: HarfBuzz faces bound to Skia typefaces,
# fallback resolution, varied-typeface clones, the content-addressed word
# shape cache, and one word shaped into a ShapedWord. HarfBuzz and Boost
# stay private — no public header names a type of either.
sigil_library(SigilWeaveFonts
  SOURCES FontContext.cpp Shaper.cpp OpticalKerning.cpp FontContextImpl.h
          OpticalKerning.h
  HEADERS FontContext.h Shaper.h
  PUBLIC SigilWeaveStyle unofficial::skia::skia
  PRIVATE harfbuzz::harfbuzz Boost::container_hash Boost::unordered)

# What the font service promises about faces this machine happens to have:
# the fallback memo, the varied clone, and the pair a shaper sets by
# measuring outlines. Each names an installed family and skips without it,
# which is what the label says, so a runner that knows its own font set can
# require them and every other runner can let them pass.
sigil_test(weave_fonts_test
  SOURCES test/FontContextTest.cpp
  LIBRARIES SigilWeaveFonts SigilWeavePorts
  LABELS fonts)

# The vertical OpenType features, asked of the face committed under
# test/assets/ rather than of this machine's. Its own binary because it is
# its own promise about what a runner needs: nothing. A label here would
# say a face is required that is already in the tree.
sigil_test(weave_vertical_features_test
  SOURCES test/VerticalFeaturesTest.cpp
  LIBRARIES SigilWeaveFonts SigilWeavePorts)

# The shaper per word, cold and warm.
sigil_bench(weave_fonts_bench
  SOURCES bench/ShaperBench.cpp
  LIBRARIES SigilWeaveFonts SigilWeavePorts SigilWeaveUnicode)
