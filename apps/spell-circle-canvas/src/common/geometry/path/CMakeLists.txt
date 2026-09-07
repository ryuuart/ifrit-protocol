# The path leaf: polylines, contours addressed by arc length, the walk
# that spaces marks along one and the scanline lattice that fills it,
# noise, the numeric routines under them, where a run of n items falls on
# a ring or in a grid, the path operators, the two comparable seams a mark is
# deviated and widened through (Shaper and Profile) with the band those
# rails cut, where a set of paths cross, the two coordinate systems a
# figure is measured in (the polar Frame and the unit-map Grid), the
# conic measured from its focus, and the sphere laid onto a plane with the
# rotation that turns it first. Skia, glm and the standard-library
# mixers under SigilCoreCompute — no mesh, no import, no
# material — so a text engine or a drawable-component library can walk an
# outline, or place a run of things, without linking the 3D half of
# geometry or its importers. Noise.h names the seeded mixers, which live
# one library down so that a shader's CPU twin and a resource key reach
# the same bodies. The uniform grid every proximity question is answered
# by, the scatter that fills a shape, and the triangulation, its dual and
# the outline at a tightness stand here too, because each of them answers
# in points and rings.
find_package(boost_unordered CONFIG REQUIRED)
find_package(CDT CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(unofficial-skia CONFIG REQUIRED)
sigil_library(SigilGeometryPath
  SOURCES Band.cpp Conic.cpp Contour.cpp Crossings.cpp Direction.cpp Edges.cpp
          Extremes.cpp Fit.cpp Hull.cpp Interpolate.cpp Lattice.cpp
          Neighbours.cpp Noise.cpp Ops.cpp Polyline.cpp Pose.cpp Projection.cpp
          Scatter.cpp
          Segments.cpp Symmetry.cpp Tidy.cpp Trace.cpp Triangulate.cpp
  HEADERS Arrange.h Band.h Cells.h Conic.h Contour.h Crossings.h Direction.h
          Edges.h Extremes.h Fit.h Frame.h Hull.h Interpolate.h Lattice.h
          Neighbours.h
          Noise.h Numeric.h Ops.h Polyline.h Pose.h Profile.h Projection.h
          Scatter.h
          Segments.h Shaper.h Skia.h Stride.h Symmetry.h Tidy.h Trace.h
          Triangulate.h
  PUBLIC unofficial::skia::skia glm::glm SigilCoreCompute Boost::unordered
  # CDT is the Delaunay triangulator, read in Triangulate.cpp alone: the
  # answer crosses the boundary in this library's own points, indices and
  # rings, so no consumer of a triangulation ever names it.
  PRIVATE CDT::CDT)

# ONE BINARY over the leaf and the shape interpolation that stands on it.
# Blend is not a boundary anyone reads: no consumer links it without Path,
# and its every claim is about the contours this leaf resamples. One file
# per subject inside it — contours, polylines, marks, operators, seams,
# crossings, frames, conics, projections, blend.
sigil_test(geometry_test
  SOURCES test/ContoursTest.cpp test/ConicsTest.cpp test/PolylinesTest.cpp
          test/MarksTest.cpp test/SegmentsTest.cpp test/NodesTest.cpp
          test/NeighboursTest.cpp test/ScatterTest.cpp
          test/TriangulateTest.cpp test/FieldsTest.cpp
          test/OpsTest.cpp test/SeamsTest.cpp test/CrossingsTest.cpp
          test/FramesTest.cpp test/ProjectionsTest.cpp test/BlendTest.cpp
  LIBRARIES SigilGeometryPath SigilGeometryPathBlend)

# One file per subject, over the one figure in bench/Figures.h.
sigil_bench(geometry_bench
  SOURCES bench/PolylinesBench.cpp bench/ConicsBench.cpp
          bench/NeighboursBench.cpp bench/ProjectionsBench.cpp
          bench/ScatterBench.cpp bench/TriangulateBench.cpp
          bench/FieldsBench.cpp bench/NodesBench.cpp
  LIBRARIES SigilGeometryPath)

add_subdirectory(blend)
