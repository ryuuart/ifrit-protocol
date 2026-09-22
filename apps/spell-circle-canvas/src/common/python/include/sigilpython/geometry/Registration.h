#pragma once

/** @file
 * Every registration this library's packages add to the extension
 * module: paths, shapes, meshes, point operators and their seams.
 *
 * One package owns one of these functions and the source file that
 * defines it, so two authors never write in one file.
 */

#include <pybind11/pybind11.h>

namespace sigil::python {

/** Registers arrangement and the mesh tier on @p module. */
void bindGeometry(pybind11::module_& module);
/** Registers conic sections, spherical projections and rotations, and
 *  crossing discovery with its rules on @p module. */
void bindGeometryCharts(pybind11::module_& module);
/** Registers the opaque device handle a runtime hands back on @p
 *  module. */
void bindGeometryDeviceHandle(pybind11::module_& module);
/** Registers polar frames, unit grids and the divisions shelf (ticks,
 *  arcs, chords) on @p module. */
void bindGeometryFrames(pybind11::module_& module);
/** Registers primitive lanes, faces, extrusion caps and the normal grid
 *  on @p module. */
void bindGeometryMesh(pybind11::module_& module);
/** Registers camera matrices, frustum extent and a readable Matrix on
 *  @p module. */
void bindGeometryMeshCamera(pybind11::module_& module);
/** Registers model import and PLY/geo export on @p module. */
void bindGeometryMeshCodec(pybind11::module_& module);
/** Registers splines, parallel-transport frames, hung rails and poses
 *  on @p module. */
void bindGeometryMeshCurve(pybind11::module_& module);
/** Registers cloud lanes, the point generators, stamping and billboards
 *  on @p module. */
void bindGeometryMeshPoints(pybind11::module_& module);
/** Registers the draw runtime seam, drawPanel and the environment on @p
 *  module. */
void bindGeometryMeshRender(pybind11::module_& module);
/** Registers segments, extremes, tidy, fit, direction, edges, exact
 *  interpolation and the blend tool on @p module. */
void bindGeometryPathEditing(pybind11::module_& module);
/** Registers the two words a widened mark states — path.Cap and
 *  path.Join — and path.operations booleans, offset, corner treatments,
 *  distorts, chains and strip joinery on @p module. */
void bindGeometryPathOperations(pybind11::module_& module);
/** Registers the artist's Builder, the sweeps and the sinks on @p
 *  module. */
void bindGeometryPointBuilder(pybind11::module_& module);
/** Registers the three kernel dispatch seams and their SPIR-V on @p
 *  module. */
void bindGeometryPointKernels(pybind11::module_& module);
/** Registers the 25-operator vocabulary, the Chain and the cook seam on
 *  @p module. */
void bindGeometryPointOperations(pybind11::module_& module);
/** Registers polylines, contours, poses, stride, the numeric leaf and
 *  sections, with the shared point-batch conversions on @p module. */
void bindGeometryPolylines(pybind11::module_& module);
/** Registers width laws, shapers, the Shaped wrapper and the band
 *  constructions on @p module. */
void bindGeometryProfiles(pybind11::module_& module);
/** Registers regions, distributions, scanline lattices, multigrid
 *  tilings and the uniform neighbour grid on @p module. */
void bindGeometryRegions(pybind11::module_& module);
/** Registers the edge, corner, profile, shaper and crossing-rule
 *  readings a decoration is written in terms of on @p module. */
void bindGeometrySeams(pybind11::module_& module);
/** Registers geometry.shapes generators, curves, corner operators and
 *  the Silhouette erasure seam on @p module. */
void bindGeometryShapes(pybind11::module_& module);
/** Registers triangulations, hulls, streamlines, symmetry, value noise
 *  and the cellular sheet on @p module. */
void bindGeometryStructures(pybind11::module_& module);

}  // namespace sigil::python
