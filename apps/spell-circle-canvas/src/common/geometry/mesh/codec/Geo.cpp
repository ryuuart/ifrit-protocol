/** @file
 * The Houdini .geo reader: a small JSON reader sized to what Houdini
 * writes, then the file's point, vertex, primitive and group classes
 * decoded into one unwelded Part — polygons fan-triangulated with each
 * polygon vertex its own mesh vertex, point attributes as lanes,
 * primitive attributes as primitive lanes, groups as 0/1 lanes named
 * after the group, and a primitive-less file as a point cloud.
 */

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <variant>

#include "Internal.h"

namespace sigil::geometry::mesh::codec::decode::detail {

namespace {

/** The little JSON reader the .geo importer needs: values as a tree of
 *  variants, numbers as doubles, no streaming. Houdini writes plain
 *  JSON with no extensions, so nothing beyond the grammar is handled. */
struct Json {
  using Array = std::vector<Json>;
  using Object = std::vector<std::pair<std::string, Json>>;
  std::variant<std::nullptr_t, bool, double, std::string, Array, Object> v;

  bool isArray() const { return std::holds_alternative<Array>(v); }
  bool isObject() const { return std::holds_alternative<Object>(v); }
  bool isString() const { return std::holds_alternative<std::string>(v); }
  bool isNumber() const { return std::holds_alternative<double>(v); }
  bool isBool() const { return std::holds_alternative<bool>(v); }
  const Array& array() const {
    static const Array empty;
    return isArray() ? std::get<Array>(v) : empty;
  }
  const Object& object() const {
    static const Object empty;
    return isObject() ? std::get<Object>(v) : empty;
  }
  const std::string& string() const {
    static const std::string empty;
    return isString() ? std::get<std::string>(v) : empty;
  }
  double number(double fallback = 0) const {
    return isNumber() ? std::get<double>(v) : fallback;
  }
  bool boolean() const { return isBool() && std::get<bool>(v); }
  /** Object member by key, or an array laid out as alternating
   *  key/value entries — the shape Houdini uses throughout the file. */
  const Json* get(std::string_view key) const {
    if (isObject()) {
      for (const auto& [k, value] : object())
        if (k == key) return &value;
      return nullptr;
    }
    if (isArray()) {
      const Array& a = array();
      for (size_t i = 0; i + 1 < a.size(); i += 2)
        if (a[i].isString() && a[i].string() == key) return &a[i + 1];
    }
    return nullptr;
  }
};

class JsonReader {
 public:
  explicit JsonReader(std::string_view text) : m_text(text) {}
  std::optional<Json> read() {
    Json out;
    if (!value(out)) return std::nullopt;
    skipSpace();
    return out;
  }

