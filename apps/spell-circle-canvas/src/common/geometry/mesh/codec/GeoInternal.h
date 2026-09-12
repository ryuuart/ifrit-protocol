#pragma once

/** @file
 * The small value a Houdini .geo is addressed through, and the file's
 * attribute classes decoded out of it. A `.geo` writes most of its
 * members as arrays laid out as alternating key and value, which no JSON
 * library models, so the document is read into this tree and addressed
 * by `Json::get`. Private to the .geo reader.
 */

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace sigil::geometry::mesh::codec::decode::detail {

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
 *  @p count tuples of @p size. */
bool geoDecodeValues(const Json& block, size_t count, int size,
                     std::vector<float>& out);

/** One attribute class list ("pointattributes" etc.), decoded. */
std::vector<GeoAttribute> geoAttributes(const Json* list, size_t count);

/** The attribute of that name, or nullptr where the class has none. */
const GeoAttribute* geoFind(const std::vector<GeoAttribute>& attributes,
                            std::string_view name);

/** A group's membership as a per-element flag list. */
std::vector<float> geoGroup(const Json& selection, size_t count);

}  // namespace sigil::geometry::mesh::codec::decode::detail
