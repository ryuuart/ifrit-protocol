# The font service and the shaper: HarfBuzz faces bound to Skia typefaces,
# fallback resolution, varied-typeface clones, the content-addressed word
# shape cache, and one word shaped into a ShapedWord. HarfBuzz and Boost
# stay private — no public header names a type of either.
find_package(boost_container_hash CONFIG REQUIRED)
find_package(boost_unordered CONFIG REQUIRED)
find_package(harfbuzz CONFIG REQUIRED COMPONENTS icu)
find_package(unofficial-skia CONFIG REQUIRED)
sigil_library(SigilWeaveFonts
  SOURCES FontContext.cpp Shaper.cpp OpticalKerning.cpp FontContextImpl.h
          OpticalKerning.h
  HEADERS FontContext.h Shaper.h
  PUBLIC SigilWeaveStyle unofficial::skia::skia
  PRIVATE harfbuzz::harfbuzz Boost::container_hash Boost::unordered)

# What the font service promises about the faces it is handed: the fallback
# memo keyed by language, the varied clone that is never retained, and the
# pair a shaper sets by measuring outlines. Every face here is an instrument
# committed under src/test/assets/, so a runner needs no font of its own and
# the answers are the same on every machine — which is why there is no label.
sigil_test(weave_test
  SOURCES test/FontContextTest.cpp
  LIBRARIES SigilWeaveFonts SigilWeavePorts)

# The vertical OpenType features, asked of the face committed under
# test/assets/ rather than of this machine's. Its own binary because it is
# its own promise about what a runner needs: nothing. A label here would
# say a face is required that is already in the tree.
sigil_test(weave_test
  SOURCES test/VerticalFeaturesTest.cpp
  LIBRARIES SigilWeaveFonts SigilWeavePorts)

# The shaper per word, cold and warm.
sigil_bench(weave_bench
  SOURCES bench/ShaperBench.cpp
  LIBRARIES SigilWeaveFonts SigilWeavePorts SigilWeaveUnicode)
