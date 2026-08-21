#pragma once

/** @file
 * @ingroup animation
 *
 * Per-glyph choreography utilities — the "letters leave their lines"
 * pattern (rain, ripples, marquees, staggered reveals) distilled from the
 * demos and the gallery. Optional layer: nothing in the core pipeline
 * includes this.
 *
 * The recipe: lay the paragraph out normally, walk every placed glyph with
 * forEachPlacedGlyph, displace/rotate/fade it however the effect wants, and
 * accumulate into GlyphRSXformBatches — one drawGlyphsRSXform call per
 * (font, paint pass) instead of thousands of per-glyph draws.
 *
 * A PlacedGlyph carries the identity an effect selects and staggers on —
 * which glyph of which word, line, style span and sentence it is — beside
 * the geometry it draws with, and the span's complete PaintStyle, so an
 * animated letter keeps the gradients, strokes and glow passes its span was
 * styled with.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRSXform.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <numbers>
#include <span>
#include <utility>
#include <vector>

#include "Paragraph.h"
#include "ParagraphLayout.h"
#include "Shaper.h"
#include "Style.h"

namespace sigil::weave {

/// One glyph of a finished layout: where it rests, what it draws with, and
/// where it sits in the text. Every index is a fact about *this* layout of
/// *this* paragraph; nothing here is stored during layout.
struct PlacedGlyph {
  const ShapedWord* shaped = nullptr;  ///< glyph source: typeface, size, scaleX
  SkGlyphID glyph = 0;                 ///< glyph ID in `shaped->typeface`
  float advance = 0;                   ///< this glyph's pen travel
  SkPoint rest = {0, 0};               ///< absolute origin the layout placed
                                       ///< it at (the effect's "rest" pose)
  /// The style span's draw-time paint — foreground plus its ordered
  /// underlay/overlay passes. Never null: it points into the paragraph's
  /// spans, so it is re-read every walk and setPaint() shows up with no
  /// relayout. Feed it straight to GlyphRSXformBatches::addGlyph.
  const PaintStyle* paint = nullptr;
  SkColor color = SK_ColorBLACK;  ///< paint->foreground's color, for effects
                                  ///< that only tint

  uint32_t ordinal = 0;     ///< 0-based position in this walk's order
  uint32_t glyphIndex = 0;  ///< index into `shaped->glyphs`
  /// UTF-16 offset of the glyph's cluster inside the text its run shaped —
  /// glyphs of one cluster (a base and its combining marks, a ligature's
  /// parts) share one value.
  uint32_t cluster = 0;
  /// The same cluster as an offset into Paragraph::text(), clamped to the
  /// word that produced it. Length-changing case mapping
  /// (ShapingStyle::textTransform) makes it approximate.
  uint32_t textIndex = 0;
  uint32_t wordIndex = 0;      ///< index into Paragraph::words()
  int lineIndex = 0;           ///< flow line; matches PositionedRun::lineIndex
  uint32_t styleIndex = 0;     ///< index into Paragraph::spans()
  uint32_t sentenceIndex = 0;  ///< 0-based sentence, per Paragraph::
                               ///< sentenceStarts(); 0 for empty text
};

/** Callable accepted by forEachPlacedGlyph(): `fn(const PlacedGlyph &)`. */
template <typename Visitor>
concept PlacedGlyphVisitor = std::invocable<Visitor&, const PlacedGlyph&>;

/** Visits every placed glyph of `layout` in draw order, each with its rest
 * position, its span's paint, and its position in the text.
 *
 * Enumeration order is stable across relayouts as long as the text itself is
 * unchanged — which is what lets per-glyph particle state keyed by `ordinal`
 * survive a per-frame relayout.
 *
 * The first walk after a text edit resolves sentence boundaries once (see
 * Paragraph::sentenceStarts()); every later walk of unchanged text reuses
 * them.
 */
