# The layout stage: the flow geometries text is broken into, the greedy and
# Knuth-Plass breakers over one flattened interval sequence, placement as
# shared or baked blobs, the ellipsis, and the per-line and per-column
# metrics derived from the placed runs, with Story — a text and the block
# styles it is set under — the value a chain of frames is filled from.
# SigilGeometryPath is public — a contour interval carries a
# geometry::path::Contour. The Unicode leaf is
# private: a reading set beside its base is split at grapheme boundaries
# in one source file and nowhere in a header.
find_package(ICU REQUIRED COMPONENTS i18n uc data)
find_package(boost_unordered CONFIG REQUIRED)
find_package(harfbuzz CONFIG REQUIRED COMPONENTS icu)
sigil_library(SigilWeaveLayout
  SOURCES Beside.cpp Flow.cpp LineBreak.cpp KnuthPlass.cpp LayoutMetrics.cpp
          ParagraphLayoutInternal.h
  HEADERS Beside.h Flow.h LayoutOptions.h PositionedRun.h ParagraphLayout.h
          Story.h
  PUBLIC SigilWeaveParagraph SigilGeometryPath
  PRIVATE SigilWeaveUnicode harfbuzz::harfbuzz ICU::uc Boost::unordered)

# Everything that places runs and reads where they landed, one subject to a
# file named for it: both breakers and the live composer over them, the
# flows, overflow and clamp, vertical writing, placeholders, relayout
# locality, text set on a geometry of its own, and each paragraph control.
# A claim that holds for either breaker is one TEST_P over both of them.
# The kit is linked for the stock hyphenation and line-edge tables the
# files that ask a table question read.
sigil_test(weave_layout_test
  SOURCES test/LayoutTest.cpp test/KnuthPlassTest.cpp
          test/LiveComposerTest.cpp test/OverflowTest.cpp
          test/VerticalTest.cpp test/FlowTest.cpp test/PathTextTest.cpp
          test/PlaceholderTest.cpp test/IncrementalTest.cpp
          test/JustificationTest.cpp test/LeadingTest.cpp test/TabStopTest.cpp
          test/FrameTest.cpp test/HyphenationTest.cpp test/LineEdgesTest.cpp
          test/BesideTest.cpp test/MojikumiTest.cpp test/BalanceTest.cpp
          test/StoryTest.cpp
  LIBRARIES SigilWeaveLayout SigilWeaveKit SigilWeavePorts
            Boost::unordered)

# layoutParagraph per word over a warm shape cache.
sigil_bench(weave_layout_bench
  SOURCES bench/LayoutBench.cpp
  LIBRARIES SigilWeaveLayout SigilWeavePorts)
