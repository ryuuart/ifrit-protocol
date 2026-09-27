#pragma once

/** @file
 * @ingroup weave-layout
 *
 * The layout stage and its result — the engine's main entry point.
 * layoutParagraph() breaks a Paragraph into a chosen flow geometry and
 * returns a ParagraphLayout: positioned runs plus overflow, ellipsis and
 * placeholder reporting. The draw members are declared here and defined
 * by the paint feature, so a program that calls them links
 * SigilWeavePaint; everything else here is SigilWeaveLayout.
 */

#include <include/core/SkPath.h>
#include <include/core/SkPoint.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "sigilweave/layout/Flow.h"
#include "sigilweave/layout/InitialLetter.h"
#include "sigilweave/layout/LayoutOptions.h"
#include "sigilweave/layout/PositionedRun.h"
#include "sigilweave/paragraph/Paragraph.h"

class SkCanvas;

namespace sigil::weave {

class FontContext;
namespace detail {
struct LayoutAccess;
}

/** HOW THE OPTIMIZING BREAKER SCORED A LINE IT CHOSE, kept as it was
 *  weighed when the break was decided rather than worked out again from
 *  the placed runs. `natural` is what the line's words and gaps measure
 *  before any fit — the content, each interior gap at the word spacing
 *  the justification aims at, a tab resolved to its stop, the room a
 *  mojikumi table puts after a word, and the hyphen of a break at a soft
 *  hyphen. The adjustment ratio is the slack against the measure over
 *  what the gaps may open, or, overfull, over what they may close; the
 *  badness is TeX's, 100·|ratio|³ with the ratio held under 500, capped
 *  at 1e7.
 *  @trap A block's last line that fits scores a ratio of zero, its slack
 *  going to the end of the paragraph, even when the justification opens
 *  its gaps to the measure; a balanced block's lines are scored against
 *  the narrowed measure the balancing settled on; a block the breaker had
 *  to rerun with emergency stretch counts each line's own measure as
 *  stretch, so a line that needed it scores looser than it looks.
 *  @silent the gaps could not move the way the slack asked — underfull
 *  with nothing to stretch, overfull with nothing it may shrink: the
 *  ratio is absent and the badness at its cap. */
struct LineScore {
  int lineIndex = 0;       ///< matches PositionedRun::lineIndex
  int intervalIndex = -1;  ///< into ParagraphLayout::intervals
  float natural = 0;
  std::optional<float> adjustmentRatio;
  float badness = 0;
  bool operator==(const LineScore&) const = default;
};

/** Positioned output of one paragraph layout pass.
 *
 * IT BORROWS THE PARAGRAPH'S GLYPHS. Every PositionedRun points at a
 * ShapedWord the paragraph holds rather than holding one itself, and every
 * word and interval index a run carries reads a table on one of the two.
 * A layout is therefore only meaningful while the paragraph it was set from
 * is alive and unedited, which is what every consumer of a run already
 * assumes — and every member below that reads the text takes that
 * paragraph back as an argument.
 */
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
  /// One entry per line the optimizing breaker decided, in the order they
  /// were placed, each naming the interval it was set in. A line the
  /// greedy breaker set — under LineBreakStrategy::kGreedy, or in a block
  /// the optimizing one ran out of candidates on — has none, and neither
  /// does a line a keep took back out of the frame.
  std::vector<LineScore> lineScores;
  /// First word that found no room (geometry exhausted); ~0u when all fit.
  uint32_t firstUnplacedWord = ~0u;
  /// An overflow marker from ParagraphLayoutOptions::overflow was appended
  /// to the final placed line. Its run is the last in `runs`.
  bool ellipsized = false;
  /// How many blocks the optimizing breaker ran out of candidates on and
  /// left to the greedy breaker (KnuthPlassOptions::candidates). Zero
  /// whenever no floor was set, and the number a caller watches to know
  /// its floor is too low for the text it is setting.
  int degradedBlocks = 0;
  /// How many blocks were set from break decisions this thread had already
  /// made for the same words at the same measure, under
  /// ParagraphLayoutOptions::live. It is what says a moving text is costing
  /// only its fill: a frame that reports as many reused blocks as it holds
  /// made no break decision at all.
  int reusedBlocks = 0;
  /// Where the initial letter landed, when a block declared one
  /// (ParagraphStyle::initial). Its glyphs are ordinary runs of this
  /// layout and draw with the rest; this is the report, not a second thing
  /// to draw.
  PlacedInitial initial;

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
   *  axes: each bucket's typeface is swapped for its varied clone while
   *  the positions computed at shaping time are reused as they are. Ask
   *  `FontContext::axisIsAdvanceInvariant` first; an axis that fails that
   *  test belongs in `ShapingStyle::variations`, which re-shapes.
   *  @silent the run is transformed or on a path, both drawing from their
   *  baked blobs. */
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

