find_package(EnTT CONFIG REQUIRED)
find_package(boost_container CONFIG REQUIRED)
find_package(boost_unordered CONFIG REQUIRED)
find_package(unofficial-skia CONFIG REQUIRED)
find_package(yoga CONFIG REQUIRED)
sigil_library(SigilComposeCore
  SOURCES
    # immutable descriptions, comparable values and storage
    description/Attributes.cpp
    description/Calc.cpp
    description/Calc.h
    description/ComposeCompare.h
    description/ComposeInternal.h
    description/DeclaredFields.h
    description/Diagnostics.cpp
    description/Dimensions.cpp
    description/ElementNode.cpp
    description/Factories.cpp
    description/Fills.cpp
    description/HeapBox.h
    description/Operator.cpp
    description/Properties.cpp
    description/PropertyBlocks.h
    description/Region.cpp
    # the fluent authoring verbs
    description/verbs/ElementBand.cpp
    description/verbs/ElementBox.cpp
    description/verbs/ElementCascade.cpp
    description/verbs/ElementDepth.cpp
    description/verbs/ElementEffects.cpp
    description/verbs/ElementFlex.cpp
    description/verbs/ElementFont.cpp
    description/verbs/ElementIdentity.cpp
    description/verbs/ElementImage.cpp
    description/verbs/ElementMarks.cpp
    description/verbs/ElementMask.cpp
    description/verbs/ElementPaint.cpp
    description/verbs/ElementPlacement.cpp
    description/verbs/ElementShape.cpp
    description/verbs/ElementText.cpp
    description/verbs/ElementTextStyle.cpp
    description/verbs/ElementTransform.cpp
    # selector matching, cascade and inherited style resolution
    style/Cascade.cpp
    style/CascadeDiagnostics.cpp
    style/ComputedStyle.h
    style/FaceChoice.cpp
    style/FaceChoice.h
    style/FamilyList.cpp
    style/FamilyList.h
    style/LonghandFold.cpp
    style/LonghandFold.h
    style/NamedStyles.cpp
    style/RuleMarks.cpp
    style/RuleMarks.h
    style/Selector.cpp
    style/SelectorInternal.h
    style/SelectorMatch.cpp
    style/SelectorMatch.h
    style/SelectorParse.cpp
    style/SheetBody.h
    style/StyleSheet.cpp
    # retained ownership, reconciliation, transitions and queries
    runtime/ComposeRuntime.h
    runtime/Composer.cpp
    runtime/ComposerImpl.h
    runtime/Instance.h
    runtime/Lanes.h
    runtime/NodeTransform.h
    runtime/Query.cpp
    runtime/Reconcile.cpp
    runtime/ReconcileEquality.cpp
    runtime/ReconcileHost.cpp
    runtime/SceneLighting.cpp
    runtime/SlotSpecs.h
    runtime/TextState.h
    runtime/Transforms.h
    runtime/Transitions.cpp
    # measurement, arrangement and derived geometry
    layout/Additions.cpp
    layout/CellFlow.cpp
    layout/Derive.cpp
    layout/DeriveInternal.h
    layout/FlowAround.cpp
    layout/Grid.cpp
    layout/Layout.cpp
    layout/LayoutProps.cpp
    layout/LayoutText.cpp
    layout/MaterializeText.cpp
    layout/Measure.cpp
    layout/Rects.cpp
    layout/Routes.cpp
    layout/Shelf.cpp
    layout/Threads.cpp
    # placement, stacking and content drawing
    paint/Bounds.cpp
    paint/BakeFormat.h
    paint/Coverage.cpp
    paint/Depth.cpp
    paint/FillLowering.h
    paint/GlyphInk.h
    paint/InstanceStamp.h
    paint/Instances.cpp
    paint/LitSurface.cpp
    paint/MaterialEffects.cpp
    paint/MaterialEffects.h
    paint/PaintContent.cpp
    paint/PaintInternal.h
    paint/PaintPass.cpp
    paint/PaintPass.h
    paint/PaintProfile.cpp
    paint/PaintProfile.h
    paint/PenOnCanvas.h
    paint/RecordingState.h
    paint/StackingPainter.cpp
    paint/TextInk.cpp
    paint/TextPainter.cpp
    paint/Tiles.cpp
    paint/WorldMatrix.cpp
    # cache validity, recordings, bakes and resource stores
    cache/GroupBake.cpp
    cache/GroupScalars.cpp
    cache/Pattern.cpp
    cache/PictureTier.cpp
    cache/PromotionTier.cpp
    cache/SplitBake.cpp
    cache/StampCache.h
    cache/TextureBake.cpp
    cache/Volatility.cpp
  HEADERS ../Compose.h Band.h Cascade.h Composer.h Core.h Declarations.h
          Derive.h Element.h
          Factories.h Grid.h Image.h Instances.h Layout.h Mask.h
          Measure.h
          Paint.h Pattern.h Property.h Selector.h Shape.h Shelf.h
          FontStyle.h SpanStyle.h Stroke.h
          StyleSheet.h Table.h
          Text.h TextPainter.h Tiles.h Utf8.h Var.h
          verbs/Box.h verbs/Cascade.h verbs/Decoration.h verbs/Depth.h
          verbs/Effects.h verbs/Flex.h verbs/Mask.h verbs/Paint.h
          verbs/Placement.h verbs/Shape.h verbs/TextStyle.h
          verbs/Transform.h
  INCLUDE_PRIVATE ${CMAKE_CURRENT_LIST_DIR}
  # Public links supply the types named by the authoring vocabulary.
  PUBLIC SigilCoreCache SigilCoreCallable SigilCoreComparable SigilCoreCompute
         SigilCoreReconcile SigilGeometryPath SigilMediaCore
         SigilMaterialColor SigilMaterialCore SigilMaterialTexture
         SigilMaterialPattern SigilMaterialSkia
         SigilMeasure SigilMotionClock SigilMotionValues SigilMotionEase
         SigilMotionSchedule SigilMotionTime
         SigilSkiaDraw SigilWeaveStyle SigilWeaveFonts SigilWeaveParagraph
         SigilWeaveLayout SigilWeaveQuery SigilDraw
         unofficial::skia::skia
  PRIVATE SigilWeaveUnicode SigilWeaveChoreograph SigilWeavePaint
          Boost::container Boost::unordered yoga::yogacore)
