#include "sigilgeometry/Save.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>

namespace sigil::geometry::save {

namespace {

int quantize(float v) {
  // Casting a NaN to int is UB and infinities clamp misleadingly;
  // non-finite channels flatten to 0 before the byte math.
  if (!std::isfinite(v)) v = 0;
  return (int)std::clamp(v * 255.0f + 0.5f, 0.0f, 255.0f);
}

/** Lane names land verbatim in the header's whitespace-delimited
 *  grammar, so a name containing whitespace (or nothing at all) would
 *  corrupt it. Both the header walk and the row walk consult this ONE
 *  predicate — they must stay in lockstep. */
bool exportableLaneName(const std::string& name) {
  return !name.empty() &&
         std::none_of(name.begin(), name.end(),
                      [](unsigned char c) { return std::isspace(c) != 0; });
}

// One row grammar, two spellings: the ascii sink speaks %g tokens with
// spaces between, the binary sink appends raw little-endian bytes
// (the host is little-endian, so memcpy IS the encoding). Both bodies
// walk the same lane selection below, so the header and the rows can
// never disagree about property order.
struct Sink {
  std::string& out;
  const bool binary;
  bool rowStart = true;

  void separate() {
    if (!binary && !rowStart) out += ' ';
    rowStart = false;
  }
  void put(float v) {
    separate();
    if (binary) {
      char raw[sizeof v];
      std::memcpy(raw, &v, sizeof v);
      out.append(raw, sizeof v);
      return;
    }
    // %g would spell NaN/inf as tokens our own importer (and most PLY
    // readers) cannot parse mid-row; non-finite values write as 0 so
    // the ascii file always parses. Binary stays bit-exact.
    char buffer[48];
    std::snprintf(buffer, sizeof(buffer), "%g",
                  std::isfinite(v) ? (double)v : 0.0);
    out += buffer;
  }
  void put(glm::vec3 v) {
    put(v.x);
    put(v.y);
    put(v.z);
  }
  void putUchar(uint8_t v) {
    separate();
    if (binary)
      out += (char)v;
    else
      out += std::to_string((unsigned)v);
  }
  void putInt(int32_t v) {
    separate();
    if (binary) {
      char raw[sizeof v];
      std::memcpy(raw, &v, sizeof v);
      out.append(raw, sizeof v);
      return;
    }
    out += std::to_string(v);
  }
  void putColorUchar(glm::vec4 c) {
    for (int i = 0; i < 4; ++i) putUchar((uint8_t)quantize(c[i]));
  }
  void endRow() {
    if (!binary) out += '\n';
    rowStart = true;
  }
};

std::string header(size_t vertexCount, bool binary) {
  std::string out = "ply\nformat ";
  out += binary ? "binary_little_endian" : "ascii";
  out +=
      " 1.0\n"
      "comment SigilGeometry export\n"
      "element vertex ";
  out += std::to_string(vertexCount);
  out += '\n';
  return out;
}

constexpr const char* kPositionProps =
    "property float x\nproperty float y\nproperty float z\n";
constexpr const char* kNormalProps =
    "property float nx\nproperty float ny\nproperty float nz\n";
constexpr const char* kUvProps = "property float s\nproperty float t\n";
constexpr const char* kColorProps =
    "property uchar red\nproperty uchar green\nproperty uchar blue\n"
    "property uchar alpha\n";

}  // namespace

std::string ply(const Cloud& cloud, const PlyOptions& options) {
  const size_t n = cloud.size();
  // Zero vertices would emit a PLY our own importer refuses; decline
  // with an empty string instead (the file overloads turn it false).
  if (n == 0) return {};
  std::string out = header(n, options.binary);

  // Conventional lanes take conventional names; everything else keeps
  // its own, suffixed per component so the importer can fold it back.
  const std::vector<glm::vec3>* normal = cloud.vectorIf("normal");
  if (normal && normal->size() != n) normal = nullptr;
  const std::vector<glm::vec4>* uv = cloud.colorIf("uv");
  if (uv && uv->size() != n) uv = nullptr;
  const std::vector<glm::vec4>* tint = cloud.colorIf("tint");
  if (tint && tint->size() != n) tint = nullptr;

  out += kPositionProps;
  if (normal) out += kNormalProps;
  if (uv) out += kUvProps;
  if (tint) out += kColorProps;
  for (const auto& [name, lane] : cloud.scalars)
    if (lane.size() == n && exportableLaneName(name))
      out += "property float " + name + "\n";
  for (const auto& [name, lane] : cloud.vectors)
    if (lane.size() == n && name != "normal" && exportableLaneName(name))
      for (const char* axis : {"_x", "_y", "_z"})
        out += "property float " + name + axis + "\n";
  for (const auto& [name, lane] : cloud.colors)
    if (lane.size() == n && name != "uv" && name != "tint" &&
        exportableLaneName(name))
      for (const char* channel : {"_r", "_g", "_b", "_a"})
        out += "property float " + name + channel + "\n";
  out += "end_header\n";

  Sink sink{out, options.binary};
  for (size_t i = 0; i < n; ++i) {
    sink.put(cloud.positions[i]);
    if (normal) sink.put((*normal)[i]);
    if (uv) {
      sink.put((*uv)[i].x);
      sink.put((*uv)[i].y);
    }
    if (tint) sink.putColorUchar((*tint)[i]);
    for (const auto& [name, lane] : cloud.scalars)
      if (lane.size() == n && exportableLaneName(name)) sink.put(lane[i]);
    for (const auto& [name, lane] : cloud.vectors)
      if (lane.size() == n && name != "normal" && exportableLaneName(name))
        sink.put(lane[i]);
    for (const auto& [name, lane] : cloud.colors)
      if (lane.size() == n && name != "uv" && name != "tint" &&
          exportableLaneName(name))
        for (int c = 0; c < 4; ++c) sink.put(lane[i][c]);
    sink.endRow();
  }
  return out;
}

std::string ply(const Mesh& mesh, const PlyOptions& options) {
  const size_t n = mesh.vertexCount();
  // Same decline as the Cloud overload: no vertices, no PLY.
  if (n == 0) return {};
  std::string out = header(n, options.binary);
  const bool normals = mesh.normals.size() == n;
  const bool uvs = mesh.uvs.size() == n;
  const bool colors = mesh.colors.size() == n;
  out += kPositionProps;
  if (normals) out += kNormalProps;
  if (uvs) out += kUvProps;
  if (colors) out += kColorProps;
  const size_t tris = mesh.triangleCount();
  out += "element face " + std::to_string(tris) + "\n";
  out += "property list uchar int vertex_indices\n";
  // The PRIMITIVE class rides the face element — PLY's one native slot
  // for per-face attributes, which is how Houdini and Blender read
  // them. Same _r/_g/_b/_a spelling the wide point lanes use.
  //
  // The list property is written FIRST for FOREIGN readers, not for
  // ours. Declaration order is what a PLY reader is supposed to honor,
  // and ours does — Import.PlyFaceLanesTakeConventionalColorAndAnyDe
  // claredOrder case (b) imports a file that declares three scalars
  // AHEAD of vertex_indices and gets the lanes right. So this ordering
  // is a compatibility CHOICE, not a correctness one: list-first is the
  // layout every tool in the exchange path already emits and expects,
  // and the cost of matching it is zero. Do not reorder to suit a
  // caller — files in this shape are already out there being read.
  const auto exportablePrim = [&](const std::string& name,
                                  const std::vector<glm::vec4>& lane) {
    return lane.size() == tris && exportableLaneName(name);
  };
  for (const auto& [name, lane] : mesh.prims)
    if (exportablePrim(name, lane))
      for (const char* channel : {"_r", "_g", "_b", "_a"})
        out += "property float " + name + channel + "\n";
  out += "end_header\n";
  Sink sink{out, options.binary};
  for (size_t i = 0; i < n; ++i) {
    sink.put(mesh.positions[i]);
    if (normals) sink.put(mesh.normals[i]);
    if (uvs) {
      sink.put(mesh.uvs[i].x);
      sink.put(mesh.uvs[i].y);
    }
    if (colors) sink.putColorUchar(mesh.colors[i]);
    sink.endRow();
  }
  for (size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
    sink.putUchar(3);
    sink.putInt((int32_t)mesh.indices[t]);
    sink.putInt((int32_t)mesh.indices[t + 1]);
    sink.putInt((int32_t)mesh.indices[t + 2]);
    for (const auto& [name, lane] : mesh.prims)
      if (exportablePrim(name, lane))
        for (int c = 0; c < 4; ++c) sink.put(lane[t / 3][c]);
    sink.endRow();
  }
  return out;
}

namespace {

bool writeFile(const std::filesystem::path& file, const std::string& bytes) {
  std::ofstream stream(file, std::ios::binary);
  if (!stream) return false;
  stream.write(bytes.data(), (std::streamsize)bytes.size());
  return (bool)stream;
}

}  // namespace

bool ply(const std::filesystem::path& file, const Cloud& cloud,
         const PlyOptions& options) {
  const std::string bytes = ply(cloud, options);
  return !bytes.empty() && writeFile(file, bytes);
}

bool ply(const std::filesystem::path& file, const Mesh& mesh,
         const PlyOptions& options) {
  const std::string bytes = ply(mesh, options);
  return !bytes.empty() && writeFile(file, bytes);
}

}  // namespace sigil::geometry::save
