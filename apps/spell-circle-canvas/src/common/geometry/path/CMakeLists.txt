# The path leaf: polylines, contours addressed by arc length, noise, the
# numeric routines under them, where a run of n items falls on a ring or
# in a grid, the path operators, the two comparable seams a mark is
# deviated and widened through (Shaper and Profile) with the band those
# rails cut, where a set of paths cross, and the two coordinate systems a
# figure is measured in (the polar Frame and the unit-map Grid). Skia, glm and the
# standard-library mixers under SigilCoreCompute — no mesh, no import, no
# material — so a text engine or a drawable-component library can walk an
# outline, or place a run of things, without linking the 3D half of
# geometry or its importers. Noise.h names the seeded mixers, which live
# one library down so that a shader's CPU twin and a resource key reach
# the same bodies.
find_package(glm CONFIG REQUIRED)
sigil_library(SigilGeometryPath
  SOURCES Band.cpp Contour.cpp Crossings.cpp Edges.cpp Noise.cpp Ops.cpp
          Polyline.cpp Pose.cpp
  HEADERS Arrange.h Band.h Contour.h Crossings.h Edges.h Frame.h Noise.h
          Numeric.h Ops.h Polyline.h Pose.h Profile.h Shaper.h Skia.h
  PUBLIC unofficial::skia::skia glm::glm SigilCoreCompute Boost::unordered)

sigil_test(geometry_path_test
  SOURCES test/PathTest.cpp
  LIBRARIES SigilGeometryPath)

sigil_bench(geometry_path_bench
  SOURCES bench/PathBench.cpp
  LIBRARIES SigilGeometryPath)

add_subdirectory(blend)
