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
#
# Sees the kernel's source directory PRIVATELY, as the typography engine
# does: resolving a span claim reads the retained Instance. What the
# headers include beyond the three tiers is PUBLIC: the image asset a
# decoration stamps, the material paint and layer-style tables a look is
# built from, the direct draws, and the type style a plate is set in. The
# seeded mixer a brush jitters with is read in the sources alone.
sigil_library(SigilComposeBrush
  SOURCES Adaptors.cpp Decorations.cpp LayerStyles.cpp Lines.cpp
          PixelStyles.cpp Brushes.cpp
          # the stroke grammar's engine and the mask gates, behind the
          # kernel's StrokeResolver and MaskResolver seams
          StrokePainting.cpp SpanNormalForm.cpp Band.cpp Masks.cpp
          SpanArithmetic.h SpanContours.h
  HEADERS Adaptors.h Brush.h Brushes.h Decorations.h GeometryOps.h Hatches.h
          LayerStyles.h Layered.h Lines.h PixelStyles.h Rails.h
          ../kit/Flourish.h ../kit/Ornament.h ../kit/Plate.h ../kit/Strokes.h
  INCLUDE_PRIVATE ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/../core
  PUBLIC SigilComposeCore SigilComposeTypography SigilGeometryKit SigilImage
         SigilMaterial SigilSkiaDraw SigilWeave
  PRIVATE SigilCoreCompute yoga::yogacore)

# The one brush binary, one file per subject: lines and rails, the brush
# kinds, the brush engine, the stroke grammar, decorations on a node and
# on text, stamps and strips, the mask gates, the paint values this tier
# spells over SigilMaterial, the pixel styles and the kit's stroke
# presets. One binary rather than two over the same two targets, because a
# second one states no boundary a consumer could read -- it only decides
# which half of a shared fixture a case can reach.
sigil_test(compose_brush_test
  SOURCES test/ComposeTestLines.cpp test/ComposeTestBrushes.cpp
          test/ComposeTestBrushEngine.cpp test/ComposeTestStrokeGrammar.cpp
          test/ComposeTestDecorations.cpp test/ComposeTestTextDecorations.cpp
          test/ComposeTestStampsAndStrips.cpp test/ComposeTestMasks.cpp
          test/ComposeTestPaintValues.cpp test/ComposeTestPixelStyles.cpp
          test/ComposeTestStrokePresets.cpp
  LIBRARIES SigilComposeBrush SigilComposeKit SigilComposeTesting
            SigilWeavePorts)

# Masks, brushes, profiled and span strokes, stamps, warps, hatches and
# the binding cost under stroked cells.
sigil_bench(compose_brush_bench
  SOURCES bench/ComposeBrushBench.cpp
  LIBRARIES SigilComposeBrush SigilComposeKit SigilWeavePorts)

# Pattern bakes and fills, SDF materials against paths, layer-style shadows.
sigil_bench(compose_paint_bench
  SOURCES bench/ComposePaintBench.cpp
  LIBRARIES SigilComposeBrush SigilWeavePorts)
