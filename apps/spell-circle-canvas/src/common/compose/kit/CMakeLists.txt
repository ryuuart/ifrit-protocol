# SigilComposeKit — the KIT tier as its own library.
#
# The kit plugs public seams and changes no kernel semantics. This target
# makes that STRUCTURAL rather than a convention: it links the Shape and
# Typography tiers, so it sees their PUBLIC headers, and nothing adds the
# compose source root to its include path — so ComposeInternal.h and
# ComposeRuntime.h are unreachable from kit code. BoundaryProbe.cpp is the
# negative control for exactly that (expected to fail; see the file).
#
# Six kit headers live with other tiers instead, because their values are
# spelled in those tiers' types: `kit/Strokes.h` (the stroke grammar),
# `kit/Plate.h` (the bordered feed plate), `kit/Ornament.h` and
# `kit/Flourish.h` (the pieces a manuscript border is made of) with Brush,
# `kit/Legibility.h` with Typography and `kit/Placers.h` with Instances.
# Include them directly.
add_library(SigilComposeKit STATIC
  Kit.cpp
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/kit/Divisions.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/kit/Frame.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/kit/Instruments.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/kit/Kit.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/kit/PixelType.h
)

target_compile_features(SigilComposeKit PUBLIC cxx_std_20)

# PUBLIC (not PRIVATE): anything that links the kit gets the tiers its values
# are expressed in — silhouettes and placement, type styles — with it.
target_link_libraries(SigilComposeKit PUBLIC
  SigilComposeShape SigilComposeTypography)

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
# with Brush for the plate.
_compose_test_binary(compose_studio_test
  LIBRARIES SigilComposeCore SigilComposeKit SigilComposeBrush
  SOURCES
    test/ComposeTestQueriesStudio.cpp
    test/ComposeTestContentStudio.cpp
)
