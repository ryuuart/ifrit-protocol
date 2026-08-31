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
