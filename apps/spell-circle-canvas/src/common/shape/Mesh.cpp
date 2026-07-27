#include "sigilshape/Mesh.h"

#include "sigilshape/Geometry.h"

#include <mapbox/earcut.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace sigil::shape {

namespace {

SkV3 cross(SkV3 a, SkV3 b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
          a.x * b.y - a.y * b.x};
}

SkV3 normalized(SkV3 v, SkV3 fallback = {0, 0, 1}) {
  const float len = v.length();
  return len < 1e-12f ? fallback : v * (1.0f / len);
}

} // namespace

void Mesh::append(const Mesh &other) {
  const uint32_t base = (uint32_t)positions.size();
  positions.insert(positions.end(), other.positions.begin(),
                   other.positions.end());
  normals.insert(normals.end(), other.normals.begin(), other.normals.end());
  uvs.insert(uvs.end(), other.uvs.begin(), other.uvs.end());
  indices.reserve(indices.size() + other.indices.size());
  for (uint32_t i : other.indices)
    indices.push_back(base + i);
}

void Mesh::transform(const SkM44 &m) {
  for (SkV3 &p : positions) {
    const SkV4 r = m * SkV4{p.x, p.y, p.z, 1};
    p = {r.x, r.y, r.z};
  }
  // Normals move by the inverse transpose of the upper-left 3x3.
  const float a00 = m.rc(0, 0), a01 = m.rc(0, 1), a02 = m.rc(0, 2);
  const float a10 = m.rc(1, 0), a11 = m.rc(1, 1), a12 = m.rc(1, 2);
  const float a20 = m.rc(2, 0), a21 = m.rc(2, 1), a22 = m.rc(2, 2);
  const float det = a00 * (a11 * a22 - a12 * a21) -
                    a01 * (a10 * a22 - a12 * a20) +
                    a02 * (a10 * a21 - a11 * a20);
  if (std::abs(det) < 1e-12f)
    return;
  const float inv = 1.0f / det;
  // inverse transpose, spelled directly from the adjugate
  const float n00 = (a11 * a22 - a12 * a21) * inv;
  const float n01 = (a12 * a20 - a10 * a22) * inv;
  const float n02 = (a10 * a21 - a11 * a20) * inv;
  const float n10 = (a02 * a21 - a01 * a22) * inv;
  const float n11 = (a00 * a22 - a02 * a20) * inv;
  const float n12 = (a01 * a20 - a00 * a21) * inv;
  const float n20 = (a01 * a12 - a02 * a11) * inv;
  const float n21 = (a02 * a10 - a00 * a12) * inv;
  const float n22 = (a00 * a11 - a01 * a10) * inv;
  for (SkV3 &n : normals) {
    n = normalized({n00 * n.x + n10 * n.y + n20 * n.z,
                    n01 * n.x + n11 * n.y + n21 * n.z,
                    n02 * n.x + n12 * n.y + n22 * n.z},
                   n);
  }
}

void Mesh::computeNormals() {
  normals.assign(positions.size(), {0, 0, 0});
  for (size_t i = 0; i + 2 < indices.size(); i += 3) {
    const SkV3 &p0 = positions[indices[i]];
    const SkV3 &p1 = positions[indices[i + 1]];
    const SkV3 &p2 = positions[indices[i + 2]];
    const SkV3 n = cross(p1 - p0, p2 - p0); // area-weighted
    normals[indices[i]] += n;
    normals[indices[i + 1]] += n;
    normals[indices[i + 2]] += n;
  }
  for (SkV3 &n : normals)
    n = normalized(n);
}

void Mesh::bounds(SkV3 *lo, SkV3 *hi) const {
  SkV3 mn = {std::numeric_limits<float>::max(),
             std::numeric_limits<float>::max(),
             std::numeric_limits<float>::max()};
  SkV3 mx = {-mn.x, -mn.y, -mn.z};
  for (const SkV3 &p : positions) {
    mn = {std::min(mn.x, p.x), std::min(mn.y, p.y), std::min(mn.z, p.z)};
    mx = {std::max(mx.x, p.x), std::max(mx.y, p.y), std::max(mx.z, p.z)};
  }
  if (positions.empty())
    mn = mx = {0, 0, 0};
  *lo = mn;
  *hi = mx;
}

