/** @file
 * The mesh currency itself: the lane pads a merge fills with, append,
 * transform, normals from the triangles, bounds, and the primitive-lane
 * accessors.
 */

#include "sigilgeometry/mesh/Mesh.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/mat3x3.hpp>
#include <limits>

#include "sigilgeometry/mesh/Vec.h"

namespace sigil::geometry::mesh {

using glm::cross;

namespace {

/** The pad value Mesh::append fills a missing primitive lane with, BY
 *  NAME — a missing "Color" means white (an untinted primitive), every
 *  other lane zeros. The prim-class twin of Points.cpp's
 *  scalarDefault/colorDefault. */
glm::vec4 primDefault(std::string_view name) {
  return name == "Color" ? glm::vec4{1, 1, 1, 1} : glm::vec4{0, 0, 0, 0};
}

/** The pad Mesh::append fills a MISSING vertex normal with.
 *
 *  Deliberately NOT zero. A zero normal is degenerate three ways over:
 *  Mesh::transform's normalized() keeps a zero vector zero forever, so
 *  the pad would never recover; every lighting term (N.L, N.V, the GGX
 *  half-vector) collapses, so the padded half renders BLACK — a louder
 *  wrong than the unlit merge this pad exists to fix; and shader-side
 *  normalize() of a zero vector is undefined.
 *
 *  +Z is the library's standing answer for "no direction" —
 *  normalized's fallback and basisFor's axis both use it. It is
 *  unit length, survives every normalize() downstream, and shades the
 *  padded half like a flat card facing the default camera. Callers who
 *  want the geometric truth call computeNormals() on the merge: append
 *  concatenates lanes, it does not invent geometry. */
constexpr glm::vec3 kNormalPad{0, 0, 1};

/** A missing uv samples texel (0, 0) — no arbitrary "interesting"
 *  coordinate, and the same convention Cloud::append gives a missing
 *  "uv" lane (Points.cpp). */
constexpr glm::vec2 kUvPad{0, 0};

/** A missing vertex colour is WHITE — the multiplicative identity every
 *  consumer applies it as (render::drawMesh, world's vertex tint), so an
 *  untinted half of a merge keeps looking untinted. The vertex twin of
 *  primDefault("Color"). */
constexpr glm::vec4 kColorPad{1, 1, 1, 1};

}  // namespace

std::vector<glm::vec4>& Mesh::prim(const std::string& name, glm::vec4 fill) {
  std::vector<glm::vec4>& lane = prims[name];
  lane.resize(triangleCount(), fill);
  return lane;
}

const std::vector<glm::vec4>* Mesh::primIf(std::string_view name) const {
  auto it = prims.find(name);
  return it == prims.end() ? nullptr : &it->second;
}

void Mesh::append(const Mesh& other) {
  const uint32_t base = (uint32_t)positions.size();
  // The vertex count the merged mesh will have. Every per-vertex lane
  // below pads to exactly this, which is also what repairs a SHORT
  // incoming lane (see the colors/normals/uvs blocks).
  const size_t merged = positions.size() + other.positions.size();
  // Primitive lanes: union of both sides, each missing side padded with
  // the lane NAME's conventional default. Counted BEFORE the indices
  // grow, so the "ours" pad lands at the old triangle count.
  if (!prims.empty() || !other.prims.empty()) {
    const size_t oldTris = triangleCount();
    const size_t newTris = oldTris + other.triangleCount();
    for (auto& [name, lane] : prims) lane.resize(oldTris, primDefault(name));
    for (const auto& [name, lane] : other.prims) {
      std::vector<glm::vec4>& mine = prims[name];
      mine.resize(oldTris, primDefault(name));
      mine.insert(mine.end(), lane.begin(), lane.end());
    }
    // The trailing pad is what repairs a SHORT incoming prim lane: an
    // `other` lane holding fewer entries than other.triangleCount()
    // leaves `mine` under newTris after the insert, and this resize
    // tops it up by name. (A too-LONG lane is truncated by the same
    // call.) Verified by Mesh.AppendRepairsShortIncomingLanes.
    for (auto& [name, lane] : prims) lane.resize(newTris, primDefault(name));
  }
  // Color lanes get the same coherence dance as normals and uvs below,
  // for the same reason: consumers read "lane sized to positions" as
  // the tint-present bit for the WHOLE mesh, so a bare insert of a
  // short or absent incoming lane leaves the merge undersized. Pad ours
  // to the old count, take theirs, pad the tail to the new count — the
  // trailing resize also repairs a SHORT incoming lane.
  if (!colors.empty() || !other.colors.empty()) {
    colors.resize(positions.size(), kColorPad);
    colors.insert(colors.end(), other.colors.begin(), other.colors.end());
    colors.resize(merged, kColorPad);
  }
  // Normals and uvs get the SAME coherence dance, and for a sharper
  // reason than colors: every consumer reads "lane sized to positions"
  // as the presence bit for the whole mesh. render::drawMesh sets
  // hasNormals = normals.size() == positions.size(), so a bare insert
  // of a normal-less side (points::instance over a stamp with no
  // normals is the everyday source) silently drops lighting for the
  // MERGED mesh, not just for the half that lacked them. Same story for
  // uvs and texturing.
  // Pad ours to the old count, take theirs, pad the tail to the new
  // count — the trailing resize also repairs a short incoming lane.
  if (!normals.empty() || !other.normals.empty()) {
    normals.resize(positions.size(), kNormalPad);
    normals.insert(normals.end(), other.normals.begin(), other.normals.end());
    normals.resize(merged, kNormalPad);
  }
  if (!uvs.empty() || !other.uvs.empty()) {
    uvs.resize(positions.size(), kUvPad);
    uvs.insert(uvs.end(), other.uvs.begin(), other.uvs.end());
    uvs.resize(merged, kUvPad);
  }
  positions.insert(positions.end(), other.positions.begin(),
                   other.positions.end());
  indices.reserve(indices.size() + other.indices.size());
  for (uint32_t i : other.indices) indices.push_back(base + i);
}

void Mesh::transform(const glm::mat4& m) {
  for (glm::vec3& p : positions) p = glm::vec3(m * glm::vec4(p, 1));
  // Normals move by the inverse transpose of the upper-left 3x3 — a
  // non-uniform scale tilts a surface and its normal the opposite way,
  // so the plain matrix would leave every normal off the surface it
  // describes.
  const glm::mat3 basis(m);
  // The determinant is tested before the inverse is taken: glm's ends
  // with an unguarded divide by it, so a placement that collapsed an
  // axis would come back as infinities. A basis with no volume has no
  // normal transform, and leaving the normals alone is the answer.
  if (std::abs(glm::determinant(basis)) < 1e-12f) return;
  const glm::mat3 normalTransform = glm::inverseTranspose(basis);
  for (glm::vec3& n : normals) n = normalized(normalTransform * n, n);
}

void Mesh::computeNormals() {
  normals.assign(positions.size(), {0, 0, 0});
  for (size_t i = 0; i + 2 < indices.size(); i += 3) {
    const glm::vec3& p0 = positions[indices[i]];
    const glm::vec3& p1 = positions[indices[i + 1]];
    const glm::vec3& p2 = positions[indices[i + 2]];
    const glm::vec3 n = cross(p1 - p0, p2 - p0);  // area-weighted
    normals[indices[i]] += n;
    normals[indices[i + 1]] += n;
    normals[indices[i + 2]] += n;
  }
  for (glm::vec3& n : normals) n = normalized(n);
}

void Mesh::bounds(glm::vec3* lo, glm::vec3* hi) const {
  glm::vec3 mn = {std::numeric_limits<float>::max(),
                  std::numeric_limits<float>::max(),
                  std::numeric_limits<float>::max()};
  glm::vec3 mx = {-mn.x, -mn.y, -mn.z};
  for (const glm::vec3& p : positions) {
    mn = {std::min(mn.x, p.x), std::min(mn.y, p.y), std::min(mn.z, p.z)};
    mx = {std::max(mx.x, p.x), std::max(mx.y, p.y), std::max(mx.z, p.z)};
  }
  if (positions.empty()) mn = mx = {0, 0, 0};
  *lo = mn;
  *hi = mx;
}
}  // namespace sigil::geometry::mesh