sigil_shader_sources(SigilComposeCore
  DIR ${CMAKE_CURRENT_LIST_DIR}/shaders NAMESPACE sigil::compose)

# Internal-state tests need private kernel and Yoga headers. Grain fixtures
# link their field generator directly rather than adding it to the kernel.
sigil_test(compose_test
  SOURCES test/ComposeTestLayout.cpp test/ComposeTestGrid.cpp
          test/ComposeTestContent.cpp
          test/ComposeTestReconcile.cpp test/ComposeTestCaching.cpp
          test/ComposeTestPromotion.cpp test/ComposeTestMotion.cpp
          test/ComposeTestBakeReuse.cpp test/ComposeTestBakeFormat.cpp
          test/ComposeTestGroupSettling.cpp
          test/ComposeTestFlowAroundCore.cpp
          test/ComposeTestPromotionBounds.cpp
          test/ComposeTestPromotionGate.cpp
          test/StandardExceptionsTest.cpp
          test/ComposeTestEffects.cpp test/ComposeTestMaterial.cpp
          test/ComposeTestLiveMaterial.cpp test/ComposeTestWorldSpace.cpp
    test/ComposeTestLighting.cpp
    test/ComposeTestSceneLighting.cpp
          test/ComposeTestMask.cpp test/ComposeTestFieldPins.cpp
          test/ComposeTestDeclareDoor.cpp
          test/ComposeTestSlotConsumers.cpp
          test/ComposeTestDepth.cpp
          test/ComposeTestInstances.cpp test/ComposeTestOperators.cpp
          test/ComposeTestShelf.cpp test/ComposeTestTable.cpp
          test/ComposeTestValues.cpp test/ComposeTestParagraphLane.cpp test/ComposeTestCascade.cpp
          test/ComposeTestRoles.cpp test/ComposeTestFlexBox.cpp
          test/ComposeTestEdges.cpp test/ComposeTestLengths.cpp
          test/ComposeTestRem.cpp
          test/ComposeTestFillSurfaces.cpp
          test/ComposeTestPatternSampling.cpp
          test/ComposeTestRootBox.cpp test/ComposeTestCenterPins.cpp
          test/ComposeTestDeclarations.cpp
          test/ComposeTestSelector.cpp test/ComposeTestStyleSheet.cpp
          test/ComposeTestMatching.cpp test/ComposeTestHas.cpp
          test/ComposeTestTypedLeaves.cpp test/ComposeTestRuleScope.cpp
          test/ComposeTestLonghandValues.cpp test/ComposeTestTextIndent.cpp
  SUPPORT_DIRS ${CMAKE_CURRENT_LIST_DIR}
  LIBRARIES SigilComposeCore SigilComposeTesting SigilMaterialField SigilWeavePorts
            yoga::yogacore
  LABELS fonts
  SUITES ComposeFaces ComposeFontFamily ComposeFontStyle)

sigil_bench(compose_bench
  SOURCES bench/ComposeCoreBench.cpp bench/ComposeCoreScoreboardBench.cpp
          bench/ComposeCoreEffectBench.cpp bench/ComposeCoreBatchBench.cpp
          bench/ComposeCoreSceneBench.cpp
  LIBRARIES SigilComposeCore EnTT::EnTT SigilWeavePorts
  GPU_LIBRARIES SigilSkia)

# Moving-image fixtures encode their own clips.
sigil_test(compose_test
  SOURCES test/ComposeTestMovingImage.cpp
  LIBRARIES SigilComposeCore SigilMediaVideoDecode SigilMediaVideoEncode
            SigilWeavePorts)
