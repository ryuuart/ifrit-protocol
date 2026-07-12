#pragma once

// Private guts of FontContext, shared by FontContext.cpp, Shaper.cpp and
// Paragraph.cpp. Keeps HarfBuzz/ICU/absl types out of the public headers.

#include "textflow/FontContext.h"
#include "textflow/Shaper.h"

#include <absl/container/flat_hash_map.h>
#include <absl/hash/hash.h>

#include <hb.h>
#include <unicode/ubrk.h>

#include <array>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace textflow {

// Everything that identifies a shaping result. Word text is embedded by
// value: two occurrences of the same word with the same style are the same
// cache entry no matter where they sit in the paragraph — that content
// addressing is what makes re-analysis of an edited paragraph cheap.
struct ShapeKey {
  uint32_t typefaceID = 0;
  uint32_t sizeBits = 0;
  uint32_t letterSpacingBits = 0;
  ScriptTag script = 0;
  bool rtl = false;
  std::string language;
  std::vector<FontFeature> features;
  std::u16string text;
};

// Borrowed view of a ShapeKey: cache probes hash and compare through this,
// so the hot path (every word of every re-analysis) allocates nothing. An
// owning ShapeKey is only materialized on a cache miss.
struct ShapeKeyView {
  uint32_t typefaceID = 0;
  uint32_t sizeBits = 0;
  uint32_t letterSpacingBits = 0;
  ScriptTag script = 0;
  bool rtl = false;
  std::string_view language;
  const FontFeature *features = nullptr;
  size_t featureCount = 0;
  std::u16string_view text;
};

inline ShapeKeyView viewOf(const ShapeKey &k) {
  return {k.typefaceID, k.sizeBits,       k.letterSpacingBits,
          k.script,     k.rtl,            k.language,
          k.features.data(), k.features.size(), k.text};
}

// FontFeature is 8 padding-free bytes ({char[4], uint32_t}) and its
// operator== is a memcmp, so hashing/comparing raw bytes is sound.
static_assert(sizeof(FontFeature) == 8);

struct ShapeKeyHash {
  using is_transparent = void;
  size_t operator()(const ShapeKeyView &v) const {
    return absl::HashOf(
        v.typefaceID, v.sizeBits, v.letterSpacingBits, v.script, v.rtl,
        v.language,
        std::string_view(reinterpret_cast<const char *>(v.features),
                         v.featureCount * sizeof(FontFeature)),
        std::string_view(reinterpret_cast<const char *>(v.text.data()),
                         v.text.size() * sizeof(char16_t)));
  }
  size_t operator()(const ShapeKey &k) const { return (*this)(viewOf(k)); }
};

struct ShapeKeyEq {
  using is_transparent = void;
  bool operator()(const ShapeKeyView &a, const ShapeKeyView &b) const {
    return a.typefaceID == b.typefaceID && a.sizeBits == b.sizeBits &&
           a.letterSpacingBits == b.letterSpacingBits &&
           a.script == b.script && a.rtl == b.rtl && a.language == b.language &&
           a.featureCount == b.featureCount &&
           std::memcmp(a.features, b.features,
                       a.featureCount * sizeof(FontFeature)) == 0 &&
           a.text == b.text;
  }
  bool operator()(const ShapeKey &a, const ShapeKey &b) const {
    return (*this)(viewOf(a), viewOf(b));
  }
  bool operator()(const ShapeKey &a, const ShapeKeyView &b) const {
    return (*this)(viewOf(a), b);
  }
  bool operator()(const ShapeKeyView &a, const ShapeKey &b) const {
    return (*this)(a, viewOf(b));
  }
};

struct FontContext::Impl {
  // One hb_face/hb_font pair per SkTypeface, scaled to the face's upem so
  // shaped positions convert to pixels with a single multiply (size/upem).
  struct FaceRec {
    hb_face_t *face = nullptr;
    hb_font_t *font = nullptr;
    int upem = 1000;
    sk_sp<SkTypeface> typeface; // pins the table-data callback's context
  };

  sk_sp<SkFontMgr> fontMgr;
  sk_sp<SkTypeface> defaultTypeface;

  absl::flat_hash_map<uint32_t, FaceRec> faces;
  // (primary typefaceID, codepoint) → typeface that renders it.
  absl::flat_hash_map<uint64_t, sk_sp<SkTypeface>> fallback;
  // ASCII fast path: per primary typeface, a direct-mapped table for the
  // codepoints that dominate Latin text, plus a one-entry memo of the last
  // primary used (itemization rarely alternates primaries mid-paragraph).
  using AsciiTable = std::array<SkTypeface *, 128>;
  absl::flat_hash_map<uint32_t, AsciiTable> asciiFallback;
  uint32_t lastAsciiID = 0;
  AsciiTable *lastAsciiTable = nullptr;
  absl::flat_hash_map<ShapeKey, ShapedWordRef, ShapeKeyHash, ShapeKeyEq>
      shapeCache;

  // Reused scratch objects (the context is single-threaded by contract).
  hb_buffer_t *buffer = nullptr;
  UBreakIterator *lineBreaker = nullptr; // lazily created, root locale

  FontContext::Stats stats;

  // Blunt cap: content-addressed entries are small, but a runaway workload
  // (e.g. fuzzing random strings) shouldn't grow without bound. Clearing
  // wholesale costs one cold frame, then re-fills.
  static constexpr size_t kMaxShapeEntries = 1 << 17;

  FaceRec &faceFor(const sk_sp<SkTypeface> &typeface);
  ~Impl();
};

} // namespace textflow
