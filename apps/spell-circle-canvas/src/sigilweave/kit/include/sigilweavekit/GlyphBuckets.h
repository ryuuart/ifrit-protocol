#pragma once

/** @file
 * Keyed glyph-bucket accumulator — the general form of the batching that
 * makes per-glyph choreography affordable. A frame of thousands of
 * individually animated letters must not become thousands of draw calls;
 * grouping glyphs by whatever determines their draw state (font source,
 * quantized brightness, fade level, …) collapses the frame into a handful
 * of drawGlyphs calls.
 *
 * sigil::weave::GlyphRSXformBatches is the fixed (font, color)+RSXform special
 * case; this template is for effects whose bucket key or placement type is
 * richer. The key is any equality-comparable value — a raw pointer, a
 * std::tuple, or a small struct with a defaulted `==` — and drawing stays a
 * caller lambda so each effect derives paints from its key however it wants
 * (including drawing the same buckets in several passes).
 */

#include <sigilweave/Shaper.h>

#include <include/core/SkCanvas.h>
#include <include/core/SkFont.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPoint.h>

#include <vector>

namespace sigil::weave::kit {

/**
 * Glyphs grouped by an arbitrary equality-comparable `Key`, with a parallel
 * per-glyph `Placement` array (SkPoint for drawGlyphs, SkRSXform for
 * drawGlyphsRSXform). Reuse one instance across frames — clear() keeps the
 * allocations. Bucket lookup is a linear scan: effects quantize their keys
 * precisely so the bucket count stays tiny.
 *
 * ```
 * struct Shade { const ShapedWord *font; int level; bool operator==(const Shade &) const = default; };
 * GlyphBuckets<Shade> m_buckets;
 * ...
 * m_buckets.add({&shaped, level}, glyphId, position);
 * ...
 * m_buckets.drawEach([&](const auto &bucket) { ... one drawGlyphs ... });
 * ```
 */
template <typename Key, typename Placement = SkPoint> struct GlyphBuckets {
  /// One key's worth of glyphs: parallel arrays feeding a single draw call.
  struct Bucket {
    Key key{};
    std::vector<SkGlyphID> glyphs;     ///< parallel to `placements`
    std::vector<Placement> placements; ///< per-glyph position/transform
  };
  std::vector<Bucket> buckets; ///< one entry per distinct key

  /** Returns the bucket for `key`, creating it when absent. */
  [[nodiscard]] Bucket &bucketFor(const Key &key) {
    for (Bucket &bucket : buckets)
      if (bucket.key == key)
        return bucket;
    buckets.push_back({key, {}, {}});
    return buckets.back();
  }

  /** Appends one glyph to its key's bucket. */
  void add(const Key &key, SkGlyphID glyph, const Placement &placement) {
    Bucket &bucket = bucketFor(key);
    bucket.glyphs.push_back(glyph);
    bucket.placements.push_back(placement);
  }

  /** Clears glyph data while retaining bucket allocations for the next
   *  frame. */
  void clear() {
    for (Bucket &bucket : buckets) {
      bucket.glyphs.clear();
      bucket.placements.clear();
    }
  }

  /** Visits every non-empty bucket with `drawFn(const Bucket &)` and
   *  returns the total glyph count visited. Call once per draw pass. */
  template <typename DrawFn> int drawEach(DrawFn &&drawFn) const {
    int total = 0;
    for (const Bucket &bucket : buckets) {
      if (bucket.glyphs.empty())
        continue;
      total += static_cast<int>(bucket.glyphs.size());
      drawFn(bucket);
    }
    return total;
  }
};

/** Draws one positioned-glyph bucket with the font of its shaped source —
 *  the standard flush for a `GlyphBuckets<Key, SkPoint>` pass. */
inline void drawPositionedGlyphs(SkCanvas *canvas,
                                 const sigil::weave::ShapedWord &font,
                                 const std::vector<SkGlyphID> &glyphs,
                                 const std::vector<SkPoint> &positions,
                                 SkPoint origin, const SkPaint &paint) {
  canvas->drawGlyphs(SkSpan<const SkGlyphID>(glyphs.data(), glyphs.size()),
                     SkSpan<const SkPoint>(positions.data(), positions.size()),
                     origin, sigil::weave::makeFont(font.typeface, font.fontSize),
                     paint);
}

} // namespace sigil::weave::kit
