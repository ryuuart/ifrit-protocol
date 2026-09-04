/** @file
 * Regions cut from a sheet, the sequences derived from frame names and
 * tags, the two sprite-tool JSON readers, and the packer that lays loose
 * images into one sheet.
 */

#include "sigilmaterial/texture/Atlas.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <simdjson.h>

#include <algorithm>
#include <boost/container/flat_map.hpp>
#include <cctype>
#include <cstdlib>
#include <vector>

#define STB_RECT_PACK_IMPLEMENTATION
#include <stb_rect_pack.h>

namespace sigil::material {

Atlas::Atlas(Texture sheet, std::vector<AtlasRegion> regions)
    : m_sheet(std::move(sheet)), m_regions(std::move(regions)) {}

Atlas Atlas::grid(Texture sheet, int cols, int rows) {
  cols = std::max(cols, 1);
  rows = std::max(rows, 1);
  const SkISize size = sheet.size();
  const int cw = size.width() / cols, ch = size.height() / rows;
  std::vector<AtlasRegion> regions;
  std::vector<size_t> all;
  regions.reserve((size_t)cols * rows);
  for (int r = 0; r < rows; ++r) {
    for (int c = 0; c < cols; ++c) {
      AtlasRegion region;
      region.name = std::to_string(regions.size());
      region.rect = SkIRect::MakeXYWH(c * cw, r * ch, cw, ch);
      region.sourceSize = {cw, ch};
      all.push_back(regions.size());
      regions.push_back(std::move(region));
    }
  }
  Atlas atlas(std::move(sheet), std::move(regions));
  atlas.sequence("all", std::move(all));
  return atlas;
}

const AtlasRegion* Atlas::find(std::string_view name) const {
  for (const AtlasRegion& r : m_regions)
    if (r.name == name) return &r;
  return nullptr;
}

Texture Atlas::region(size_t index) const {
  if (index >= m_regions.size()) return {};
  Texture out = m_sheet;
  out.region(m_regions[index].rect);
  return out;
}

Texture Atlas::region(std::string_view name) const {
  for (size_t i = 0; i < m_regions.size(); ++i)
    if (m_regions[i].name == name) return region(i);
  return {};
}

Atlas& Atlas::sequence(std::string name, std::vector<size_t> frames) {
  m_sequences[std::move(name)] = std::move(frames);
  return *this;
}

const std::vector<size_t>* Atlas::sequence(std::string_view name) const {
  auto it = m_sequences.find(std::string(name));
  return it == m_sequences.end() ? nullptr : &it->second;
}

Texture Atlas::frame(std::string_view sequence, size_t index) const {
  const std::vector<size_t>* frames = this->sequence(sequence);
  if (!frames || frames->empty()) return {};
  return region((*frames)[index % frames->size()]);
}

// ---------------------------------------------------------------------------
// The sprite-tool JSON: both tools write the same frame record, keyed by
// name in the hash form or carrying a "filename" in the array form.

namespace {

/** A frame name without its extension. */
std::string stem(std::string_view name) {
  const size_t dot = name.rfind('.');
  return std::string(dot == std::string_view::npos ? name
                                                   : name.substr(0, dot));
}

/** `walk_01` → ("walk", 1); a name without a trailing number → (name, -1). */
std::pair<std::string, long> split(std::string_view name) {
  size_t end = name.size();
  while (end > 0 && std::isdigit((unsigned char)name[end - 1])) --end;
  if (end == name.size()) return {std::string(name), -1};
  const long number =
      std::strtol(std::string(name.substr(end)).c_str(), nullptr, 10);
  while (end > 0 &&
         (name[end - 1] == '_' || name[end - 1] == '-' || name[end - 1] == ' '))
    --end;
  return {std::string(name.substr(0, end)), number};
}

bool readRect(simdjson::simdjson_result<simdjson::dom::element> e,
              SkIRect* out) {
  int64_t x, y, w, h;
  if (e["x"].get(x) || e["y"].get(y) || e["w"].get(w) || e["h"].get(h))
    return false;
  *out = SkIRect::MakeXYWH((int)x, (int)y, (int)w, (int)h);
  return true;
}

bool readFrame(std::string_view name, simdjson::dom::element e,
               AtlasRegion* out) {
  SkIRect frame;
  if (!readRect(e["frame"], &frame)) return false;
  out->name = stem(name);
  out->rect = frame;
  bool rotated = false;
  if (!e["rotated"].get(rotated)) out->rotated = rotated;
  out->sourceSize = frame.size();
  int64_t w, h;
  if (!e["sourceSize"]["w"].get(w) && !e["sourceSize"]["h"].get(h))
    out->sourceSize = {(int)w, (int)h};
  SkIRect sprite;
  if (readRect(e["spriteSourceSize"], &sprite))
    out->sourceOffset = {sprite.left(), sprite.top()};
  return true;
}

/** Every frame record of @p doc in file order; false when there is no
 *  "frames" member of either shape. */
bool readFrames(simdjson::dom::element doc, std::vector<AtlasRegion>* out) {
  simdjson::dom::element frames;
  if (doc["frames"].get(frames)) return false;
  if (frames.is_object()) {
    for (auto [key, value] : simdjson::dom::object(frames)) {
      AtlasRegion region;
      if (readFrame(key, value, &region)) out->push_back(std::move(region));
    }
    return true;
  }
  if (frames.is_array()) {
    for (simdjson::dom::element value : simdjson::dom::array(frames)) {
      std::string_view filename;
      if (value["filename"].get(filename)) filename = "";
      AtlasRegion region;
      if (readFrame(filename, value, &region))
        out->push_back(std::move(region));
    }
    return true;
  }
  return false;
}

/** Sequences from the regions' names: frames sharing a stem, ordered by
 *  their trailing number. A name with no number is a sequence of one. */
void sequencesFromNames(Atlas* atlas) {
  boost::container::flat_map<std::string, std::vector<std::pair<long, size_t>>>
      groups;
  const std::span<const AtlasRegion> regions = atlas->regions();
  for (size_t i = 0; i < regions.size(); ++i) {
    auto [name, number] = split(regions[i].name);
    groups[name].emplace_back(number, i);
  }
  for (auto& [name, frames] : groups) {
    std::stable_sort(
        frames.begin(), frames.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });
    std::vector<size_t> indices;
    indices.reserve(frames.size());
    for (const auto& [number, index] : frames) indices.push_back(index);
    atlas->sequence(name, std::move(indices));
  }
}

}  // namespace

