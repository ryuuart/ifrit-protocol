# SigilComposeTypography — the text vocabulary and the text engine.
#
# The VOCABULARY is the public headers: the unit, the selector, the effect
# and the effects the runtime evaluates by structure, the track and the
# beat, mixed text and the story, the reading beside the type, and the
# path a baseline can be. The kernel's text leaf is described in it — a
# description stores tracks, runs and readings — so every member the
# kernel stores, compares or evaluates is defined in the header that
# declares it, and the kernel links nothing here to do so. What the
# archive holds is the members that carry a diagnostic, the builders of
# mixed text, and the ENGINE behind dressed type: fx() tracks resolved and
# drawn per glyph, a run laid along a path, mark() anchors, span restyles
# folded into axis tracks, and the schedule read back — reached by the
# kernel through the TextPainter value the text verbs install on a
# description, which is why those verbs (`fx`, `onPath`, `mark`,
# `spanStyle`, `spanPaint`, `variationDrive`, `annotate`) are defined here
# though the kernel declares them. With it the kit's legibility helpers
# (kit/Legibility.h), spelled in this tier's types.
#
# Sees the kernel's source directory PRIVATELY: the engine reads the
# retained Instance and Composer::Impl the way a kernel phase does, because
# it is one — the one that moved out. Nothing above this target inherits
# that path.
add_library(SigilComposeTypography STATIC
  # the vocabulary's out-of-line members
  TextEffect.cpp
  RichText.cpp
  # the engine
  TextFx.cpp
  TextFxPainting.cpp
  TextAnnotations.cpp
  TextMarks.cpp
  PathText.cpp
  TextPainter.cpp
  AxisGate.h
  TextEngine.h
  TextPose.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/typography/Annotation.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/typography/RichText.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/typography/Selector.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/typography/TextEffect.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/typography/TextPath.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/typography/Track.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/typography/Typography.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/typography/Units.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/kit/Legibility.h
)
target_compile_features(SigilComposeTypography PUBLIC cxx_std_20)
target_include_directories(SigilComposeTypography
  PRIVATE ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/../core)
# The vocabulary's headers spell the seeded mixer an effect draws from, the
# field count a track's pin reads, the paint a pass runs, the spread a
# cascade is and the type a run is set in, so those libraries are PUBLIC.
# The engine's sources walk a geometry contour, stamp image assets, read
# the inherited style set a named run resolves through, and shape,
# choreograph, decorate, lay out and query text through SigilWeave; none of
# that reaches a public header, so those are PRIVATE.
target_link_libraries(SigilComposeTypography
  PUBLIC SigilComposeCore SigilCoreComparable SigilCoreCompute SigilMaterial
         SigilMotion SigilWeave
  PRIVATE SigilCoreReconcile SigilGeometryPath SigilImage yoga::yogacore)

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

# Dense text replayed, baked and projected; kinetic text every frame. The
# kit is here for the stock effect the kinetic arms drive.
_compose_bench(compose_text_bench GPU
  LIBRARIES SigilComposeTypography SigilComposeKit
  SOURCES bench/ComposeTextBench.cpp)
