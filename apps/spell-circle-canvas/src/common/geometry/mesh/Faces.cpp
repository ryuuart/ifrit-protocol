/** @file
 * The face readers and the face-up rotation.
 */

#include "sigilgeometry/mesh/Faces.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

#include "sigilgeometry/mesh/Vec.h"

namespace sigil::geometry::mesh {

namespace {

/** The `"Id"` lane when it is sized to the triangles, null otherwise —
 *  a short or absent lane is read as "one face per triangle". */
const std::vector<glm::vec4>* faceLane(const Mesh& mesh) {
  const std::vector<glm::vec4>* lane = mesh.primIf("Id");
  if (lane == nullptr || lane->size() != mesh.triangleCount()) return nullptr;
  return lane;
}

/** Which triangles carry face @p face. */
std::vector<size_t> trianglesOf(const Mesh& mesh, size_t face) {
  std::vector<size_t> out;
  const std::vector<glm::vec4>* lane = faceLane(mesh);
  if (lane == nullptr) {
    if (face < mesh.triangleCount()) out.push_back(face);
    return out;
  }
  for (size_t t = 0; t < lane->size(); ++t)
    if ((size_t)std::lround((*lane)[t].x) == face) out.push_back(t);
  return out;
}

}  // namespace

size_t faceCount(const Mesh& mesh) {
  const std::vector<glm::vec4>* lane = faceLane(mesh);
  if (lane == nullptr) return mesh.triangleCount();
  long highest = -1;
  for (const glm::vec4& id : *lane)
    highest = std::max(highest, std::lround(id.x));
  return (size_t)(highest + 1);
}

glm::vec3 faceNormal(const Mesh& mesh, size_t face) {
  glm::vec3 sum{0, 0, 0};
  for (size_t t : trianglesOf(mesh, face)) {
    const glm::vec3& a = mesh.positions[mesh.indices[t * 3]];
    const glm::vec3& b = mesh.positions[mesh.indices[t * 3 + 1]];
    const glm::vec3& c = mesh.positions[mesh.indices[t * 3 + 2]];
    sum += glm::cross(b - a, c - a);  // length is twice the area
  }
  return normalized(sum, {0, 1, 0});
}

glm::vec3 faceCentroid(const Mesh& mesh, size_t face) {
  std::vector<uint32_t> corners;
  for (size_t t : trianglesOf(mesh, face))
    for (int k = 0; k < 3; ++k) {
      const uint32_t index = mesh.indices[t * 3 + (size_t)k];
      if (std::find(corners.begin(), corners.end(), index) == corners.end())
        corners.push_back(index);
    }
  if (corners.empty()) return {0, 0, 0};
  glm::vec3 sum{0, 0, 0};
  for (uint32_t index : corners) sum += mesh.positions[index];
  return sum / (float)corners.size();
}

std::optional<size_t> opposedFace(const Mesh& mesh, size_t face,
                                  float tolerance) {
  const size_t faces = faceCount(mesh);
  if (face >= faces) return std::nullopt;
  const glm::vec3 here = faceCentroid(mesh, face);
  // The tolerance is relative to how far the face stands from the
  // centre, so the same call holds for a solid of any size.
  const float slack = tolerance * std::max(glm::length(here), 1.0f);
  for (size_t other = 0; other < faces; ++other) {
    if (other == face) continue;
    if (glm::length(faceCentroid(mesh, other) + here) <= slack) return other;
  }
  return std::nullopt;
}

std::vector<Edge> edges(const Mesh& mesh) {
  const std::vector<glm::vec4>* lane = faceLane(mesh);
  std::vector<Edge> out;
  for (size_t t = 0; t < mesh.triangleCount(); ++t) {
    const size_t face = lane == nullptr ? t : (size_t)std::lround((*lane)[t].x);
    for (int e = 0; e < 3; ++e) {
      uint32_t from = mesh.indices[t * 3 + (size_t)e];
      uint32_t to = mesh.indices[t * 3 + (size_t)((e + 1) % 3)];
      if (from > to) std::swap(from, to);
      auto found = std::find_if(out.begin(), out.end(), [&](const Edge& had) {
        return had.from == from && had.to == to;
      });
      if (found == out.end()) {
        out.push_back({from, to, face, kNoFace});
      } else if (found->face != face) {
        found->opposite = face;
      } else {
        // Both triangles belong to one face, so the two of them are its
        // own fan and this is a seam across the inside of it.
        found->opposite = found->face;
      }
    }
  }
  std::erase_if(out, [](const Edge& e) { return e.opposite == e.face; });
  return out;
}

glm::mat4 faceUp(const Mesh& mesh, size_t face, glm::vec3 up) {
  const glm::vec3 from = faceNormal(mesh, face);
  const glm::vec3 to = normalized(up, {0, 1, 0});
  const float alignment = std::clamp(glm::dot(from, to), -1.0f, 1.0f);
  if (alignment > 1.0f - 1e-7f) return glm::mat4(1.0f);
  glm::vec3 axis = glm::cross(from, to);
  if (glm::dot(axis, axis) < 1e-12f) {
    // Antipodal: every axis perpendicular to the normal turns it onto
    // the target, so take the one the stamping basis would.
    glm::vec3 x, y, z;
    basisFor(from, {0, 1, 0}, &x, &y, &z);
    axis = x;
  }
  return glm::rotate(glm::mat4(1.0f), std::acos(alignment), normalized(axis));
}

}  // namespace sigil::geometry::mesh
