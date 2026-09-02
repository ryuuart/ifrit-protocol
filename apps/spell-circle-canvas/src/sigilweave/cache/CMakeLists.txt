# Single-style paragraphs memoized by text, typeface and quantized size,
# for labels and captions rebuilt at high frequency. The node-based map
# that keeps returned references stable hides behind a pimpl, so abseil
# stays private.
add_library(SigilWeaveCache STATIC
  SingleLineParagraphCache.cpp
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/cache/SingleLineParagraphCache.h
)
target_compile_features(SigilWeaveCache PUBLIC cxx_std_20)
target_include_directories(SigilWeaveCache PUBLIC
  $<BUILD_INTERFACE:${SIGIL_WEAVE_INCLUDE_DIR}>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(SigilWeaveCache
  PUBLIC
    SigilWeaveParagraph
  PRIVATE
    absl::node_hash_map
    absl::strings
    ICU::uc
)

# The key, the quantisation step, and the two promises the node-based
# storage makes about a reference it handed out. Nothing here shapes, so
# the port is linked only for the committed face the typeface half of the
# key is varied with.
add_executable(weave_cache_test
  test/CacheTest.cpp
)
target_compile_definitions(weave_cache_test PRIVATE
  SIGILWEAVE_TEST_ASSET_DIR="${SIGIL_WEAVE_TEST_ASSET_DIR}")
target_link_libraries(weave_cache_test PRIVATE
  SigilWeaveCache
  SigilWeavePorts
  GTest::gtest_main
)
add_test(NAME weave_cache_test COMMAND weave_cache_test)
