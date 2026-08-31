# Range search over a paragraph — substring, word, ICU regular expression
# — and the marker set that follows the paragraph's edit log. ICU is
# private and linked directly for the regex search.
add_library(SigilWeaveQuery STATIC
  Query.cpp
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/query/Query.h
)
target_compile_features(SigilWeaveQuery PUBLIC cxx_std_20)
target_include_directories(SigilWeaveQuery PUBLIC
  $<BUILD_INTERFACE:${SIGIL_WEAVE_INCLUDE_DIR}>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(SigilWeaveQuery
  PUBLIC
    SigilWeaveParagraph
  PRIVATE
    ICU::i18n
    ICU::uc
    ICU::data
)

# The query tests place through the layout to check that a paint-only
# restyle skips re-analysis, so the binary links the layout as well.
add_executable(weave_query_test
  test/QueryTest.cpp
)
target_include_directories(weave_query_test PRIVATE ${SIGIL_WEAVE_TEST_SUPPORT_DIR})
target_link_libraries(weave_query_test PRIVATE
  SigilWeaveQuery
  SigilWeaveLayout
  SigilWeavePorts
  GTest::gtest_main
)
add_test(NAME weave_query_test COMMAND weave_query_test)
