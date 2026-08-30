/** @file
 * The types every reader produces and the doors every caller enters
 * by: a Part poured into a Cloud, a Model's counts, bounds, merge and
 * fit, and the dispatcher that picks a reader by extension or by
 * sniffing the bytes — plus the file overload that resolves a model's
 * sibling references against its directory.
 */

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
#include <string>

#include "Internal.h"
#include "sigilgeometry/codec/Decode.h"

namespace sigil::geometry::decode {

namespace detail {

std::string_view asText(const void* bytes, size_t size) {
  return {static_cast<const char*>(bytes), size};
}

std::string lowerExtension(std::string_view pathHint) {
  const size_t slash = pathHint.find_last_of("/\\");
  const size_t dot = pathHint.rfind('.');
  if (dot == std::string_view::npos ||
      (slash != std::string_view::npos && dot < slash))
    return {};
  std::string ext(pathHint.substr(dot + 1));
  for (char& c : ext) c = (char)std::tolower((unsigned char)c);
  return ext;
}

/** Every importer's closing chores: lanes sized to positions (the Mesh
 *  contract), normals derived from the triangles when the file carried
 *  none. */
void finishPart(Part& part, bool hasNormals) {
  Mesh& mesh = part.mesh;
  if (!hasNormals || mesh.normals.size() != mesh.positions.size())
    mesh.computeNormals();
  if (mesh.uvs.size() != mesh.positions.size())
    mesh.uvs.resize(mesh.positions.size(), {0, 0});
  if (!mesh.colors.empty() && mesh.colors.size() != mesh.positions.size())
    mesh.colors.resize(mesh.positions.size(), kWhite);
}
}  // namespace detail

using namespace detail;

Cloud Part::asCloud() const {
  Cloud out;
  out.positions = mesh.positions;
  const size_t n = out.positions.size();
  std::vector<float>& t = out.scalar("t");
  for (size_t i = 0; i < n; ++i)
    t[i] = n > 1 ? (float)i / (float)(n - 1) : 0.0f;
  if (mesh.normals.size() == n) out.vectors["normal"] = mesh.normals;
  if (mesh.uvs.size() == n) {
    std::vector<glm::vec4>& uv = out.color("uv", {0, 0, 0, 0});
    for (size_t i = 0; i < n; ++i) uv[i] = {mesh.uvs[i].x, mesh.uvs[i].y, 0, 0};
  }
  if (mesh.colors.size() == n) out.colors["tint"] = mesh.colors;
  for (const auto& [name, lane] : scalarLanes)
    if (lane.size() == n) out.scalars[name] = lane;
  for (const auto& [name, lane] : vectorLanes)
    if (lane.size() == n) out.vectors[name] = lane;
  for (const auto& [name, lane] : colorLanes)
    if (lane.size() == n) out.colors[name] = lane;
  return out;
}

int Model::materialSlotCount() const {
  int slots = 0;
  for (const Part& part : parts)
    slots = std::max(slots, part.materialIndex + 1);
  return slots;
}

Cloud Model::mergedCloud() const {
  Cloud out;
  for (const Part& part : parts) out.append(part.asCloud());
  return out;
}

size_t Model::vertexCount() const {
  size_t count = 0;
  for (const Part& part : parts) count += part.mesh.vertexCount();
  return count;
}

size_t Model::triangleCount() const {
  size_t count = 0;
  for (const Part& part : parts) count += part.mesh.triangleCount();
  return count;
}

void Model::bounds(glm::vec3* lo, glm::vec3* hi) const {
  glm::vec3 mn = {std::numeric_limits<float>::max(),
                  std::numeric_limits<float>::max(),
                  std::numeric_limits<float>::max()};
  glm::vec3 mx = {-mn.x, -mn.y, -mn.z};
  bool any = false;
  for (const Part& part : parts) {
    if (part.mesh.positions.empty()) continue;
    glm::vec3 partLo, partHi;
    part.mesh.bounds(&partLo, &partHi);
    mn = {std::min(mn.x, partLo.x), std::min(mn.y, partLo.y),
          std::min(mn.z, partLo.z)};
    mx = {std::max(mx.x, partHi.x), std::max(mx.y, partHi.y),
          std::max(mx.z, partHi.z)};
    any = true;
  }
  if (!any) mn = mx = {0, 0, 0};
  *lo = mn;
  *hi = mx;
}

Mesh Model::merged() const {
  Mesh out;
  for (const Part& part : parts) {
    Mesh mesh = part.mesh;
    if (!(part.baseColor == kWhite)) {
      if (mesh.colors.empty())
        mesh.colors.assign(mesh.positions.size(), part.baseColor);
      else
        for (glm::vec4& c : mesh.colors) c *= part.baseColor;
    }
    out.append(mesh);
  }
  return out;
}

glm::mat4 Model::fitTransform(float size) const {
  glm::vec3 lo, hi;
  bounds(&lo, &hi);
  const glm::vec3 center = (lo + hi) * 0.5f;
  const glm::vec3 extent = hi - lo;
  const float largest = std::max(extent.x, std::max(extent.y, extent.z));
  const float scale = largest > 1e-12f ? size / largest : 1;
  return glm::scale(glm::mat4(1.0f), {scale, scale, scale}) *
         glm::translate(glm::mat4(1.0f), -center);
}

std::optional<Model> model(const void* bytes, size_t size,
                           std::string_view pathHint, const Resolver& resolve) {
  if (!bytes || size == 0) return std::nullopt;
  const auto* data = static_cast<const std::byte*>(bytes);
  const std::string ext = lowerExtension(pathHint);
  if (ext == "obj") return importObj(asText(bytes, size), resolve);
  if (ext == "gltf" || ext == "glb")
    return importGltf(bytes, size, pathHint, resolve);
  if (ext == "stl") return importStl(data, size);
  if (ext == "ply") return importPly(data, size);
  if (ext == "abc") return alembic(bytes, size);
  if (ext == "geo") return importHoudiniGeo(asText(bytes, size));

  // No useful extension: sniff. GLB and Ogawa magics and JSON are
  // unambiguous; binary STL is identified by its size arithmetic.
  if (size >= 4 && std::memcmp(bytes, "glTF", 4) == 0)
    return importGltf(bytes, size, pathHint, resolve);
  if (size >= 5 && std::memcmp(bytes, "Ogawa", 5) == 0)
    return alembic(bytes, size);
  const std::string_view text = asText(bytes, size);
  const size_t start = text.find_first_not_of(" \t\r\n");
  if (start != std::string_view::npos && text[start] == '{')
    return importGltf(bytes, size, pathHint, resolve);
  if (looksLikeHoudiniGeo(text)) return importHoudiniGeo(text);
  if (looksLikePly(text)) return importPly(data, size);
  if (looksLikeBinaryStl(data, size) || looksLikeAsciiStl(text))
    return importStl(data, size);
  return std::nullopt;
}

std::optional<Model> model(const std::filesystem::path& file) {
  std::ifstream stream(file, std::ios::binary | std::ios::ate);
  if (!stream) return std::nullopt;
  const std::streamsize size = stream.tellg();
  stream.seekg(0);
  std::vector<std::byte> bytes((size_t)size);
  if (!stream.read(reinterpret_cast<char*>(bytes.data()), size))
    return std::nullopt;
  const std::filesystem::path dir = file.parent_path();
  const Resolver siblings =
      [dir](std::string_view uri) -> std::optional<std::vector<std::byte>> {
    std::ifstream ref(dir / std::filesystem::path(std::string(uri)),
                      std::ios::binary | std::ios::ate);
    if (!ref) return std::nullopt;
    const std::streamsize refSize = ref.tellg();
    ref.seekg(0);
    std::vector<std::byte> refBytes((size_t)refSize);
    if (!ref.read(reinterpret_cast<char*>(refBytes.data()), refSize))
      return std::nullopt;
    return refBytes;
  };
  return model(bytes.data(), bytes.size(), file.filename().string(), siblings);
}

}  // namespace sigil::geometry::decode
