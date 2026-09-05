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
# `kit/Solids.h` (the stock meshes over the parametric-sheet seam). Beside
# them the stock values over the leaf's other two 2D seams —
# `kit/Shapers.h` (what bends one continuous mark, and the width laws that
# oscillate) and `kit/Divisions.h` (a figure's tick ladder and chord fan as
# one path). The blob's lobes are placed by the seeded mixer one library
# down, so a blob is the same blob in a stored render and in a resource
# key.
find_package(earcut_hpp CONFIG REQUIRED)
sigil_library(SigilGeometryKit
  SOURCES Silhouettes.cpp Solids.cpp
  HEADERS Corners.h Curves.h Divisions.h Generators.h Shapers.h
          Silhouettes.h Solids.h
  PUBLIC SigilGeometryPath SigilGeometryMesh SigilCoreCompute
  PRIVATE earcut_hpp::earcut_hpp)

# Links only the shelves, over Skia — a link edge that pulled a component
# or scene library in would show up as a build failure here.
sigil_test(geometry_kit_test
  SOURCES test/SilhouettesTest.cpp test/SolidsTest.cpp
  LIBRARIES SigilGeometryKit)

sigil_bench(geometry_kit_bench
  SOURCES bench/KitBench.cpp bench/SolidsBench.cpp
  LIBRARIES SigilGeometryKit)
