find_package(yoga CONFIG REQUIRED)
sigil_library(SigilComposeBrush
  SOURCES Adaptors.cpp Decorations.cpp Lines.cpp Relief.cpp
          PixelStyles.cpp Ink.h
          Composites.cpp Stamps.cpp Ribbon.cpp Art.cpp BakedArt.h
          # Stroke and mask executors.
          StrokePainting.cpp SpanNormalForm.cpp Band.cpp Masks.cpp
          SpanArithmetic.h SpanContours.h
  HEADERS Adaptors.h Brush.h Brushes.h Decorations.h Hatches.h
          Ribbons.h Stamps.h ../advanced/PathEffect.h
          Layered.h Lines.h PixelStyles.h Rails.h Relief.h
          ../kit/Plate.h ../kit/Strokes.h
  # Span resolution reads retained kernel state through private seams.
  INCLUDE_PRIVATE ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/../core
  PUBLIC SigilComposeCore SigilComposeTypography SigilGeometryKit SigilMediaCore
         SigilMaterial SigilSkiaDraw SigilWeave
  PRIVATE SigilCoreCompute yoga::yogacore)

sigil_test(compose_test
  SOURCES test/ComposeTestLines.cpp test/ComposeTestBrushes.cpp
          test/ComposeTestBorders.cpp test/ComposeTestBrushEngine.cpp
          test/ComposeTestStrokeGrammar.cpp test/ComposeTestSpanWindows.cpp
          test/ComposeTestBands.cpp test/ComposeTestCrossings.cpp
          test/ComposeTestStrokeWords.cpp test/ComposeTestInkStroke.cpp
          test/ComposeTestDecorations.cpp test/ComposeTestTextDecorations.cpp
          test/ComposeTestSlices.cpp test/ComposeTestStamps.cpp
          test/ComposeTestStripTiles.cpp test/ComposeTestMasks.cpp
          test/ComposeTestSettledFill.cpp test/ComposeTestPatternPan.cpp
          test/ComposeTestPaintValues.cpp test/ComposeTestFields.cpp
          test/ComposeTestPixelStyles.cpp test/ComposeTestStyleEffects.cpp
          test/ComposeTestStrokePresets.cpp test/ComposeTestRelief.cpp
          test/ComposeTestReliefRibbon.cpp
          test/ComposeTestTextAndLayout.cpp test/ComposeTestPromotion.cpp
          test/ComposeTestTextureBake.cpp test/ComposeTestTemporalCache.cpp
          test/ComposeTestSplitBake.cpp test/ComposeTestGroupBake.cpp
          test/ComposeTestRuleMarks.cpp test/ComposeTestCascadePaints.cpp
  LIBRARIES SigilComposeBrush SigilComposeKit SigilComposeTesting
            SigilWeavePorts)

sigil_bench(compose_bench
  SOURCES bench/ComposeBrushBench.cpp bench/ComposePaintBench.cpp
  LIBRARIES SigilComposeBrush SigilComposeKit SigilWeavePorts)