std::optional<Atlas> Atlas::fromTexturePacker(Texture sheet,
                                              std::string_view json) {
  simdjson::dom::parser parser;
  simdjson::dom::element doc;
  if (parser.parse(simdjson::padded_string(json)).get(doc)) return std::nullopt;
  std::vector<AtlasRegion> regions;
  if (!readFrames(doc, &regions)) return std::nullopt;
  Atlas atlas(std::move(sheet), std::move(regions));
  sequencesFromNames(&atlas);
  return atlas;
}

std::optional<Atlas> Atlas::fromAseprite(Texture sheet, std::string_view json) {
  simdjson::dom::parser parser;
  simdjson::dom::element doc;
  if (parser.parse(simdjson::padded_string(json)).get(doc)) return std::nullopt;
  std::vector<AtlasRegion> regions;
  if (!readFrames(doc, &regions)) return std::nullopt;
  const size_t count = regions.size();
  Atlas atlas(std::move(sheet), std::move(regions));
  simdjson::dom::array tags;
  bool tagged = false;
  if (!doc["meta"]["frameTags"].get(tags)) {
    for (simdjson::dom::element tag : tags) {
      std::string_view name;
      int64_t from, to;
      if (tag["name"].get(name) || tag["from"].get(from) || tag["to"].get(to))
        continue;
      std::vector<size_t> frames;
      for (int64_t i = from; i <= to && i >= 0 && (size_t)i < count; ++i)
        frames.push_back((size_t)i);
      atlas.sequence(std::string(name), std::move(frames));
      tagged = true;
    }
  }
  if (!tagged) {
    std::vector<size_t> all(count);
    for (size_t i = 0; i < count; ++i) all[i] = i;
    atlas.sequence("all", std::move(all));
  }
  return atlas;
}

// ---------------------------------------------------------------------------
// Packing

Atlas Atlas::pack(
    const std::vector<std::pair<std::string, sk_sp<SkImage>>>& images,
    int padding, int maxSide) {
  padding = std::max(padding, 0);
  std::vector<stbrp_rect> rects;
  rects.reserve(images.size());
  long area = 0;
  for (size_t i = 0; i < images.size(); ++i) {
    const sk_sp<SkImage>& img = images[i].second;
    if (!img) continue;
    stbrp_rect r{};
    r.id = (int)i;
    r.w = (stbrp_coord)(img->width() + padding);
    r.h = (stbrp_coord)(img->height() + padding);
    area += (long)r.w * r.h;
    rects.push_back(r);
  }
  // The smallest power-of-two square whose area holds every rect, grown
  // until the packer places them all or the side reaches the ceiling.
  int side = 16;
  while ((long)side * side < area && side < maxSide) side *= 2;
  side = std::min(side, std::max(maxSide, 16));
  std::vector<stbrp_node> nodes;
  for (;;) {
    nodes.assign((size_t)side + 16, stbrp_node{});
    stbrp_context context;
    stbrp_init_target(&context, side, side, nodes.data(), (int)nodes.size());
    const int all = rects.empty() ? 1
                                  : stbrp_pack_rects(&context, rects.data(),
                                                     (int)rects.size());
    if (all || side >= maxSide) break;
    side *= 2;
  }

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(side, side));
  std::vector<AtlasRegion> regions;
  if (surface) {
    SkCanvas* canvas = surface->getCanvas();
    canvas->clear(SK_ColorTRANSPARENT);
    for (const stbrp_rect& r : rects) {
      if (!r.was_packed) continue;
      const auto& [name, img] = images[(size_t)r.id];
      canvas->drawImage(img, (float)r.x, (float)r.y);
      AtlasRegion region;
      region.name = name;
      region.rect = SkIRect::MakeXYWH(r.x, r.y, img->width(), img->height());
      region.sourceSize = region.rect.size();
      regions.push_back(std::move(region));
    }
  }
  return Atlas(Texture::of(surface ? surface->makeImageSnapshot() : nullptr),
               std::move(regions));
}

}  // namespace sigil::material
