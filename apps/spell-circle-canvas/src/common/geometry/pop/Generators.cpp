/** @file
 * The cloud generators: points along a spline, on a grid, around a
 * ring, scattered through a box, and scattered over a mesh's faces by
 * area — each writing the conventional lanes its consumers read back
 * by name.
 */

#include <algorithm>
#include <cmath>
#include <numeric>

#include "sigilgeometry/mesh/Vec.h"
#include "sigilgeometry/path/Noise.h"
#include "sigilgeometry/pop/Points.h"

namespace sigil::geometry {

using glm::cross;

namespace points {

Cloud onSpline(const Spline3& spline, int count, glm::vec3 up) {
  Cloud out;
  const std::vector<Frame3> rail = curves::frames(spline, count, up);
  out.positions.reserve(rail.size());
  std::vector<float> t;
  std::vector<glm::vec3> tangent, normal, binormal;
  for (const Frame3& f : rail) {
    out.positions.push_back(f.position);
    t.push_back(f.t);
    tangent.push_back(f.tangent);
    normal.push_back(f.normal);
    binormal.push_back(f.binormal);
  }
  out.scalars["t"] = std::move(t);
  out.vectors["tangent"] = std::move(tangent);
  out.vectors["normal"] = std::move(normal);
  out.vectors["binormal"] = std::move(binormal);
  return out;
}

Cloud grid(glm::vec3 origin, glm::vec3 du, glm::vec3 dv, int nu, int nv) {
  Cloud out;
  nu = std::max(nu, 1);
  nv = std::max(nv, 1);
  const glm::vec3 n = normalized(cross(du, dv));
  const size_t total = (size_t)nu * (size_t)nv;
  out.positions.reserve(total);
  for (int j = 0; j < nv; ++j)
    for (int i = 0; i < nu; ++i) {
      const float fu = nu > 1 ? (float)i / (float)(nu - 1) : 0.0f;
      const float fv = nv > 1 ? (float)j / (float)(nv - 1) : 0.0f;
      out.positions.push_back(origin + du * fu + dv * fv);
    }
  std::vector<float>& t = out.scalar("t");
  for (size_t i = 0; i < total; ++i)
    t[i] = total > 1 ? (float)i / (float)(total - 1) : 0.0f;
  out.vector("normal", n);
  return out;
}

Cloud ring(glm::vec3 center, float radius, int count, glm::vec3 axis) {
  Cloud out;
  count = std::max(count, 1);
  glm::vec3 x, y, z;
  basisFor(axis,
           std::abs(axis.y) > 0.9f ? glm::vec3{1, 0, 0} : glm::vec3{0, 1, 0},
           &x, &y, &z);
  out.positions.reserve((size_t)count);
  for (int i = 0; i < count; ++i) {
    const float a = (float)i / (float)count * 2.0f * (float)M_PI;
    out.positions.push_back(center + x * (radius * std::cos(a)) +
                            y * (radius * std::sin(a)));
  }
  std::vector<float>& t = out.scalar("t");
  std::vector<glm::vec3>& normal = out.vector("normal");
  for (int i = 0; i < count; ++i) {
    t[(size_t)i] = (float)i / (float)count;
    normal[(size_t)i] = normalized(out.positions[(size_t)i] - center);
  }
  return out;
}

Cloud scatterBox(glm::vec3 lo, glm::vec3 hi, int count, uint32_t seed) {
  Cloud out;
  count = std::max(count, 0);
  uint32_t state = seed * 2654435761u + 1u;
  out.positions.reserve((size_t)count);
  for (int i = 0; i < count; ++i) {
    out.positions.push_back({lo.x + (hi.x - lo.x) * noise::pcgUnitNext(state),
                             lo.y + (hi.y - lo.y) * noise::pcgUnitNext(state),
                             lo.z + (hi.z - lo.z) * noise::pcgUnitNext(state)});
  }
  std::vector<float>& t = out.scalar("t");
  for (int i = 0; i < count; ++i)
    t[(size_t)i] = count > 1 ? (float)i / (float)(count - 1) : 0.0f;
  return out;
}

Cloud onMesh(const Mesh& mesh, int count, uint32_t seed) {
  Cloud out;
  const size_t triangles = mesh.triangleCount();
  if (triangles == 0 || count <= 0) return out;

  // Cumulative area table for area-weighted triangle picks.
  std::vector<double> cumulative(triangles + 1, 0.0);
  for (size_t t = 0; t < triangles; ++t) {
    const glm::vec3& a = mesh.positions[mesh.indices[t * 3]];
    const glm::vec3& b = mesh.positions[mesh.indices[t * 3 + 1]];
    const glm::vec3& c = mesh.positions[mesh.indices[t * 3 + 2]];
    cumulative[t + 1] =
        cumulative[t] + 0.5 * (double)glm::length(cross(b - a, c - a));
  }
  const double total = cumulative.back();
  if (total <= 0) return out;

  uint32_t state = seed * 2654435761u + 17u;
  out.positions.reserve((size_t)count);
  std::vector<glm::vec3> pickedNormals;
  pickedNormals.reserve((size_t)count);
  for (int i = 0; i < count; ++i) {
    const double target = (double)noise::pcgUnitNext(state) * total;
    const size_t t = (size_t)(std::upper_bound(cumulative.begin(),
                                               cumulative.end(), target) -
                              cumulative.begin()) -
                     1;
    const size_t tri = std::min(t, triangles - 1);
    // Uniform barycentric.
    float u = noise::pcgUnitNext(state), v = noise::pcgUnitNext(state);
    if (u + v > 1) {
      u = 1 - u;
      v = 1 - v;
    }
    const uint32_t i0 = mesh.indices[tri * 3];
    const uint32_t i1 = mesh.indices[tri * 3 + 1];
    const uint32_t i2 = mesh.indices[tri * 3 + 2];
    const glm::vec3& a = mesh.positions[i0];
    const glm::vec3& b = mesh.positions[i1];
    const glm::vec3& c = mesh.positions[i2];
    out.positions.push_back(a + (b - a) * u + (c - a) * v);
    if (mesh.normals.size() == mesh.positions.size()) {
      pickedNormals.push_back(normalized(mesh.normals[i0] * (1 - u - v) +
                                         mesh.normals[i1] * u +
                                         mesh.normals[i2] * v));
    } else {
      pickedNormals.push_back(normalized(cross(b - a, c - a)));
    }
  }
  out.vectors["normal"] = std::move(pickedNormals);
  std::vector<float>& t = out.scalar("t");
  for (int i = 0; i < count; ++i)
    t[(size_t)i] = count > 1 ? (float)i / (float)(count - 1) : 0.0f;
  return out;
}
}  // namespace points

}  // namespace sigil::geometry
