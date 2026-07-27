#pragma once

/** @file
 * SigilShape procedural geometry — one Mesh currency for two renderers.
 * The same vertex/index buffers draw through Skia (Space.h: SkM44
 * projection + SkVertices) and upload to SigilWorld (Diligent vertex
 * buffers) — positions, normals, uvs, indices, nothing renderer-shaped.
 *
 * Generators cover the diegetic-surface needs: extrude() lifts any
 * SkPath into a solid (caps earcut-triangulated with holes intact,
 * walls from the flattened contours), revolve() lathes a profile,
 * grid() evaluates any parametric sheet, and the named surfaces
 * (torus, superellipsoid, cylinderPanel) are grid() presets. UVs are
 * always present so panel textures land without ceremony.
 */

#include <include/core/SkColor.h>
#include <include/core/SkM44.h>
#include <include/core/SkPath.h>
#include <include/core/SkPoint.h>

#include <cstdint>
#include <functional>
#include <vector>

namespace sigil::shape {

/** Renderer-neutral triangle mesh. Indices are 32-bit; Skia's 16-bit
 *  SkVertices limit is handled by the Space.h drawer (chunking), not by
 *  the data. */
struct Mesh {
  std::vector<SkV3> positions;
  std::vector<SkV3> normals;   // unit, same count as positions
  std::vector<SkPoint> uvs;    // [0,1]^2, same count as positions
  /** Optional per-vertex tint (instancing writes it); empty = none.
   *  Space.h's Lit mode multiplies baseColor by it when present. */
  std::vector<SkColor4f> colors;
  std::vector<uint32_t> indices;

  size_t vertexCount() const { return positions.size(); }
  size_t triangleCount() const { return indices.size() / 3; }

  /** Append another mesh (indices re-based). */
  void append(const Mesh &other);
  /** Transform positions by @p m and normals by its inverse transpose. */
  void transform(const SkM44 &m);
  /** Recompute vertex normals as area-weighted triangle-normal sums. */
  void computeNormals();
  /** Axis-aligned bounds. */
  void bounds(SkV3 *lo, SkV3 *hi) const;
};

namespace mesh {

struct ExtrudeOptions {
  float depth = 24;          ///< total thickness, centered on z = 0
  float tolerance = 0.25f;   ///< curve flattening tolerance for walls/caps
  bool frontCap = true;
  bool backCap = true;
  bool walls = true;
};

/** Lift a filled path into a solid: front/back caps triangulated with
 *  hole support (even-odd containment decides outer vs hole rings),
 *  walls swept between them. Cap UVs are the path's unit bounds; wall
 *  UVs run u = contour arc length, v = depth. */
Mesh extrude(const SkPath &path, const ExtrudeOptions &options = {});

struct RevolveOptions {
  int segments = 48;        ///< steps around the axis
  float sweepDeg = 360;     ///< partial sweeps leave an open seam
  bool close = true;        ///< duplicate seam ring for clean UV wrap
};

/** Lathe a profile polyline around the +y axis: each profile point is
 *  (radius, height). UVs: u around the sweep, v along the profile. */
Mesh revolve(const std::vector<SkPoint> &profile,
             const RevolveOptions &options = {});

/** Evaluate a parametric sheet on an nu x nv vertex grid. UVs are the
 *  (u,v) parameters. Normals from the analytic cross of numeric partial
 *  derivatives. */
Mesh grid(int nu, int nv, const std::function<SkV3(float u, float v)> &fn);

/** Torus around +y: major radius R in xz, tube radius r. */
Mesh torus(float R, float r, int nu = 64, int nv = 32);

/** Superellipsoid (exponent 2 = sphere, higher = rounded box). */
Mesh superellipsoid(SkV3 radii, float exponent, int nu = 48, int nv = 32);

/** A width x height panel curved around a vertical cylinder of
 *  @p radius (0 or infinite radius = flat), facing +z, centered at the
 *  origin. The natural diegetic-UI surface. */
Mesh cylinderPanel(float width, float height, float radius, int nu = 32,
                   int nv = 8);

/** Flat quad panel in the xy plane facing +z, centered at origin. */
Mesh quad(float width, float height);

} // namespace mesh

} // namespace sigil::shape
