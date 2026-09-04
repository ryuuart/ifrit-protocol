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

add_library(SigilComposeCore STATIC
  # the element value and its builders, one subject each
  ElementLayout.cpp
  ElementShape.cpp
  ElementMask.cpp
  ElementPaint.cpp
  ElementMarks.cpp
  ElementTransform.cpp
  ElementText.cpp
  ElementIdentity.cpp
  ElementImage.cpp
  Factories.cpp
  Region.cpp
  Effects.cpp
  Diagnostics.cpp
  # the composer and its phases
  Composer.cpp
  Reconcile.cpp
  ReconcileHost.cpp
  Layout.cpp
  Derive.cpp
  Transitions.cpp
  Query.cpp
  # the paint phase
  Volatility.cpp
  Bounds.cpp
  Depth.cpp
  StackingPainter.cpp
  ${_shaders}/LumaCoverage.sksl
  Coverage.cpp
  # text
  TextPainter.cpp
  # the instanced leaf
  Instances.cpp
  # paint values
  Fills.cpp
  # internal headers
  ComposeInternal.h
  ComposeRuntime.h
  Instance.h
  SlotSpecs.h
  Lanes.h
  Transforms.h
  ComposerImpl.h
  PaintInternal.h
  # public headers
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/Compose.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/Composer.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/Core.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/Derive.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/Element.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/Factories.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/Feed.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/Instances.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/Layout.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/Mask.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/Measure.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/Paint.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/Pattern.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/Shape.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/Stroke.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/TextPainter.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/Tiles.h
)
target_compile_features(SigilComposeCore PUBLIC cxx_std_20)
target_include_directories(SigilComposeCore
  PUBLIC ${SIGIL_COMPOSE_INCLUDE_DIR}
  PRIVATE ${CMAKE_CURRENT_LIST_DIR}
)
target_compile_definitions(SigilComposeCore
  PRIVATE SIGIL_COMPOSE_SHADER_DIR="${_shaders}")
target_link_libraries(SigilComposeCore
  PUBLIC
    SigilCoreCache
    SigilCoreComparable
    SigilCoreCompute
    SigilCoreReconcile
    SigilGeometryPath
    SigilImage
    SigilMaterial
    SigilMeasure
    SigilMotion
    SigilSkiaDraw
    SigilWeave
    unofficial::skia::skia
  PRIVATE
    SigilIOHub
    yoga::yogacore
)

# The kernel: elements, the reconciler, layout, paint, transitions, text,
# the feed, the instanced leaf, masks and the field walks. Links Core
# ALONE, which is the proof
# that the kernel stands without its catalogs. The field walks include
# ComposeInternal.h AND ComposeRuntime.h (the only test TU that does
# either) so the equality comparators and the kSlotSpecs table can be
# reached DIRECTLY — see the file header — which is why this binary alone
# sees the library's source directory. yoga::yogacore is PRIVATE to Core,
# so its headers are not on this target's path by inheritance — and
# ComposeRuntime.h includes <yoga/Yoga.h>. Listed for the INCLUDE PATH; the
# archive itself was already in the link closure.
_compose_test_binary(compose_core_test
  LIBRARIES SigilComposeCore
  SOURCES
    test/ComposeTestKernel.cpp
    test/ComposeTestMask.cpp
    test/ComposeTestFieldPins.cpp
    test/ComposeTestContentCore.cpp
    test/ComposeTestDepth.cpp
    test/ComposeTestInstances.cpp
)
target_include_directories(compose_core_test PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(compose_core_test PRIVATE yoga::yogacore)

# The pre-implementation feasibility spike stays: it pins the
# Yoga+SigilWeave measurement contract independently of the kernel.
add_executable(compose_spike_test
  test/ComposeSpikeTest.cpp
)
target_link_libraries(compose_spike_test PRIVATE
  SigilWeave SigilWeavePorts yoga::yogacore GTest::gtest_main)
add_test(NAME compose_spike_test COMMAND compose_spike_test)

# The kernel: describe, reconcile, layout, query, caching, and the
# whole-scene scoreboard, blur, instances and particle arms.
_compose_bench(compose_core_bench GPU
  LIBRARIES SigilComposeCore EnTT::EnTT
  SOURCES bench/ComposeCoreBench.cpp bench/ComposeCoreSceneBench.cpp)
