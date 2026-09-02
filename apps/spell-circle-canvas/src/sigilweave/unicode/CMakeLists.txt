# The Unicode leaf: transcoding, case mapping, script itemization, bidi and
# segmentation as plain values over UTF-16 text. No Skia and no other weave
# header, so it compiles in isolation and any consumer that needs text
# analysis without fonts can take it alone. It links ICU, which answers
# every question here but one: the tag a shaper is told a run's script in
# is HarfBuzz's translation of an ICU script code, so the leaf also links
# HarfBuzz and its ICU bridge and the features above it never open an ICU
# header to shape.

# The bridge ships as an archive of its own and the package config exports
# targets for the main library and the subsetter only, so it is named
# directly. Debug and release archives are separate builds; each config
# links its own.
find_library(SIGIL_HARFBUZZ_ICU_RELEASE NAMES harfbuzz-icu
  PATHS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib"
  NO_DEFAULT_PATH REQUIRED)
find_library(SIGIL_HARFBUZZ_ICU_DEBUG NAMES harfbuzz-icu
  PATHS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/lib"
  NO_DEFAULT_PATH)
if(SIGIL_HARFBUZZ_ICU_DEBUG)
  set(SIGIL_HARFBUZZ_ICU
      "$<IF:$<CONFIG:Debug>,${SIGIL_HARFBUZZ_ICU_DEBUG},${SIGIL_HARFBUZZ_ICU_RELEASE}>")
else()
  set(SIGIL_HARFBUZZ_ICU "${SIGIL_HARFBUZZ_ICU_RELEASE}")
endif()

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
    ${SIGIL_HARFBUZZ_ICU}
    harfbuzz::harfbuzz
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
