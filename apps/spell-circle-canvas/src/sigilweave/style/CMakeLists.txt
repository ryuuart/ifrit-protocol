# The style vocabulary every other feature speaks, one header per subject:
# a TextStyle's shaping half (the shape-cache key) and paint half (resolved
# at draw time), paint layers, decorations, the named-style registry, and
# the OpenType feature presets, with Style.h the umbrella over them.
# Header-only over Skia's paint and typeface types; a paint layer names a
# SigilMaterial instance by pointer and links nothing for it.
add_library(SigilWeaveStyle INTERFACE
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/style/Decoration.h
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/style/Features.h
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/style/PaintLayer.h
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/style/PaintStyle.h
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/style/ShapingStyle.h
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/style/Style.h
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/style/StyleSet.h
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/style/TextStyle.h
)
target_compile_features(SigilWeaveStyle INTERFACE cxx_std_20)
target_include_directories(SigilWeaveStyle INTERFACE
  $<BUILD_INTERFACE:${SIGIL_WEAVE_INCLUDE_DIR}>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(SigilWeaveStyle
  INTERFACE
    unofficial::skia::skia
)

# Styles are plain values, so their test shapes nothing and links no fonts.
add_executable(weave_style_test
  test/StyleTest.cpp
)
target_include_directories(weave_style_test PRIVATE ${SIGIL_WEAVE_TEST_SUPPORT_DIR})
target_link_libraries(weave_style_test PRIVATE
  SigilWeaveStyle
  GTest::gtest_main
)
add_test(NAME weave_style_test COMMAND weave_style_test)
