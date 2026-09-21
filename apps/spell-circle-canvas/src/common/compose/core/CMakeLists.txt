# SigilComposeCore — the kernel.
#
# Holds the element value and its authoring grammar, the keyed reconciler,
# Yoga layout with SigilWeave-measured text, stacking paint with automatic
# picture caching of provably-static subtrees, transitions and bindings, the
# composer's queries, the streaming feed and the instanced sprite leaf.
# Dressed
# type, the stroke grammar and the mask gates reach the kernel through the
# seam values a description carries (TextPainter, StrokeResolver,
# MaskResolver); their engines are the typography and brush tiers'. The
# reconciler that drives the retained tree is SigilCoreReconcile's, and the
# settled-subtree proof its caches turn on is SigilCoreCache's, with this
# target as the host of both. Links SigilCoreCache, SigilCoreCallable,
# SigilCoreComparable, SigilCoreCompute, SigilCoreReconcile,
# SigilGeometryPath, SigilImage, SigilMotion, SigilSkiaDraw, SigilWeave and
# Skia publicly, because those
# types appear in its headers — the seam value's erasure, the paint value
# a fill carries, the prefix call a paint program is taken through, the
# glyph mixer and the direct atlas draw the instanced leaf stamps through
# among them, each named by the header that spells it rather than reached
# through a neighbour that happens to link it;
# SigilMaterial and SigilMeasure are public for the same reason. Yoga is
# private, because no header spells it.
#
# The translation units that see this directory — ComposeInternal.h,
# ComposeRuntime.h and the paint-phase internals — are this target's and
# the two engines' that implement its seams (typography/ and brush/); the
# include path is PRIVATE to each. Every other target in this tree compiles
# against the public headers alone.
set(_shaders ${CMAKE_CURRENT_LIST_DIR}/shaders)

find_package(EnTT CONFIG REQUIRED)
find_package(boost_container CONFIG REQUIRED)
find_package(boost_unordered CONFIG REQUIRED)
find_package(unofficial-skia CONFIG REQUIRED)
find_package(yoga CONFIG REQUIRED)
sigil_library(SigilComposeCore
  SOURCES
    # the element value and its builders, one verb family each
    ElementBox.cpp ElementFlex.cpp ElementPlacement.cpp
    ElementShape.cpp ElementBand.cpp ElementMask.cpp
    ElementPaint.cpp ElementEffects.cpp ElementMarks.cpp
    ElementTransform.cpp ElementDepth.cpp
    ElementTextStyle.cpp ElementText.cpp
    ElementIdentity.cpp ElementImage.cpp Factories.cpp Region.cpp
    ElementNode.cpp Diagnostics.cpp CellFlow.cpp Grid.cpp
    ElementCascade.cpp
    # which elements a rule speaks about, the CSS text that says so,
    # and the rules a sheet states about them
    Selector.cpp SelectorParse.cpp StyleSheet.cpp SelectorMatch.cpp
    # the composer and its phases
    Composer.cpp Measure.cpp Tiles.cpp Cascade.cpp
    Reconcile.cpp ReconcileEquality.cpp MaterializeText.cpp
    LayoutProps.cpp ReconcileHost.cpp Layout.cpp LayoutText.cpp Rects.cpp
    Derive.cpp FlowAround.cpp Threads.cpp Routes.cpp
    Transitions.cpp Query.cpp
    # the paint phase
    Volatility.cpp GroupScalars.cpp WorldMatrix.cpp Bounds.cpp Depth.cpp
    Coverage.cpp
    # the paint of one node: the walk, its shared state, and the cache
    # ladder it dispatches through, one tier per file
    StackingPainter.cpp PaintContent.cpp PaintPass.cpp PaintProfile.cpp
    PromotionTier.cpp SplitBake.cpp GroupBake.cpp TextureBake.cpp
    PictureTier.cpp
    # text
    TextPainter.cpp
    # the instanced leaf
    Instances.cpp Shelf.cpp
    # paint values
    Fills.cpp
    # internal headers
    ComposeInternal.h ComposeCompare.h ComposeRuntime.h Instance.h
    SelectorInternal.h SelectorMatch.h
    TextState.h SlotSpecs.h
    Lanes.h
    Transforms.h NodeTransform.h ComposerImpl.h DeriveInternal.h
    PaintInternal.h PaintPass.h
    PaintProfile.h InstanceStamp.h
  HEADERS ../Compose.h Band.h Cascade.h Composer.h Core.h Declarations.h
          Derive.h Element.h
          Factories.h Feed.h Grid.h Image.h Instances.h Layout.h Mask.h
          Measure.h
          Paint.h Pattern.h Selector.h Shape.h Shelf.h Stroke.h
          StyleSheet.h SurfacePaint.h Table.h
          Text.h TextPainter.h Tiles.h Utf8.h Var.h
          verbs/Box.h verbs/Cascade.h verbs/Decoration.h verbs/Depth.h
          verbs/Effects.h verbs/Flex.h verbs/Mask.h verbs/Paint.h
          verbs/Placement.h verbs/Shape.h verbs/TextStyle.h
          verbs/Transform.h
  INCLUDE_PRIVATE ${CMAKE_CURRENT_LIST_DIR}
  PUBLIC SigilCoreCache SigilCoreCallable SigilCoreComparable SigilCoreCompute
         SigilCoreReconcile SigilGeometryPath SigilImage SigilMaterial
         SigilMeasure SigilMotion SigilSkiaDraw SigilWeave
         unofficial::skia::skia
  PRIVATE Boost::container Boost::unordered yoga::yogacore)
