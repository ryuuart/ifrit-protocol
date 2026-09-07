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

/** Which of a box's six faces to emit, and the colour the emitted ones
 *  carry.
 *
 *  Faces are named by the axis they look along: `front` is +z — the
 *  facing `mesh::quad()` uses — `top` is +y, `right` is +x. Dropping one
 *  is what a solid standing on a ground plane or seen from one side is
 *  for: the underside of a column a camera never gets beneath is a sixth
 *  of the triangles for nothing, and a face nothing sees is a face
 *  nothing misses.
 *
 *  `tint` and `sideShade` write the colors lane, and only a stated one
 *  does: a plain box carries no colour and takes whatever the fill
 *  gives it. `sideShade` multiplies the rgb of the four SIDE faces —
 *  front, back, left and right — leaving top and bottom at full tint,
 *  which is the flat-shaded reading that makes a field of boxes read as
 *  blocks rather than as one surface. It decides nothing at its
 *  default: 1 shades nothing. */
struct BoxOptions {
  bool front = true;   ///< +z
  bool back = true;    ///< -z
  bool right = true;   ///< +x
  bool left = true;    ///< -x
  bool top = true;     ///< +y
  bool bottom = true;  ///< -y
  glm::vec4 tint = {1, 1, 1, 1};
  float sideShade = 1;
};

/** The axis-aligned box spanning @p lo to @p hi — the one primitive
 *  solid that is not a parametric sheet, because its normals are flat
 *  and its corners are hard: every face carries its own four vertices,
 *  its own outward normal and its own (0,0)–(1,1) UV square, so a
 *  texture lands square on each face and no edge is smoothed across.
 *
 *  The corners are sorted, so a caller that hands the two points the
 *  other way about still gets a box wound outward rather than one
 *  turned inside out. A degenerate span (equal on an axis) emits its
 *  faces flat rather than nothing. */
Mesh box(glm::vec3 lo, glm::vec3 hi, const BoxOptions& options = {});

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
