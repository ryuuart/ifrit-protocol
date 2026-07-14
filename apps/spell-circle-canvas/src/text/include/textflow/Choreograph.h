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

#include "Paragraph.h"
#include "ParagraphLayout.h"
#include "Shaper.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRSXform.h>

#include <array>
#include <cmath>
#include <concepts>
#include <numbers>
#include <utility>
#include <vector>

namespace textflow {

// Visits every placed glyph of a layout with its shaped source, glyph ID,
// advance, span color, and absolute rest position. Enumeration order is
// stable across relayouts as long as the text itself is unchanged — which
// is what lets per-glyph particle state survive a per-frame relayout.
// `fn(const ShapedWord *, SkGlyphID, float advance, SkColor, SkPoint rest)`.
/** Callable accepted by forEachPlacedGlyph(). */
template <typename Visitor>
concept PlacedGlyphVisitor = std::invocable<Visitor &, const ShapedWord *,
                                            SkGlyphID, float, SkColor, SkPoint>;

template <PlacedGlyphVisitor Visitor>
inline void forEachPlacedGlyph(const ParagraphLayout &layout,
                               const Paragraph &paragraph, Visitor &&visitor) {
  static thread_local std::vector<uint32_t> segmentCounters;
  segmentCounters.assign(paragraph.words().size(), 0);
  for (const PositionedRun &run : layout.runs) {
    const Word &word = paragraph.words()[run.wordIndex];
    if (word.segments.empty())
      continue; // placeholder slots have no glyphs
    const WordSegment &segment =
        word.segments[segmentCounters[run.wordIndex]++ % word.segments.size()];
    const SkColor color =
        paragraph.spans()[segment.styleIndex].style.paint.color;
    const ShapedWord *shapedWord = segment.shaped.get();
    for (size_t glyphIndex = 0; glyphIndex < shapedWord->glyphs.size();
         ++glyphIndex)
      visitor(shapedWord, shapedWord->glyphs[glyphIndex],
              shapedWord->advances[glyphIndex], color,
              run.origin + shapedWord->positions[glyphIndex]);
  }
}

// Quantized rotation (64 steps ≈ 5.6°, visually indistinguishable for
// tumbling letters): continuous per-letter angles would re-rasterize every
// glyph mask every frame on the CPU raster backend. The GPU backend doesn't
// need this, but it doesn't hurt there either.
inline void quantizeAngle(float angle, float &cosine, float &sine) {
  constexpr int kSteps = 64;
  constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;
  static const auto angleTable = [] {
    std::array<std::pair<float, float>, kSteps> table;
    for (int stepIndex = 0; stepIndex < kSteps; ++stepIndex)
      table[static_cast<size_t>(stepIndex)] = {
          std::cos(stepIndex * kTwoPi / kSteps),
          std::sin(stepIndex * kTwoPi / kSteps)};
    return table;
  }();
  int stepIndex =
      static_cast<int>(std::lround(angle / kTwoPi * kSteps)) % kSteps;
  if (stepIndex < 0)
    stepIndex += kSteps;
  cosine = angleTable[static_cast<size_t>(stepIndex)].first;
  sine = angleTable[static_cast<size_t>(stepIndex)].second;
}

// Glyphs grouped by (font source, color): a frame of thousands of animated
// letters collapses into a handful of drawGlyphsRSXform calls. Reuse one
// instance across frames — clear() keeps the allocations.
struct GlyphRSXformBatches {
  struct Batch {
    const ShapedWord *font = nullptr;
    SkColor color = 0;
    std::vector<SkGlyphID> glyphs;
    std::vector<SkRSXform> transforms;
  };
  std::vector<Batch> batches;

  /** Returns the batch for a shaped font source and color. */
  [[nodiscard]] Batch &batchForStyle(const ShapedWord *font, SkColor color) {
    for (Batch &batch : batches)
      if (batch.font == font && batch.color == color)
        return batch;
    batches.push_back({font, color, {}, {}});
    return batches.back();
  }

  // Convenience for the common "advance-centre anchored at `at`, rotated by
  // (`cosine`, `sine`) placement the effects use.
  void addGlyph(const ShapedWord *font, SkColor color, SkGlyphID glyph,
                float halfAdvance, SkPoint centerPosition, float cosine = 1,
                float sine = 0) {
    Batch &batch = batchForStyle(font, color);
    batch.glyphs.push_back(glyph);
    batch.transforms.push_back({cosine, sine,
                                centerPosition.x() - cosine * halfAdvance,
                                centerPosition.y() - sine * halfAdvance});
  }

  /** Clears glyph data while retaining batch allocations for the next frame. */
  void clear() {
    for (Batch &batch : batches) {
      batch.glyphs.clear();
      batch.transforms.clear();
    }
  }

  // Returns the number of glyphs drawn.
  int draw(SkCanvas *canvas) const {
    SkPaint paint;
    paint.setAntiAlias(true);
    int total = 0;
    for (const Batch &batch : batches) {
      if (batch.glyphs.empty())
        continue;
      total += static_cast<int>(batch.glyphs.size());
      paint.setColor(batch.color);
      SkFont font = makeFont(batch.font->typeface, batch.font->fontSize);
      // Tumbling letters move whole pixels every frame; subpixel phases
      // would only multiply each (glyph, angle) into fresh atlas strikes —
      // per-frame mask rasterization is exactly what caps these effects.
      font.setSubpixel(false);
      canvas->drawGlyphsRSXform(
          SkSpan<const SkGlyphID>(batch.glyphs.data(), batch.glyphs.size()),
          SkSpan<const SkRSXform>(batch.transforms.data(),
                                  batch.transforms.size()),
          {0, 0}, font, paint);
    }
    return total;
  }
};

} // namespace textflow
