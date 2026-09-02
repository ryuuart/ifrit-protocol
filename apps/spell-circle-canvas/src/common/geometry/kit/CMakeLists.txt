# SigilGeometryKit — the stock shelves over the tiers beneath.
#
# A KIT value is a value anyone could have written: it plugs a seam the
# tier below states and decides nothing the tier does not already let a
# caller decide. That is why the catalog is a separate archive from the
# currency — a consumer that brings its own generators links the tier and
# not this, and nothing in the tier below may reach back up into here.
#
# One shelf per dimension: `kit/Silhouettes.h` (the 2D generators, curves
# and corner treatments, as comparable `path(SkSize)` values) and
# `kit/Solids.h` (the stock meshes over the parametric-sheet seam).
add_library(SigilGeometryKit STATIC
  Silhouettes.cpp
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/kit/Corners.h
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/kit/Curves.h
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/kit/Generators.h
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/kit/Silhouettes.h
)
target_compile_features(SigilGeometryKit PUBLIC cxx_std_20)
target_include_directories(SigilGeometryKit PUBLIC
  ${SIGIL_GEOMETRY_INCLUDE_DIR})
# The blob's lobes are placed by the seeded mixer one library down, so a
# blob is the same blob in a stored render and in a resource key.
target_link_libraries(SigilGeometryKit PUBLIC SigilGeometryPath SigilCoreCompute)

# Links only the shelves, over Skia — a link edge that pulled a component
# or scene library in would show up as a build failure here.
add_executable(geometry_kit_test test/SilhouettesTest.cpp)
target_link_libraries(geometry_kit_test PRIVATE SigilGeometryKit
  GTest::gtest_main)
add_test(NAME geometry_kit_test COMMAND geometry_kit_test)

# Google Benchmark (use a Release build), linking only what it measures.
# Not a test: it builds through the `benches` target and runs through
# scripts/bench_ledger.py.
add_executable(geometry_kit_bench bench/KitBench.cpp)
target_link_libraries(geometry_kit_bench PRIVATE SigilGeometryKit
  benchmark::benchmark)
add_dependencies(benches geometry_kit_bench)
