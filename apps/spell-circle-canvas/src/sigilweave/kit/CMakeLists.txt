# Companion utilities distilled from the gallery, demo and app consumers:
# keyed rebuild guards, paragraph-aware layout memoization, glyph
# bucketing, label and style shorthand, and shared sample content. They
# make the invisible parts of fast usage — cache invalidation keys,
# draw-call batching — explicit and reusable (see README.md here).
# Qt-free; a label draws, so the kit rests on the paint feature, and it
# asks the Unicode leaf the character questions its tables are derived from
# rather than holding a copy of the answers.
add_library(SigilWeaveKit STATIC
  Hyphenation.cpp
  Labels.cpp
  LineTables.cpp
  SampleText.cpp
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/kit/SigilWeaveKit.h
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/kit/RebuildGuard.h
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/kit/CachedValue.h
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/kit/LayoutGuard.h
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/kit/Quantize.h
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/kit/GlyphBuckets.h
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/kit/Hyphenation.h
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/kit/Labels.h
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/kit/LineTables.h
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/kit/Palette.h
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/kit/SampleText.h
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/kit/Timing.h
)
target_compile_features(SigilWeaveKit PUBLIC cxx_std_20)
target_include_directories(SigilWeaveKit PUBLIC
  $<BUILD_INTERFACE:${SIGIL_WEAVE_INCLUDE_DIR}>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(SigilWeaveKit
  PUBLIC
    SigilWeavePaint
    unofficial::skia::skia
  PRIVATE
    SigilWeaveUnicode
)

# The invalidation semantics the kit exists to make explicit.
add_executable(weave_kit_test
  test/KitTest.cpp
)
target_include_directories(weave_kit_test PRIVATE ${SIGIL_WEAVE_TEST_SUPPORT_DIR})
target_link_libraries(weave_kit_test PRIVATE
  SigilWeaveKit
  SigilWeavePorts
  GTest::gtest_main
)
add_test(NAME weave_kit_test COMMAND weave_kit_test)
