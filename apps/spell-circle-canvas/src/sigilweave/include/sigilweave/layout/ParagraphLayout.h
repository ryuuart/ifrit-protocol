#pragma once

/** @file
 * @ingroup layout
 *
 * The layout stage and its result — the engine's main entry point. Call
 * \code
 *   layoutParagraph(fontContext, paragraph, flow, options)
 * \endcode
 * to break a Paragraph (paragraph/Paragraph.h) into a chosen Flow geometry
 * (Flow.h) and get back a ParagraphLayout: positioned runs plus overflow /
 * ellipsis / placeholder reporting. draw() or drawBatched() paints it to an
 * SkCanvas (paint resolved per span at draw time), or walk `runs` yourself.
 * The options live in LayoutOptions.h and the run and band types in
 * PositionedRun.h; this header holds the result and the two entry points.
 *
 * The draw members are declared here and defined by the paint feature, so
 * a program that calls them links SigilWeavePaint; everything else in this
 * header is SigilWeaveLayout.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkPoint.h>

#include <cstdint>
#include <span>
#include <vector>

#include "sigilweave/layout/Flow.h"
#include "sigilweave/layout/LayoutOptions.h"
#include "sigilweave/layout/PositionedRun.h"
#include "sigilweave/paragraph/Paragraph.h"

namespace sigil::weave {

class FontContext;

/** Positioned output of one paragraph layout pass. */
struct ParagraphLayout {
  std::vector<PositionedRun> runs;  ///< in logical word order, ready to draw
  /// Every flow interval the layout consumed, in the order the geometry
  /// handed them over — the numbering PositionedRun::intervalIndex uses.
  /// A caller that re-places transformed runs reads their geometry here
  /// rather than rebuilding it and hoping the two agree.
  std::vector<LineInterval> intervals;
  /// The tangent snapping the placement used, carried so a re-placement can
  /// match it (see LineInterval::placeAt).
  int tangentRotationSteps = 0;
  /// The pitch every line was queried at — the resolved line height, which
  /// is a vertical flow's COLUMN WIDTH. Carried because the flow's band is
  /// not recoverable from an interval: a LineInterval states where the pen
  /// travels, never how wide the band around it is.
  float linePitch = 0;
  int lineCount = 0;  ///< lines actually produced
  /// First word that found no room (geometry exhausted); ~0u when all fit.
  uint32_t firstUnplacedWord = ~0u;
  /// An overflow marker from ParagraphLayoutOptions::overflow was appended
  /// to the final placed line. Its run is the last in `runs`.
  bool ellipsized = false;

  /** Returns whether geometry ended before all paragraph words were placed. */
  [[nodiscard]] bool overflowed() const noexcept {
    return firstUnplacedWord != ~0u;
  }

  /** Draws every run, resolving its ordered paint layers from the paragraph's
   * current spans — so paint-only tweaks show up without any relayout.
   * `overridePaint` replaces every span's paint (labels drawn in a
   * caller-chosen color without touching the paragraph).
   */
  void draw(SkCanvas* canvas, const Paragraph& paragraph,
            const PaintStyle* overridePaint = nullptr) const;

  /** Draw-time font-variation override, valid only for ADVANCE-INVARIANT
   *  axes. Every shaped bucket's typeface is swapped for its varied clone
   *  (memoized by `fonts`) while the glyph positions computed at shaping
   *  time are reused as they are. That is correct exactly when driving the
   *  axis leaves every glyph advance alone — GRAD behaves that way on faces
   *  that have it, whereas wght moves advances on most faces and would leave
   *  the glyphs sitting at the wrong pen positions. Ask
   *  FontContext::axisIsAdvanceInvariant before animating an axis here; an
   *  axis that fails that test belongs in ShapingStyle::variations, which
   *  re-shapes. Transformed and path runs draw from their baked blobs and
   *  ignore this override entirely. */
  struct LiveVariations {
    FontContext* fonts = nullptr;
    std::span<const FontVariation> variations;
  };

  /** Draws the same output with minimal draw calls: horizontal runs are
   * merged into one SkCanvas::drawGlyphs per (font, PaintStyle) bucket and
   * configured paint layer instead of one drawTextBlob per word and layer.
   * A default style is one call per bucket; each underlay/overlay adds one.
   * Transformed runs fall back to their baked blobs.
   */
  void drawBatched(SkCanvas* canvas, const Paragraph& paragraph,
                   const PaintStyle* overridePaint = nullptr,
                   const LiveVariations* liveVariations = nullptr) const;

  /// Where every inline placeholder landed, ready to draw pills/images into.
  struct PlacedPlaceholder {
    int index = 0;                      ///< into Paragraph::placeholders()
    SkRect rect = SkRect::MakeEmpty();  ///< where to draw the inline object
    int lineIndex = 0;                  ///< 0-based line it landed on
  };
  /** Returns rectangles for inline objects in the paragraph. */
  [[nodiscard]] std::vector<PlacedPlaceholder> placeholderRects(
      const Paragraph& paragraph) const;

  /** Returns per-line geometry derived from the placed runs, ascending by
   * line index — the building block for selection bands, line backgrounds,
   * and point-to-line hit-testing that per-span decorations don't cover.
   *
   * Derived, not stored: nothing is recorded during layout and calling this
   * costs one pass over `runs` (metrics resolved per font change). Mixed
   * fonts on a line report the tallest ascent/deepest descent, matching how
   * a line box grows. Straight horizontal lines only: transformed (path /
   * rotated) and vertical runs are skipped, and lines whose geometry placed
   * nothing do not appear.
   */
  [[nodiscard]] std::vector<LineMetrics> lineMetrics(
      const Paragraph& paragraph) const;

  /** Returns per-COLUMN geometry for a vertical layout, ascending by column
   * index — what lineMetrics() is for a horizontal one, and the only one of
   * the two that answers in a vertical paragraph.
   *
   * Derived, not stored: one pass over `runs`, with each run's extent down
   * the column taken from the pen it was placed at. Every vertical form
   * counts — upright, rotated and tate-chu-yoko alike — because all three
   * consume column pitch. Columns that placed nothing do not appear, and a
   * horizontal layout returns an empty list.
   */
  [[nodiscard]] std::vector<ColumnMetrics> columnMetrics(
      const Paragraph& paragraph) const;
};

/** Lays `paragraph` out into `geometry`. Ensures the paragraph is shaped
 * (cache-hot when little changed), breaks it into lines with the configured
 * breaker, and returns positioned runs backed by shared word blobs.
 */
ParagraphLayout layoutParagraph(FontContext& fontContext, Paragraph& paragraph,
                                FlowGeometry& geometry,
                                const ParagraphLayoutOptions& options = {});

/**
 * Lays a paragraph out as one unconstrained horizontal line whose baseline
 * begins at `baselineOrigin`.
 *
 * This is the ergonomic path for labels and captions: callers do not need to
 * construct a one-entry LineSetFlow or precompute the paragraph width.
 */
ParagraphLayout layoutSingleLine(FontContext& fontContext, Paragraph& paragraph,
                                 SkPoint baselineOrigin,
                                 const PathTextOptions& pathText = {});

}  // namespace sigil::weave
