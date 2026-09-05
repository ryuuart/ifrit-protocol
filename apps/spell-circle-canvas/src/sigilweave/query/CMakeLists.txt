# Range search over a paragraph — substring, word, ICU regular expression
# — and the marker set that follows the paragraph's edit log. ICU is
# private and linked directly for the regex search.
sigil_library(SigilWeaveQuery
  SOURCES Query.cpp
  HEADERS Query.h
  PUBLIC SigilWeaveParagraph
  PRIVATE ICU::i18n ICU::uc ICU::data)

# The query tests place through the layout to check that a paint-only
# restyle skips re-analysis, so the binary links the layout as well.
sigil_test(weave_query_test
  SOURCES test/QueryTest.cpp
  LIBRARIES SigilWeaveQuery SigilWeaveLayout SigilWeavePorts)
