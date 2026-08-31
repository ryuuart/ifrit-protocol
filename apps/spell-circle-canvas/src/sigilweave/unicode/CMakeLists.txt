# The Unicode leaf: transcoding, case mapping, script itemization, bidi and
# segmentation as plain values over UTF-16 text. Links ICU and nothing else
# — no Skia, no other weave header — so it compiles in isolation and any
# consumer that needs text analysis without fonts can take it alone.
add_library(SigilWeaveUnicode STATIC
  Unicode.cpp
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/unicode/Unicode.h
)
target_compile_features(SigilWeaveUnicode PUBLIC cxx_std_20)
target_include_directories(SigilWeaveUnicode PUBLIC
  $<BUILD_INTERFACE:${SIGIL_WEAVE_INCLUDE_DIR}>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(SigilWeaveUnicode
  PUBLIC
  PRIVATE
    ICU::i18n
    ICU::uc
    ICU::data
)

# The leaf shapes nothing, so its test links neither fonts nor a font port.
add_executable(weave_unicode_test
  test/UnicodeTest.cpp
)
target_include_directories(weave_unicode_test PRIVATE ${SIGIL_WEAVE_TEST_SUPPORT_DIR})
target_link_libraries(weave_unicode_test PRIVATE
  SigilWeaveUnicode
  GTest::gtest_main
)
add_test(NAME weave_unicode_test COMMAND weave_unicode_test)

# Google Benchmark, one binary per feature so each links only what it
# measures; built through the `benches` target and run through
# scripts/bench_ledger.py from a Release build.
add_executable(weave_unicode_bench bench/UnicodeBench.cpp)
target_include_directories(weave_unicode_bench PRIVATE ${SIGIL_WEAVE_BENCH_SUPPORT_DIR})
target_link_libraries(weave_unicode_bench PRIVATE
  SigilWeaveUnicode
  benchmark::benchmark
)
add_dependencies(benches weave_unicode_bench)
