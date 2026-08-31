#pragma once

/** @file
 * @ingroup animation
 *
 * One glyph of a finished layout, as an effect sees it: where it rests,
 * what it draws with, and where it sits in the text — which glyph of which
 * word, line, style span and sentence — beside the tangent and pen
 * coordinate a glyph on a curve was placed with. forEachPlacedGlyph walks a
 * layout in draw order and hands every glyph over as one of these; nothing
 * here is stored during layout.
 */

#include <include/core/SkPoint.h>

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <span>
#include <vector>

#include "sigilweave/fonts/Shaper.h"
#include "sigilweave/layout/ParagraphLayout.h"
#include "sigilweave/paragraph/Paragraph.h"
#include "sigilweave/style/Style.h"

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

  /// The layout TURNED this glyph — it rides a contour, or a rotated
  /// interval — rather than sitting on a horizontal baseline. `rest` is
  /// still the absolute origin, and `tangent` is the direction the glyph
  /// was turned to; an untransformed glyph reports (1, 0).
  bool transformed = false;
  SkVector tangent = {1, 0};  ///< unit direction, already snapped

  /// WHERE ALONG ITS FLOW INTERVAL the glyph's ADVANCE CENTRE sits, in
  /// advance units, together with which interval that is (an index into
  /// ParagraphLayout::intervals). Feed the pair back to
  /// LineInterval::placeAt to re-place the glyph — with a phase, to run a
  /// marquee around a closed contour without laying the paragraph out
  /// again. `intervalIndex` is -1 when the layout reported no geometry.
  float pen = 0;
  int intervalIndex = -1;
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
    if (segmentCursor < word.segments().size() &&
        word.segments()[segmentCursor].shaped.get() == run.shaped.get())
      segment = &word.segments()[segmentCursor++];

    placed.shaped = run.shaped.get();
    placed.paint = run.styleIndex < spans.size()
                       ? &spans[run.styleIndex].style.paint
                       : &kUnstyled;
    placed.color = placed.paint->foreground.getColor();
    placed.wordIndex = run.wordIndex;
    placed.lineIndex = run.lineIndex;
    placed.styleIndex = run.styleIndex;
    placed.transformed = run.transformed;
    placed.intervalIndex = run.intervalIndex;
    // The interval this run was placed on, when the layout kept one. A
    // transformed run's rest position is READ BACK FROM IT rather than from
    // run.origin, which such a run leaves at zero because its placement is
    // baked per glyph.
    const LineInterval* interval =
        run.intervalIndex >= 0 &&
                (size_t)run.intervalIndex < layout.intervals.size()
            ? &layout.intervals[(size_t)run.intervalIndex]
            : nullptr;
    float penLocal = 0;

    const uint32_t textBegin = segment ? segment->textBegin : word.textBegin;
    const uint32_t textLimit =
        word.textEnd > textBegin ? word.textEnd - 1 : textBegin;
    for (size_t glyphIndex = 0; glyphIndex < placed.shaped->glyphs.size();
         ++glyphIndex) {
      placed.glyph = placed.shaped->glyphs[glyphIndex];
      placed.advance = placed.shaped->advances[glyphIndex];
      placed.pen = run.penOffset + penLocal + placed.advance * 0.5f;
      if (run.transformed && interval) {
        SkPoint centre;
        interval->placeAt(placed.pen, 0.0f, layout.tangentRotationSteps,
                          &centre, &placed.tangent);
        // From the advance CENTRE back to the glyph's origin, through the
        // same back-out of HarfBuzz's offsets the blob was baked with —
        // otherwise an accented glyph's rest drifts off the curve.
        const float offsetX =
            placed.shaped->positions[glyphIndex].x() - penLocal;
        const float offsetY = placed.shaped->positions[glyphIndex].y();
        const float centreX = placed.advance * 0.5f - offsetX;
        const float centreY = -offsetY;
        placed.rest = {centre.x() - (placed.tangent.x() * centreX -
                                     placed.tangent.y() * centreY),
                       centre.y() - (placed.tangent.y() * centreX +
                                     placed.tangent.x() * centreY)};
      } else {
        placed.tangent = {1, 0};
        placed.rest = run.origin + placed.shaped->positions[glyphIndex];
      }
      penLocal += placed.advance;
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

}  // namespace sigil::weave
