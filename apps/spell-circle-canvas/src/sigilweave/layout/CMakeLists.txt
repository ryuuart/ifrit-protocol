# The layout stage: the flow geometries text is broken into, the greedy and
# Knuth-Plass breakers over one flattened interval sequence, placement as
# shared or baked blobs, the ellipsis, and the per-line and per-column
# metrics derived from the placed runs, with Story — a text and the block
# styles it is set under — the value a chain of frames is filled from.
# SigilGeometryPath is public — a contour interval carries a
# geometry::path::Contour. SigilImageField is private: a silhouette asked
# for a standoff measures an exact distance field over its own coverage,
# and that primitive belongs where image meaning lives. The Unicode leaf is
# private: a reading set beside its base is split at grapheme boundaries
# in one source file and nowhere in a header. SigilCoreCompute is private
# too: the fold a break decision is cached under is the fold every cache
# key in this repository is accumulated with.
find_package(ICU REQUIRED COMPONENTS i18n uc data)
find_package(boost_unordered CONFIG REQUIRED)
find_package(harfbuzz CONFIG REQUIRED COMPONENTS icu)
sigil_library(SigilWeaveLayout
  SOURCES BandScan.cpp BandScan.h Beside.cpp Blobs.cpp Blobs.h Blocks.cpp
          Ellipsis.cpp Flow.cpp Greedy.cpp InitialLetter.cpp KnuthPlass.cpp
          LayoutMetrics.cpp LineBreak.cpp Placement.cpp Silhouette.cpp
          KnuthPlassPrefix.h ParagraphLayoutInternal.h
  HEADERS Beside.h Breaking.h Flow.h Frame.h InitialLetter.h Justification.h
          LayoutOptions.h Mojikumi.h Overflow.h ParagraphLayout.h
          ParagraphStyle.h PositionedRun.h Story.h TabStops.h
  PUBLIC SigilWeaveParagraph SigilGeometryPath
  PRIVATE SigilWeaveUnicode SigilImageField SigilCoreCompute
          harfbuzz::harfbuzz ICU::uc Boost::unordered)

# Everything that places runs and reads where they landed, one subject to a
# file named for it: both breakers and the live composer over them, the
# flows, overflow and clamp, vertical writing, placeholders, relayout
# locality, text set on a geometry of its own, and each paragraph control.
# A claim that holds for either breaker is one TEST_P over both of them.
# The kit is linked for the stock hyphenation and line-edge tables the
# files that ask a table question read. Every case sets the instrument
# face from src/test/assets/ except the ones named below, which set CJK,
# Hebrew or a mathematical letter in whatever the machine resolves for it,
# because no instrument carries a script's worth of them: those alone
# carry the label.
sigil_test(weave_test
  SOURCES test/LayoutTest.cpp test/KnuthPlassTest.cpp
          test/LiveComposerTest.cpp test/OverflowTest.cpp
          test/VerticalTest.cpp test/FlowTest.cpp test/SilhouetteTest.cpp test/PathTextTest.cpp
          test/PlaceholderTest.cpp test/IncrementalTest.cpp
          test/JustificationTest.cpp test/LeadingTest.cpp test/TabStopTest.cpp
          test/FrameTest.cpp test/HyphenationTest.cpp test/LineEdgesTest.cpp
          test/BesideTest.cpp test/MojikumiTest.cpp test/BalanceTest.cpp
          test/InitialLetterTest.cpp
          test/StoryTest.cpp
  LIBRARIES SigilWeaveLayout SigilWeaveKit SigilWeavePorts
            Boost::unordered
  LABELS fonts
  SUITES Vertical KnuthPlass.AJustifiedCjkBlockKeepsEveryColumnInsideTheMeasure
         BidiOrder.AReorderedPairRendersInVisualOrderBetweenItsNeighbours
         EditSafety.ACutThroughASurrogatePairLeavesEveryWordInsideTheText
         LineEdges.KinsokuNeverOpensALineWithAProhibitedCharacter)

# layoutParagraph per word over a warm shape cache.
sigil_bench(weave_bench
  SOURCES bench/LayoutBench.cpp bench/UpdateBench.cpp
          bench/ConfettiBench.cpp bench/LiveComposerBench.cpp
  LIBRARIES SigilWeaveLayout SigilWeavePorts)
