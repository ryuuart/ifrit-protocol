# The layout stage: the flow geometries text is broken into, the greedy and
# Knuth-Plass breakers over one flattened interval sequence, placement as
# shared or baked blobs, the ellipsis, and the per-line and per-column
# metrics derived from the placed runs. SigilGeometryPath is public — a
# contour interval carries a geometry::path::Contour. The Unicode leaf is
# private: a reading set beside its base is split at grapheme boundaries
# in one source file and nowhere in a header.
sigil_library(SigilWeaveLayout
  SOURCES Beside.cpp Flow.cpp LineBreak.cpp KnuthPlass.cpp LayoutMetrics.cpp
          ParagraphLayoutInternal.h
  HEADERS Beside.h Flow.h LayoutOptions.h PositionedRun.h ParagraphLayout.h
  PUBLIC SigilWeaveParagraph SigilGeometryPath
  PRIVATE SigilWeaveUnicode harfbuzz::harfbuzz ICU::uc Boost::unordered)

# Everything that places runs and reads where they landed: both breakers,
# the flows, overflow and clamp, vertical writing, placeholders, relayout
# locality, the typographic options the breakers read, text on a path, the
# paragraph controls one subject to a file, and the large-paragraph stress
# cases. The stress case that restyles through the query layer links it,
# and the files that read a stock table link the kit.
sigil_test(weave_layout_test
  SOURCES test/LayoutTest.cpp test/KnuthPlassTest.cpp test/OverflowTest.cpp
          test/VerticalTest.cpp test/FlowTest.cpp test/PathTextTest.cpp
          test/PlaceholderTest.cpp test/IncrementalTest.cpp
          test/TypographyTest.cpp test/LeadingTest.cpp test/TabStopTest.cpp
          test/FrameTest.cpp test/HyphenationTest.cpp test/LineEdgesTest.cpp
          test/BesideTest.cpp test/MojikumiTest.cpp test/StressTest.cpp
  LIBRARIES SigilWeaveLayout SigilWeaveKit SigilWeaveQuery SigilWeavePorts
            Boost::unordered)

# layoutParagraph per word over a warm shape cache.
sigil_bench(weave_layout_bench
  SOURCES bench/LayoutBench.cpp
  LIBRARIES SigilWeaveLayout SigilWeavePorts)