template <PlacedGlyphVisitor Visitor>
inline void forEachPlacedGlyph(const ParagraphLayout& layout,
                               const Paragraph& paragraph, Visitor&& visitor) {
  static thread_local std::vector<uint32_t> segmentCounters;
  segmentCounters.assign(paragraph.words().size(), 0);
  const std::span<const uint32_t> sentenceStarts = paragraph.sentenceStarts();
  const std::vector<StyleSpan>& spans = paragraph.spans();
  static const PaintStyle kUnstyled;

  PlacedGlyph placed;
  for (const PositionedRun& run : layout.runs) {
    if (!run.shaped) continue;  // placeholder slots carry no glyphs
    const Word& word = paragraph.words()[run.wordIndex];

    // Runs of a word arrive in segment order, so one cursor per word finds
    // the segment a run came from. A run whose glyphs are not a segment's —
    // a rendered discretionary hyphen, an overflow ellipsis — matches
    // nothing, leaves the cursor alone, and anchors its text offsets at the
    // word it follows.
    uint32_t& segmentCursor = segmentCounters[run.wordIndex];
    const WordSegment* segment = nullptr;
    if (segmentCursor < word.segments.size() &&
        word.segments[segmentCursor].shaped.get() == run.shaped.get())
      segment = &word.segments[segmentCursor++];

    placed.shaped = run.shaped.get();
    placed.paint = run.styleIndex < spans.size()
                       ? &spans[run.styleIndex].style.paint
                       : &kUnstyled;
    placed.color = placed.paint->foreground.getColor();
    placed.wordIndex = run.wordIndex;
    placed.lineIndex = run.lineIndex;
    placed.styleIndex = run.styleIndex;

    const uint32_t textBegin = segment ? segment->textBegin : word.textBegin;
    const uint32_t textLimit =
        word.textEnd > textBegin ? word.textEnd - 1 : textBegin;
    for (size_t glyphIndex = 0; glyphIndex < placed.shaped->glyphs.size();
         ++glyphIndex) {
      placed.glyph = placed.shaped->glyphs[glyphIndex];
      placed.advance = placed.shaped->advances[glyphIndex];
      placed.rest = run.origin + placed.shaped->positions[glyphIndex];
      placed.glyphIndex = static_cast<uint32_t>(glyphIndex);
      placed.cluster = glyphIndex < placed.shaped->clusters.size()
                           ? placed.shaped->clusters[glyphIndex]
                           : 0;
      placed.textIndex = std::min(textBegin + placed.cluster, textLimit);
      placed.sentenceIndex =
          sentenceStarts.empty()
              ? 0
              : static_cast<uint32_t>(std::upper_bound(sentenceStarts.begin(),
                                                       sentenceStarts.end(),
                                                       placed.textIndex) -
                                      sentenceStarts.begin() - 1);
      visitor(placed);
      ++placed.ordinal;
    }
  }
}

/** Quantizes `angle` to `cosine`/`sine` on a 64-step table (≈5.6° per step,
 * visually indistinguishable for tumbling letters): continuous per-letter
 * angles would re-rasterize every glyph mask every frame on the CPU raster
 * backend. The GPU backend doesn't need this, but it doesn't hurt there
 * either.
 */
inline void quantizeAngle(float angle, float& cosine, float& sine) {
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
  if (stepIndex < 0) stepIndex += kSteps;
  cosine = angleTable[static_cast<size_t>(stepIndex)].first;
  sine = angleTable[static_cast<size_t>(stepIndex)].second;
}

/// Glyphs grouped by (font, paint pass): a frame of thousands of animated
/// letters collapses into a handful of drawGlyphsRSXform calls. Reuse one
/// instance across frames — clear() keeps the allocations.
///
/// A glyph is added with a whole PaintStyle and lands in one batch per pass
/// it draws: each underlay in order, then the foreground, then each overlay.
/// Because a batch's key is a complete SkPaint, a pass keeps its gradient,
/// stroke, blend mode and mask filter — animating letters and styling them
/// are not alternatives. Batches draw in creation order, so within a style
/// every underlay lands beneath every foreground.
struct GlyphRSXformBatches {
  /// One (font, paint pass) bucket: parallel glyph/transform arrays that feed
  /// a single drawGlyphsRSXform call. The font is held as the identity
  /// makeFont() needs rather than as the shaped word it came from, so words
  /// set in the same face and size share one bucket — and so a bucket kept
  /// across frames never outlives a shaped word the cache has since evicted.
  struct Batch {
    sk_sp<SkTypeface> typeface;         ///< bucket key: the face to draw with
    float fontSize = 0;                 ///< bucket key: px size
    float scaleX = 1.0f;                ///< bucket key: horizontal
                                        ///< condensation baked into shaping
    bool aliased = false;               ///< bucket key: hard-edged raster
    SkPaint paint;                      ///< bucket key: the complete pass
    SkVector offset = {0, 0};           ///< bucket key: the pass's own
                                        ///< translation (shadows, echoes)
    std::vector<SkGlyphID> glyphs;      ///< parallel to `transforms`
    std::vector<SkRSXform> transforms;  ///< per-glyph scale/rotate/translate
  };
  std::vector<Batch> batches;  ///< one entry per distinct (font, pass) pair
  /// Where the last pass landed. Neighbouring glyphs repeat a pass, and a
  /// full SkPaint is dearer to compare than a color, so the scan starts
  /// where it last succeeded. Bounds-checked: callers own `batches`.
  size_t recentBatch = 0;

