# The font service and the shaper: HarfBuzz faces bound to Skia typefaces,
# fallback resolution, varied-typeface clones, the content-addressed word
# shape cache, and one word shaped into a ShapedWord. HarfBuzz and abseil
# stay private — no public header names a type of either.
add_library(SigilWeaveFonts STATIC
  FontContext.cpp
  Shaper.cpp
  OpticalKerning.cpp
  FontContextImpl.h
  OpticalKerning.h
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/fonts/FontContext.h
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/fonts/Shaper.h
)
target_compile_features(SigilWeaveFonts PUBLIC cxx_std_20)
target_include_directories(SigilWeaveFonts PUBLIC
  $<BUILD_INTERFACE:${SIGIL_WEAVE_INCLUDE_DIR}>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(SigilWeaveFonts
  PUBLIC
    SigilWeaveStyle
    unofficial::skia::skia
  PRIVATE
    harfbuzz::harfbuzz
    absl::flat_hash_map
)

# Tests shape through the system font port; every weave test binary
# shares the one process-wide FontContext in test/support/Fonts.h.
add_executable(weave_fonts_test
  test/FontContextTest.cpp
  test/VerticalFeaturesTest.cpp
)
target_include_directories(weave_fonts_test PRIVATE ${SIGIL_WEAVE_TEST_SUPPORT_DIR})
target_compile_definitions(weave_fonts_test PRIVATE
  SIGILWEAVE_TEST_ASSET_DIR="${SIGIL_WEAVE_TEST_ASSET_DIR}")
target_link_libraries(weave_fonts_test PRIVATE
  SigilWeaveFonts
  SigilWeavePorts
  GTest::gtest_main
)
add_test(NAME weave_fonts_test COMMAND weave_fonts_test)
# The varied-clone and optical-kerning cases name an installed family and
# skip without it. The label says so, so a runner that knows its own font
# set can require them.
set_tests_properties(weave_fonts_test PROPERTIES LABELS fonts)

# The shaper per word, cold and warm. Built through `benches`, run through
# scripts/bench_ledger.py from a Release build.
add_executable(weave_fonts_bench bench/ShaperBench.cpp)
target_include_directories(weave_fonts_bench PRIVATE ${SIGIL_WEAVE_BENCH_SUPPORT_DIR})
target_link_libraries(weave_fonts_bench PRIVATE
  SigilWeaveFonts
  SigilWeavePorts
  SigilWeaveUnicode
  benchmark::benchmark
)
add_dependencies(benches weave_fonts_bench)
