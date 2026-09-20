#pragma once

/** @file
 * Every public header of SigilGeometry in one include, for a consumer
 * that takes the whole library rather than a tier of it. Narrowing to
 * the feature headers actually used is always available.
 *
 * The device headers are the one exception — `device/Device.h`, the
 * point operators' `mesh/pop/device/` headers and the mesh painter's
 * `mesh/render/device/Painter.h`. Each names a GPU device that has to
 * be brought up and handed in, so a consumer takes one deliberately
 * rather than by asking for the library; the residency feature's
 * headers spell the engine's interfaces and stand on a private include
 * path, so they are not the library's to offer at all.
 */

/** @namespace sigil::geometry
 *  Drawing over Skia in two currencies. The `path` tier is 2D: an
 *  `SkPath` resampled, addressed by arc length, operated on, distorted
 *  and interpolated. The `mesh` tier is 3D: a renderer-neutral triangle
 *  mesh, the camera that looks at it, splines with swept geometry, point
 *  clouds carrying named attribute lanes, model interchange, and a
 *  runtime that draws the result onto an ordinary `SkCanvas`. Values in,
 *  values out: there is no windowing, no scene graph and no UI. */

/** @defgroup geometry-path Path operators
 *  The 2D tier: an outline resampled to polylines and segments, addressed
 *  by arc length through contours and poses, cut and combined by boolean
 *  operations, walked by shapers and profiles, and interpolated between
 *  two compatible outlines. Everything here takes and answers `SkPath`
 *  and glm vectors (path/Contour.h, path/Polyline.h, path/Operations.h,
 *  path/Shaper.h, path/Profile.h). */
/** @defgroup geometry-mesh Meshes and point clouds
 *  The 3D tier: the triangle mesh and its faces, the camera, splines and
 *  the frames they carry, point clouds with named attribute lanes and the
 *  operator chain over them, model import and export, and the painter
 *  that draws a mesh onto a canvas (mesh/Mesh.h, mesh/camera/Camera.h,
 *  mesh/curve/Curve.h, mesh/pop/Pop.h, mesh/codec/Model.h,
 *  mesh/render/Painter.h). */
/** @defgroup geometry-device The device tier
 *  What needs a GPU device handed to it: the device this library creates
 *  for whoever draws with both Diligent and Skia, and the device
 *  executors that stand beside the CPU executors of the seams they serve
 *  (device/Device.h, mesh/pop/device/, mesh/render/device/Painter.h). */
/** @defgroup geometry-kit The kit
 *  Stock values over the operators: named silhouettes and curve families,
 *  corner treatments, tick and chord divisions, hatches, section solids
 *  and the shapers a stroke is walked with (kit/Generators.h,
 *  kit/Curves.h, kit/Corners.h, kit/Shapers.h). */

#include "sigilgeometry/kit/Corners.h"
#include "sigilgeometry/kit/Curves.h"
#include "sigilgeometry/kit/Divisions.h"
#include "sigilgeometry/kit/Generators.h"
#include "sigilgeometry/kit/Hatches.h"
#include "sigilgeometry/kit/Sections.h"
#include "sigilgeometry/kit/Shapers.h"
#include "sigilgeometry/kit/Silhouettes.h"
#include "sigilgeometry/kit/Solids.h"
#include "sigilgeometry/mesh/Faces.h"
#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/mesh/Vec.h"
#include "sigilgeometry/mesh/camera/Camera.h"
#include "sigilgeometry/mesh/codec/Decode.h"
#include "sigilgeometry/mesh/codec/Encode.h"
#include "sigilgeometry/mesh/codec/Model.h"
#include "sigilgeometry/mesh/curve/Curve.h"
#include "sigilgeometry/mesh/curve/Frame.h"
#include "sigilgeometry/mesh/curve/Pose.h"
#include "sigilgeometry/mesh/pop/Builder.h"
#include "sigilgeometry/mesh/pop/Kernel.h"
#include "sigilgeometry/mesh/pop/Operations.h"
#include "sigilgeometry/mesh/pop/Points.h"
#include "sigilgeometry/mesh/pop/Pop.h"
#include "sigilgeometry/mesh/pop/Runtime.h"
#include "sigilgeometry/mesh/pop/Sinks.h"
#include "sigilgeometry/mesh/pop/Stamp.h"
#include "sigilgeometry/mesh/pop/Sweep.h"
#include "sigilgeometry/mesh/render/Painter.h"
#include "sigilgeometry/mesh/render/Runtime.h"
#include "sigilgeometry/mesh/render/Shading.h"
#include "sigilgeometry/path/Arrange.h"
#include "sigilgeometry/path/Band.h"
#include "sigilgeometry/path/Cells.h"
#include "sigilgeometry/path/Conic.h"
#include "sigilgeometry/path/Contour.h"
#include "sigilgeometry/path/Crossings.h"
#include "sigilgeometry/path/Direction.h"
#include "sigilgeometry/path/Edges.h"
#include "sigilgeometry/path/Extremes.h"
#include "sigilgeometry/path/Fit.h"
#include "sigilgeometry/path/Frame.h"
#include "sigilgeometry/path/Hull.h"
#include "sigilgeometry/path/Interpolate.h"
#include "sigilgeometry/path/Lattice.h"
#include "sigilgeometry/path/Neighbours.h"
#include "sigilgeometry/path/Noise.h"
#include "sigilgeometry/path/Numeric.h"
#include "sigilgeometry/path/Operations.h"
#include "sigilgeometry/path/Polyline.h"
#include "sigilgeometry/path/Pose.h"
#include "sigilgeometry/path/Profile.h"
#include "sigilgeometry/path/Projection.h"
#include "sigilgeometry/path/Scatter.h"
#include "sigilgeometry/path/Segments.h"
#include "sigilgeometry/path/Shaper.h"
#include "sigilgeometry/path/Skia.h"
#include "sigilgeometry/path/Stride.h"
#include "sigilgeometry/path/Symmetry.h"
#include "sigilgeometry/path/Tidy.h"
#include "sigilgeometry/path/Trace.h"
#include "sigilgeometry/path/Triangulate.h"
#include "sigilgeometry/path/blend/Blend.h"
