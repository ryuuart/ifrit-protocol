/** @file
 * The PLY reader, parsed by hand: the header names every element and
 * property, the body is rows, ascii and binary little-endian alike.
 * Conventional vertex properties route into the Mesh, every other one
 * becomes a lane under its own name, face properties become primitive
 * lanes, suffixed scalar triples and quads fold back into vectors and
 * colours, and a file without faces is a point cloud.
 */

#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <sstream>
#include <string>

#include "Internal.h"

namespace sigil::geometry::decode::detail {

namespace {

// --- PLY ------------------------------------------------------------------
// Hand-rolled like STL: the header names every element and property,
// the body is rows. ascii and binary_little_endian both supported.
// Conventional property names route into the Mesh (x/y/z, nx/ny/nz,
// s/t or u/v, red/green/blue/alpha — integer colors normalize to
// 0..1); EVERY other vertex property becomes a scalar lane under its
// own name. Files without a face element are honest point clouds
// (empty indices) — asCloud() is their natural consumer.
//
// FACE properties are the PRIMITIVE class and land in Mesh::prims, the
// read leg of encode::ply's per-face write: one value per TRIANGLE,
// replicated when a polygon fans. Point lanes and prim lanes never
// share a container, so their cardinalities cannot be confused.

struct PlyScalarType {
  int size = 0;
  bool floating = false;
  bool signedInt = false;
  double intMax = 1;  ///< color normalization divisor for integers
};

std::optional<PlyScalarType> plyScalarType(std::string_view name) {
  if (name == "char" || name == "int8")
    return PlyScalarType{1, false, true, 127.0};
  if (name == "uchar" || name == "uint8")
    return PlyScalarType{1, false, false, 255.0};
  if (name == "short" || name == "int16")
    return PlyScalarType{2, false, true, 32767.0};
  if (name == "ushort" || name == "uint16")
    return PlyScalarType{2, false, false, 65535.0};
  if (name == "int" || name == "int32")
    return PlyScalarType{4, false, true, 2147483647.0};
  if (name == "uint" || name == "uint32")
    return PlyScalarType{4, false, false, 4294967295.0};
  if (name == "float" || name == "float32")
    return PlyScalarType{4, true, false, 1.0};
  if (name == "double" || name == "float64")
    return PlyScalarType{8, true, false, 1.0};
  return std::nullopt;
}

double plyLoadBinary(const std::byte*& cursor, const PlyScalarType& type) {
  uint64_t raw = 0;
  std::memcpy(&raw, cursor, (size_t)type.size);  // little-endian host
  cursor += type.size;
  if (type.floating) {
    if (type.size == 4) {
      float f;
      std::memcpy(&f, &raw, 4);
      return f;
    }
    double d;
    std::memcpy(&d, &raw, 8);
    return d;
  }
  if (type.signedInt) {
    // Sign-extend from the value's width.
    const int shift = 64 - type.size * 8;
    return (double)((int64_t)(raw << shift) >> shift);
  }
  return (double)raw;
}

struct PlyProperty {
  std::string name;
  PlyScalarType type;
  bool list = false;
  PlyScalarType countType;
};

struct PlyElement {
  std::string name;
  size_t count = 0;
  std::vector<PlyProperty> properties;
};

/** Fold suffixed scalar triples/quads back into wide lanes — the
 *  return leg of encode::ply's spelling (name_x/_y/_z, name_r/_g/_b/_a)
 *  — so a round trip reconstitutes vectors and colors, not loose
 *  floats. Alpha is optional; it defaults to 1. Consumed components
 *  leave @p scalars; a partial group, or one whose members disagree on
 *  length, stays scalar.
 *
 *  BOTH attribute classes fold through here: the POINT lanes on the
 *  Part and the PRIMITIVE lanes bound for Mesh::prims. The suffix
 *  grammar is one grammar, so it gets one implementation — a second
 *  copy would be the thing that drifts. */
void foldSuffixedLanes(
    std::map<std::string, std::vector<float>, std::less<>>& scalars,
    std::map<std::string, std::vector<glm::vec3>, std::less<>>& vectors,
    std::map<std::string, std::vector<glm::vec4>, std::less<>>& colors) {
  std::vector<std::string> vectorBases, colorBases;
  for (const auto& [name, lane] : scalars) {
    if (name.size() > 2 && name.ends_with("_x"))
      vectorBases.push_back(name.substr(0, name.size() - 2));
    else if (name.size() > 2 && name.ends_with("_r"))
      colorBases.push_back(name.substr(0, name.size() - 2));
  }
  for (const std::string& base : vectorBases) {
    const auto x = scalars.find(base + "_x");
    const auto y = scalars.find(base + "_y");
    const auto z = scalars.find(base + "_z");
    if (x == scalars.end() || y == scalars.end() || z == scalars.end())
      continue;
    const size_t n = x->second.size();
    if (y->second.size() != n || z->second.size() != n) continue;
    std::vector<glm::vec3>& lane = vectors[base];
    lane.resize(n);
    for (size_t i = 0; i < n; ++i)
      lane[i] = {x->second[i], y->second[i], z->second[i]};
    scalars.erase(x);
    scalars.erase(y);
    scalars.erase(z);
  }
  for (const std::string& base : colorBases) {
    const auto r = scalars.find(base + "_r");
    const auto g = scalars.find(base + "_g");
    const auto b = scalars.find(base + "_b");
    if (r == scalars.end() || g == scalars.end() || b == scalars.end())
      continue;
    const size_t n = r->second.size();
    if (g->second.size() != n || b->second.size() != n) continue;
    const auto alpha = scalars.find(base + "_a");
    const bool alphaMatched =
        alpha != scalars.end() && alpha->second.size() == n;
    std::vector<glm::vec4>& lane = colors[base];
    lane.resize(n);
    for (size_t i = 0; i < n; ++i)
      lane[i] = {r->second[i], g->second[i], b->second[i],
                 alphaMatched ? alpha->second[i] : 1.0f};
    scalars.erase(r);
    scalars.erase(g);
    scalars.erase(b);
    // A size-mismatched "_a" was ignored above; only a consumed alpha
    // lane is erased.
    if (alphaMatched) scalars.erase(base + "_a");
  }
}
}  // namespace

std::optional<Model> importPly(const std::byte* bytes, size_t size) {
  const std::string_view text = asText(bytes, size);
  const size_t headerEnd = text.find("end_header");
  if (!text.starts_with("ply") || headerEnd == std::string_view::npos)
    return std::nullopt;
  size_t bodyStart = text.find('\n', headerEnd);
  if (bodyStart == std::string_view::npos) return std::nullopt;
  ++bodyStart;

  // Header.
  bool binary = false;
  std::vector<PlyElement> elements;
  {
    std::istringstream header(std::string(text.substr(0, headerEnd)));
    std::string line;
    while (std::getline(header, line)) {
      std::istringstream words(line);
      std::string word;
      words >> word;
      if (word == "format") {
        std::string format;
        words >> format;
        if (format == "binary_little_endian")
          binary = true;
        else if (format != "ascii")
          return std::nullopt;  // big-endian: not this century
      } else if (word == "element") {
        // The count is file-supplied: a negative (or missing) value
        // must never reach the size_t — it would wrap huge.
        PlyElement element;
        long long declared = -1;
        words >> element.name >> declared;
        if (declared < 0) return std::nullopt;
        element.count = (size_t)declared;
        elements.push_back(std::move(element));
      } else if (word == "property" && !elements.empty()) {
        PlyProperty property;
        std::string type;
        words >> type;
        if (type == "list") {
          std::string countName, valueName;
          words >> countName >> valueName >> property.name;
          const auto count = plyScalarType(countName);
          const auto value = plyScalarType(valueName);
          if (!count || !value) return std::nullopt;
          property.list = true;
          property.countType = *count;
          property.type = *value;
        } else {
          const auto value = plyScalarType(type);
          if (!value) return std::nullopt;
          words >> property.name;
          property.type = *value;
        }
        elements.back().properties.push_back(std::move(property));
      }
    }
  }

  // Element counts are trusted nowhere yet: before any resize acts on
  // them, bound each against the bytes that actually follow the header
  // — a row costs at least one byte per property (two in ascii), so a
  // count beyond the remaining byte count cannot be backed by data.
  const size_t remaining = size - bodyStart;
  for (const PlyElement& element : elements)
    if (element.count > remaining) return std::nullopt;

  Part part;
  Mesh& mesh = part.mesh;
  bool hasNormals = false;
  /** PRIMITIVE-class values as they arrive: raw floats under the
   *  property's own name, one entry per TRIANGLE (not per face row —
   *  see the fan replication below). Accumulated across every face
   *  element, folded and widened into Mesh::prims once the body is
   *  read. */
  std::map<std::string, std::vector<float>, std::less<>> primScalars;

  // One reader per source; ascii tokenizes, binary walks a cursor.
  std::istringstream ascii(binary ? std::string()
                                  : std::string(text.substr(bodyStart)));
  const std::byte* cursor = bytes + bodyStart;
  const std::byte* end = bytes + size;
  const auto read = [&](const PlyScalarType& type, double* value) -> bool {
    if (binary) {
      if (cursor + type.size > end) return false;
      *value = plyLoadBinary(cursor, type);
      return true;
    }
    return (bool)(ascii >> *value);
  };

  for (const PlyElement& element : elements) {
    const bool isVertex = element.name == "vertex";
    const bool isFace = element.name == "face";

    // Per-property sinks, resolved once. index in [0, count).
    std::vector<std::function<void(size_t, double)>> sinks;
    if (isVertex) {
      mesh.positions.resize(element.count, glm::vec3{0});
      // "s"/"t" and "u"/"v" are texture coordinates only as PAIRS. A
      // lone "t" — the scalar every readChain/cook/asCloud cloud
      // carries — must stay a lane, not clobber uv.y.
      const auto hasProp = [&element](std::string_view want) {
        for (const PlyProperty& p : element.properties)
          if (!p.list && p.name == want) return true;
        return false;
      };
      const bool uvST = hasProp("s") && hasProp("t");
      const bool uvUV = hasProp("u") && hasProp("v");
      for (const PlyProperty& property : element.properties) {
        const std::string& n = property.name;
        if (property.list) {
          // A list on the vertex element has no lane shape here: keep
          // the sink walk aligned with a no-op and create no lane.
          sinks.push_back([](size_t, double) {});
          continue;
        }
        const double norm =
            property.type.floating ? 1.0 : 1.0 / property.type.intMax;
        auto axis = [&](auto member) {
          return [&mesh, member](size_t i, double v) {
            mesh.positions[i].*member = (float)v;
          };
        };
        if (n == "x")
          sinks.push_back(axis(&glm::vec3::x));
        else if (n == "y")
          sinks.push_back(axis(&glm::vec3::y));
        else if (n == "z")
          sinks.push_back(axis(&glm::vec3::z));
        else if (n == "nx" || n == "ny" || n == "nz") {
          hasNormals = true;
          mesh.normals.resize(element.count, glm::vec3{0});
          const int c = n == "nx" ? 0 : n == "ny" ? 1 : 2;
          sinks.push_back([&mesh, c](size_t i, double v) {
            mesh.normals[i][c] = (float)v;
          });
        } else if (((n == "s" || n == "t") && uvST) ||
                   ((n == "u" || n == "v") && uvUV)) {
          mesh.uvs.resize(element.count, glm::vec2{0});
          const int c = (n == "s" || n == "u") ? 0 : 1;
          sinks.push_back(
              [&mesh, c](size_t i, double v) { mesh.uvs[i][c] = (float)v; });
        } else if (n == "red" || n == "green" || n == "blue" || n == "alpha") {
          mesh.colors.resize(element.count, kWhite);
          const int c = n == "red" ? 0 : n == "green" ? 1 : n == "blue" ? 2 : 3;
          sinks.push_back([&mesh, c, norm](size_t i, double v) {
            mesh.colors[i][c] = (float)(v * norm);
          });
        } else {
          std::vector<float>& lane = part.scalarLanes[n];
          lane.resize(element.count, 0.0f);
          sinks.push_back([&lane](size_t i, double v) {
            lane[i] = (float)v;  // raw — ids stay ids
          });
        }
      }
    }

    // Per-face lane targets, resolved once and parallel to
    // element.properties (null for lists, for an unnamed property, and
    // for a name already claimed in THIS element — a duplicate
    // property would append twice and desync its lane). Nothing is
    // sized from element.count: a header that promises face rows it
    // never delivers allocates nothing here.
    struct FaceLane {
      std::vector<float>* lane = nullptr;
      double scale = 1;
    };
    std::vector<FaceLane> faceLanes;
    std::vector<double> faceRow;
    if (isFace) {
      faceLanes.resize(element.properties.size());
      faceRow.assign(element.properties.size(), 0.0);
      for (size_t p = 0; p < element.properties.size(); ++p) {
        const PlyProperty& property = element.properties[p];
        if (property.list || property.name.empty()) continue;
        // MeshLab-style conventional per-face color is spelled
        // red/green/blue/alpha; it is collected under the SUFFIXED
        // name so the one folder below reconstitutes it as the same
        // "Color" lane encode::ply writes as Color_r/_g/_b/_a. Integer
        // channels normalize, exactly like the vertex leg; every other
        // property stays raw (ids stay ids).
        const std::string& n = property.name;
        const std::string laneName = n == "red"     ? "Color_r"
                                     : n == "green" ? "Color_g"
                                     : n == "blue"  ? "Color_b"
                                     : n == "alpha" ? "Color_a"
                                                    : n;
        std::vector<float>* target = &primScalars[laneName];
        const bool duplicate = std::any_of(
            faceLanes.begin(), faceLanes.begin() + (std::ptrdiff_t)p,
            [target](const FaceLane& existing) {
              return existing.lane == target;
            });
        if (duplicate) continue;
        const bool color = laneName != n;
        faceLanes[p] = {target, color && !property.type.floating
                                    ? 1.0 / property.type.intMax
                                    : 1.0};
      }
    }

    for (size_t i = 0; i < element.count; ++i) {
      size_t sink = 0;
      size_t faceTriangles = 0;
      for (const PlyProperty& property : element.properties) {
        if (property.list) {
          double countValue = 0;
          if (!read(property.countType, &countValue)) return std::nullopt;
          // Malformed list counts — negative, NaN, or beyond anything
          // the file's bytes could back — must never reach the size_t
          // cast or the reserve below.
          if (!(countValue >= 0) || countValue > (double)size)
            return std::nullopt;
          const size_t count = (size_t)countValue;
          std::vector<uint32_t> row;
          row.reserve(count);
          for (size_t k = 0; k < count; ++k) {
            double value = 0;
            if (!read(property.type, &value)) return std::nullopt;
            // Negative (or NaN) list values would wrap in the
            // uint32_t cast — malformed file.
            if (!(value >= 0)) return std::nullopt;
            row.push_back((uint32_t)value);
          }
          if (isFace && (property.name == "vertex_indices" ||
                         property.name == "vertex_index")) {
            // File-supplied indices feed every later positions[] and
            // normals[] access; a face naming a vertex that does not
            // exist is dropped whole, the rest still imports.
            const bool inRange =
                std::all_of(row.begin(), row.end(), [&mesh](uint32_t v) {
                  return (size_t)v < mesh.positions.size();
                });
            if (inRange) {
              for (size_t k = 1; k + 1 < row.size(); ++k)
                mesh.indices.insert(mesh.indices.end(),
                                    {row[0], row[k], row[k + 1]});
              faceTriangles += row.size() > 2 ? row.size() - 2 : 0;
            }
          }
        } else {
          double value = 0;
          if (!read(property.type, &value)) return std::nullopt;
          if (isVertex)
            sinks[sink](i, value);
          else if (isFace)
            faceRow[sink] = value;
        }
        ++sink;
      }
      // The row's per-face values are REPLICATED across exactly the
      // triangles the row produced: an n-gon fans into n-2 triangles,
      // and a face naming a vertex that does not exist produces NONE.
      // So the lanes stay in lockstep with triangleCount() by
      // construction, and every byte allocated here is backed by index
      // data that was actually read.
      for (size_t p = 0; faceTriangles > 0 && p < faceLanes.size(); ++p)
        if (faceLanes[p].lane)
          faceLanes[p].lane->insert(faceLanes[p].lane->end(), faceTriangles,
                                    (float)(faceRow[p] * faceLanes[p].scale));
    }
  }

  if (mesh.positions.empty()) return std::nullopt;

  foldSuffixedLanes(part.scalarLanes, part.vectorLanes, part.colorLanes);

  // The PRIMITIVE class comes home to Mesh::prims — its OWN container,
  // triangleCount()-sized BY DEFINITION, so a per-face lane can never
  // be mistaken for a per-vertex one: Part's scalar/vector/color lanes
  // and asCloud() stay strictly point-class. Same folder as the point
  // lanes, then widened to the vec4 currency prims speak: a folded
  // color IS the vec4 (this is encode::ply's own Color_r/_g/_b/_a leg),
  // a folded vector takes w = 0 (Mesh::append's pad for non-"Color"
  // lanes), and a lone scalar lands in .x — the "Id" convention.
  {
    std::map<std::string, std::vector<glm::vec3>, std::less<>> primVectors;
    std::map<std::string, std::vector<glm::vec4>, std::less<>> primColors;
    foldSuffixedLanes(primScalars, primVectors, primColors);
    const size_t tris = mesh.triangleCount();
    // A lane the file under- or over-supplied is DROPPED whole rather
    // than published at a lying cardinality — the same posture the
    // dropped-face path takes, and the reason a header that promises
    // face properties it never delivers cannot desync mesh.prims.
    const auto publish = [tris](const std::string& name, size_t n) {
      return tris > 0 && n == tris && !name.empty();
    };
    for (const auto& [name, lane] : primColors)
      if (publish(name, lane.size())) mesh.prims[name] = lane;
    for (const auto& [name, lane] : primVectors)
      if (publish(name, lane.size())) {
        std::vector<glm::vec4>& out = mesh.prims[name];
        out.resize(lane.size());
        for (size_t i = 0; i < lane.size(); ++i)
          out[i] = glm::vec4(lane[i], 0.0f);
      }
    for (const auto& [name, lane] : primScalars)
      if (publish(name, lane.size())) {
        std::vector<glm::vec4>& out = mesh.prims[name];
        out.assign(lane.size(), glm::vec4{0});
        for (size_t i = 0; i < lane.size(); ++i) out[i].x = lane[i];
      }
  }

  finishPart(part, hasNormals);
  Model out;
  out.parts.push_back(std::move(part));
  return out;
}

bool looksLikePly(std::string_view text) {
  return text.starts_with("ply") &&
         text.find("end_header") != std::string_view::npos;
}

}  // namespace sigil::geometry::decode::detail
