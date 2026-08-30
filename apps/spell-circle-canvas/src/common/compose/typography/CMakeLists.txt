# SigilComposeTypography — type styles and text-fx presets.
#
# Header-only: `type()` and `pickFace()` (Type.h), the stock text effects
# and the marquee (TextFx.h), and the kit's legibility helpers
# (kit/Legibility.h). Everything here is a value built from Core's public
# types, so the target carries no archive and links Core as an interface.
add_library(SigilComposeTypography INTERFACE
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/typography/TextFx.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/typography/Type.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/typography/Typography.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/kit/Legibility.h
)
target_link_libraries(SigilComposeTypography INTERFACE SigilComposeCore)

# Text data, the text pass, vertical writing, motion along paths, the
# text-fx presets, rich spans and the variation drive. Typography is
# header-only over Core; Shape is here because a path baseline and an
# upright column are exercised on `shapes::` silhouettes.
_compose_test_binary(compose_text_test
  LIBRARIES SigilComposeTypography SigilComposeShape
  SOURCES
    test/ComposeTestTextData.cpp
    test/ComposeTestTextPass.cpp
    test/ComposeTestVertical.cpp
    test/ComposeTestPathMotion.cpp
    test/ComposeTestContentText.cpp
    test/ComposeTestEngineDocsText.cpp
)
target_compile_definitions(compose_text_test PRIVATE
  SIGILCOMPOSE_TEST_ASSET_DIR="${SIGIL_COMPOSE_TEST_ASSET_DIR}")

# Dense text replayed, baked and projected; kinetic text every frame.
_compose_bench(compose_text_bench GPU
  LIBRARIES SigilComposeTypography
  SOURCES bench/ComposeTextBench.cpp)
