# The document model: styled UTF-16 text analysed into words through the
# Unicode leaf and shaped lazily through the font service, with the edit
# log external range trackers follow. The Unicode leaf is private — no
# public header names one of its types — and it is also where the tag a
# shaper is told a run's script in comes from, so nothing here names a
# HarfBuzz type. The one Boost container inside a Word hides behind opaque
# bytes in WordSegmentList.
sigil_library(SigilWeaveParagraph
  SOURCES Paragraph.cpp WordSegmentList.cpp
  HEADERS Paragraph.h Word.h
  PUBLIC SigilWeaveFonts
  PRIVATE SigilWeaveUnicode Boost::container)

# Shaping as the paragraph drives it, the document model, the typographic
# correctness a shaping engine must hold, and the options that reach
# shaping. Nothing here lays a line out. Several cases can only be asked
# on a machine that has the face for the script they are about, and they
# skip when it does not; the label says so, so a runner that knows its
# own font set can require them.
sigil_test(weave_paragraph_test
  SOURCES test/ShapingTest.cpp test/ParagraphTest.cpp test/CorrectnessTest.cpp
          test/TypographyTest.cpp
  LIBRARIES SigilWeaveParagraph SigilWeavePorts Boost::container
            Boost::unordered
  LABELS fonts)

# A whole paragraph analysed and shaped, cold and warm.
sigil_bench(weave_paragraph_bench
  SOURCES bench/ParagraphBench.cpp
  LIBRARIES SigilWeaveParagraph SigilWeavePorts)
