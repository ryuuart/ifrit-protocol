# The document model: styled UTF-16 text analysed into words through the
# Unicode leaf and shaped lazily through the font service, with the edit
# log external range trackers follow. The Unicode leaf is private — no
# public header names one of its types — and it is also where the tag a
# shaper is told a run's script in comes from, so nothing here names a
# HarfBuzz type. The one Boost container inside a Word hides behind opaque
# bytes in WordSegmentList. Beside the document: RichText, the same content
# said as runs and styles in one comparable value, and Unit, the
# granularity a passage is addressed by.
find_package(boost_container CONFIG REQUIRED)
find_package(boost_unordered CONFIG REQUIRED)
sigil_library(SigilWeaveParagraph
  SOURCES Paragraph.cpp WordSegmentList.cpp RichText.cpp
  HEADERS Paragraph.h Word.h RichText.h Unit.h
  PUBLIC SigilWeaveFonts
  PRIVATE SigilWeaveUnicode Boost::container)

# Shaping as the paragraph drives it, the document model, the typographic
# correctness a shaping engine must hold, and the options that reach
# shaping. Nothing here lays a line out. Every case shapes the instrument
# faces from src/test/assets/ and needs nothing of the machine, except the
# ones named below, whose claim is the MACHINE'S OWN fallback — that a
# paragraph of mixed scripts, Hebrew, an accent or emoji set in no named
# face resolves to real glyphs at all — which cannot be asked without the
# machine having faces for them, so they alone carry the label.
sigil_test(weave_test
  SOURCES test/ShapingTest.cpp test/ParagraphTest.cpp test/CorrectnessTest.cpp
          test/TypographyTest.cpp test/RichTextTest.cpp
  LIBRARIES SigilWeaveParagraph SigilWeavePorts Boost::container
            Boost::unordered
  LABELS fonts
  SUITES Itemization.MixedLatinCjkSplitsIntoWords
         Itemization.CjkGetsPerCharacterBreakOpportunities
         Itemization.FallbackResolvesCjkGlyphs
         Itemization.ARightToLeftWordIsShapedRightToLeft
         EmojiClusters Correctness.ClusterCoverageIsComplete
         Correctness.ZwnjBlocksArabicJoining
         Correctness.CombiningMarkAttachesToBase
         Shaper.PurgeAllCachesRefillsIdentically)

# A whole paragraph analysed and shaped, cold and warm.
sigil_bench(weave_bench
  SOURCES bench/ParagraphBench.cpp
  LIBRARIES SigilWeaveParagraph SigilWeavePorts)
