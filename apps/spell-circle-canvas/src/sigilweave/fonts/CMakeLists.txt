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

# Tests shape through the system font port; every weave test binary
# shares the one process-wide FontContext in test/support/Fonts.h. The
# varied-clone and optical-kerning cases name an installed family and
# skip without it; the label says so, so a runner that knows its own
# font set can require them.
sigil_test(weave_fonts_test
  SOURCES test/FontContextTest.cpp test/VerticalFeaturesTest.cpp
  LIBRARIES SigilWeaveFonts SigilWeavePorts
  LABELS fonts)

# The shaper per word, cold and warm.
sigil_bench(weave_fonts_bench
  SOURCES bench/ShaperBench.cpp
  LIBRARIES SigilWeaveFonts SigilWeavePorts SigilWeaveUnicode)
