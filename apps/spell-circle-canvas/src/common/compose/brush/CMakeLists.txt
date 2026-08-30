# SigilComposeBrush — what draws along an outline.
#
# Decorations (strokes, shadows, washes, contour walks), the line vocabulary
# (`lines::` as lines, rails and hatches) and the brush engine (the layered
# stack, the geometry ops and the `brush::` kinds), with the kit's stroke
# grammar (`kit/Strokes.h`) and the bordered feed plate (`kit/Plate.h`)
# that are spelled in this tier's types. Links Shape publicly: a brush is
# applied to a silhouette.
add_library(SigilComposeBrush STATIC
  Decorations.cpp
  Lines.cpp
  Brushes.cpp
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/brush/Brush.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/brush/Brushes.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/brush/Decorations.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/brush/GeometryOps.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/brush/Hatches.h
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

# Masks, brushes, profiled and span strokes, stamps, warps, hatches and
# the binding cost under stroked cells.
_compose_bench(compose_brush_bench
  LIBRARIES SigilComposeBrush
  SOURCES bench/ComposeBrushBench.cpp)
