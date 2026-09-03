# SigilComposeKit — the KIT tier as its own library.
#
# The kit plugs public seams and changes no kernel semantics. This target
# makes that STRUCTURAL rather than a convention: it links the kernel and
# the Typography and Brush tiers, so it sees their PUBLIC headers, and nothing adds
# the compose source root to its include path — so ComposeInternal.h and
# ComposeRuntime.h are unreachable from kit code. BoundaryProbe.cpp is the
# negative control for exactly that (expected to fail; see the file).
#
# The shelves of stock values are here: WHERE a child lands (`layouts::`),
# WHAT JOINS two of them (`routers::`), HOW a glyph deviates from rest
# (the `fx::` presets over the typography tier's effect seam, with the
# marquee, in kit/Kinetic.h), and the two outline adaptors that only mean
# anything against a node (`shapes::onEdges`, `shapes::inset`).
# The silhouette generators themselves are the geometry library's, spelled
# under `shapes::` by `kit/Silhouettes.h`, because a `path(SkSize)` value
# needs nothing of a component tree.
#
# Five kit headers live with other tiers instead, because their values are
# spelled in those tiers' types: `kit/Strokes.h` (the stroke grammar),
# `kit/Plate.h` (the bordered feed plate), `kit/Ornament.h` and
# `kit/Flourish.h` (the pieces a manuscript border is made of) with Brush,
# and `kit/Legibility.h` with Typography. Include them directly.
add_library(SigilComposeKit STATIC
  Eras.cpp
  Kit.cpp
  Routers.cpp
  Silhouettes.cpp
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/kit/Chrome.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/kit/Frame.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/kit/Gel.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/kit/Gloss.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/kit/Annotations.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/kit/Typeset.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/kit/Instruments.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/kit/Kinetic.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/kit/Kit.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/kit/Layouts.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/kit/PixelType.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/kit/Placers.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/kit/Routers.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/kit/Silhouettes.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/kit/Sprites.h
)

target_compile_features(SigilComposeKit PUBLIC cxx_std_20)

# PUBLIC (not PRIVATE): anything that links the kit gets the tiers its values
# are expressed in — the kernel, type styles, the brush tier whose
# decorations the era looks are built from, and the geometry library's
# silhouette shelf — with it, and the libraries the kit headers include
# themselves: the material tables and paints the chrome, gel and gloss
# looks are drawn with, the type style a typeset takes, and the seeded
# mixer a layout scatters with. The arrow points one way: this tier links
# Brush, and Brush links nothing here.
target_link_libraries(SigilComposeKit PUBLIC
  SigilComposeCore SigilComposeTypography SigilComposeBrush SigilGeometryKit
  SigilCoreCompute SigilMaterial SigilWeave)

# The negative control: a target that must FAIL to build, and a test that
# builds it and demands the one message that says the boundary held. The
# check cannot live inside a gtest binary — a TU that does not compile
# cannot also run assertions — and it cannot be an exit status either,
# since any breakage would then read as a pass.
include(BoundaryProbe)
sigil_boundary_probe(
  TARGET compose_kit_boundary_probe
  SOURCES BoundaryProbe.cpp
  LIBRARIES SigilComposeKit
  EXPECT "'ComposeInternal.h' file not found")

# The kit tier's tests. Separate binary rather than more cases in the
# kernel binaries: the kit changes nothing in the kernel, so a kit failure
# must never look like a kernel failure on the dashboard. Links the kit
# ALONE — the stroke-grammar cases, which need the Brush tier, are the
# brush binary's.
add_executable(compose_kit_test
  test/ComposeKitTest.cpp
)
target_include_directories(compose_kit_test PRIVATE
  ${SIGIL_COMPOSE_TEST_SUPPORT_DIR})
target_link_libraries(compose_kit_test PRIVATE
  SigilComposeKit SigilComposeTesting SigilWeavePorts GTest::gtest_main)
add_test(NAME compose_kit_test COMMAND compose_kit_test)

# Composer queries, the studio, the feed plate and the instruments that
# read a cascade back — the kit's instruments over the kernel's queries,
# with Brush for the plate and SigilMeasure for the checks a plate reports.
_compose_test_binary(compose_studio_test
  LIBRARIES SigilComposeCore SigilComposeKit SigilComposeBrush SigilMeasure
  SOURCES
    test/ComposeTestQueriesStudio.cpp
    test/ComposeTestContentStudio.cpp
)

# Silhouettes over the kernel's shape seam, the layout schemes, the
# routers, the placers that fill an instanced leaf's pool, rails and
# travel along a path — the shelves this tier adds.
_compose_test_binary(compose_shape_test
  LIBRARIES SigilComposeKit SigilComposeBrush
  SOURCES
    test/ComposeTestContentShape.cpp
    test/ComposeTestKernelShape.cpp
    test/ComposeTestLinesShape.cpp
    test/ComposeTestPlacers.cpp
)

# Comparable silhouettes, hit testing and blended shaped leaves.
_compose_bench(compose_shape_bench
  LIBRARIES SigilComposeKit
  SOURCES bench/ComposeShapeBench.cpp)
