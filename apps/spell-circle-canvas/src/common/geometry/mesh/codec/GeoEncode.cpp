/** @file
 * The Houdini `.geo` writer: a cloud's points or a mesh's polygons as the
 * JSON document the reader beside it accepts, attributes and all.
 */

#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "sigilgeometry/mesh/codec/Encode.h"

namespace sigil::geometry::mesh::codec::encode {

namespace {

/** A number JSON can read back as the float that was written.
 *
 *  Nine significant digits, because that is what round-trips a binary32
 *  exactly; the six a plain `%g` gives would make the writer lossy in a
 *  way nothing downstream could detect. A non-finite value is written as
 *  0 rather than as the bare `nan` token, which is not JSON at all and
 *  would make the whole document unreadable over one bad sample. */
void number(std::string& out, float v) {
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%.9g",
                std::isfinite(v) ? (double)v : 0.0);
  out += buffer;
}

/** A name a `.geo` can carry: non-empty, and no quote or backslash to
 *  escape. Attribute names come from lane names, which are the caller's
 *  strings, and one of them holding a quote would produce a document that
 *  parses as something else entirely rather than one that fails. */
bool writableName(std::string_view name) {
  if (name.empty()) return false;
  for (const char c : name)
    if (c == '"' || c == '\\' || (unsigned char)c < 0x20) return false;
  return true;
}

/** One numeric point or primitive attribute, values listed as tuples.
 *
 *  Tuples rather than the paged form Houdini itself writes: the reader
 *  takes either, and a page is a compression of a flat list that buys
 *  nothing at the sizes a writer here produces while adding a packing,
 *  a page size and a per-page constant flag to get wrong.
 *
 *  @p at reads component `c` of element `i`. */
template <typename At>
void attribute(std::string& out, std::string_view name, int size, size_t count,
               At at) {
  out += "[[\"scope\",\"public\",\"type\",\"numeric\",\"name\",\"";
  out += name;
  out += "\",\"options\",{}],[\"size\",";
  out += std::to_string(size);
  out += ",\"storage\",\"fpreal32\",\"values\",[\"size\",";
  out += std::to_string(size);
  out += ",\"storage\",\"fpreal32\",\"tuples\",[";
  for (size_t i = 0; i < count; ++i) {
    if (i) out += ',';
    out += '[';
    for (int c = 0; c < size; ++c) {
      if (c) out += ',';
      number(out, at(i, c));
    }
    out += ']';
  }
  out += "]]]]";
}

bool writeFile(const std::filesystem::path& file, const std::string& bytes) {
  std::ofstream stream(file, std::ios::binary);
  if (!stream) return false;
  stream.write(bytes.data(), (std::streamsize)bytes.size());
  return (bool)stream;
}

/** The document's opening: the version the reader looks for in the first
 *  bytes when it has no extension to go on, and the three counts. */
void header(std::string& out, size_t points, size_t vertices,
            size_t primitives) {
  out += "[\"fileversion\",\"20.5.278\",\"pointcount\",";
  out += std::to_string(points);
  out += ",\"vertexcount\",";
  out += std::to_string(vertices);
  out += ",\"primitivecount\",";
  out += std::to_string(primitives);
}

}  // namespace

std::string geo(const Cloud& cloud) {
  const size_t n = cloud.size();
  if (n == 0) return {};

  std::string out;
  header(out, n, 0, 0);
  out += ",\"topology\",[\"pointref\",[\"indices\",[]]],";
  out += "\"attributes\",[\"pointattributes\",[";

  attribute(out, "P", 3, n,
            [&](size_t i, int c) { return cloud.positions[i][c]; });

  // The conventional lanes under the names the reader knows them by. A
  // lane whose length does not match the cloud is skipped rather than
  // written short: the reader sizes every attribute by the point count
  // and would read whatever followed it in memory.
  if (const std::vector<glm::vec3>* normal = cloud.vectorIf("normal");
      normal && normal->size() == n) {
    out += ',';
    attribute(out, "N", 3, n,
              [&](size_t i, int c) { return (*normal)[i][c]; });
  }
  if (const std::vector<glm::vec4>* uv = cloud.colorIf("uv");
      uv && uv->size() == n) {
    // The reader flips v on the way in, to the top-left convention every
    // image in this tree is sampled with; the writer flips it back, so
    // the file is in the convention Houdini's is.
    out += ',';
    attribute(out, "uv", 2, n, [&](size_t i, int c) {
      return c == 1 ? 1.0f - (*uv)[i].y : (*uv)[i].x;
    });
  }
  if (const std::vector<glm::vec4>* tint = cloud.colorIf("tint");
      tint && tint->size() == n) {
    // Four components, so the alpha travels in Cd rather than in a
    // separate Alpha attribute: the reader takes Cd[3] when it is there.
    out += ',';
    attribute(out, "Cd", 4, n, [&](size_t i, int c) { return (*tint)[i][c]; });
  }

  // Then every other lane under its own name, at the width that brings it
  // back to the same kind of lane: 1 is a scalar, 3 a vector, 4 a colour.
  // A GROUP is not written as a group — the reader turns a group into a
  // 0/1 scalar lane, and nothing here can tell such a lane from any other
  // scalar, so writing it as the attribute it arrived as is the reading
  // that round-trips.
  for (const auto& [name, lane] : cloud.scalars) {
    if (lane.size() != n || !writableName(name)) continue;
    out += ',';
    attribute(out, name, 1, n, [&](size_t i, int) { return lane[i]; });
  }
  for (const auto& [name, lane] : cloud.vectors) {
    if (lane.size() != n || name == "normal" || !writableName(name)) continue;
    out += ',';
    attribute(out, name, 3, n, [&](size_t i, int c) { return lane[i][c]; });
  }
  for (const auto& [name, lane] : cloud.colors) {
    if (lane.size() != n || name == "uv" || name == "tint" ||
        !writableName(name))
      continue;
    out += ',';
    attribute(out, name, 4, n, [&](size_t i, int c) { return lane[i][c]; });
  }

  out += "]],\"primitives\",[]]";
  return out;
}

