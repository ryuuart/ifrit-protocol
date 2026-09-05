# Range search over a paragraph — substring, word, ICU regular expression
# — and the marker set that follows the paragraph's edit log, with
# Selector and the sel:: vocabulary beside them: the same question written
# down as a comparable value rather than asked of a paragraph now. ICU is
# private and linked directly for the regex search; a UTF-8 needle is
# transcoded by the Unicode leaf, which owns that answer.
find_package(ICU REQUIRED COMPONENTS i18n uc data)
sigil_library(SigilWeaveQuery
  SOURCES Query.cpp
  HEADERS Query.h Selector.h
  PUBLIC SigilWeaveParagraph
  PRIVATE SigilWeaveUnicode ICU::i18n ICU::uc ICU::data)

# The query tests place through the layout to check that a paint-only
# restyle skips re-analysis, so the binary links the layout as well.
sigil_test(weave_query_test
  SOURCES test/QueryTest.cpp test/SelectorTest.cpp
  LIBRARIES SigilWeaveQuery SigilWeaveLayout SigilWeavePorts)
