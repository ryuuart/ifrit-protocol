# SigilComposeCore — the kernel.
#
# Holds the element value and its authoring grammar, the keyed reconciler,
# Yoga layout with SigilWeave-measured text, stacking paint with automatic
# picture caching of provably-static subtrees, transitions and bindings, the
# per-glyph text seam, the composer's queries, the material value and the
# streaming feed. Links SigilGeometryPath, SigilImage, SigilMotion,
# SigilWeave and Skia publicly, because those types appear in its headers;
# Yoga is private, because no header spells it.
#
# This is the ONLY target whose translation units see this directory —
# ComposeInternal.h, ComposeRuntime.h and the paint-phase internals — and
# that include path is PRIVATE. Every other target in this tree, including
# the tiers above, compiles against the public headers alone.
add_library(SigilComposeCore STATIC
  # the element value and its builders
  Element.cpp
  RichText.cpp
  Masks.cpp
  Effects.cpp
  SpanNormalForm.cpp
  Band.cpp
  Diagnostics.cpp
  Env.cpp
  # the composer and its phases
  Composer.cpp
  Reconcile.cpp
  Layout.cpp
  Derive.cpp
  Transitions.cpp
  Query.cpp
  # the paint phase
  Volatility.cpp
  StrokePainting.cpp
  PathText.cpp
  TextFxPainting.cpp
  TextMarks.cpp
  Bounds.cpp
  StackingPainter.cpp
  # text
  TextFx.cpp
  TextAtRest.cpp
  # paint values
  Material.cpp
  # internal headers
  ComposeInternal.h
  ComposeRuntime.h
  Instance.h
  SlotSpecs.h
  Transforms.h
  AxisGate.h
  ComposerImpl.h
  PaintInternal.h
  SpanContours.h
  # public headers
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/Compose.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/Composer.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/Core.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/Derive.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/Element.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/Env.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/Factories.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/Feed.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/GpuImage.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/Layout.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/Mask.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/Material.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/Measure.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/Motion.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/Paint.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/Pattern.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/Patterns.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/Sdf.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/Shape.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/Stroke.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/Text.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/core/Tiles.h
)
target_compile_features(SigilComposeCore PUBLIC cxx_std_20)
target_include_directories(SigilComposeCore
  PUBLIC ${SIGIL_COMPOSE_INCLUDE_DIR}
  PRIVATE ${CMAKE_CURRENT_LIST_DIR}
)
target_link_libraries(SigilComposeCore
  PUBLIC
    SigilGeometryPath
    SigilImage
    SigilMaterial
    SigilMeasure
    SigilMotion
    SigilWeave
    unofficial::skia::skia
  PRIVATE
    yoga::yogacore
)

# The kernel: elements, the reconciler, layout, paint, transitions, text,
# the feed, masks and the field walks. Links Core ALONE, which is the proof
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
  LIBRARIES SigilComposeCore SigilComposeInstances EnTT::EnTT
  SOURCES bench/ComposeCoreBench.cpp bench/ComposeCoreSceneBench.cpp)
