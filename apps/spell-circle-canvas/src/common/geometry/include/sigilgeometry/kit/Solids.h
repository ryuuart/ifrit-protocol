#pragma once

/** @file
 * The 3D shelf of the geometry kit — the stock solids.
 *
 * Two of them LIFT another currency into a mesh: `extrude()` raises a
 * filled path, `revolve()` lathes a profile polyline. Most of the rest
 * are the named surfaces, each one the parametric sheet seam
 * `mesh::grid()` evaluated through a formula anyone could have written —
 * which is why they are a shelf and not the currency. A consumer with its
 * own formula calls `grid()` and is a peer of these. `box()` and
 * `platonic()` are the two that are not sheets: their faces are flat and
 * their corners hard, and they are stated by a corner table rather than
 * by a formula.
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

/** The five regular solids, each named by the face it is made of: four
 *  triangles, six squares, eight triangles, twelve pentagons, twenty
 *  triangles. */
enum class Platonic {
  Tetrahedron,
  Cube,
  Octahedron,
  Dodecahedron,
  Icosahedron
};

/** How `platonic()` stands a regular solid up.
 *
 *  `sharedVertices` picks which of two meshes the same solid is. Left
 *  alone, every face carries its OWN corners and its own flat normal, as
 *  `box()` does — the hard-cornered reading a die or a machined solid
 *  wants, where a face is one tone across and the edges between faces are
 *  edges. Turned on, the solid is its corner cage instead: one vertex per
 *  corner, shared by every face that meets there, each carrying the
 *  outward direction it stands in. That is the form to walk as topology —
 *  a wireframe, an edge list, a subdivision seed — because two faces that
 *  meet name the SAME two vertices. It carries no UVs, since a shared
 *  corner has no one place in a face's texture square. */
struct PlatonicOptions {
  float circumradius = 1.0f;  ///< every corner stands this far from the centre
  bool sharedVertices = false;
};

/** One of the five regular solids, centred on the origin, wound outward.
 *
 *  Its faces are gathered rather than tabulated: the face PLANES of a
 *  regular solid are the corner directions of its dual — the cube's six
 *  faces look along the octahedron's six corners, the dodecahedron's
 *  twelve along the icosahedron's twelve — so one corner table per solid
 *  states the whole of it, and each face is the corners standing furthest
 *  along its own plane normal, ordered around it. A polygon face is
 *  fanned into triangles and every triangle of it carries the face's
 *  index in the `"Id"` primitive lane, so `mesh::faceCount()`,
 *  `faceNormal()` and `faceUp()` read a dodecahedron as twelve pentagons
 *  and not as thirty-six triangles.
 *
 *  `Platonic::Cube` in its hard-cornered form IS `box()` — an axis-aligned
 *  box with equal sides — and answers exactly that, with box's own UV
 *  square on each face. */
Mesh platonic(Platonic solid, const PlatonicOptions& options = {});

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
