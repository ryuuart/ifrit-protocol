#pragma once

// Per-glyph choreography utilities — the "letters leave their lines"
// pattern (rain, ripples, marquees) distilled from the demos and the
// gallery. Optional layer: nothing in the core pipeline includes this.
//
// The recipe: lay the paragraph out normally, walk every placed glyph with
// its rest position via forEachPlacedGlyph, displace/rotate it however the
// effect wants, and accumulate into GlyphRSXformBatches — one
// drawGlyphsRSXform call per (font, color) instead of thousands of
// per-glyph draws.

#include "Layout.h"
#include "Paragraph.h"
#include "Shaper.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRSXform.h>

#include <array>
#include <cmath>
#include <utility>
#include <vector>

namespace textflow {

// Visits every placed glyph of a layout with its shaped source, glyph ID,
// advance, span color, and absolute rest position. Enumeration order is
// stable across relayouts as long as the text itself is unchanged — which
// is what lets per-glyph particle state survive a per-frame relayout.
// `fn(const ShapedWord *, SkGlyphID, float advance, SkColor, SkPoint rest)`.
template <typename F>
inline void forEachPlacedGlyph(const Layout &layout, const Paragraph &para,
                               F &&fn) {
  static thread_local std::vector<uint32_t> segCounter;
  segCounter.assign(para.words().size(), 0);
  for (const PlacedRun &run : layout.runs) {
    const Word &word = para.words()[run.wordIndex];
    if (word.segments.empty())
      continue; // placeholder slots have no glyphs
    const WordSegment &seg =
        word.segments[segCounter[run.wordIndex]++ % word.segments.size()];
    const SkColor color = para.spans()[seg.styleIndex].style.paint.color;
    const ShapedWord *sw = seg.shaped.get();
    for (size_t i = 0; i < sw->glyphs.size(); ++i)
      fn(sw, sw->glyphs[i], sw->advances[i], color,
         run.origin + sw->positions[i]);
  }
}

// Quantized rotation (64 steps ≈ 5.6°, visually indistinguishable for
// tumbling letters): continuous per-letter angles would re-rasterize every
// glyph mask every frame on the CPU raster backend. The GPU backend doesn't
// need this, but it doesn't hurt there either.
inline void quantAngle(float angle, float &c, float &sn) {
  constexpr int kSteps = 64;
  constexpr float kTwoPi = 6.2831853f;
  static const auto table = [] {
    std::array<std::pair<float, float>, kSteps> t;
    for (int i = 0; i < kSteps; ++i)
      t[static_cast<size_t>(i)] = {std::cos(i * kTwoPi / kSteps),
                                   std::sin(i * kTwoPi / kSteps)};
    return t;
  }();
  int i = static_cast<int>(std::lround(angle / kTwoPi * kSteps)) % kSteps;
  if (i < 0)
    i += kSteps;
  c = table[static_cast<size_t>(i)].first;
  sn = table[static_cast<size_t>(i)].second;
}

// Glyphs grouped by (font source, color): a frame of thousands of animated
// letters collapses into a handful of drawGlyphsRSXform calls. Reuse one
// instance across frames — clear() keeps the allocations.
struct GlyphRSXformBatches {
  struct Batch {
    const ShapedWord *font = nullptr;
    SkColor color = 0;
    std::vector<SkGlyphID> glyphs;
    std::vector<SkRSXform> xforms;
  };
  std::vector<Batch> batches;

  Batch &batchFor(const ShapedWord *font, SkColor color) {
    for (Batch &b : batches)
      if (b.font == font && b.color == color)
        return b;
    batches.push_back({font, color, {}, {}});
    return batches.back();
  }

  // Convenience for the common "advance-centre anchored at `at`, rotated by
  // (c, sn)" placement the effects use.
  void add(const ShapedWord *font, SkColor color, SkGlyphID glyph,
           float halfAdvance, SkPoint at, float c = 1, float sn = 0) {
    Batch &b = batchFor(font, color);
    b.glyphs.push_back(glyph);
    b.xforms.push_back(
        {c, sn, at.x() - c * halfAdvance, at.y() - sn * halfAdvance});
  }

  void clear() {
    for (Batch &b : batches) {
      b.glyphs.clear();
      b.xforms.clear();
    }
  }

  // Returns the number of glyphs drawn.
  int draw(SkCanvas *canvas) const {
    SkPaint paint;
    paint.setAntiAlias(true);
    int total = 0;
    for (const Batch &b : batches) {
      if (b.glyphs.empty())
        continue;
      total += static_cast<int>(b.glyphs.size());
      paint.setColor(b.color);
      SkFont font = makeFont(b.font->typeface, b.font->size);
      // Tumbling letters move whole pixels every frame; subpixel phases
      // would only multiply each (glyph, angle) into fresh atlas strikes —
      // per-frame mask rasterization is exactly what caps these effects.
      font.setSubpixel(false);
      canvas->drawGlyphsRSXform(
          SkSpan<const SkGlyphID>(b.glyphs.data(), b.glyphs.size()),
          SkSpan<const SkRSXform>(b.xforms.data(), b.xforms.size()), {0, 0},
          font, paint);
    }
    return total;
  }
};

} // namespace textflow
