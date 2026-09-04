#pragma once

/** @file
 * Private guts of FontContext, shared by FontContext.cpp and Shaper.cpp:
 * the content-addressed shape key in owning and borrowed forms, the
 * fallback and varied-typeface memo keys, and the Impl that holds every
 * cache. Keeps HarfBuzz and Boost container types out of the public headers
 * and never leaves the fonts directory.
 */

#include <hb.h>

#include <array>
#include <boost/container_hash/hash.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/unordered_node_map.hpp>
#include <concepts>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "OpticalKerning.h"
#include "sigilweave/fonts/FontContext.h"
#include "sigilweave/fonts/Shaper.h"

namespace sigil::weave {

// Everything that identifies a shaping result. Word text is embedded by
// value: two occurrences of the same word with the same style are the same
// cache entry no matter where they sit in the paragraph — that content
// addressing is what makes re-analysis of an edited paragraph cheap.
struct ShapeKey {
  uint32_t typefaceId = 0;
  uint32_t fontSizeBits = 0;
  uint32_t letterSpacingBits = 0;
  uint32_t scaleXBits = 0;
  ScriptTag script = 0;
  bool rightToLeft = false;
  bool vertical = false;
  // Edging does not change SHAPING — the glyph ids, positions and
  // advances are identical either way — but it does change the cached
  // ShapedWord, which carries the flag through to the draw. Keying on it
  // costs one duplicate entry per face-size actually used both ways,
  // and not keying on it silently hands back the wrong rasterisation.
  bool aliased = false;
  // Optical kerning moves the advances, so two words shaped from the same
  // text under the two answers are two different shaped words.
  bool opticalKerning = false;
  std::string languageTag;
  std::vector<FontFeature> fontFeatures;
  std::u16string text;
};

// Borrowed view of a ShapeKey: cache probes hash and compare through this,
// so the hot path (every word of every re-analysis) allocates nothing. An
// owning ShapeKey is only materialized on a cache miss.
struct ShapeKeyView {
  uint32_t typefaceId = 0;
  uint32_t fontSizeBits = 0;
  uint32_t letterSpacingBits = 0;
  uint32_t scaleXBits = 0;
  ScriptTag script = 0;
  bool rightToLeft = false;
  bool vertical = false;
  bool aliased = false;
  bool opticalKerning = false;
  std::string_view languageTag;
  const FontFeature* fontFeatures = nullptr;
  size_t featureCount = 0;
  std::u16string_view text;
};

inline ShapeKeyView makeShapeKeyView(const ShapeKey& key) {
  return {key.typefaceId,
          key.fontSizeBits,
          key.letterSpacingBits,
          key.scaleXBits,
          key.script,
          key.rightToLeft,
          key.vertical,
          key.aliased,
          key.opticalKerning,
          key.languageTag,
          key.fontFeatures.data(),
          key.fontFeatures.size(),
          key.text};
}

inline ShapeKeyView makeShapeKeyView(ShapeKeyView key) { return key; }

/** Owning and borrowed key forms accepted by transparent cache operations. */
template <typename Key>
concept ShapeCacheKey = std::same_as<std::remove_cvref_t<Key>, ShapeKey> ||
                        std::same_as<std::remove_cvref_t<Key>, ShapeKeyView>;

/** Values whose object bytes uniquely and safely represent equality. */
template <typename Value>
concept StableObjectRepresentation =
    std::is_trivially_copyable_v<Value> &&
    std::has_unique_object_representations_v<Value>;

/** Views a non-empty sequence as bytes without constructing from nullptr. */
template <StableObjectRepresentation Value>
std::string_view objectBytes(const Value* values, size_t count) {
  if (count == 0) return {};
  return {reinterpret_cast<const char*>(values), count * sizeof(Value)};
}

// FontFeature is 8 padding-free bytes ({char[4], uint32_t}), so its object
// representation can be hashed and compared directly.
static_assert(sizeof(FontFeature) == 8);
static_assert(StableObjectRepresentation<FontFeature>);

struct ShapeKeyHash {
  using is_transparent = void;
  template <ShapeCacheKey Key>
  size_t operator()(const Key& source) const {
    const ShapeKeyView key = makeShapeKeyView(source);
    const std::array<uint64_t, 3> fixed = {
        uint64_t(key.typefaceId) << 32u | uint64_t(key.fontSizeBits),
        uint64_t(key.letterSpacingBits) << 32u | uint64_t(key.scaleXBits),
        uint64_t(key.script) << 32u | uint64_t(key.rightToLeft) << 3u |
            uint64_t(key.vertical) << 2u | uint64_t(key.aliased) << 1u |
            uint64_t(key.opticalKerning)};
    size_t hash = boost::hash_range(fixed.begin(), fixed.end());
    if (!key.languageTag.empty())
      boost::hash_range(hash, key.languageTag.begin(), key.languageTag.end());
    if (key.featureCount != 0) {
      const std::string_view features =
          objectBytes(key.fontFeatures, key.featureCount);
      boost::hash_range(hash, features.begin(), features.end());
    }
    const std::string_view text = objectBytes(key.text.data(), key.text.size());
    boost::hash_range(hash, text.begin(), text.end());
    return hash;
  }
};

struct ShapeKeyEq {
  using is_transparent = void;
  template <ShapeCacheKey Left, ShapeCacheKey Right>
  bool operator()(const Left& leftSource, const Right& rightSource) const {
    const ShapeKeyView left = makeShapeKeyView(leftSource);
    const ShapeKeyView right = makeShapeKeyView(rightSource);
    return left.typefaceId == right.typefaceId &&
           left.fontSizeBits == right.fontSizeBits &&
           left.letterSpacingBits == right.letterSpacingBits &&
           left.scaleXBits == right.scaleXBits && left.script == right.script &&
           left.rightToLeft == right.rightToLeft &&
           left.vertical == right.vertical && left.aliased == right.aliased &&
           left.opticalKerning == right.opticalKerning &&
           left.languageTag == right.languageTag &&
           left.featureCount == right.featureCount &&
           (left.featureCount == 0 ||
            std::memcmp(left.fontFeatures, right.fontFeatures,
                        left.featureCount * sizeof(FontFeature)) == 0) &&
           left.text == right.text;
  }
};

// (primary typefaceId, codepoint, interned language) -> fallback typeface.
// Interning keeps the hot cache key compact while preserving exact language
// identity (rather than accepting collisions from a truncated string hash).
struct FallbackKey {
  uint32_t typefaceId = 0;
  int32_t codePoint = 0;
  uint32_t languageId = 0;
  bool operator==(const FallbackKey&) const = default;
};

struct FallbackKeyHash {
  size_t operator()(const FallbackKey& key) const {
    size_t hash = 0;
    boost::hash_combine(hash, key.typefaceId);
    boost::hash_combine(hash, key.codePoint);
    boost::hash_combine(hash, key.languageId);
    return hash;
  }
};

// FontVariation is 8 padding-free bytes ({char[4], float}), so a variation
// list memoizes by its raw bytes (bit equality; -0.0/0.0 would be distinct
// entries, which is harmless for a memo). Floats have no unique object
// representation, so this bypasses objectBytes()'s concept on purpose.
static_assert(sizeof(FontVariation) == 8);
static_assert(std::is_trivially_copyable_v<FontVariation>);

// (base typefaceId, variation bytes) -> memoized varied SkTypeface clone.
struct VariedTypefaceKey {
  uint32_t baseTypefaceId = 0;
  std::string variationBytes;
  bool operator==(const VariedTypefaceKey&) const = default;
};

struct VariedTypefaceKeyHash {
  size_t operator()(const VariedTypefaceKey& key) const {
    size_t hash = 0;
    boost::hash_combine(hash, key.baseTypefaceId);
    boost::hash_combine(hash, key.variationBytes);
    return hash;
  }
};

struct FontContext::Impl {
  // One hb_face/hb_font pair per SkTypeface, scaled to the face's unitsPerEm so
  // shaped positions convert to pixels with a single multiply
  // (size/unitsPerEm).
  struct TypefaceRecord {
    hb_face_t* harfBuzzFace = nullptr;
    hb_font_t* harfBuzzFont = nullptr;
    int unitsPerEm = 1000;
    sk_sp<SkTypeface> typeface;  // pins the table-data callback's context
  };

