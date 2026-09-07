/** @file
 * The stock solids: a path extruded into one with earcut caps, a profile
 * lathed, and the named surfaces evaluated through the parametric sheet.
 */

#include "sigilgeometry/kit/Solids.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <mapbox/earcut.hpp>

#include "sigilgeometry/mesh/Vec.h"
#include "sigilgeometry/path/Direction.h"
#include "sigilgeometry/path/Numeric.h"
#include "sigilgeometry/path/Polyline.h"

namespace sigil::geometry::mesh {

// The outline resampling the extrusion and the lathe stand on lives in
// the path tier; a parameter here is called `path`, so the names are
// pulled in rather than spelled through it.
using glm::cross;
using path::flatten;
using path::Polyline;
using sigil::geometry::mesh::normalized;

namespace {

/** Ensure the geometric normal of triangle (i0,i1,i2) points along
 *  @p wanted, swapping winding when it does not. */
void orientTriangle(const std::vector<glm::vec3>& positions, uint32_t* tri,
                    glm::vec3 wanted) {
  const glm::vec3& p0 = positions[tri[0]];
  const glm::vec3 n = cross(positions[tri[1]] - p0, positions[tri[2]] - p0);
  if (glm::dot(n, wanted) < 0) std::swap(tri[1], tri[2]);
}

}  // namespace

Mesh extrude(const SkPath& path, const ExtrudeOptions& options) {
  Mesh out;
  std::vector<Polyline> rings = flatten(path, options.tolerance);
  std::erase_if(rings, [](const Polyline& r) { return r.points.size() < 3; });
  if (rings.empty()) return out;

  // Center on the path bounds, flip y so the mesh sits in y-up space
  // with the artwork upright.
  const SkRect pathBounds = path.computeTightBounds();
  const glm::vec2 center = {pathBounds.centerX(), pathBounds.centerY()};
  for (Polyline& ring : rings)
    for (glm::vec2& p : ring.points) p = {p.x - center.x, -(p.y - center.y)};

  // Even-odd containment depth: even = outer ring, odd = hole of the
  // innermost containing outer.
  const size_t ringCount = rings.size();
  const std::vector<path::Nesting> where = path::nesting(rings);

  // Canonical winding in y-up space: outers CCW (positive area), holes CW.
  // The y flip above put the rings in y-up space, where a positive signed
  // area is counter-clockwise.
  for (size_t i = 0; i < ringCount; ++i) {
    const bool isHole = where[i].depth % 2 == 1;
    const float area = rings[i].signedArea();
    if ((isHole && area > 0) || (!isHole && area < 0)) rings[i].reverse();
  }

  const float uvW = std::max(pathBounds.width(), 1.0f);
  const float uvH = std::max(pathBounds.height(), 1.0f);
  const float half = options.depth * 0.5f;

  // Caps: one earcut polygon per outer ring with its direct holes.
  auto addCap = [&](float z, bool front) {
    for (size_t i = 0; i < ringCount; ++i) {
      if (where[i].depth % 2 != 0) continue;
      using EarPoint = std::array<double, 2>;
      std::vector<std::vector<EarPoint>> polygon;
      std::vector<const Polyline*> ringsUsed;
      auto pushRing = [&](const Polyline& ring) {
        std::vector<EarPoint> ear;
        ear.reserve(ring.points.size());
        for (const glm::vec2& p : ring.points)
          ear.push_back({(double)p.x, (double)p.y});
        polygon.push_back(std::move(ear));
        ringsUsed.push_back(&ring);
      };
      pushRing(rings[i]);
      for (size_t h = 0; h < ringCount; ++h)
        if (where[h].depth % 2 == 1 && where[h].parent == (int)i)
          pushRing(rings[h]);

      const std::vector<uint32_t> tris = mapbox::earcut<uint32_t>(polygon);
      const uint32_t base = (uint32_t)out.positions.size();
      const glm::vec3 normal = {0, 0, front ? 1.0f : -1.0f};
      for (const Polyline* ring : ringsUsed) {
        for (const glm::vec2& p : ring->points) {
          out.positions.emplace_back(p.x, p.y, z);
          out.normals.push_back(normal);
          out.uvs.emplace_back((p.x + uvW * 0.5f) / uvW,
                               1.0f - (p.y + uvH * 0.5f) / uvH);
        }
      }
      for (size_t t = 0; t + 2 < tris.size(); t += 3) {
        uint32_t tri[3] = {base + tris[t], base + tris[t + 1],
                           base + tris[t + 2]};
        orientTriangle(out.positions, tri, normal);
        out.indices.insert(out.indices.end(), {tri[0], tri[1], tri[2]});
      }
    }
  };
  if (options.frontCap) addCap(half, true);
  if (options.backCap) addCap(-half, false);

  // Walls: one flat-shaded quad per contour edge, outward normals from
  // the canonical winding.
  if (options.walls && options.depth > 0) {
    for (const Polyline& ring : rings) {
      const size_t n = ring.points.size();
      float arc = 0;
      float total = ring.length();
      if (total < 1e-6f) total = 1;
      for (size_t e = 0; e < n; ++e) {
        const glm::vec2 a = ring.points[e];
        const glm::vec2 b = ring.points[(e + 1) % n];
        const glm::vec2 edge = b - a;
        const float len = length(edge);
        if (len < 1e-9f) continue;
        // CCW outer ring in y-up space: outward = edge rotated -90.
        const glm::vec3 normal = normalized({edge.y / len, -edge.x / len, 0});
        const uint32_t base = (uint32_t)out.positions.size();
        const float u0 = arc / total, u1 = (arc + len) / total;
        out.positions.emplace_back(a.x, a.y, half);
        out.positions.emplace_back(b.x, b.y, half);
        out.positions.emplace_back(b.x, b.y, -half);
        out.positions.emplace_back(a.x, a.y, -half);
        for (int k = 0; k < 4; ++k) out.normals.push_back(normal);
        out.uvs.emplace_back(u0, 0);
        out.uvs.emplace_back(u1, 0);
        out.uvs.emplace_back(u1, 1);
        out.uvs.emplace_back(u0, 1);
        uint32_t tri0[3] = {base, base + 1, base + 2};
        uint32_t tri1[3] = {base, base + 2, base + 3};
        orientTriangle(out.positions, tri0, normal);
        orientTriangle(out.positions, tri1, normal);
        out.indices.insert(out.indices.end(), {tri0[0], tri0[1], tri0[2],
                                               tri1[0], tri1[1], tri1[2]});
        arc += len;
      }
    }
  }
  return out;
}

Mesh revolve(const std::vector<glm::vec2>& profile,
             const RevolveOptions& options) {
  Mesh out;
  if (profile.size() < 2) return out;
  const float sweep = options.sweepDeg * (float)M_PI / 180.0f;
  const int nu = std::max(options.segments, 3) + (options.close ? 1 : 0);
  const int nv = (int)profile.size();
  auto sample = [&](float v) -> glm::vec2 {
    const float f = v * (float)(nv - 1);
    const int i = std::clamp((int)f, 0, nv - 2);
    const float t = f - (float)i;
    const glm::vec2 a = profile[(size_t)i], b = profile[(size_t)i + 1];
    return a + (b - a) * t;
  };
  return grid(nu, nv, [&](float u, float v) -> glm::vec3 {
    const glm::vec2 p = sample(v);
    const float theta = u * sweep;
    return {p.x * std::cos(theta), p.y, -p.x * std::sin(theta)};
  });
}

Mesh box(glm::vec3 lo, glm::vec3 hi, const BoxOptions& options) {
  const glm::vec3 a{std::min(lo.x, hi.x), std::min(lo.y, hi.y),
                    std::min(lo.z, hi.z)};
  const glm::vec3 b{std::max(lo.x, hi.x), std::max(lo.y, hi.y),
                    std::max(lo.z, hi.z)};
  // The eight corners, then each face as four of them wound outward.
  const std::array<glm::vec3, 8> corner = {
      glm::vec3{a.x, a.y, b.z}, glm::vec3{b.x, a.y, b.z},
      glm::vec3{b.x, b.y, b.z}, glm::vec3{a.x, b.y, b.z},
      glm::vec3{a.x, a.y, a.z}, glm::vec3{b.x, a.y, a.z},
      glm::vec3{b.x, b.y, a.z}, glm::vec3{a.x, b.y, a.z}};
  struct Face {
    bool BoxOptions::* wanted;
    glm::vec3 normal;
    std::array<int, 4> ring;
    bool side;  ///< sideShade applies to the four that look sideways
  };
  static const std::array<Face, 6> kFaces = {
      Face{&BoxOptions::front, {0, 0, 1}, {0, 1, 2, 3}, true},
      Face{&BoxOptions::back, {0, 0, -1}, {5, 4, 7, 6}, true},
      Face{&BoxOptions::right, {1, 0, 0}, {1, 5, 6, 2}, true},
      Face{&BoxOptions::left, {-1, 0, 0}, {4, 0, 3, 7}, true},
      Face{&BoxOptions::top, {0, 1, 0}, {3, 2, 6, 7}, false},
      Face{&BoxOptions::bottom, {0, -1, 0}, {4, 5, 1, 0}, false}};

  const bool colored =
      options.tint != glm::vec4{1, 1, 1, 1} || options.sideShade != 1.0f;
  Mesh out;
  for (const Face& face : kFaces) {
    if (!(options.*face.wanted)) continue;
    const auto base = (uint32_t)out.positions.size();
    const float shade = face.side ? options.sideShade : 1.0f;
    for (int k = 0; k < 4; ++k) {
      out.positions.push_back(corner[(size_t)face.ring[(size_t)k]]);
      out.normals.push_back(face.normal);
      out.uvs.emplace_back((float)(k == 1 || k == 2), (float)(k >= 2));
      if (colored)
        out.colors.emplace_back(options.tint.r * shade, options.tint.g * shade,
                                options.tint.b * shade, options.tint.a);
    }
    out.indices.insert(out.indices.end(),
                       {base, base + 1, base + 2, base, base + 2, base + 3});
  }
  return out;
}

namespace {

/** The golden ratio, which three of the five solids' corners are stated
 *  in: a wrong value here is a solid whose faces are not planes, which is
 *  what the equal-edge and equal-plane checks over these meshes catch. */
constexpr float kPhi = 1.6180339887498949f;

/** Every sign of @p v, appended to @p into — the corner tables are all
 *  sign families of two or three numbers. A zero coordinate is not
 *  doubled, so (0, 1, phi) yields four corners and not eight. */
void signsOf(glm::vec3 v, std::vector<glm::vec3>* into) {
  for (float sx : {1.0f, -1.0f}) {
    if (v.x == 0 && sx < 0) continue;
    for (float sy : {1.0f, -1.0f}) {
      if (v.y == 0 && sy < 0) continue;
      for (float sz : {1.0f, -1.0f}) {
        if (v.z == 0 && sz < 0) continue;
        into->push_back({v.x * sx, v.y * sy, v.z * sz});
      }
    }
  }
}

/** The corner directions of a regular solid, unnormalised. */
std::vector<glm::vec3> platonicCorners(Platonic solid) {
  std::vector<glm::vec3> out;
  switch (solid) {
    case Platonic::Tetrahedron:
      // The four corners of the cube whose coordinates multiply to +1.
      out = {{1, 1, 1}, {1, -1, -1}, {-1, 1, -1}, {-1, -1, 1}};
      break;
    case Platonic::Cube:
      signsOf({1, 1, 1}, &out);
      break;
    case Platonic::Octahedron:
      signsOf({1, 0, 0}, &out);
      signsOf({0, 1, 0}, &out);
      signsOf({0, 0, 1}, &out);
      break;
    case Platonic::Dodecahedron:
      signsOf({1, 1, 1}, &out);
      signsOf({0, 1 / kPhi, kPhi}, &out);
      signsOf({1 / kPhi, kPhi, 0}, &out);
      signsOf({kPhi, 0, 1 / kPhi}, &out);
      break;
    case Platonic::Icosahedron:
      signsOf({0, 1, kPhi}, &out);
      signsOf({1, kPhi, 0}, &out);
      signsOf({kPhi, 0, 1}, &out);
      break;
  }
  return out;
}

/** Where a solid's faces look, found from the corners themselves: two
 *  edges meeting at a corner lie in one face, so the plane through that
 *  corner and two of its neighbours is a face plane. A candidate whose
 *  plane runs through the centre bounds nothing and is dropped; the rest
 *  are supporting planes of a convex solid, and what stands furthest
 *  along one is a face of it.
 *
 *  A table of face normals could say the same thing, but only when its
 *  handedness matches the corner table's — two regular solids of the
 *  same name are mirror images as often as not — and a mismatch is a
 *  solid with no faces at all. */
std::vector<glm::vec3> facePlanes(const std::vector<glm::vec3>& corners,
                                  float radius) {
  // Adjacent corners are the closest ones, and on a regular solid every
  // edge is the same length.
  float edge = std::numeric_limits<float>::max();
  for (size_t i = 0; i < corners.size(); ++i)
    for (size_t j = i + 1; j < corners.size(); ++j)
      edge = std::min(edge, glm::length(corners[i] - corners[j]));
  const float slack = radius * 1e-3f;

  std::vector<glm::vec3> planes;
  for (size_t i = 0; i < corners.size(); ++i) {
    std::vector<size_t> neighbours;
    for (size_t j = 0; j < corners.size(); ++j)
      if (j != i &&
          std::abs(glm::length(corners[i] - corners[j]) - edge) < slack)
        neighbours.push_back(j);
    for (size_t a = 0; a < neighbours.size(); ++a)
      for (size_t b = a + 1; b < neighbours.size(); ++b) {
        const glm::vec3 n = cross(corners[neighbours[a]] - corners[i],
                                  corners[neighbours[b]] - corners[i]);
        if (glm::length(n) < slack) continue;
        glm::vec3 unit = normalized(n);
        const float reach = dot(unit, corners[i]);
        if (std::abs(reach) < slack) continue;  // a plane through the centre
        if (reach < 0) unit = -unit;
        bool known = false;
        for (const glm::vec3& had : planes)
          known = known || glm::length(had - unit) < 1e-3f;
        if (!known) planes.push_back(unit);
      }
  }
  return planes;
}

/** The corners standing furthest along @p normal, ordered around it so
 *  the ring runs counter-clockwise seen from outside. */
std::vector<uint32_t> faceRing(const std::vector<glm::vec3>& corners,
                               glm::vec3 normal, float radius) {
  float furthest = -std::numeric_limits<float>::max();
  for (const glm::vec3& c : corners)
    furthest = std::max(furthest, dot(c, normal));
  // The gap between a face's own corners and the next ring in is a
  // fraction of the radius on every one of the five, so the slack can be
  // that coarse and still admit exactly one plane.
  const float slack = radius * 1e-3f;
  glm::vec3 x, y, z;
  basisFor(normal, {0, 1, 0}, &x, &y, &z);
  std::vector<std::pair<float, uint32_t>> ring;
  for (uint32_t i = 0; i < (uint32_t)corners.size(); ++i) {
    if (dot(corners[i], normal) < furthest - slack) continue;
    ring.emplace_back(std::atan2(dot(corners[i], y), dot(corners[i], x)), i);
  }
  std::sort(ring.begin(), ring.end());
  std::vector<uint32_t> out;
  out.reserve(ring.size());
  for (const auto& [angle, index] : ring) out.push_back(index);
  return out;
}

}  // namespace

Mesh platonic(Platonic solid, const PlatonicOptions& options) {
  const float radius = options.circumradius;
  if (solid == Platonic::Cube && !options.sharedVertices) {
    const float half = radius / std::sqrt(3.0f);
    Mesh cube = box({-half, -half, -half}, {half, half, half});
    // Box emits its six faces as two triangles each, in face order, so
    // the lane that names them is that pairing written down.
    std::vector<glm::vec4>& ids = cube.prim("Id", {0, 0, 0, 0});
    for (size_t t = 0; t < ids.size(); ++t) ids[t].x = (float)(t / 2);
    return cube;
  }

  std::vector<glm::vec3> corners = platonicCorners(solid);
  for (glm::vec3& c : corners) c = normalized(c) * radius;
  // Two candidate planes can stand over the same face, so what is
  // gathered decides: one face per set of corners, in the order the
  // planes were found.
  std::vector<std::pair<glm::vec3, std::vector<uint32_t>>> faces;
  for (const glm::vec3& plane : facePlanes(corners, radius)) {
    std::vector<uint32_t> ring = faceRing(corners, plane, radius);
    if (ring.size() < 3) continue;
    std::vector<uint32_t> sorted = ring;
    std::sort(sorted.begin(), sorted.end());
    bool known = false;
    for (const auto& [had, hadRing] : faces) {
      std::vector<uint32_t> other = hadRing;
      std::sort(other.begin(), other.end());
      known = known || other == sorted;
    }
    if (!known) faces.emplace_back(plane, std::move(ring));
  }

  Mesh out;
  if (options.sharedVertices) {
    out.positions = corners;
    for (const glm::vec3& c : corners) out.normals.push_back(normalized(c));
  }
  std::vector<glm::vec4>& ids = out.prim("Id", {0, 0, 0, 0});
  for (uint32_t face = 0; face < (uint32_t)faces.size(); ++face) {
    const glm::vec3 normal = faces[face].first;
    const std::vector<uint32_t>& ring = faces[face].second;
    std::vector<uint32_t> fan;
    if (options.sharedVertices) {
      fan = ring;
    } else {
      // The face's own corners, in its own plane: u across, v up, both
      // scaled to the ring's extent so the texture square lands on the
      // face whatever its shape.
      glm::vec3 x, y, z;
      basisFor(normal, {0, 1, 0}, &x, &y, &z);
      float lo = std::numeric_limits<float>::max(), hi = -lo;
      for (uint32_t index : ring) {
        lo = std::min({lo, dot(corners[index], x), dot(corners[index], y)});
        hi = std::max({hi, dot(corners[index], x), dot(corners[index], y)});
      }
      const float span = std::max(hi - lo, 1e-6f);
      for (uint32_t index : ring) {
        fan.push_back((uint32_t)out.positions.size());
        out.positions.push_back(corners[index]);
        out.normals.push_back(normal);
        out.uvs.emplace_back((dot(corners[index], x) - lo) / span,
                             (dot(corners[index], y) - lo) / span);
      }
    }
    for (size_t k = 1; k + 1 < fan.size(); ++k) {
      uint32_t tri[3] = {fan[0], fan[k], fan[k + 1]};
      orientTriangle(out.positions, tri, normal);
      out.indices.insert(out.indices.end(), {tri[0], tri[1], tri[2]});
      ids.emplace_back((float)face, 0, 0, 0);
    }
  }
  return out;
}

Mesh torus(float R, float r, int nu, int nv) {
  return grid(nu, nv, [=](float u, float v) -> glm::vec3 {
    const float theta = u * 2.0f * (float)M_PI;
    const float phi = v * 2.0f * (float)M_PI;
    const float ring = R + r * std::cos(phi);
    return {ring * std::cos(theta), r * std::sin(phi), -ring * std::sin(theta)};
  });
}

Mesh superellipsoid(glm::vec3 radii, float exponent, int nu, int nv) {
  const float p = 2.0f / std::max(exponent, 0.01f);
  auto shaped = [p](float c) {
    return (c < 0 ? -1.0f : 1.0f) * std::pow(std::abs(c), p);
  };
  return grid(nu, nv, [=](float u, float v) -> glm::vec3 {
    const float theta = u * 2.0f * (float)M_PI - (float)M_PI;
    // Full pole-to-pole sweep: shaped(cos(±pi/2)) = 0 closes the poles
    // to points; grid()'s fallback covers the degenerate normals there.
    const float phi = (v - 0.5f) * (float)M_PI;
    const float cp = std::cos(phi), sp = std::sin(phi);
    const float ct = std::cos(theta), st = std::sin(theta);
    return {radii.x * shaped(cp) * shaped(ct), radii.y * shaped(sp),
            -radii.z * shaped(cp) * shaped(st)};
  });
}

Mesh cylinderPanel(float width, float height, float radius, int nu, int nv) {
  if (radius <= 0 || !std::isfinite(radius)) return quad(width, height);
  const float arc = width / radius;
  return grid(nu, nv, [=](float u, float v) -> glm::vec3 {
    const float theta = (u - 0.5f) * arc;
    return {radius * std::sin(theta), (v - 0.5f) * height,
            radius * std::cos(theta) - radius};
  });
}

}  // namespace sigil::geometry::mesh