  /** Returns the batch for one shaped word's font and one resolved pass. */
  [[nodiscard]] Batch& batchForPass(const ShapedWord* font,
                                    const SkPaint& paint, SkVector offset) {
    auto matches = [&](const Batch& batch) {
      return batch.typeface.get() == font->typeface.get() &&
             batch.fontSize == font->fontSize && batch.scaleX == font->scaleX &&
             batch.aliased == font->aliased && batch.offset == offset &&
             batch.paint == paint;
    };
    if (recentBatch < batches.size() && matches(batches[recentBatch]))
      return batches[recentBatch];
    for (size_t index = 0; index < batches.size(); ++index)
      if (matches(batches[index])) {
        recentBatch = index;
        return batches[index];
      }
    recentBatch = batches.size();
    batches.push_back({font->typeface,
                       font->fontSize,
                       font->scaleX,
                       font->aliased,
                       paint,
                       offset,
                       {},
                       {}});
    return batches.back();
  }

  /** Appends one glyph — once per pass of `style` — anchored at its
   * advance-centre `centerPosition` and rotated by (`cosine`, `sine`), the
   * placement convention the effects use.
   *
   * `alphaScale` multiplies every pass's alpha, which is how a per-glyph
   * fade stays batched: the style itself is untouched and only the resolved
   * paint differs. Quantize it if the effect drives it continuously —
   * distinct alphas are distinct buckets. Passes with nothing to draw are
   * skipped, so a fully faded glyph costs no bucket at all.
   */
  void addGlyph(const ShapedWord* font, const PaintStyle& style,
                SkGlyphID glyph, float halfAdvance, SkPoint centerPosition,
                float cosine = 1, float sine = 0, float alphaScale = 1.0f) {
    const SkRSXform transform = {cosine, sine,
                                 centerPosition.x() - cosine * halfAdvance,
                                 centerPosition.y() - sine * halfAdvance};
    auto addPass = [&](const SkPaint& source, SkVector offset) {
      if (alphaScale >= 1.0f) {
        if (source.nothingToDraw()) return;
        Batch& batch = batchForPass(font, source, offset);
        batch.glyphs.push_back(glyph);
        batch.transforms.push_back(transform);
        return;
      }
      SkPaint faded = source;
      faded.setAlphaf(source.getAlphaf() * alphaScale);
      if (faded.nothingToDraw()) return;
      Batch& batch = batchForPass(font, faded, offset);
      batch.glyphs.push_back(glyph);
      batch.transforms.push_back(transform);
    };
    for (const PaintLayer& layer : style.underlays)
      addPass(layer.paint, layer.offset);
    addPass(style.foreground, {0, 0});
    for (const PaintLayer& layer : style.overlays)
      addPass(layer.paint, layer.offset);
  }

  /** Appends a visited glyph at `centerPosition`, taking its font, advance
   * and span paint from the walk. The ergonomic pairing with
   * forEachPlacedGlyph.
   */
  void addGlyph(const PlacedGlyph& placed, SkPoint centerPosition,
                float cosine = 1, float sine = 0, float alphaScale = 1.0f) {
    addGlyph(placed.shaped, *placed.paint, placed.glyph, placed.advance * 0.5f,
             centerPosition, cosine, sine, alphaScale);
  }

  /** Clears glyph data while retaining batch allocations for the next frame.
   *
   * A frame that minted a pathological number of buckets releases them
   * instead, because a retained bucket also retains its paint — and with it
   * every shader, filter and blender that paint holds a reference to.
   */
  void clear() {
    constexpr size_t kRetainedBucketCap = 256;
    recentBatch = 0;
    if (batches.size() > kRetainedBucketCap) {
      batches.clear();
      return;
    }
    for (Batch& batch : batches) {
      batch.glyphs.clear();
      batch.transforms.clear();
    }
  }

  /** Draws every batch and returns the number of glyph draws it issued —
   * one per glyph per pass. */
  int draw(SkCanvas* canvas) const {
    int total = 0;
    for (const Batch& batch : batches) {
      if (batch.glyphs.empty()) continue;
      total += static_cast<int>(batch.glyphs.size());
      SkFont font =
          makeFont(batch.typeface, batch.fontSize, batch.scaleX, batch.aliased);
      // Tumbling letters move whole pixels every frame; subpixel phases
      // would only multiply each (glyph, angle) into fresh atlas strikes —
      // per-frame mask rasterization is exactly what caps these effects.
      font.setSubpixel(false);
      canvas->drawGlyphsRSXform(
          SkSpan<const SkGlyphID>(batch.glyphs.data(), batch.glyphs.size()),
          SkSpan<const SkRSXform>(batch.transforms.data(),
                                  batch.transforms.size()),
          {batch.offset.x(), batch.offset.y()}, font, batch.paint);
    }
    return total;
  }
};

}  // namespace sigil::weave