std::string geo(const Mesh& mesh) {
  const size_t points = mesh.vertexCount();
  const size_t tris = mesh.triangleCount();
  if (points == 0) return {};
  if (tris == 0) {
    // No faces: a mesh with only vertices IS a point cloud, and there is
    // one spelling of that here rather than two.
    Cloud cloud;
    cloud.positions = mesh.positions;
    if (mesh.normals.size() == points) cloud.vector("normal") = mesh.normals;
    if (mesh.colors.size() == points) cloud.color("tint") = mesh.colors;
    if (mesh.uvs.size() == points) {
      std::vector<glm::vec4>& uv = cloud.color("uv");
      for (size_t i = 0; i < points; ++i)
        uv[i] = {mesh.uvs[i].x, mesh.uvs[i].y, 0, 0};
    }
    return geo(cloud);
  }

  std::string out;
  header(out, points, mesh.indices.size(), tris);

  // One file vertex per triangle corner, each naming the point it uses.
  out += ",\"topology\",[\"pointref\",[\"indices\",[";
  for (size_t i = 0; i < mesh.indices.size(); ++i) {
    if (i) out += ',';
    out += std::to_string(mesh.indices[i]);
  }
  out += "]]],\"attributes\",[\"pointattributes\",[";

  attribute(out, "P", 3, points,
            [&](size_t i, int c) { return mesh.positions[i][c]; });
  if (mesh.normals.size() == points) {
    out += ',';
    attribute(out, "N", 3, points,
              [&](size_t i, int c) { return mesh.normals[i][c]; });
  }
  if (mesh.uvs.size() == points) {
    out += ',';
    attribute(out, "uv", 2, points, [&](size_t i, int c) {
      return c == 1 ? 1.0f - mesh.uvs[i].y : mesh.uvs[i].x;
    });
  }
  if (mesh.colors.size() == points) {
    out += ',';
    attribute(out, "Cd", 4, points,
              [&](size_t i, int c) { return mesh.colors[i][c]; });
  }
  out += ']';

  // The per-triangle lanes, each at four components under its own name.
  // Not three: the reader fills what a narrower attribute does not reach
  // with zero, so a colour written at three components comes back with no
  // alpha unless it happens to be called Color.
  bool wrotePrimClass = false;
  for (const auto& [name, lane] : mesh.prims) {
    if (lane.size() != tris || !writableName(name)) continue;
    out += wrotePrimClass ? "," : ",\"primitiveattributes\",[";
    wrotePrimClass = true;
    attribute(out, name, 4, tris, [&](size_t i, int c) { return lane[i][c]; });
  }
  if (wrotePrimClass) out += ']';
  out += "],\"primitives\",[";

  // Every triangle as its own closed polygon. A run would be shorter and
  // the reader takes one, but a polygon per face is the form that stays
  // right when the faces stop being uniform.
  for (size_t t = 0; t < tris; ++t) {
    if (t) out += ',';
    out += "[[\"type\",\"Polygon\"],[\"vertex\",[";
    out += std::to_string(t * 3);
    out += ',';
    out += std::to_string(t * 3 + 1);
    out += ',';
    out += std::to_string(t * 3 + 2);
    out += "],\"closed\",true]]";
  }
  out += "]]";
  return out;
}

bool geo(const std::filesystem::path& file, const Cloud& cloud) {
  const std::string bytes = geo(cloud);
  return !bytes.empty() && writeFile(file, bytes);
}

bool geo(const std::filesystem::path& file, const Mesh& mesh) {
  const std::string bytes = geo(mesh);
  return !bytes.empty() && writeFile(file, bytes);
}

}  // namespace sigil::geometry::mesh::codec::encode