namespace mesh {

namespace {

bool pointInRing(SkPoint p, const std::vector<SkPoint> &ring) {
  bool inside = false;
  const size_t n = ring.size();
  for (size_t i = 0, j = n - 1; i < n; j = i++) {
    const SkPoint a = ring[i], b = ring[j];
    if ((a.fY > p.fY) != (b.fY > p.fY) &&
        p.fX < (b.fX - a.fX) * (p.fY - a.fY) / (b.fY - a.fY) + a.fX)
      inside = true;
  }
  return inside;
}

/** Ensure the geometric normal of triangle (i0,i1,i2) points along
 *  @p wanted, swapping winding when it does not. */
void orientTriangle(const std::vector<SkV3> &positions, uint32_t *tri,
                    SkV3 wanted) {
  const SkV3 &p0 = positions[tri[0]];
  const SkV3 &p1 = positions[tri[1]];
  const SkV3 n = cross(positions[tri[1]] - p0, positions[tri[2]] - p0);
  (void)p1;
  if (n.dot(wanted) < 0)
    std::swap(tri[1], tri[2]);
}

} // namespace

Mesh extrude(const SkPath &path, const ExtrudeOptions &options) {
  Mesh out;
  std::vector<Polyline> rings = flatten(path, options.tolerance);
  std::erase_if(rings, [](const Polyline &r) { return r.points.size() < 3; });
  if (rings.empty())
    return out;

  // Center on the path bounds, flip y so the mesh sits in y-up space
  // with the artwork upright.
  const SkRect pathBounds = path.computeTightBounds();
  const SkPoint center = {pathBounds.centerX(), pathBounds.centerY()};
  for (Polyline &ring : rings)
    for (SkPoint &p : ring.points)
      p = {p.fX - center.fX, -(p.fY - center.fY)};

  // Even-odd containment depth: even = outer ring, odd = hole of the
  // innermost containing outer.
  const size_t ringCount = rings.size();
  std::vector<int> depth(ringCount, 0);
  std::vector<int> parent(ringCount, -1);
  for (size_t i = 0; i < ringCount; ++i) {
    float bestArea = std::numeric_limits<float>::max();
    for (size_t j = 0; j < ringCount; ++j) {
      if (i == j)
        continue;
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
    if ((isHole && area > 0) || (!isHole && area < 0))
      rings[i].reverse();
  }

  const float uvW = std::max(pathBounds.width(), 1.0f);
  const float uvH = std::max(pathBounds.height(), 1.0f);
  const float half = options.depth * 0.5f;

  // Caps: one earcut polygon per outer ring with its direct holes.
  auto addCap = [&](float z, bool front) {
    for (size_t i = 0; i < ringCount; ++i) {
      if (depth[i] % 2 != 0)
        continue;
      using EarPoint = std::array<double, 2>;
      std::vector<std::vector<EarPoint>> polygon;
      std::vector<const Polyline *> ringsUsed;
      auto pushRing = [&](const Polyline &ring) {
        std::vector<EarPoint> ear;
        ear.reserve(ring.points.size());
        for (const SkPoint &p : ring.points)
          ear.push_back({(double)p.fX, (double)p.fY});
        polygon.push_back(std::move(ear));
        ringsUsed.push_back(&ring);
      };
      pushRing(rings[i]);
      for (size_t h = 0; h < ringCount; ++h)
        if (depth[h] % 2 == 1 && parent[h] == (int)i)
          pushRing(rings[h]);

      const std::vector<uint32_t> tris =
          mapbox::earcut<uint32_t>(polygon);
      const uint32_t base = (uint32_t)out.positions.size();
      const SkV3 normal = {0, 0, front ? 1.0f : -1.0f};
      for (const Polyline *ring : ringsUsed) {
        for (const SkPoint &p : ring->points) {
          out.positions.push_back({p.fX, p.fY, z});
          out.normals.push_back(normal);
          out.uvs.push_back({(p.fX + uvW * 0.5f) / uvW,
                             1.0f - (p.fY + uvH * 0.5f) / uvH});
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
  if (options.frontCap)
    addCap(half, true);
  if (options.backCap)
    addCap(-half, false);

  // Walls: one flat-shaded quad per contour edge, outward normals from
  // the canonical winding.
  if (options.walls && options.depth > 0) {
    for (const Polyline &ring : rings) {
      const size_t n = ring.points.size();
      float arc = 0;
      float total = ring.length();
      if (total < 1e-6f)
        total = 1;
      for (size_t e = 0; e < n; ++e) {
        const SkPoint a = ring.points[e];
        const SkPoint b = ring.points[(e + 1) % n];
        SkVector edge = b - a;
        const float len = edge.length();
        if (len < 1e-9f)
          continue;
        // CCW outer ring in y-up space: outward = edge rotated -90.
        const SkV3 normal =
            normalized({edge.fY / len, -edge.fX / len, 0});
        const uint32_t base = (uint32_t)out.positions.size();
        const float u0 = arc / total, u1 = (arc + len) / total;
        out.positions.push_back({a.fX, a.fY, half});
        out.positions.push_back({b.fX, b.fY, half});
        out.positions.push_back({b.fX, b.fY, -half});
        out.positions.push_back({a.fX, a.fY, -half});
        for (int k = 0; k < 4; ++k)
          out.normals.push_back(normal);
        out.uvs.push_back({u0, 0});
        out.uvs.push_back({u1, 0});
        out.uvs.push_back({u1, 1});
        out.uvs.push_back({u0, 1});
        uint32_t tri0[3] = {base, base + 1, base + 2};
        uint32_t tri1[3] = {base, base + 2, base + 3};
        orientTriangle(out.positions, tri0, normal);
        orientTriangle(out.positions, tri1, normal);
        out.indices.insert(out.indices.end(),
                           {tri0[0], tri0[1], tri0[2], tri1[0], tri1[1],
                            tri1[2]});
        arc += len;
      }
    }
  }
  return out;
}

Mesh grid(int nu, int nv, const std::function<SkV3(float, float)> &fn) {
  Mesh out;
  nu = std::max(nu, 2);
  nv = std::max(nv, 2);
  out.positions.reserve((size_t)nu * nv);
  out.uvs.reserve((size_t)nu * nv);
  out.normals.reserve((size_t)nu * nv);
  const float eps = 1e-3f;
  for (int j = 0; j < nv; ++j) {
    const float v = (float)j / (float)(nv - 1);
    for (int i = 0; i < nu; ++i) {
      const float u = (float)i / (float)(nu - 1);
      out.positions.push_back(fn(u, v));
      // Image-convention UVs: (0,0) samples the texture's TOP-left, so
      // v runs opposite the parameter (v param 0 is the sheet's bottom
      // in y-up space). Both renderers (Space.h texs, SigilWorld)
      // assume this.
      out.uvs.push_back({u, 1.0f - v});
      const SkV3 du = fn(std::min(u + eps, 1.0f), v) -
                      fn(std::max(u - eps, 0.0f), v);
      const SkV3 dv = fn(u, std::min(v + eps, 1.0f)) -
                      fn(u, std::max(v - eps, 0.0f));
      out.normals.push_back(normalized(cross(du, dv)));
    }
  }
  for (int j = 0; j + 1 < nv; ++j) {
    for (int i = 0; i + 1 < nu; ++i) {
      const uint32_t a = (uint32_t)(j * nu + i);
      const uint32_t b = a + 1;
      const uint32_t c = a + (uint32_t)nu;
      const uint32_t d = c + 1;
      out.indices.insert(out.indices.end(), {a, b, d, a, d, c});
    }
  }
  // Degenerate-partial fallback (poles): borrow the nearest valid normal.
  for (size_t i = 0; i < out.normals.size(); ++i)
    if (out.normals[i].lengthSquared() < 0.5f) {
      for (size_t j = 1; j < out.normals.size(); ++j) {
        const size_t k = (i + j) % out.normals.size();
        if (out.normals[k].lengthSquared() > 0.5f) {
          out.normals[i] = out.normals[k];
          break;
        }
      }
    }
  return out;
}

Mesh revolve(const std::vector<SkPoint> &profile,
             const RevolveOptions &options) {
  Mesh out;
  if (profile.size() < 2)
    return out;
  const float sweep = options.sweepDeg * (float)M_PI / 180.0f;
  const int nu = std::max(options.segments, 3) + (options.close ? 1 : 0);
  const int nv = (int)profile.size();
  auto sample = [&](float v) -> SkPoint {
    const float f = v * (float)(nv - 1);
    const int i = std::clamp((int)f, 0, nv - 2);
    const float t = f - (float)i;
    const SkPoint a = profile[(size_t)i], b = profile[(size_t)i + 1];
    return {a.fX + (b.fX - a.fX) * t, a.fY + (b.fY - a.fY) * t};
  };
  return grid(nu, nv, [&](float u, float v) -> SkV3 {
    const SkPoint p = sample(v);
    const float theta = u * sweep;
    return {p.fX * std::cos(theta), p.fY, -p.fX * std::sin(theta)};
  });
}

Mesh torus(float R, float r, int nu, int nv) {
  return grid(nu, nv, [=](float u, float v) -> SkV3 {
    const float theta = u * 2.0f * (float)M_PI;
    const float phi = v * 2.0f * (float)M_PI;
    const float ring = R + r * std::cos(phi);
    return {ring * std::cos(theta), r * std::sin(phi),
            -ring * std::sin(theta)};
  });
}

Mesh superellipsoid(SkV3 radii, float exponent, int nu, int nv) {
  const float p = 2.0f / std::max(exponent, 0.01f);
  auto shaped = [p](float c) {
    return (c < 0 ? -1.0f : 1.0f) * std::pow(std::abs(c), p);
  };
  return grid(nu, nv, [=](float u, float v) -> SkV3 {
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

Mesh cylinderPanel(float width, float height, float radius, int nu,
                   int nv) {
  if (radius <= 0 || !std::isfinite(radius))
    return quad(width, height);
  const float arc = width / radius;
  return grid(nu, nv, [=](float u, float v) -> SkV3 {
    const float theta = (u - 0.5f) * arc;
    return {radius * std::sin(theta), (v - 0.5f) * height,
            radius * std::cos(theta) - radius};
  });
}

Mesh quad(float width, float height) {
  return grid(2, 2, [=](float u, float v) -> SkV3 {
    return {(u - 0.5f) * width, (v - 0.5f) * height, 0};
  });
}

} // namespace mesh

} // namespace sigil::shape
