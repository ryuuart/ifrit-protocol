/** @file
 * The STL reader, parsed by hand: binary files by their size arithmetic
 * (an 80-byte header, a triangle count, fifty bytes a triangle), ascii
 * files by their facet grammar. Every triangle is three fresh vertices
 * wearing the facet normal, or the geometric normal when the file's is
 * zero.
 */

#include <cstring>
#include <sstream>
#include <string>

#include "Internal.h"

namespace sigil::geometry::decode::detail {

namespace {

void pushStlTriangle(Mesh& mesh, const glm::vec3 corners[3], glm::vec3 normal) {
  if (glm::dot(normal, normal) < 1e-12f) {
    const glm::vec3 a = corners[1] - corners[0];
    const glm::vec3 b = corners[2] - corners[0];
    normal = {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
              a.x * b.y - a.y * b.x};
  }
  const float length = glm::length(normal);
  if (length > 1e-12f)
    normal = normal * (1.0f / length);
  else
    normal = {0, 0, 1};
  for (int i = 0; i < 3; ++i) {
    mesh.indices.push_back((uint32_t)mesh.positions.size());
    mesh.positions.push_back(corners[i]);
    mesh.normals.push_back(normal);
    mesh.uvs.emplace_back(0, 0);
  }
}

std::optional<Model> importStlBinary(const std::byte* bytes, size_t size) {
  uint32_t count = 0;
  std::memcpy(&count, bytes + 80, 4);
  Part part;
  const std::byte* cursor = bytes + 84;
  for (uint32_t t = 0; t < count; ++t, cursor += 50) {
    float f[12];
    std::memcpy(f, cursor, sizeof(f));
    const glm::vec3 corners[3] = {
        {f[3], f[4], f[5]}, {f[6], f[7], f[8]}, {f[9], f[10], f[11]}};
    pushStlTriangle(part.mesh, corners, {f[0], f[1], f[2]});
  }
  (void)size;
  if (part.mesh.indices.empty()) return std::nullopt;
  Model out;
  out.parts.push_back(std::move(part));
  return out;
}

std::optional<Model> importStlAscii(std::string_view text) {
  std::istringstream words{std::string(text)};
  std::string word;
  Part part;
  glm::vec3 normal = {0, 0, 0};
  glm::vec3 corners[3];
  int corner = 0;
  bool named = false;
  while (words >> word) {
    if (word == "solid" && !named) {
      std::string rest;
      std::getline(words, rest);
      const size_t start = rest.find_first_not_of(" \t\r");
      part.name = start == std::string::npos ? "" : rest.substr(start);
      named = true;
    } else if (word == "facet") {
      words >> word;  // "normal"
      words >> normal.x >> normal.y >> normal.z;
      corner = 0;
    } else if (word == "vertex" && corner < 3) {
      words >> corners[corner].x >> corners[corner].y >> corners[corner].z;
      if (++corner == 3) pushStlTriangle(part.mesh, corners, normal);
    }
  }
  if (part.mesh.indices.empty()) return std::nullopt;
  Model out;
  out.parts.push_back(std::move(part));
  return out;
}

}  // namespace

/** 80-byte header, uint32 triangle count, 50 bytes a triangle: the
 *  arithmetic identifies binary STL beyond doubt (some binary files
 *  even start with "solid"). */
bool looksLikeBinaryStl(const std::byte* bytes, size_t size) {
  if (size < 84) return false;
  uint32_t count = 0;
  std::memcpy(&count, bytes + 80, 4);
  return 84 + 50ull * count == size;
}

std::optional<Model> importStl(const std::byte* bytes, size_t size) {
  if (looksLikeBinaryStl(bytes, size)) return importStlBinary(bytes, size);
  return importStlAscii(asText(bytes, size));
}

bool looksLikeAsciiStl(std::string_view text) {
  const size_t start = text.find_first_not_of(" \t\r\n");
  return start != std::string_view::npos &&
         text.substr(start).starts_with("solid") &&
         text.find("facet") != std::string_view::npos;
}

}  // namespace sigil::geometry::decode::detail
