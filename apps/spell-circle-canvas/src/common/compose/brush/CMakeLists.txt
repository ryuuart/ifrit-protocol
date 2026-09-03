# SigilComposeBrush — what draws along an outline.
#
# Decorations (strokes, shadows, washes, contour walks) and the two
# adaptors that run one on another outline, the line vocabulary
# (`lines::` as lines, rails and hatches), the brush engine (the layered
# stack, the geometry ops and the `brush::` kinds) and the layer styles
# (`styles::` — inner shadows, glows, bevels and the gel and chrome
# bundles over SigilMaterial's kit tables), with the kit's stroke grammar
# (`kit/Strokes.h`), the bordered feed plate (`kit/Plate.h`) and the
# ornament and flourish pieces (`kit/Ornament.h`, `kit/Flourish.h`) that are
# spelled in this tier's types. And the engine behind the kernel's stroke
# grammar and mask gates — span claims, the span normal form, bands, the
# `by::` gates — reached by the kernel through the StrokeResolver and
# MaskResolver values the verbs install, which is why `stroke(spans, …)`,
# `background(spans, …)`, `across()` and every `by::` constructor are
# defined here though the kernel declares them. Links the kernel, the
# typography tier and the geometry library's silhouette shelf publicly — a
# brush is applied to a silhouette — and names every other library its
# headers include. The kit tier links this one, and nothing here reaches
# up into the kit.
add_library(SigilComposeBrush STATIC
  Adaptors.cpp
  Decorations.cpp
  LayerStyles.cpp
  Lines.cpp
  PixelStyles.cpp
  Brushes.cpp
  # the stroke grammar's engine and the mask gates, behind the kernel's
  # StrokeResolver and MaskResolver seams
  StrokePainting.cpp
  SpanNormalForm.cpp
  Band.cpp
  Masks.cpp
  SpanArithmetic.h
  SpanContours.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/brush/Adaptors.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/brush/Brush.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/brush/Brushes.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/brush/Decorations.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/brush/GeometryOps.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/brush/Hatches.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/brush/LayerStyles.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/brush/Layered.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/brush/Lines.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/brush/PixelStyles.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/brush/Rails.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/kit/Flourish.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/kit/Ornament.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/kit/Plate.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/kit/Strokes.h
)
target_compile_features(SigilComposeBrush PUBLIC cxx_std_20)
# Sees the kernel's source directory PRIVATELY, as the typography engine
# does: resolving a span claim reads the retained Instance.
target_include_directories(SigilComposeBrush
  PRIVATE ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/../core)
# What the headers include beyond the three tiers is PUBLIC: the image
# asset a decoration stamps, the material paint and layer-style tables a
# look is built from, the direct draws, and the type style a plate is set
# in. The seeded mixer a brush jitters with is read in the sources alone.
target_link_libraries(SigilComposeBrush
  PUBLIC SigilComposeCore SigilComposeTypography SigilGeometryKit
         SigilImage SigilMaterial SigilSkiaDraw SigilWeave
  PRIVATE SigilCoreCompute yoga::yogacore)

# Decorations, lines, rails, hatches, brushes, the stroke grammar (which is
# what the authoring-grammar file exercises), the kit's stroke presets, the
# brushed masks and the stamped and stroked slices of the other catalogs.
_compose_test_binary(compose_brush_test
  LIBRARIES SigilComposeBrush SigilComposeKit
  SOURCES
    test/ComposeTestLines.cpp
    test/ComposeTestBrushes.cpp
    test/ComposeTestGrammar.cpp
    test/ComposeTestContentBrush.cpp
    test/ComposeTestEngineDocsBrush.cpp
    test/ComposeTestKernelBrush.cpp
    test/ComposeTestMaskBrush.cpp
    test/ComposeTestTextDataBrush.cpp
    test/ComposeTestKitBrush.cpp
)

# Patterns, SDF materials, layer styles, the pattern pan and colour
# management — the paint vocabulary, which is the core's materials and
# this tier's styles over SigilMaterial. Its own binary, so the paint
# suites keep their name.
_compose_test_binary(compose_paint_test
  LIBRARIES SigilComposeBrush SigilComposeKit
  SOURCES
    test/ComposeTestBrushesPaint.cpp
    test/ComposeTestContentPaint.cpp
    test/ComposeTestEngineDocsPaint.cpp
    test/ComposeTestGrammarPaint.cpp
    test/ComposeTestKernelPaint.cpp
    test/ComposeTestLinesPaint.cpp
    test/ComposeTestMaskPaint.cpp
    test/ComposeTestPixelStylesPaint.cpp
    test/ComposeTestTextDataPaint.cpp
)

# Masks, brushes, profiled and span strokes, stamps, warps, hatches and
# the binding cost under stroked cells.
_compose_bench(compose_brush_bench
  LIBRARIES SigilComposeBrush SigilComposeKit
  SOURCES bench/ComposeBrushBench.cpp)

# Pattern bakes and fills, SDF materials against paths, layer-style shadows.
_compose_bench(compose_paint_bench
  LIBRARIES SigilComposeBrush
  SOURCES bench/ComposePaintBench.cpp)
