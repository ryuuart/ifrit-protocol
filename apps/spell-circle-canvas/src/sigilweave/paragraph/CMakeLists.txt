# The document model: styled UTF-16 text analysed into words through the
# Unicode leaf and shaped lazily through the font service, with the edit
# log external range trackers follow. The Unicode leaf is private — no
# public header names one of its types — and it is also where the tag a
# shaper is told a run's script in comes from, so nothing here names a
# HarfBuzz type. The one Boost container inside a Word hides behind opaque
# bytes in WordSegmentList.
add_library(SigilWeaveParagraph STATIC
  Paragraph.cpp
  WordSegmentList.cpp
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/paragraph/Paragraph.h
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/paragraph/Word.h
)
target_compile_features(SigilWeaveParagraph PUBLIC cxx_std_20)
target_include_directories(SigilWeaveParagraph PUBLIC
  $<BUILD_INTERFACE:${SIGIL_WEAVE_INCLUDE_DIR}>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(SigilWeaveParagraph
  PUBLIC
    SigilWeaveFonts
  PRIVATE
    SigilWeaveUnicode
    Boost::container
)

# Shaping as the paragraph drives it, the document model, the typographic
# correctness a shaping engine must hold, and the options that reach
# shaping. Nothing here lays a line out.
add_executable(weave_paragraph_test
  test/ShapingTest.cpp
  test/ParagraphTest.cpp
  test/CorrectnessTest.cpp
  test/TypographyTest.cpp
)
target_include_directories(weave_paragraph_test PRIVATE ${SIGIL_WEAVE_TEST_SUPPORT_DIR})
target_link_libraries(weave_paragraph_test PRIVATE
  SigilWeaveParagraph
  SigilWeavePorts
  Boost::container
  Boost::unordered
  GTest::gtest_main
)
add_test(NAME weave_paragraph_test COMMAND weave_paragraph_test)
# Several cases here can only be asked on a machine that has the face for
# the script they are about, and they skip when it does not. The label says
# so, so a runner that knows its own font set can require them.
set_tests_properties(weave_paragraph_test PROPERTIES LABELS fonts)

# A whole paragraph analysed and shaped, cold and warm. Built through
# `benches`, run through scripts/bench_ledger.py from a Release build.
add_executable(weave_paragraph_bench bench/ParagraphBench.cpp)
target_include_directories(weave_paragraph_bench PRIVATE ${SIGIL_WEAVE_BENCH_SUPPORT_DIR})
target_link_libraries(weave_paragraph_bench PRIVATE
  SigilWeaveParagraph
  SigilWeavePorts
  benchmark::benchmark
)
add_dependencies(benches weave_paragraph_bench)
