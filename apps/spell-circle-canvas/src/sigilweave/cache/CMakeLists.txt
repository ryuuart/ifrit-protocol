# Single-style paragraphs memoized by text, typeface and quantized size,
# for labels and captions rebuilt at high frequency. The node-based map
# that keeps returned references stable hides behind a pimpl, so Boost
# stays private.
sigil_library(SigilWeaveCache
  SOURCES SingleLineParagraphCache.cpp
  HEADERS SingleLineParagraphCache.h
  PUBLIC SigilWeaveParagraph
  PRIVATE Boost::unordered ICU::uc)

# The key, the quantisation step, and the two promises the node-based
# storage makes about a reference it handed out. Nothing here shapes, so
# the port is linked only for the committed face the typeface half of the
# key is varied with.
sigil_test(weave_cache_test
  SOURCES test/CacheTest.cpp
  LIBRARIES SigilWeaveCache SigilWeavePorts)
