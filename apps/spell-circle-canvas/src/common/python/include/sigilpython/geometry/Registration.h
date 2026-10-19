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
/** Registers primitive lanes, faces, extrusion caps and the normal grid
 *  on @p module. */
void bindGeometryMesh(pybind11::module_& module);
/** Registers camera matrices, frustum extent and a readable Matrix on
 *  @p module. */
void bindGeometryMeshCamera(pybind11::module_& module);
/** Registers the 3D spline, its rule and what it answers on @p
 *  module. */
void bindGeometryMeshCurve(pybind11::module_& module);
/** Registers the outline value and its algebra, the transform, the
 *  path through points, the rail and the band at a width, the radial
 *  options and the point patterns on @p module. */
void bindGeometryOutlines(pybind11::module_& module);
/** Registers the two words a widened mark states — path.Cap and
 *  path.Join — and path.operations booleans, offset, corner treatments,
 *  distorts, chains and strip joinery on @p module. */
void bindGeometryPathOperations(pybind11::module_& module);
/** Registers the artist's Builder, the sweeps and the sinks on @p
 *  module. */
void bindGeometryPointBuilder(pybind11::module_& module);
/** Registers width laws, shapers, the Shaped wrapper and the band
 *  constructions on @p module. */
void bindGeometryProfiles(pybind11::module_& module);
/** Registers the hatch value, the two general shapes over a box — radial
 *  and ellipse — an outline fitted to a box, and the stock values over
 *  them on @p module. */
void bindGeometryShapes(pybind11::module_& module);
/** Registers the solid values — each a comparable description of a stock
 *  solid that forms its mesh on asking — on @p module. */
void bindGeometrySolids(pybind11::module_& module);

}  // namespace sigil::python
