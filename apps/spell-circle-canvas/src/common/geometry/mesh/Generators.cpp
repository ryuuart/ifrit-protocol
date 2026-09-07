/** @file
 * The parametric sheet every stock surface is evaluated through, the flat
 * quad, and a primitive colour lane baked into unwelded vertex colours.
 */

#include <algorithm>
#include <cmath>
#include <vector>

#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/mesh/Vec.h"

namespace sigil::geometry::mesh {

using glm::cross;

Mesh grid(int nu, int nv, const std::function<glm::vec3(float, float)>& fn) {
  Mesh out;
  nu = std::max(nu, 2);
  nv = std::max(nv, 2);
  out.positions.reserve((size_t)nu * nv);
  out.uvs.reserve((size_t)nu * nv);
  out.normals.reserve((size_t)nu * nv);
  const float eps = 1e-3f;
  // A pole is where the sheet's two parameter directions collapse onto
  // one point: the cross product there has no length and so no direction
  // to normalize. Which entries those are has to be recorded while the
  // raw product is still in hand — once normalized, a pole is
  // indistinguishable from the unit fallback that replaced it.
  std::vector<size_t> degenerate;
  for (int j = 0; j < nv; ++j) {
    const float v = (float)j / (float)(nv - 1);
    for (int i = 0; i < nu; ++i) {
      const float u = (float)i / (float)(nu - 1);
      out.positions.push_back(fn(u, v));
      // Image-convention UVs: (0,0) samples the texture's TOP-left, so
      // v runs opposite the parameter (v param 0 is the sheet's bottom
      // in y-up space). Every renderer that samples one of these sheets
      // assumes it.
      out.uvs.emplace_back(u, 1.0f - v);
      const glm::vec3 du =
          fn(std::min(u + eps, 1.0f), v) - fn(std::max(u - eps, 0.0f), v);
      const glm::vec3 dv =
          fn(u, std::min(v + eps, 1.0f)) - fn(u, std::max(v - eps, 0.0f));
      const glm::vec3 raw = cross(du, dv);
      if (glm::dot(raw, raw) < 1e-16f) degenerate.push_back(out.normals.size());
      out.normals.push_back(normalized(raw));
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
  // The poles borrow the nearest normal that had a direction of its own,
  // nearest measured in vertex order. One sweep each way carries the last
  // such normal, and every pole takes whichever of the two is closer, so
  // the borrow costs one pass however many poles a sheet has. A sheet
  // with no valid normal anywhere keeps the unit fallback.
  if (!degenerate.empty() && degenerate.size() < out.normals.size()) {
    const size_t n = out.normals.size();
    std::vector<bool> pole(n, false);
    for (const size_t i : degenerate) pole[i] = true;
    std::vector<size_t> before(n, n), after(n, n);
    size_t last = n;
    for (size_t i = 0; i < n; ++i) {
      if (!pole[i]) last = i;
      before[i] = last;
    }
    last = n;
    for (size_t i = n; i-- > 0;) {
      if (!pole[i]) last = i;
      after[i] = last;
    }
    for (const size_t i : degenerate) {
      const size_t b = before[i], a = after[i];
      const size_t take = b == n ? a : (a == n ? b : (i - b <= a - i ? b : a));
      out.normals[i] = out.normals[take];
    }
  }
  return out;
}

Mesh quad(float width, float height) {
  return grid(2, 2, [=](float u, float v) -> glm::vec3 {
    return {(u - 0.5f) * width, (v - 0.5f) * height, 0};
  });
}

Mesh bakePrimColor(const Mesh& mesh, std::string_view lane) {
  const std::vector<glm::vec4>* prim = mesh.primIf(lane);
  const size_t tris = mesh.triangleCount();
  if (!prim || prim->size() != tris || tris == 0) return mesh;

  Mesh out;
  out.prims = mesh.prims;  // triangle order survives the unweld
  const bool normals = mesh.normals.size() == mesh.positions.size();
  const bool uvs = mesh.uvs.size() == mesh.positions.size();
  const bool colors = mesh.colors.size() == mesh.positions.size();
  out.positions.reserve(tris * 3);
  out.indices.reserve(tris * 3);
  if (normals) out.normals.reserve(tris * 3);
  if (uvs) out.uvs.reserve(tris * 3);
  out.colors.reserve(tris * 3);
  for (size_t t = 0; t < tris; ++t) {
    const glm::vec4 flat = (*prim)[t];
    for (size_t k = 0; k < 3; ++k) {
      const uint32_t idx = mesh.indices[t * 3 + k];
      out.indices.push_back((uint32_t)out.positions.size());
      out.positions.push_back(mesh.positions[idx]);
      if (normals) out.normals.push_back(mesh.normals[idx]);
      if (uvs) out.uvs.push_back(mesh.uvs[idx]);
      out.colors.push_back(colors ? mesh.colors[idx] * flat : flat);
    }
  }
  return out;
}

}  // namespace sigil::geometry::mesh
