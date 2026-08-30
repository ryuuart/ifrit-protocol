/** @file
 * What is done to a cloud once it exists: positions jittered or
 * drifted by noise, a stamp mesh instanced at every point into one
 * merged mesh, and point lanes promoted onto the merged mesh's
 * primitive lanes.
 */

#include <algorithm>
#include <cmath>

#include "sigilgeometry/mesh/Vec.h"
#include "sigilgeometry/path/Noise.h"
#include "sigilgeometry/pop/Points.h"

namespace sigil::geometry {

using glm::cross;

namespace points {

void jitter(Cloud& cloud, float amplitude, uint32_t seed) {
  uint32_t state = seed * 2654435761u + 101u;
  for (glm::vec3& p : cloud.positions)
    p += glm::vec3{(noise::pcgUnitNext(state) * 2 - 1) * amplitude,
                   (noise::pcgUnitNext(state) * 2 - 1) * amplitude,
                   (noise::pcgUnitNext(state) * 2 - 1) * amplitude};
}

void displaceNoise(Cloud& cloud, float amplitude, float frequency,
                   uint32_t seed) {
  for (glm::vec3& p : cloud.positions) {
    const glm::vec3 q = p * frequency;
    p += glm::vec3{noise::value3(q, seed) * amplitude,
                   noise::value3(q + glm::vec3{31.7f, 0, 0}, seed) * amplitude,
                   noise::value3(q + glm::vec3{0, 47.3f, 0}, seed) * amplitude};
  }
}

Mesh instance(const Cloud& cloud, const Mesh& stamp,
              const InstanceOptions& options) {
  Mesh out;
  const size_t n = cloud.size();
  const size_t stampVerts = stamp.vertexCount();
  out.positions.reserve(n * stampVerts);
  out.normals.reserve(n * stampVerts);
  out.uvs.reserve(n * stampVerts);
  out.indices.reserve(n * stamp.indices.size());

  const std::vector<float>* scaleLane =
      options.scaleLane.empty() ? nullptr : cloud.scalarIf(options.scaleLane);
  const std::vector<glm::vec4>* tintLane =
      options.tintLane.empty() ? nullptr : cloud.colorIf(options.tintLane);
  const std::vector<glm::vec3>* orientLane =
      options.orientLane.empty() ? nullptr : cloud.vectorIf(options.orientLane);
  const bool tinted = tintLane != nullptr || !stamp.colors.empty();

  for (size_t i = 0; i < n; ++i) {
    const float s =
        options.scale *
        (scaleLane && i < scaleLane->size() ? (*scaleLane)[i] : 1.0f);
    glm::vec3 bx{1, 0, 0}, by{0, 1, 0}, bz{0, 0, 1};
    if (orientLane && i < orientLane->size())
      basisFor((*orientLane)[i], options.up, &bx, &by, &bz);
    const glm::vec3 origin = cloud.positions[i];
    const uint32_t base = (uint32_t)out.positions.size();
    for (size_t v = 0; v < stampVerts; ++v) {
      const glm::vec3& p = stamp.positions[v];
      out.positions.push_back(origin + (bx * p.x + by * p.y + bz * p.z) * s);
      if (v < stamp.normals.size()) {
        const glm::vec3& nrm = stamp.normals[v];
        out.normals.push_back(bx * nrm.x + by * nrm.y + bz * nrm.z);
      }
      if (v < stamp.uvs.size()) out.uvs.push_back(stamp.uvs[v]);
      if (tinted) {
        glm::vec4 tint = tintLane && i < tintLane->size()
                             ? (*tintLane)[i]
                             : glm::vec4{1, 1, 1, 1};
        if (v < stamp.colors.size()) tint *= stamp.colors[v];
        out.colors.push_back(tint);
      }
    }
    for (uint32_t idx : stamp.indices) out.indices.push_back(base + idx);
  }
  return out;
}

Mesh quads(const Cloud& cloud, float width, float height,
           const InstanceOptions& options) {
  return instance(cloud, mesh::quad(width, height), options);
}

void promoteToPrims(Mesh& mesh, const Cloud& cloud, std::string_view cloudLane,
                    const std::string& primLane) {
  const size_t points = cloud.size();
  const size_t tris = mesh.triangleCount();
  if (points == 0 || tris == 0 || tris % points != 0) return;
  const size_t perPoint = tris / points;

  // "Id" is the reserved source: the owning point's own index, the
  // only per-piece identity this layer needs (a stamp instance is a
  // run of triangles sharing an Id value, not a separate container).
  const bool wantId = cloudLane == "Id";
  const std::vector<float>* scalars =
      wantId ? nullptr : cloud.scalarIf(cloudLane);
  const std::vector<glm::vec3>* vectors =
      wantId ? nullptr : cloud.vectorIf(cloudLane);
  const std::vector<glm::vec4>* colors =
      wantId ? nullptr : cloud.colorIf(cloudLane);
  if (!wantId && !scalars && !vectors && !colors) return;

  std::vector<glm::vec4>& lane = mesh.prim(primLane);
  lane.assign(tris, glm::vec4{0, 0, 0, 0});
  for (size_t i = 0; i < points; ++i) {
    glm::vec4 value{0, 0, 0, 0};
    if (wantId) {
      value = {(float)i, 0, 0, 0};
    } else if (scalars) {
      if (i >= scalars->size()) continue;
      const float s = (*scalars)[i];
      value = {s, s, s, s};
    } else if (vectors) {
      if (i >= vectors->size()) continue;
      const glm::vec3& v = (*vectors)[i];
      value = {v.x, v.y, v.z, 0};
    } else {
      if (i >= colors->size()) continue;
      value = (*colors)[i];
    }
    for (size_t k = 0; k < perPoint; ++k) lane[i * perPoint + k] = value;
  }
}
}  // namespace points

}  // namespace sigil::geometry