  /** A STYLE PER GLYPH: which of @p styles each glyph draws with, by its
   *  place in the walk `forEachPlacedGlyph` takes — every glyph of every
   *  shaped run, in draw order. A glyph past the end of `styleOfGlyph`, or
   *  naming no entry of `styles`, draws with its span's paint. Both spans
   *  are borrowed for the draw. */
  struct GlyphStyles {
    std::span<const uint32_t> styleOfGlyph;
    std::span<const PaintStyle> styles;
  };

  /** The batched draw with a style per glyph standing in for the spans'
   *  paint and @p overridePaint on the glyphs — a gradient restarted on
   *  every word is a style per word. The decoration bands still read
   *  @p overridePaint or the spans. The passes draw BAND BY BAND, every
   *  underlay beneath every foreground, so one glyph's shadow never lands
   *  on its neighbour. One draw per bucket and pass, and a style is a
   *  bucket of its own.
   *  @trap A run the layout TURNED draws from its baked blob in one style,
   *  the one its first glyph names. */
  void drawBatched(SkCanvas* canvas, const Paragraph& paragraph,
                   const GlyphStyles& glyphStyles,
                   const PaintStyle* overridePaint = nullptr) const;

  /// Where every inline placeholder landed, ready to draw pills/images into.
  struct PlacedPlaceholder {
    int index = 0;                      ///< into Paragraph::placeholders()
    geometry::path::Rect rect;  ///< where to draw the inline object
    int lineIndex = 0;                  ///< 0-based line it landed on
  };
  /** Returns physical rectangles for inline objects in the paragraph.
   *  A placeholder's logical width advances along the reading direction;
   *  its logical height spans the line or column. Vertical objects are
   *  centred on the column axis and ignore the horizontal baseline drop. */
  [[nodiscard]] std::vector<PlacedPlaceholder> placeholderRects(
      const Paragraph& paragraph) const;

  /** Returns per-line geometry derived from the placed runs, ascending by
   * line index — the building block for selection bands, line backgrounds
   * and point-to-line hit-testing. Derived, not stored, with mixed fonts
   * reporting the tallest ascent and deepest descent.
   * @silent the line is vertical or transformed, or its geometry placed
   * nothing: those lines do not appear at all. */
  [[nodiscard]] std::vector<LineMetrics> lineMetrics(
      const Paragraph& paragraph) const;

  /** Returns the OUTLINE OF EVERY GLYPH this layout placed, as one path in
   * the layout's own coordinate space: not the ink bounds and not the
   * advance boxes but the actual contours, the per-glyph transforms of a
   * rotated or curved run included. Derived, not stored.
   * @trap Glyphs a face reports no path for — bitmap and colour glyphs —
   * are absent, having no contour to give. */
  [[nodiscard]] SkPath glyphOutline() const;

  /** Returns per-COLUMN geometry for a vertical layout, ascending by
   * column index — what `lineMetrics` is for a horizontal one, and the
   * only one of the two that answers in a vertical paragraph. Every
   * vertical form counts, all three consuming column pitch.
   * @silent the layout is horizontal, which answers an empty list, as do
   * columns that placed nothing. */
  [[nodiscard]] std::vector<ColumnMetrics> columnMetrics(
      const Paragraph& paragraph) const;

  /** The shaped words this layout made and OWNS, rather than borrowed
   * from the paragraph: the overflow marker, a tab leader, the glyphs an
   * initial letter was cut from. Nothing else is holding them.
   * @trap The span dies with the layout: a caller handing one on takes a
   * copy of the HANDLE from here, which keeps the glyphs alive on its own.
   */
  [[nodiscard]] std::span<const ShapedWordReference> ownedWords() const {
    return m_shapedWords;
  }

 private:
  friend struct detail::LayoutAccess;
  // Owns auxiliary glyphs for leaders, overflow markers and initial letters.
  std::vector<ShapedWordReference> m_shapedWords;
  // Owns the paints a run of the layout's own points at: an initial
  // letter's, when its style states one.
  std::vector<std::shared_ptr<const PaintStyle>> m_paints;
};

/** Lays @p paragraph out into @p geometry from @p firstWord: shapes what
 * is needed, breaks lines with the configured breaker, and returns
 * positioned runs backed by shared word blobs. @p firstWord IS THE RESUME
 * POINT, the last pass's `ParagraphLayout::firstUnplacedWord`; blocks are
 * numbered from the START of the text wherever a pass begins.
 * @trap OVERFLOW IS NORMAL: no marker unless `OverflowOptions::ellipsis`. */
ParagraphLayout layoutParagraph(FontContext& fontContext, Paragraph& paragraph,
                                FlowGeometry& geometry,
                                const ParagraphLayoutOptions& options = {},
                                uint32_t firstWord = 0);

/** Lays a paragraph out as one unconstrained horizontal line whose
 * baseline begins at @p baselineOrigin — the ergonomic path for labels
 * and captions, needing neither a one-entry LineSetFlow nor a precomputed
 * paragraph width.
 */
ParagraphLayout layoutSingleLine(FontContext& fontContext, Paragraph& paragraph,
                                 glm::vec2 baselineOrigin,
                                 const PathTextOptions& pathText = {});

}  // namespace sigil::weave
