#pragma once

/** @file
 * Atlas — one sheet texture, the named regions cut from it, and the
 * named sequences of regions that play as frames. Built from a grid, from
 * the JSON a sprite tool writes (TexturePacker, Aseprite), or by packing
 * loose images into a sheet.
 */

#include <include/core/SkImage.h>
#include <include/core/SkRect.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSize.h>
#include <sigilmaterial/texture/Texture.h>

#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::material {

/** One region of a sheet: where it sits, and — for a sprite the tool
 *  trimmed — where the trimmed pixels sit in the sprite's untrimmed
 *  frame. */
struct AtlasRegion {
  std::string name;
  /** The pixels on the sheet. */
  SkIRect rect = SkIRect::MakeEmpty();
  /** The sheet holds this region turned a quarter turn clockwise. */
  bool rotated = false;
  /** The untrimmed sprite size; `rect`'s size when nothing was trimmed. */
  SkISize sourceSize = SkISize::MakeEmpty();
  /** Where `rect` sits inside the untrimmed sprite. */
  SkIPoint sourceOffset = {0, 0};

  bool operator==(const AtlasRegion&) const = default;
};

/** A sheet and its regions. Copyable; the sheet texture is shared. */
class Atlas {
 public:
  Atlas() = default;
  Atlas(Texture sheet, std::vector<AtlasRegion> regions);

  /** @p cols by @p rows equal cells over the whole sheet, row-major,
   *  named by index, with one sequence "all" running through them. */
  static Atlas grid(Texture sheet, int cols, int rows);
  /** Regions from TexturePacker's JSON (hash or array form). A sequence
   *  is derived per name stem: `walk_01`, `walk_02` become "walk" in
   *  numeric order. Nullopt when the text is not that JSON. */
  static std::optional<Atlas> fromTexturePacker(Texture sheet,
                                                std::string_view json);
  /** Regions from Aseprite's JSON export (hash or array form), with a
   *  sequence per frame tag and, when there are no tags, one sequence
   *  "all". Nullopt when the text is not that JSON. */
  static std::optional<Atlas> fromAseprite(Texture sheet,
                                           std::string_view json);
  /** Packs @p images into one sheet with @p padding pixels between them,
   *  the sheet a power of two no larger than @p maxSide on a side. An
   *  image that does not fit is left out of the regions. */
  static Atlas pack(
      const std::vector<std::pair<std::string, sk_sp<SkImage>>>& images,
      int padding = 1, int maxSide = 4096);

  const Texture& sheet() const { return m_sheet; }
  std::span<const AtlasRegion> regions() const { return m_regions; }
  /** The region named @p name, or null. */
  const AtlasRegion* find(std::string_view name) const;
  /** The sheet cut to region @p index; an empty texture past the end. */
  Texture region(size_t index) const;
  /** The sheet cut to the region named @p name; empty when unknown. */
  Texture region(std::string_view name) const;

  /** Names a sequence of region indices; replaces one of the same name. */
  Atlas& sequence(std::string name, std::vector<size_t> frames);
  const std::map<std::string, std::vector<size_t>>& sequences() const {
    return m_sequences;
  }
  /** The frames of @p name, or null. */
  const std::vector<size_t>* sequence(std::string_view name) const;
  /** Frame @p index of @p sequence, wrapping past the end; empty when the
   *  sequence is unknown or has no frames. */
  Texture frame(std::string_view sequence, size_t index) const;

  bool operator==(const Atlas& other) const {
    return m_sheet == other.m_sheet && m_regions == other.m_regions &&
           m_sequences == other.m_sequences;
  }

 private:
  Texture m_sheet;
  std::vector<AtlasRegion> m_regions;
  std::map<std::string, std::vector<size_t>> m_sequences;
};

}  // namespace sigil::material
