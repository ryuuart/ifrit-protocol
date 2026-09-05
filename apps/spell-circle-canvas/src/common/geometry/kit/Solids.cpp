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
#include "sigilgeometry/path/Numeric.h"
#include "sigilgeometry/path/Polyline.h"

namespace sigil::geometry::mesh {

// The outline resampling the extrusion and the lathe stand on lives in
// the path tier; a parameter here is called `path`, so the names are
// pulled in rather than spelled through it.
using path::flatten;
using path::Polyline;
using glm::cross;
using sigil::geometry::mesh::normalized;

namespace {

bool pointInRing(glm::vec2 p, const std::vector<glm::vec2>& ring) {
  bool inside = false;
  const size_t n = ring.size();
  for (size_t i = 0, j = n - 1; i < n; j = i++) {
    const glm::vec2 a = ring[i], b = ring[j];
    if ((a.y > p.y) != (b.y > p.y) &&
        p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x)
      inside = true;
  }
  return inside;
}

/** Ensure the geometric normal of triangle (i0,i1,i2) points along
 *  @p wanted, swapping winding when it does not. */
void orientTriangle(const std::vector<glm::vec3>& positions, uint32_t* tri,
                    glm::vec3 wanted) {
  const glm::vec3& p0 = positions[tri[0]];
  const glm::vec3& p1 = positions[tri[1]];
  const glm::vec3 n = cross(positions[tri[1]] - p0, positions[tri[2]] - p0);
  (void)p1;
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
  std::vector<int> depth(ringCount, 0);
  std::vector<int> parent(ringCount, -1);
  for (size_t i = 0; i < ringCount; ++i) {
    float bestArea = std::numeric_limits<float>::max();
    for (size_t j = 0; j < ringCount; ++j) {
      if (i == j) continue;
      if (pointInRing(rings[i].points[0], rings[j].points)) {
        ++depth[i];
        const float area = std::abs(rings[j].signedArea());
        if (area < bestArea) {
          bestArea = area;
          parent[i] = (int)j;
        }
      }
    }
  }

  // Canonical winding in y-up space: outers CCW (positive area), holes CW.
  for (size_t i = 0; i < ringCount; ++i) {
    const bool isHole = depth[i] % 2 == 1;
    const float area = rings[i].signedArea();
    if ((isHole && area > 0) || (!isHole && area < 0)) rings[i].reverse();
  }

  const float uvW = std::max(pathBounds.width(), 1.0f);
  const float uvH = std::max(pathBounds.height(), 1.0f);
  const float half = options.depth * 0.5f;

  // Caps: one earcut polygon per outer ring with its direct holes.
  auto addCap = [&](float z, bool front) {
    for (size_t i = 0; i < ringCount; ++i) {
      if (depth[i] % 2 != 0) continue;
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
        if (depth[h] % 2 == 1 && parent[h] == (int)i) pushRing(rings[h]);

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
    bool BoxOptions::*wanted;
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
