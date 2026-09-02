# SigilComposeTypography — the text engine and the type vocabulary.
#
# The engine behind dressed type: fx() tracks resolved and drawn per glyph,
# a run laid along a path, mark() anchors, span restyles folded into axis
# tracks, and the schedule read back — reached by the kernel through the
# TextPainter value the text verbs install on a description, which is why
# those verbs (`fx`, `onPath`, `mark`, `spanStyle`, `spanPaint`,
# `variationDrive`) are defined here though the kernel declares them.
# With it the header-only vocabulary: the stock text effects and the
# marquee (TextFx.h), and the kit's legibility helpers (kit/Legibility.h).
#
# Sees the kernel's source directory PRIVATELY: the engine reads the
# retained Instance and Composer::Impl the way a kernel phase does, because
# it is one — the one that moved out. Nothing above this target inherits
# that path.
add_library(SigilComposeTypography STATIC
  TextFx.cpp
  TextFxPainting.cpp
  TextAnnotations.cpp
  TextMarks.cpp
  PathText.cpp
  TextPainter.cpp
  AxisGate.h
  TextEngine.h
  TextPose.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/typography/TextFx.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/typography/Typography.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/kit/Legibility.h
)
target_compile_features(SigilComposeTypography PUBLIC cxx_std_20)
target_include_directories(SigilComposeTypography
  PRIVATE ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/../core)
target_link_libraries(SigilComposeTypography
  PUBLIC SigilComposeCore
  PRIVATE yoga::yogacore)

# Text data, the text pass, vertical writing, motion along paths, the
# text-fx presets, rich spans and the variation drive. The kit is here
# because a path baseline and an upright column are exercised on `shapes::`
# silhouettes.
_compose_test_binary(compose_text_test
  LIBRARIES SigilComposeTypography SigilComposeKit
  SOURCES
    test/ComposeTestTextData.cpp
    test/ComposeTestTextPass.cpp
    test/ComposeTestVertical.cpp
    test/ComposeTestPathMotion.cpp
    test/ComposeTestContentText.cpp
    test/ComposeTestParagraphs.cpp
    test/ComposeTestEngineDocsText.cpp
)
target_compile_definitions(compose_text_test PRIVATE
  SIGILCOMPOSE_TEST_ASSET_DIR="${SIGIL_COMPOSE_TEST_ASSET_DIR}")

# Dense text replayed, baked and projected; kinetic text every frame.
_compose_bench(compose_text_bench GPU
  LIBRARIES SigilComposeTypography
  SOURCES bench/ComposeTextBench.cpp)
