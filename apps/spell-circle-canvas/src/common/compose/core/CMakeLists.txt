# SigilComposeCore — the kernel.
#
# Holds the element value and its authoring grammar, the keyed reconciler,
# Yoga layout with SigilWeave-measured text, stacking paint with automatic
# picture caching of provably-static subtrees, transitions and bindings, the
# composer's queries, the material value, the streaming feed and the
# instanced sprite leaf. Dressed
# type, the stroke grammar and the mask gates reach the kernel through the
# seam values a description carries (TextPainter, StrokeResolver,
# MaskResolver); their engines are the typography and brush tiers'. The
# reconciler that drives the retained tree is SigilCoreReconcile's, and the
# settled-subtree proof its caches turn on is SigilCoreCache's, with this
# target as the host of both. Links SigilCoreCache, SigilCoreComparable,
# SigilCoreCompute, SigilCoreReconcile, SigilGeometryPath, SigilImage,
# SigilMotion, SigilSkiaDraw, SigilWeave and Skia publicly, because those
# types appear in its headers — the seam value's erasure, the glyph mixer
# and the direct atlas draw the instanced leaf stamps through among them,
# each named by the header that spells it rather than reached through a
# neighbour that happens to link it; Yoga is private, because no header
# spells it.
#
# The translation units that see this directory — ComposeInternal.h,
# ComposeRuntime.h and the paint-phase internals — are this target's and
# the two engines' that implement its seams (typography/ and brush/); the
# include path is PRIVATE to each. Every other target in this tree compiles
# against the public headers alone.
set(_shaders ${CMAKE_CURRENT_LIST_DIR}/shaders)

sigil_library(SigilComposeCore
  SOURCES
    # the element value and its builders, one subject each
    ElementLayout.cpp ElementShape.cpp ElementMask.cpp ElementPaint.cpp
    ElementMarks.cpp ElementTransform.cpp ElementText.cpp
    ElementIdentity.cpp ElementImage.cpp Factories.cpp Region.cpp
    Effects.cpp Diagnostics.cpp
    # the composer and its phases
    Composer.cpp Reconcile.cpp ReconcileHost.cpp Layout.cpp Derive.cpp
    Transitions.cpp Query.cpp
    # the paint phase
    Volatility.cpp Bounds.cpp Depth.cpp StackingPainter.cpp Coverage.cpp
    # text
    TextPainter.cpp
    # the instanced leaf
    Instances.cpp
    # paint values
    Fills.cpp
    # internal headers
    ComposeInternal.h ComposeRuntime.h Instance.h SlotSpecs.h Lanes.h
    Transforms.h ComposerImpl.h PaintInternal.h
  HEADERS ../Compose.h Composer.h Core.h Derive.h Element.h Factories.h
          Feed.h Instances.h Layout.h Mask.h Measure.h Paint.h Pattern.h
          Shape.h Stroke.h Table.h TextPainter.h Tiles.h
  INCLUDE_PRIVATE ${CMAKE_CURRENT_LIST_DIR}
  PUBLIC SigilCoreCache SigilCoreComparable SigilCoreCompute
         SigilCoreReconcile SigilGeometryPath SigilImage SigilMaterial
         SigilMeasure SigilMotion SigilSkiaDraw SigilWeave Boost::unordered
         unofficial::skia::skia
  PRIVATE Boost::container yoga::yogacore)
sigil_shader_sources(SigilComposeCore DIR ${_shaders} NAMESPACE sigil::compose)

# The kernel: elements, the reconciler, layout, paint, transitions, text,
# the feed, the instanced leaf, masks and the field walks. Links Core
# ALONE, which is the proof that the kernel stands without its catalogs.
# The field walks include ComposeInternal.h AND ComposeRuntime.h (the
# only test TU that does either) so the equality comparators and the
# kSlotSpecs table can be reached DIRECTLY, which is why this binary
# alone sees the library's source directory. yoga::yogacore is PRIVATE to
# Core, so its headers are not on this target's path by inheritance — and
# ComposeRuntime.h includes <yoga/Yoga.h>. Listed for the INCLUDE PATH;
# the archive itself was already in the link closure.
sigil_test(compose_core_test
  SOURCES test/ComposeTestKernel.cpp test/ComposeTestMask.cpp
          test/ComposeTestFieldPins.cpp test/ComposeTestContentCore.cpp
          test/ComposeTestDepth.cpp test/ComposeTestInstances.cpp
  SUPPORT_DIRS ${CMAKE_CURRENT_LIST_DIR}
  LIBRARIES SigilComposeCore SigilComposeTesting SigilWeavePorts
            yoga::yogacore)

# The kernel: describe, reconcile, layout, query, caching, and the
# whole-scene scoreboard, blur, instances and particle arms.
sigil_bench(compose_core_bench
  SOURCES bench/ComposeCoreBench.cpp bench/ComposeCoreSceneBench.cpp
  LIBRARIES SigilComposeCore EnTT::EnTT SigilWeavePorts
  GPU)
