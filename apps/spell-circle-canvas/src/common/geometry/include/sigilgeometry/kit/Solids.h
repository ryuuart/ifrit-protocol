#pragma once

/** @file
 * The 3D shelf of the geometry kit — the stock solids.
 *
 * Two of them LIFT another currency into a mesh: `extrude()` raises a
 * filled path, `revolve()` lathes a profile polyline. The rest are the
 * named surfaces, each one the parametric sheet seam `mesh::grid()`
 * evaluated through a formula anyone could have written — which is why
 * they are a shelf and not the currency. A consumer with its own formula
 * calls `grid()` and is a peer of these.
 *
 * They produce `mesh::Mesh` values and are spelled in its namespace, so a
 * caller reaching for `mesh::torus` links this shelf and nothing changes
 * in what it says.
 */

#include <include/core/SkPath.h>

#include <glm/glm.hpp>
#include <vector>

#include "sigilgeometry/mesh/Mesh.h"

namespace sigil::geometry::mesh {

/** How `extrude()` thickens a path: the total depth and which of the
 *  three surfaces — front cap, back cap, swept walls — to emit.
 *  Dropping caps leaves an open shell, which is what a wall-only
 *  extrusion is for. */
struct ExtrudeOptions {
  float depth = 24;         ///< total thickness, centered on z = 0
  float tolerance = 0.25f;  ///< curve flattening tolerance for walls/caps
  bool frontCap = true;
  bool backCap = true;
  bool walls = true;
};

/** Lift a filled path into a solid: front/back caps triangulated with
 *  hole support (even-odd containment decides outer vs hole rings),
 *  walls swept between them. Cap UVs are the path's unit bounds; wall
 *  UVs run u = contour arc length, v = depth. */
Mesh extrude(const SkPath& path, const ExtrudeOptions& options = {});

/** How `revolve()` lathes a profile: the number of steps around the
 *  axis and how far around to go. A partial sweep leaves the surface
 *  open at the seam; a full one duplicates the seam ring so the u
 *  coordinate can reach 1 instead of wrapping to 0. */
struct RevolveOptions {
  int segments = 48;     ///< steps around the axis
  float sweepDeg = 360;  ///< partial sweeps leave an open seam
  bool close = true;     ///< duplicate seam ring for clean UV wrap
};

/** Lathe a profile polyline around the +y axis: each profile point is
 *  (radius, height). UVs: u around the sweep, v along the profile. */
Mesh revolve(const std::vector<glm::vec2>& profile,
             const RevolveOptions& options = {});

/** Torus around +y: major radius R in xz, tube radius r. */
Mesh torus(float R, float r, int nu = 64, int nv = 32);

/** Superellipsoid (exponent 2 = sphere, higher = rounded box). */
Mesh superellipsoid(glm::vec3 radii, float exponent, int nu = 48, int nv = 32);

/** A width x height panel curved around a vertical cylinder of
 *  @p radius (0 or infinite radius = flat), facing +z, centered at the
 *  origin. The natural diegetic-UI surface. */
Mesh cylinderPanel(float width, float height, float radius, int nu = 32,
                   int nv = 8);

}  // namespace sigil::geometry::mesh