  sk_sp<SkFontMgr> fontManager;
  sk_sp<SkTypeface> defaultTypeface;
  FontContext::FallbackResolver fallbackResolver;

  boost::unordered_flat_map<uint32_t, TypefaceRecord> typefaceRecords;
  boost::unordered_flat_map<FallbackKey, sk_sp<SkTypeface>, FallbackKeyHash>
      fallbackTypefaces;
  boost::unordered_flat_map<VariedTypefaceKey, sk_sp<SkTypeface>,
                            VariedTypefaceKeyHash>
      variedTypefaces;
  boost::unordered_flat_map<std::string, uint32_t> fallbackLanguageIds;
  std::string lastFallbackLanguageTag;
  uint32_t lastFallbackLanguageId = 0;
  uint32_t nextFallbackLanguageId = 1;
  // ASCII fast path: per primary typeface, a direct-mapped table for the
  // codepoints that dominate Latin text, plus a one-entry memo of the last
  // primary used (itemization rarely alternates primaries mid-paragraph).
  using AsciiTable = std::array<SkTypeface*, 128>;
  boost::unordered_node_map<uint32_t, AsciiTable> asciiFallbackTypefaces;
  uint32_t lastAsciiTypefaceId = 0;
  AsciiTable* lastAsciiFallbackTable = nullptr;
  boost::unordered_flat_map<ShapeKey, ShapedWordRef, ShapeKeyHash, ShapeKeyEq>
      shapeCache;