 private:
  bool value(Json& out) {
    skipSpace();
    if (m_pos >= m_text.size()) return false;
    const char c = m_text[m_pos];
    if (c == '{') return object(out);
    if (c == '[') return array(out);
    if (c == '"') {
      std::string s;
      if (!string(s)) return false;
      out.v = std::move(s);
      return true;
    }
    if (m_text.substr(m_pos, 4) == "true") {
      m_pos += 4;
      out.v = true;
      return true;
    }
    if (m_text.substr(m_pos, 5) == "false") {
      m_pos += 5;
      out.v = false;
      return true;
    }
    if (m_text.substr(m_pos, 4) == "null") {
      m_pos += 4;
      out.v = nullptr;
      return true;
    }
    return number(out);
  }
  bool object(Json& out) {
    ++m_pos;  // {
    Json::Object members;
    skipSpace();
    if (peek() == '}') {
      ++m_pos;
      out.v = std::move(members);
      return true;
    }
    for (;;) {
      skipSpace();
      std::string key;
      if (!string(key)) return false;
      skipSpace();
      if (peek() != ':') return false;
      ++m_pos;
      Json member;
      if (!value(member)) return false;
      members.emplace_back(std::move(key), std::move(member));
      skipSpace();
      if (peek() == ',') {
        ++m_pos;
        continue;
      }
      if (peek() == '}') {
        ++m_pos;
        out.v = std::move(members);
        return true;
      }
      return false;
    }
  }
  bool array(Json& out) {
    ++m_pos;  // [
    Json::Array items;
    skipSpace();
    if (peek() == ']') {
      ++m_pos;
      out.v = std::move(items);
      return true;
    }
    for (;;) {
      Json item;
      if (!value(item)) return false;
      items.push_back(std::move(item));
      skipSpace();
      if (peek() == ',') {
        ++m_pos;
        continue;
      }
      if (peek() == ']') {
        ++m_pos;
        out.v = std::move(items);
        return true;
      }
      return false;
    }
  }
  bool string(std::string& out) {
    if (peek() != '"') return false;
    ++m_pos;
    while (m_pos < m_text.size()) {
      const char c = m_text[m_pos++];
      if (c == '"') return true;
      if (c != '\\') {
        out += c;
        continue;
      }
      if (m_pos >= m_text.size()) return false;
      const char e = m_text[m_pos++];
      switch (e) {
        case '"':
          out += '"';
          break;
        case '\\':
          out += '\\';
          break;
        case '/':
          out += '/';
          break;
        case 'b':
          out += '\b';
          break;
        case 'f':
          out += '\f';
          break;
        case 'n':
          out += '\n';
          break;
        case 'r':
          out += '\r';
          break;
        case 't':
          out += '\t';
          break;
        case 'u': {
          if (m_pos + 4 > m_text.size()) return false;
          unsigned code = 0;
          for (int i = 0; i < 4; ++i) {
            const char h = m_text[m_pos++];
            code <<= 4u;
            if (h >= '0' && h <= '9')
              code |= (unsigned)(h - '0');
            else if (h >= 'a' && h <= 'f')
              code |= (unsigned)(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F')
              code |= (unsigned)(h - 'A' + 10);
            else
              return false;
          }
          // Basic-plane code points to UTF-8; surrogates are rare in
          // attribute names and are passed through as replacement.
          if (code < 0x80) {
            out += (char)code;
          } else if (code < 0x800) {
            out += (char)(0xC0u | (code >> 6u));
            out += (char)(0x80u | (code & 0x3Fu));
          } else {
            out += (char)(0xE0u | (code >> 12u));
            out += (char)(0x80u | ((code >> 6u) & 0x3Fu));
            out += (char)(0x80u | (code & 0x3Fu));
          }
          break;
        }
        default:
          return false;
      }
    }
    return false;
  }
  bool number(Json& out) {
    const size_t start = m_pos;
    if (peek() == '-' || peek() == '+') ++m_pos;
    while (m_pos < m_text.size() &&
           (std::isdigit((unsigned char)m_text[m_pos]) ||
            m_text[m_pos] == '.' || m_text[m_pos] == 'e' ||
            m_text[m_pos] == 'E' || m_text[m_pos] == '-' ||
            m_text[m_pos] == '+'))
      ++m_pos;
    if (m_pos == start) return false;
    const std::string token(m_text.substr(start, m_pos - start));
    char* end = nullptr;
    const double d = std::strtod(token.c_str(), &end);
    if (!end || *end != '\0') return false;
    out.v = d;
    return true;
  }
  char peek() const { return m_pos < m_text.size() ? m_text[m_pos] : '\0'; }
  void skipSpace() {
    while (m_pos < m_text.size() && std::isspace((unsigned char)m_text[m_pos]))
      ++m_pos;
  }
  std::string_view m_text;
  size_t m_pos = 0;
};

/** One decoded .geo attribute: a name, its tuple size, and every
 *  element's tuple flattened. Strings arrive as their table indices. */
struct GeoAttribute {
  std::string name;
  int size = 1;
  bool isString = false;
  std::vector<std::string> strings;
  std::vector<float> values;  ///< count * size
  size_t count() const { return size > 0 ? values.size() / (size_t)size : 0; }
  float at(size_t element, int component) const {
    return values[element * (size_t)size + (size_t)component];
  }
};

/** Decode a "values" (or "indices") block into a flat float array of
 *  @p count tuples of @p size. Three spellings exist: "tuples" (a list
 *  of tuples), "arrays" (a list of per-component arrays), and the paged
 *  form — "packing" splits the tuple into slices, "pagesize" elements
 *  per page, "constantpageflags" one bool per slice per page, and
 *  "rawpagedata" the concatenation, slice-major, page by page, a page
 *  being one tuple-slice when its flag is set and pagesize of them
 *  otherwise (the last page shorter). */
bool geoDecodeValues(const Json& block, size_t count, int size,
                     std::vector<float>& out) {
  out.assign(count * (size_t)size, 0.0f);
  if (count == 0) return true;
  if (const Json* tuples = block.get("tuples")) {
    const Json::Array& list = tuples->array();
    if (list.size() < count) return false;
    for (size_t i = 0; i < count; ++i) {
      const Json::Array& tuple = list[i].array();
      for (int c = 0; c < size && (size_t)c < tuple.size(); ++c)
        out[i * (size_t)size + (size_t)c] = (float)tuple[(size_t)c].number();
    }
    return true;
  }
  if (const Json* arrays = block.get("arrays")) {
    // One array per component (size 1: one array of the values).
    const Json::Array& list = arrays->array();
    if (list.size() < (size_t)size) return false;
    for (int c = 0; c < size; ++c) {
      const Json::Array& column = list[(size_t)c].array();
      if (column.size() < count) return false;
      for (size_t i = 0; i < count; ++i)
        out[i * (size_t)size + (size_t)c] = (float)column[i].number();
    }
    return true;
  }
  const Json* raw = block.get("rawpagedata");
  if (!raw) return false;
  const Json::Array& data = raw->array();
  std::vector<int> packing;
  if (const Json* p = block.get("packing"))
    for (const Json& n : p->array()) packing.push_back((int)n.number());
  if (packing.empty()) packing.push_back(size);
  int packed = 0;
  for (int p : packing) packed += p;
  if (packed != size) return false;
  const size_t pageSize =
      block.get("pagesize") ? (size_t)block.get("pagesize")->number() : count;
  if (pageSize == 0) return false;
  const size_t pages = (count + pageSize - 1) / pageSize;
  const Json* flags = block.get("constantpageflags");
  size_t cursor = 0;
  int componentBase = 0;
  for (size_t slice = 0; slice < packing.size(); ++slice) {
    const int width = packing[slice];
    const Json::Array* sliceFlags = nullptr;
    if (flags && slice < flags->array().size())
      sliceFlags = &flags->array()[slice].array();
    for (size_t page = 0; page < pages; ++page) {
      const size_t first = page * pageSize;
      const size_t n = std::min(pageSize, count - first);
      const bool constant = sliceFlags && page < sliceFlags->size() &&
                            (*sliceFlags)[page].boolean();
      if (constant) {
        if (cursor + (size_t)width > data.size()) return false;
        for (size_t i = 0; i < n; ++i)
          for (int c = 0; c < width; ++c)
            out[(first + i) * (size_t)size + (size_t)(componentBase + c)] =
                (float)data[cursor + (size_t)c].number();
        cursor += (size_t)width;
      } else {
        if (cursor + n * (size_t)width > data.size()) return false;
        for (size_t i = 0; i < n; ++i)
          for (int c = 0; c < width; ++c)
            out[(first + i) * (size_t)size + (size_t)(componentBase + c)] =
                (float)data[cursor + i * (size_t)width + (size_t)c].number();
        cursor += n * (size_t)width;
      }
    }
    componentBase += width;
  }
  return true;
}

/** Decode one attribute class list ("pointattributes" etc.). Each entry
 *  is a two-element array: [descriptor, payload]. Numeric attributes
 *  carry "size", "storage" and "values"; string attributes carry
 *  "strings" and "indices". Anything else (dicts, arrays-of-arrays) is
 *  skipped. */
std::vector<GeoAttribute> geoAttributes(const Json* list, size_t count) {
  std::vector<GeoAttribute> out;
  if (!list) return out;
  for (const Json& entry : list->array()) {
    const Json::Array& pair = entry.array();
    if (pair.size() < 2) continue;
    const Json& desc = pair[0];
    const Json& payload = pair[1];
    const Json* type = desc.get("type");
    const Json* name = desc.get("name");
    if (!type || !name) continue;
    GeoAttribute attr;
    attr.name = name->string();
    if (type->string() == "numeric") {
      attr.size = payload.get("size") ? (int)payload.get("size")->number() : 1;
      const Json* values = payload.get("values");
      if (!values || attr.size < 1 ||
          !geoDecodeValues(*values, count, attr.size, attr.values))
        continue;
    } else if (type->string() == "string") {
      attr.isString = true;
      attr.size = 1;
      if (const Json* strings = payload.get("strings"))
        for (const Json& str : strings->array())
          attr.strings.push_back(str.string());
      const Json* indices = payload.get("indices");
      if (!indices || !geoDecodeValues(*indices, count, 1, attr.values))
        continue;
    } else {
      continue;
    }
    out.push_back(std::move(attr));
  }
  return out;
}

const GeoAttribute* geoFind(const std::vector<GeoAttribute>& attrs,
                            std::string_view name) {
  for (const GeoAttribute& a : attrs)
    if (a.name == name) return &a;
  return nullptr;
}

/** A group's membership as a per-element flag list. Two spellings:
 *  "unordered" with a "boolRLE" run-length list ([count, flag, ...])
 *  or "i8" bytes; "ordered" with an explicit index list. */
std::vector<float> geoGroup(const Json& selection, size_t count) {
  std::vector<float> flags(count, 0.0f);
  if (const Json* unordered = selection.get("unordered")) {
    if (const Json* rle = unordered->get("boolRLE")) {
      const Json::Array& runs = rle->array();
      size_t at = 0;
      for (size_t i = 0; i + 1 < runs.size() && at < count; i += 2) {
        const size_t n = (size_t)runs[i].number();
        const float flag = runs[i + 1].boolean() ? 1.0f : 0.0f;
        for (size_t k = 0; k < n && at < count; ++k) flags[at++] = flag;
      }
    } else if (const Json* bytes = unordered->get("i8")) {
      const Json::Array& list = bytes->array();
      for (size_t i = 0; i < count && i < list.size(); ++i)
        flags[i] = list[i].number() != 0 ? 1.0f : 0.0f;
    }
  } else if (const Json* ordered = selection.get("ordered")) {
    for (const Json& index : ordered->array()) {
      const size_t i = (size_t)index.number();
      if (i < count) flags[i] = 1.0f;
    }
  }
  return flags;
}

/** The polygons: a list of (vertex index list, closed) pairs, from
 *  "Polygon" entries, "Polygon_run" runs (start vertex plus a
 *  run-length list of vertex counts) and generic "run" entries whose
 *  runtype is Polygon. Every other primitive type is skipped. */
struct GeoPolygon {
  std::vector<uint32_t> vertices;
  size_t primitiveIndex = 0;  ///< position among ALL primitives
  bool closed = true;
};
std::vector<GeoPolygon> geoPolygons(const Json* primitives) {
  std::vector<GeoPolygon> out;
  if (!primitives) return out;
  size_t primitiveIndex = 0;
  for (const Json& entry : primitives->array()) {
    const Json::Array& pair = entry.array();
    if (pair.empty()) continue;
    const Json& head = pair[0];
    const Json* type = head.get("type");
    if (!type) continue;
    const std::string& kind = type->string();
    if (kind == "Polygon" && pair.size() >= 2) {
      GeoPolygon poly;
      poly.primitiveIndex = primitiveIndex++;
      if (const Json* vertex = pair[1].get("vertex"))
        for (const Json& v : vertex->array())
          poly.vertices.push_back((uint32_t)v.number());
      if (const Json* closed = pair[1].get("closed"))
        poly.closed = closed->boolean();
      out.push_back(std::move(poly));
    } else if (kind == "Polygon_run" && pair.size() >= 2) {
      const Json& body = pair[1];
      uint32_t vertex = body.get("startvertex")
                            ? (uint32_t)body.get("startvertex")->number()
                            : 0;
      const size_t nprims = body.get("nprimitives")
                                ? (size_t)body.get("nprimitives")->number()
                                : 0;
      std::vector<size_t> counts;
      if (const Json* rle = body.get("nvertices_rle")) {
        const Json::Array& runs = rle->array();
        for (size_t i = 0; i + 1 < runs.size(); i += 2) {
          const size_t n = (size_t)runs[i].number();
          const size_t repeat = (size_t)runs[i + 1].number();
          for (size_t k = 0; k < repeat; ++k) counts.push_back(n);
        }
      } else if (const Json* list = body.get("nvertices")) {
        for (const Json& n : list->array())
          counts.push_back((size_t)n.number());
      }
      const bool closed = !body.get("closed") || body.get("closed")->boolean();
      for (size_t p = 0; p < nprims && p < counts.size(); ++p) {
        GeoPolygon poly;
        poly.primitiveIndex = primitiveIndex++;
        poly.closed = closed;
        for (size_t k = 0; k < counts[p]; ++k)
          poly.vertices.push_back(vertex++);
        out.push_back(std::move(poly));
      }
    } else if (kind == "run" && pair.size() >= 2) {
      const Json* runtype = head.get("runtype");
      const Json* varying = head.get("varyingfields");
      const Json* uniform = head.get("uniformfields");
      const bool polygonRun = runtype && runtype->string() == "Polygon";
      int vertexField = -1;
      if (varying)
        for (size_t i = 0; i < varying->array().size(); ++i)
          if (varying->array()[i].string() == "vertex") vertexField = (int)i;
      bool closed = true;
      if (uniform)
        if (const Json* c = uniform->get("closed")) closed = c->boolean();
      const Json::Array& items = pair[1].array();
      for (const Json& item : items) {
        if (!polygonRun || vertexField < 0) {
          ++primitiveIndex;
          continue;
        }
        GeoPolygon poly;
        poly.primitiveIndex = primitiveIndex++;
        poly.closed = closed;
        const Json::Array& fields = item.array();
        if ((size_t)vertexField < fields.size())
          for (const Json& v : fields[(size_t)vertexField].array())
            poly.vertices.push_back((uint32_t)v.number());
        out.push_back(std::move(poly));
      }
    } else {
      // Another primitive type, or a run of one: it still occupies
      // primitive indices, so primitive attributes stay aligned.
      size_t occupied = 1;
      if (kind == "run" && pair.size() >= 2) occupied = pair[1].array().size();
      if (kind.ends_with("_run") && pair.size() >= 2 &&
          pair[1].get("nprimitives"))
        occupied = (size_t)pair[1].get("nprimitives")->number();
      primitiveIndex += occupied;
    }
  }
  return out;
}

}  // namespace

/** Houdini .geo: one Part. Polygons unweld into a triangle mesh whose
 *  vertices are the file's VERTICES (each carrying its point's position
 *  and the vertex- or point-class N, uv, Cd and Alpha it resolves to);
 *  a file with no polygons is a point cloud whose mesh vertices are its
 *  points. Point attributes beyond the conventional ones become named
 *  lanes on the Part; primitive attributes become Mesh::prims lanes,
 *  replicated across each polygon's fan triangles; point groups become
 *  0/1 scalar lanes under the group's name and primitive groups the
 *  same on the primitive class. Detail attributes have no home and are
 *  dropped. */
std::optional<Model> importHoudiniGeo(std::string_view text) {
  std::optional<Json> rootOpt = JsonReader(text).read();
  if (!rootOpt || !rootOpt->isArray()) return std::nullopt;
  const Json& root = *rootOpt;
  if (!root.get("fileversion") || !root.get("pointcount")) return std::nullopt;
  const size_t pointCount = (size_t)root.get("pointcount")->number();
  const size_t vertexCount =
      root.get("vertexcount") ? (size_t)root.get("vertexcount")->number() : 0;
  const size_t primitiveCount =
      root.get("primitivecount") ? (size_t)root.get("primitivecount")->number()
                                 : 0;

  // Topology: vertex -> point.
  std::vector<uint32_t> pointRef;
  if (const Json* topology = root.get("topology"))
    if (const Json* pointref = topology->get("pointref"))
      if (const Json* indices = pointref->get("indices"))
        for (const Json& i : indices->array())
          pointRef.push_back((uint32_t)i.number());
  if (pointRef.size() < vertexCount) pointRef.resize(vertexCount, 0);

  const Json* attributes = root.get("attributes");
  const std::vector<GeoAttribute> pointAttrs = geoAttributes(
      attributes ? attributes->get("pointattributes") : nullptr, pointCount);
  const std::vector<GeoAttribute> vertexAttrs = geoAttributes(
      attributes ? attributes->get("vertexattributes") : nullptr, vertexCount);
  const std::vector<GeoAttribute> primAttrs = geoAttributes(
      attributes ? attributes->get("primitiveattributes") : nullptr,
      primitiveCount);
  const GeoAttribute* P = geoFind(pointAttrs, "P");
  if (!P || P->size < 3 || P->count() < pointCount) return std::nullopt;

  const std::vector<GeoPolygon> polygons = geoPolygons(root.get("primitives"));

  Part part;
  part.name = "geo";
  Mesh& mesh = part.mesh;

  // Which class an attribute is read from, vertex first: a vertex uv
  // outranks a point uv, as in Houdini.
  const auto attr =
      [&](std::string_view name) -> std::pair<const GeoAttribute*, bool> {
    if (const GeoAttribute* v = geoFind(vertexAttrs, name)) return {v, true};
    if (const GeoAttribute* p = geoFind(pointAttrs, name)) return {p, false};
    return {nullptr, false};
  };
  const auto [N, nVertex] = attr("N");
  const auto [uv, uvVertex] = attr("uv");
  const auto [Cd, cdVertex] = attr("Cd");
  const auto [Alpha, alphaVertex] = attr("Alpha");

  const bool cloud = polygons.empty();
  // Emit one mesh vertex per (vertex, or point in the cloud case).
  const auto emit = [&](uint32_t vertex, uint32_t point) {
    mesh.positions.emplace_back(P->at(point, 0), P->at(point, 1),
                                P->at(point, 2));
    const auto pick = [&](const GeoAttribute* a, bool fromVertex) -> size_t {
      return fromVertex ? (size_t)vertex : (size_t)point;
    };
    if (N && N->size >= 3) {
      const size_t i = pick(N, nVertex);
      mesh.normals.emplace_back(N->at(i, 0), N->at(i, 1), N->at(i, 2));
    }
    if (uv && uv->size >= 2) {
      const size_t i = pick(uv, uvVertex);
      // Houdini's v runs up the image; the mesh currency's runs down.
      mesh.uvs.emplace_back(uv->at(i, 0), 1.0f - uv->at(i, 1));
    }
    if (Cd && Cd->size >= 3) {
      const size_t i = pick(Cd, cdVertex);
      const float a = Alpha && Alpha->size >= 1
                          ? Alpha->at(pick(Alpha, alphaVertex), 0)
                          : (Cd->size >= 4 ? Cd->at(i, 3) : 1.0f);
      mesh.colors.emplace_back(Cd->at(i, 0), Cd->at(i, 1), Cd->at(i, 2), a);
    }
  };
  std::vector<uint32_t> ownerPoint;  // mesh vertex -> point index
  if (cloud) {
    for (uint32_t p = 0; p < pointCount; ++p) {
      emit(p, p);
      ownerPoint.push_back(p);
    }
  } else {
    // Unwelded: every polygon vertex is its own mesh vertex, so vertex
    // attributes (uv seams, hard normals) survive as authored.
    std::vector<size_t> triPrim;  // triangle -> primitive index
    for (const GeoPolygon& poly : polygons) {
      if (poly.vertices.size() < 3) continue;
      const uint32_t base = (uint32_t)mesh.positions.size();
      for (uint32_t v : poly.vertices) {
        if (v >= pointRef.size()) return std::nullopt;
        emit(v, pointRef[v]);
        ownerPoint.push_back(pointRef[v]);
      }
      for (uint32_t k = 1; k + 1 < (uint32_t)poly.vertices.size(); ++k) {
        mesh.indices.insert(mesh.indices.end(), {base, base + k, base + k + 1});
        triPrim.push_back(poly.primitiveIndex);
      }
    }
    // Primitive attributes and groups: one value per triangle, from
    // the polygon that produced it.
    const auto primLane = [&](const std::string& name, const GeoAttribute& a) {
      std::vector<glm::vec4>& lane = mesh.prim(name, {0, 0, 0, 0});
      for (size_t t = 0; t < triPrim.size(); ++t) {
        const size_t p = triPrim[t];
        if (p >= a.count()) continue;
        glm::vec4 v{0, 0, 0, 0};
        for (int c = 0; c < a.size && c < 4; ++c) v[c] = a.at(p, c);
        if (a.size == 1) v = {v.x, v.x, v.x, v.x};
        if (name == "Color" && a.size == 3) v.w = 1;
        lane[t] = v;
      }
    };
    for (const GeoAttribute& a : primAttrs) {
      if (a.isString) {
        // Houdini's material assignment is a string per primitive; the
        // string table's index is the material slot, and the table's
        // order is the slot order.
        if (a.name == "shop_materialpath") primLane("Material", a);
        continue;
      }
      primLane(a.name == "Cd" ? "Color" : a.name, a);
    }
    if (const Json* groups = root.get("primitivegroups"))
      for (const Json& entry : groups->array()) {
        const Json::Array& pair = entry.array();
        if (pair.size() < 2 || !pair[0].get("name") ||
            !pair[1].get("selection"))
          continue;
        GeoAttribute flags;
        flags.name = pair[0].get("name")->string();
        flags.size = 1;
        flags.values = geoGroup(*pair[1].get("selection"), primitiveCount);
        primLane(flags.name, flags);
      }
  }
  if (mesh.normals.size() != mesh.positions.size()) mesh.normals.clear();
  if (mesh.uvs.size() != mesh.positions.size()) mesh.uvs.clear();
  if (mesh.colors.size() != mesh.positions.size()) mesh.colors.clear();
  if (!cloud && mesh.normals.empty()) mesh.computeNormals();

  // Point-class lanes on the Part, one value per mesh vertex, read
  // through the owning point: every point attribute the conventional
  // slots did not take, and every point group as a 0/1 scalar.
  const size_t n = mesh.positions.size();
  const auto pointLane = [&](const GeoAttribute& a, const std::string& name) {
    if (a.isString) return;
    if (a.size == 1) {
      std::vector<float>& lane = part.scalarLanes[name];
      lane.resize(n);
      for (size_t i = 0; i < n; ++i) lane[i] = a.at(ownerPoint[i], 0);
    } else if (a.size == 3) {
      std::vector<glm::vec3>& lane = part.vectorLanes[name];
      lane.resize(n);
      for (size_t i = 0; i < n; ++i)
        lane[i] = {a.at(ownerPoint[i], 0), a.at(ownerPoint[i], 1),
                   a.at(ownerPoint[i], 2)};
    } else if (a.size == 2 || a.size == 4) {
      std::vector<glm::vec4>& lane = part.colorLanes[name];
      lane.resize(n);
      for (size_t i = 0; i < n; ++i) {
        glm::vec4 v{0, 0, 0, 0};
        for (int c = 0; c < a.size; ++c) v[c] = a.at(ownerPoint[i], c);
        lane[i] = v;
      }
    }
  };
  for (const GeoAttribute& a : pointAttrs) {
    if (a.name == "P") continue;
    if ((a.name == "N" && !nVertex && N) ||
        (a.name == "uv" && !uvVertex && uv) ||
        (a.name == "Cd" && !cdVertex && Cd) ||
        (a.name == "Alpha" && !alphaVertex && Alpha))
      continue;  // taken by the conventional slot
    pointLane(a, a.name);
  }
  if (const Json* groups = root.get("pointgroups"))
    for (const Json& entry : groups->array()) {
      const Json::Array& pair = entry.array();
      if (pair.size() < 2 || !pair[0].get("name") || !pair[1].get("selection"))
        continue;
      GeoAttribute flags;
      flags.name = pair[0].get("name")->string();
      flags.size = 1;
      flags.values = geoGroup(*pair[1].get("selection"), pointCount);
      pointLane(flags, flags.name);
    }

  Model model;
  model.parts.push_back(std::move(part));
  return model;
}

bool looksLikeHoudiniGeo(std::string_view text) {
  const size_t start = text.find_first_not_of(" \t\r\n");
  if (start == std::string_view::npos || text[start] != '[') return false;
  return text.substr(start, 256).find("\"fileversion\"") !=
         std::string_view::npos;
}

}  // namespace sigil::geometry::mesh::codec::decode::detail
