/** @file
 * THE .geo ATTRIBUTE CLASSES: a values block in each of the three
 * spellings a Houdini file writes it in, one class list decoded into
 * named attributes, and a group read as a per-element flag.
 *
 * Every one of them answers in flat floats, so the topology decoding
 * that places them on a Part never learns which spelling the file used.
 */

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "GeoInternal.h"

namespace sigil::geometry::mesh::codec::decode::detail {

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
    GeoAttribute attribute;
    attribute.name = name->string();
    if (type->string() == "numeric") {
      attribute.size =
          payload.get("size") ? (int)payload.get("size")->number() : 1;
      const Json* values = payload.get("values");
      if (!values || attribute.size < 1 ||
          !geoDecodeValues(*values, count, attribute.size, attribute.values))
        continue;
    } else if (type->string() == "string") {
      attribute.isString = true;
      attribute.size = 1;
      if (const Json* strings = payload.get("strings"))
        for (const Json& str : strings->array())
          attribute.strings.push_back(str.string());
      const Json* indices = payload.get("indices");
      if (!indices || !geoDecodeValues(*indices, count, 1, attribute.values))
        continue;
    } else {
      continue;
    }
    out.push_back(std::move(attribute));
  }
  return out;
}

const GeoAttribute* geoFind(const std::vector<GeoAttribute>& attributes,
                            std::string_view name) {
  for (const GeoAttribute& a : attributes)
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

}  // namespace sigil::geometry::mesh::codec::decode::detail
