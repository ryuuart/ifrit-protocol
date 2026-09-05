/** @file
 * What is done to a cloud once it exists: positions jittered or
 * drifted by noise, the table a stamp rides its lanes by, and point
 * lanes promoted onto a stamped mesh's primitive lanes. The stamping
 * itself is an operator with a kernel and lives in `Stamp.cpp`.
 *
 * ONE VERB IS ONE FIELD. The two modifiers here are the chain
 * operators of the same names with their arguments applied directly —
 * `jitter` runs the operator's own kernel over the positions, and
 * `displaceNoise` reads the field `Noise` displaces by — so a cloud
 * perturbed with a chain and a cloud perturbed without one move by the
 * same offsets. A second arithmetic under one name would mean nobody
 * could say which of them a picture came from.
 */

#include <algorithm>
#include <cmath>
#include <vector>

#include "sigilgeometry/mesh/Vec.h"
#include "sigilgeometry/mesh/pop/Kernel.h"
#include "sigilgeometry/mesh/pop/Points.h"
#include "sigilgeometry/mesh/pop/Pop.h"

namespace sigil::geometry::mesh {

using glm::cross;

namespace points {

void jitter(Cloud& cloud, float amplitude, uint32_t seed) {
  const size_t n = cloud.size();
  if (n == 0) return;
  mesh::kernel::OpDispatch work;
  if (!mesh::kernel::describe(pop::Op{pop::Jitter{pop::Lane::P, amplitude, seed}}, n,
                        &work))
    return;
  // The kernel reads and writes one four-wide lane; the positions are
  // poured across it and back, which is the whole of what reaching the
  // operator without a chain costs.
  std::vector<glm::vec4> lane(n);
  for (size_t i = 0; i < n; ++i)
    lane[i] = {cloud.positions[i].x, cloud.positions[i].y,
               cloud.positions[i].z, 0};
  glm::vec4* const values = lane.data();
  mesh::kernel::run(work, values, values, values, values, values);
  for (size_t i = 0; i < n; ++i)
    cloud.positions[i] = {lane[i].x, lane[i].y, lane[i].z};
}

void displaceNoise(Cloud& cloud, float amplitude, float frequency,
                   uint32_t seed) {
  for (glm::vec3& p : cloud.positions)
    p += pop::noiseField(p, frequency, (float)seed) * amplitude;
}

InstanceOptions stampOptions(const Cloud& cloud) {
  InstanceOptions options;
  options.scaleLane = cloud.scalarIf("size") ? "size" : "";
  options.tintLane = cloud.colorIf("tint") ? "tint" : "";
  // "dir" is what a chain exports and "normal" what a generator or an
  // importer writes; either stands the stamp up, "dir" winning where
  // both are present.
  if (cloud.vectorIf("dir"))
    options.orientLane = "dir";
  else if (cloud.vectorIf("normal"))
    options.orientLane = "normal";
  return options;
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

}  // namespace sigil::geometry::mesh
