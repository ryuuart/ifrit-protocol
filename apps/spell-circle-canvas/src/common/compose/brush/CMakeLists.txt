# SigilComposeBrush — what draws along an outline.
#
# Decorations (strokes, shadows, washes, contour walks), the line vocabulary
# (`lines::` as lines, rails and hatches), the brush engine (the layered
# stack, the geometry ops and the `brush::` kinds) and the layer styles
# (`styles::` — inner shadows, glows, bevels and the gel and chrome
# bundles over SigilMaterial's kit tables), with the kit's stroke grammar
# (`kit/Strokes.h`) and the bordered feed plate (`kit/Plate.h`) that are
# spelled in this tier's types. Links Shape publicly: a brush is applied
# to a silhouette.
add_library(SigilComposeBrush STATIC
  Decorations.cpp
  LayerStyles.cpp
  Lines.cpp
  Brushes.cpp
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/brush/Brush.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/brush/Brushes.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/brush/Decorations.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/brush/GeometryOps.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/brush/Hatches.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/brush/LayerStyles.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/brush/Layered.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/brush/Lines.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/brush/Rails.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/kit/Plate.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/kit/Strokes.h
)
target_compile_features(SigilComposeBrush PUBLIC cxx_std_20)
target_link_libraries(SigilComposeBrush PUBLIC SigilComposeShape
                                              SigilComposeTypography)

# Decorations, lines, rails, hatches, brushes, the stroke grammar (which is
# what the authoring-grammar file exercises), the kit's stroke presets, the
# brushed masks and the stamped and stroked slices of the other catalogs.
_compose_test_binary(compose_brush_test
  LIBRARIES SigilComposeBrush
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
  LIBRARIES SigilComposeBrush
  SOURCES
    test/ComposeTestBrushesPaint.cpp
    test/ComposeTestContentPaint.cpp
    test/ComposeTestEngineDocsPaint.cpp
    test/ComposeTestGrammarPaint.cpp
    test/ComposeTestKernelPaint.cpp
    test/ComposeTestLinesPaint.cpp
    test/ComposeTestMaskPaint.cpp
    test/ComposeTestTextDataPaint.cpp
)

# Masks, brushes, profiled and span strokes, stamps, warps, hatches and
# the binding cost under stroked cells.
_compose_bench(compose_brush_bench
  LIBRARIES SigilComposeBrush
  SOURCES bench/ComposeBrushBench.cpp)

# Pattern bakes and fills, SDF materials against paths, layer-style shadows.
_compose_bench(compose_paint_bench
  LIBRARIES SigilComposeBrush
  SOURCES bench/ComposePaintBench.cpp)