  // (typefaceId, glyph) -> the glyph's edge profile in ems, measured once
  // and good for every size: optical kerning asks for a handful of glyphs
  // per word and the same handful for every word after it.
  boost::unordered_flat_map<uint64_t, detail::GlyphProfile> glyphProfiles;
  // The distance the face's own reference pair leaves, in ems, per face —
  // what optical kerning closes every other pair to. Absent from the map
  // until the face has been asked; kNoInk when the face has no such pair.
  boost::unordered_flat_map<uint32_t, float> referenceGaps;

  // Reused scratch object (the context is single-threaded by contract).
  hb_buffer_t* shapingBuffer = nullptr;

  FontContext::Stats stats;

  // Blunt cap: content-addressed entries are small, but a runaway workload
  // (e.g. fuzzing random strings) shouldn't grow without bound. Clearing
  // wholesale costs one cold frame, then re-fills.
  static constexpr size_t kMaxShapeEntries = size_t{1} << 17u;

  uint32_t fallbackLanguageId(std::string_view languageTag) {
    if (languageTag.empty()) return 0;
    if (languageTag == lastFallbackLanguageTag) return lastFallbackLanguageId;
    auto [entry, inserted] = fallbackLanguageIds.try_emplace(
        std::string(languageTag), nextFallbackLanguageId);
    if (inserted) ++nextFallbackLanguageId;
    lastFallbackLanguageTag = entry->first;
    lastFallbackLanguageId = entry->second;
    return lastFallbackLanguageId;
  }

  TypefaceRecord& recordForTypeface(const sk_sp<SkTypeface>& typeface);
  /** Destroys every HarfBuzz face/font and clears the record map. */
  void destroyTypefaceRecords();
  ~Impl();
};

}  // namespace sigil::weave
