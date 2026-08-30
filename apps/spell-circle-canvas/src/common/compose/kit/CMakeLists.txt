# SigilComposeKit — the KIT tier as its own library.
#
# The kit plugs public seams and changes no kernel semantics. This target
# makes that STRUCTURAL rather than a convention: it links the Shape and
# Typography tiers, so it sees their PUBLIC headers, and nothing adds the
# compose source root to its include path — so ComposeInternal.h and
# ComposeRuntime.h are unreachable from kit code. BoundaryProbe.cpp is the
# negative control for exactly that (built on demand, expected to fail; see
# the file).
#
# Four kit headers live with other tiers instead, because their values are
# spelled in those tiers' types: `kit/Strokes.h` (the stroke grammar) and
# `kit/Plate.h` (the bordered feed plate) with Brush, `kit/Legibility.h`
# with Typography and `kit/Placers.h` with Instances. Include them directly.
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

# The negative control. EXCLUDE_FROM_ALL so the ordinary build never sees
# it, and deliberately NOT an add_test(): a compile-failure check cannot
# live inside a gtest binary, and wiring it as a ctest would put a build
# error on the dashboard as a pass. Run it by hand when the boundary is in
# question — it must fail with "'ComposeInternal.h' file not found".
add_executable(compose_kit_boundary_probe EXCLUDE_FROM_ALL BoundaryProbe.cpp)
target_link_libraries(compose_kit_boundary_probe PRIVATE SigilComposeKit)

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
