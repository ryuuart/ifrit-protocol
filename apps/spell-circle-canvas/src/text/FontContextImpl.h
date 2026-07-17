#pragma once

// Private guts of FontContext, shared by FontContext.cpp, Shaper.cpp and
// Paragraph.cpp. Keeps HarfBuzz/ICU/absl types out of the public headers.

#include "textflow/FontContext.h"
#include "textflow/Shaper.h"

#include <absl/container/flat_hash_map.h>
#include <absl/hash/hash.h>

#include <hb.h>
#include <unicode/ubidi.h>
#include <unicode/ubrk.h>

#include <array>
#include <concepts>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace textflow {

// Everything that identifies a shaping result. Word text is embedded by
// value: two occurrences of the same word with the same style are the same
// cache entry no matter where they sit in the paragraph — that content
// addressing is what makes re-analysis of an edited paragraph cheap.
struct ShapeKey {
  uint32_t typefaceId = 0;
  uint32_t fontSizeBits = 0;
  uint32_t letterSpacingBits = 0;
  ScriptTag script = 0;
  bool rightToLeft = false;
  bool vertical = false;
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
  ScriptTag script = 0;
  bool rightToLeft = false;
  bool vertical = false;
  std::string_view languageTag;
  const FontFeature *fontFeatures = nullptr;
  size_t featureCount = 0;
  std::u16string_view text;
};

inline ShapeKeyView makeShapeKeyView(const ShapeKey &key) {
  return {key.typefaceId,  key.fontSizeBits,        key.letterSpacingBits,
          key.script,      key.rightToLeft,         key.vertical,
          key.languageTag, key.fontFeatures.data(), key.fontFeatures.size(),
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
std::string_view objectBytes(const Value *values, size_t count) {
  if (count == 0)
    return {};
  return {reinterpret_cast<const char *>(values), count * sizeof(Value)};
}

// FontFeature is 8 padding-free bytes ({char[4], uint32_t}), so its object
// representation can be hashed and compared directly.
static_assert(sizeof(FontFeature) == 8);
static_assert(StableObjectRepresentation<FontFeature>);

struct ShapeKeyHash {
  using is_transparent = void;
  template <ShapeCacheKey Key> size_t operator()(const Key &source) const {
    const ShapeKeyView key = makeShapeKeyView(source);
    return absl::HashOf(key.typefaceId, key.fontSizeBits, key.letterSpacingBits,
                        key.script, key.rightToLeft, key.vertical,
                        key.languageTag,
                        objectBytes(key.fontFeatures, key.featureCount),
                        objectBytes(key.text.data(), key.text.size()));
  }
};

struct ShapeKeyEq {
  using is_transparent = void;
  template <ShapeCacheKey Left, ShapeCacheKey Right>
  bool operator()(const Left &leftSource, const Right &rightSource) const {
    const ShapeKeyView left = makeShapeKeyView(leftSource);
    const ShapeKeyView right = makeShapeKeyView(rightSource);
    return left.typefaceId == right.typefaceId &&
           left.fontSizeBits == right.fontSizeBits &&
           left.letterSpacingBits == right.letterSpacingBits &&
           left.script == right.script &&
           left.rightToLeft == right.rightToLeft &&
           left.vertical == right.vertical &&
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
  bool operator==(const FallbackKey &) const = default;
};

template <typename H> H AbslHashValue(H hashState, const FallbackKey &key) {
  return H::combine(std::move(hashState), key.typefaceId, key.codePoint,
                    key.languageId);
}

struct FontContext::Impl {
  // One hb_face/hb_font pair per SkTypeface, scaled to the face's unitsPerEm so
  // shaped positions convert to pixels with a single multiply
  // (size/unitsPerEm).
  struct TypefaceRecord {
    hb_face_t *harfBuzzFace = nullptr;
    hb_font_t *harfBuzzFont = nullptr;
    int unitsPerEm = 1000;
    sk_sp<SkTypeface> typeface; // pins the table-data callback's context
  };

  sk_sp<SkFontMgr> fontManager;
  sk_sp<SkTypeface> defaultTypeface;
  FontContext::FallbackResolver fallbackResolver;

  absl::flat_hash_map<uint32_t, TypefaceRecord> typefaceRecords;
  absl::flat_hash_map<FallbackKey, sk_sp<SkTypeface>> fallbackTypefaces;
  absl::flat_hash_map<std::string, uint32_t> fallbackLanguageIds;
  std::string lastFallbackLanguageTag;
  uint32_t lastFallbackLanguageId = 0;
  uint32_t nextFallbackLanguageId = 1;
  // ASCII fast path: per primary typeface, a direct-mapped table for the
  // codepoints that dominate Latin text, plus a one-entry memo of the last
  // primary used (itemization rarely alternates primaries mid-paragraph).
  using AsciiTable = std::array<SkTypeface *, 128>;
  absl::flat_hash_map<uint32_t, AsciiTable> asciiFallbackTypefaces;
  uint32_t lastAsciiTypefaceId = 0;
  AsciiTable *lastAsciiFallbackTable = nullptr;
  absl::flat_hash_map<ShapeKey, ShapedWordRef, ShapeKeyHash, ShapeKeyEq>
      shapeCache;

  // Reused scratch objects (the context is single-threaded by contract).
  hb_buffer_t *shapingBuffer = nullptr;
  UBreakIterator *lineBreakIterator = nullptr; // lazily created, root locale
  UBiDi *bidirectionalAnalyzer = nullptr;      // setPara() reuses it

  FontContext::Stats stats;

  // Blunt cap: content-addressed entries are small, but a runaway workload
  // (e.g. fuzzing random strings) shouldn't grow without bound. Clearing
  // wholesale costs one cold frame, then re-fills.
  static constexpr size_t kMaxShapeEntries = 1 << 17;

  uint32_t fallbackLanguageId(std::string_view languageTag) {
    if (languageTag.empty())
      return 0;
    if (languageTag == lastFallbackLanguageTag)
      return lastFallbackLanguageId;
    auto [entry, inserted] = fallbackLanguageIds.try_emplace(
        std::string(languageTag), nextFallbackLanguageId);
    if (inserted)
      ++nextFallbackLanguageId;
    lastFallbackLanguageTag = entry->first;
    lastFallbackLanguageId = entry->second;
    return lastFallbackLanguageId;
  }

  TypefaceRecord &recordForTypeface(const sk_sp<SkTypeface> &typeface);
  /** Destroys every HarfBuzz face/font and clears the record map. */
  void destroyTypefaceRecords();
  ~Impl();
};

} // namespace textflow