sigil_shader_sources(SigilComposeCore DIR ${_shaders} NAMESPACE sigil::compose)

# The kernel: elements, the reconciler, layout, paint, transitions, text,
# the feed, the instanced leaf, masks and the field walks. Links Core
# ALONE, which is the proof that the kernel stands without its catalogs.
# The field walks include ComposeInternal.h AND ComposeRuntime.h (the
# only test TU that does either) so the equality comparators and the
# kSlotSpecs table can be reached DIRECTLY, which is why this binary
# alone sees the library's source directory. yoga::yogacore is
# PRIVATE to Core, so its headers are not on this target's path by
# inheritance — and ComposeRuntime.h includes <yoga/Yoga.h>. Listed for
# the INCLUDE PATH; the archive itself was already in the link closure.
sigil_test(compose_test
  SOURCES test/ComposeTestLayout.cpp test/ComposeTestGrid.cpp
          test/ComposeTestContent.cpp
          test/ComposeTestReconcile.cpp test/ComposeTestCaching.cpp
          test/ComposeTestPromotion.cpp test/ComposeTestMotion.cpp
          test/ComposeTestBakeReuse.cpp test/ComposeTestFeed.cpp
          test/ComposeTestFlowAroundCore.cpp test/ComposeTestEdgeStore.cpp
          test/ComposeTestPromotionBounds.cpp
          test/ComposeTestPromotionGate.cpp
          test/StandardExceptionsTest.cpp
          test/ComposeTestEffects.cpp test/ComposeTestMaterial.cpp
          test/ComposeTestLiveMaterial.cpp test/ComposeTestWorldSpace.cpp
          test/ComposeTestMask.cpp test/ComposeTestFieldPins.cpp
          test/ComposeTestDepth.cpp
          test/ComposeTestInstances.cpp test/ComposeTestTether.cpp
          test/ComposeTestShelf.cpp test/ComposeTestTable.cpp
          test/ComposeTestValues.cpp test/ComposeTestBlockLane.cpp test/ComposeTestCascade.cpp
          test/ComposeTestRoles.cpp test/ComposeTestFlexBox.cpp
          test/ComposeTestEdges.cpp
          test/ComposeTestSelector.cpp test/ComposeTestStyleSheet.cpp
          test/ComposeTestMatching.cpp test/ComposeTestTypedLeaves.cpp
  SUPPORT_DIRS ${CMAKE_CURRENT_LIST_DIR}
  LIBRARIES SigilComposeCore SigilComposeTesting SigilWeavePorts
            yoga::yogacore
  LABELS fonts
  SUITES ComposeFaces)

# The kernel: describe, reconcile, layout, query and caching in the
# first file, then one whole-scene file per subject — the scoreboard,
# the effects, the batches that stand for many items, and the two scenes
# that price a bake against drawing the same picture again.
sigil_bench(compose_bench
  SOURCES bench/ComposeCoreBench.cpp bench/ComposeCoreScoreboardBench.cpp
          bench/ComposeCoreEffectBench.cpp bench/ComposeCoreBatchBench.cpp
          bench/ComposeCoreSceneBench.cpp
  LIBRARIES SigilComposeCore EnTT::EnTT SigilWeavePorts
  GPU_LIBRARIES SigilSkia)
