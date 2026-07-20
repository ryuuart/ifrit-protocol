#pragma once

/** @file
 * @ingroup layout
 *
 * The layout stage and its result — the engine's main entry point. Call
 * \code
 *   layoutParagraph(fontContext, paragraph, flow, options)
 * \endcode
 * to break a Paragraph (Paragraph.h) into a chosen Flow geometry (Flow.h)
 * and get back a ParagraphLayout: positioned runs plus overflow / ellipsis /
 * placeholder reporting. draw() or drawBatched() paints it to an SkCanvas
 * (paint resolved per span at draw time), or walk `runs` yourself.
 * ParagraphLayoutOptions gathers every layout-time knob — alignment, line
 * metrics, break strategy, hyphenation, justification, overflow, path text.
 */

#include "Flow.h"
#include "Paragraph.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkFontMetrics.h>
#include <include/core/SkTextBlob.h>

#include <cstdint>
#include <utility>
#include <vector>

namespace sigil::weave {

class FontContext;

/** Specifies how text is aligned inside each available line interval. */
enum class TextAlignment : uint8_t { kStart, kCenter, kEnd, kJustify };

/** Selects the fast greedy breaker or optimal Knuth-Plass line breaking. */
enum class LineBreakStrategy : uint8_t { kGreedy, kKnuthPlass };

/** Overrides the paragraph's font-derived line metrics when non-zero. */
struct LineMetricsOptions {
  float height = 0; ///< line height, px; 0 keeps the font-derived value
  float ascent = 0; ///< baseline offset below the line top, px; 0 keeps
                    ///< the font-derived value
};

/** Controls soft-hyphen handling independently from the break strategy. */
struct HyphenationOptions {
  bool enabled = true; ///< false ignores soft-hyphen break opportunities
  /// Added as squared demerits by Knuth-Plass to discourage repeated
  /// discretionary hyphen breaks.
  float penalty = 50.0f;
};

/** Controls spacing when TextAlignment::kJustify is selected. */
struct JustificationOptions {
  /// Paragraph-final and hard-break-final lines use this alignment unless
  /// `justifyLastLine` requests full justification.
  TextAlignment lastLineAlignment = TextAlignment::kStart;
  bool justifyLastLine = false; ///< stretch final lines to full measure too

  /// CJK text has no spaces, so eligible zero-width ideographic gaps may be
  /// expanded up to `maxIdeographicExpansion * fontSize` per gap.
  bool expandIdeographicGaps = true;
  float maxIdeographicExpansion = 0.5f; ///< per-gap cap, fraction of fontSize

  /// Space elasticity, expressed as fractions of the natural space width.
  float spaceStretch = 0.5f;
  float spaceShrink = 0.333f; ///< maximum shrink per space, as a fraction
};

/** Advanced tuning used only by LineBreakStrategy::kKnuthPlass. */
struct KnuthPlassOptions {
  /// Maximum TeX-style badness before the breaker uses its forced-fit path.
  float tolerance = 4000.0f;
  /// Intervals narrower than this are ignored so the algorithm never has to
  /// force a word into exclusion-shape slivers.
  float minimumIntervalWidth = 0.0f;
};

/** Controls how overflowing text is represented. */
struct OverflowOptions {
  /// Empty disables the marker. Only straight horizontal flows can render
  /// an ellipsis; curved and vertical flows still report overflow normally.
  std::u16string ellipsis;
  /// > 0: the layout uses at most this many of the geometry's lines
  /// (CSS line-clamp); remaining text reports as overflow and `ellipsis`
  /// (when set) lands on the clamped line. Works with every breaker and
  /// geometry — the limit wraps the FlowGeometry, so exclusion flows and
  /// Knuth-Plass need no special handling.
  int maxLines = 0;
};

/** Tab-character handling for straight horizontal flows.
 *
 * A word whose trailing whitespace contains a tab advances the pen to the
 * next stop instead of its measured glue: first through `positions`
 * (ascending, px from each line interval's start), then repeating every
 * `interval` px past the last explicit stop. With no stop ahead (or no
 * configuration at all — the default) tabs keep their shaped
 * space-equivalent width.
 *
 * Both breakers resolve stops identically: greedy fits against tab-resolved
 * widths as it goes, and Knuth-Plass scores every candidate line at its
 * tab-resolved width. Stops are line-local — alignment other than kStart
 * shifts the resolved line as a whole. Tab gaps are rigid under
 * justification, and gaps at or before a line's last tab never stretch or
 * shrink (the following stop would swallow the adjustment and unpin the
 * column); only the gaps past the last tab absorb slack.
 * Scope: straight horizontal intervals, LTR lines.
 */
struct TabStopOptions {
  std::vector<float> positions; ///< explicit stops, ascending px from the
                                ///< interval start
  float interval = 0;           ///< repeat spacing past the last explicit
                                ///< stop; 0 disables repetition
};

/** Rendering-only controls that do not affect line breaking. */
struct PathTextOptions {
  /// Animated path tangents snap to this many directions to avoid creating
  /// a fresh glyph-atlas strike for every tiny rotation change. Zero
  /// preserves exact rotations for static artwork.
  int tangentRotationSteps = 512;
};

/**
 * Groups the independent stages of paragraph layout.
 *
 * The common path normally sets only `alignment`. More specialized callers
 * opt into `lineBreakStrategy`, `hyphenation`, `justification`, `overflow`, or
 * `pathText` without scanning a flat collection of unrelated tuning knobs.
 */
struct ParagraphLayoutOptions {
  TextAlignment alignment = TextAlignment::kStart; ///< per-interval placement
  /// Greedy is the fast default; Knuth-Plass trades speed for even spacing.
  LineBreakStrategy lineBreakStrategy = LineBreakStrategy::kGreedy;
  LineMetricsOptions lineMetrics; ///< non-zero fields override font metrics
  HyphenationOptions hyphenation; ///< soft-hyphen breaks and their penalty
  JustificationOptions justification; ///< only used under kJustify
  KnuthPlassOptions knuthPlass;       ///< only used under kKnuthPlass
  OverflowOptions overflow;           ///< ellipsis marker and line clamping
  TabStopOptions tabStops; ///< empty/zero → tabs measure as shaped spaces
  PathTextOptions pathText; ///< draw-time only, never affects breaking
};

/// One draw call: a shared word blob translated to `origin`, or a fully
/// positioned RSXform blob (contour/rotated intervals) drawn at (0,0).
/// Placeholder runs carry no blob at all — just the flow position where the
/// caller should draw its inline object (see
/// ParagraphLayout::placeholderRects).
struct PositionedRun {
  sk_sp<SkTextBlob> blob; ///< null for placeholder runs
  ShapedWordRef shaped; ///< glyph source (batched drawing, choreography)
  SkPoint origin = {0, 0}; ///< draw position; already baked into
                           ///< transformed blobs
  uint32_t styleIndex = 0; ///< paint lookup into Paragraph::spans()
  uint32_t wordIndex = 0;  ///< which Word produced this run
  int lineIndex = 0;       ///< 0-based flow line the run landed on
  bool transformed = false;  ///< RSXform blob (positions baked into the blob)
  int placeholderIndex = -1; ///< \>= 0: index into Paragraph::placeholders()
};

/// Geometry of one laid-out line, derived on demand from its placed runs
/// (ParagraphLayout::lineMetrics). The extent is the advance extent of what
/// actually landed — selection bands, line backgrounds, and line hit-testing
/// live here — not the flow interval's full measure (query the FlowGeometry
/// itself for raw interval geometry).
struct LineMetrics {
  int lineIndex = 0;  ///< matches PositionedRun::lineIndex
  float baseline = 0; ///< baseline y shared by the line's runs
  float ascent = 0;   ///< tallest ascent above the baseline (positive)
  float descent = 0;  ///< deepest descent below the baseline (positive)
  float left = 0;     ///< leftmost run origin
  float right = 0;    ///< rightmost run end (advance extent)
  uint32_t textBegin = 0; ///< first UTF-16 unit placed on the line
  uint32_t textEnd = 0;   ///< one past the last unit, trailing glue included

  /** Returns the line's bounding band (ascent above to descent below). */
  [[nodiscard]] SkRect rect() const {
    return SkRect::MakeLTRB(left, baseline - ascent, right, baseline + descent);
  }
};

/** Positioned output of one paragraph layout pass. */
struct ParagraphLayout {
  std::vector<PositionedRun> runs; ///< in logical word order, ready to draw
  int lineCount = 0;               ///< lines actually produced
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
  void draw(SkCanvas *canvas, const Paragraph &paragraph,
            const PaintStyle *overridePaint = nullptr) const;

  /** Draws the same output with minimal draw calls: horizontal runs are
   * merged into one SkCanvas::drawGlyphs per (font, PaintStyle) bucket and
   * configured paint layer instead of one drawTextBlob per word and layer.
   * A default style is one call per bucket; each underlay/overlay adds one.
   * Transformed runs fall back to their baked blobs.
   */
  void drawBatched(SkCanvas *canvas, const Paragraph &paragraph,
                   const PaintStyle *overridePaint = nullptr) const;

  /// Where every inline placeholder landed, ready to draw pills/images into.
  struct PlacedPlaceholder {
    int index = 0; ///< into Paragraph::placeholders()
    SkRect rect = SkRect::MakeEmpty(); ///< where to draw the inline object
    int lineIndex = 0;                 ///< 0-based line it landed on
  };
  /** Returns rectangles for inline objects in the paragraph. */
  [[nodiscard]] std::vector<PlacedPlaceholder>
  placeholderRects(const Paragraph &paragraph) const;

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
  [[nodiscard]] std::vector<LineMetrics>
  lineMetrics(const Paragraph &paragraph) const;
};

namespace detail {

/// A Decoration resolved against one run's font metrics: concrete band
/// geometry (top edge relative to the baseline, px, y-down) and color.
struct ResolvedDecorationBand {
  float position = 0; ///< band top, relative to the baseline
  float thickness = 1;           ///< band height, px; floored at 1
  SkColor color = SK_ColorBLACK; ///< resolved draw color, never transparent
};

/** Resolves a decoration's thickness, position, and color against font
 * metrics (deterministic geometry, exposed for tests): explicit values win;
 * zeros fall back to the face's underline/strikeout metrics, a mid-x-height
 * strikethrough, or the ascent line for overlines, with a 1px thickness
 * floor throughout. */
[[nodiscard]] ResolvedDecorationBand
resolveDecorationBand(const Decoration &decoration,
                      const SkFontMetrics &metrics, SkColor foregroundColor);

/** Resolves the paint a decoration band draws with — the fill concern,
 * separate from the band geometry above: the decoration's `paint` override
 * verbatim when present, otherwise an anti-aliased fill of the band's
 * resolved color. */
[[nodiscard]] SkPaint decorationBandPaint(const Decoration &decoration,
                                          const ResolvedDecorationBand &band);

/** Returns the absolute x spans the decoration actually draws for `run` —
 * one span covering the run's advance, minus glyph-ink intercepts (grown by
 * one thickness of standoff) when the decoration skips ink. Empty for
 * transformed, vertical, and placeholder runs. */
[[nodiscard]] std::vector<std::pair<float, float>>
decorationSegments(const PositionedRun &run, const Decoration &decoration,
                   const ResolvedDecorationBand &band);

} // namespace detail

/** Lays `paragraph` out into `geometry`. Ensures the paragraph is shaped
 * (cache-hot when little changed), breaks it into lines with the configured
 * breaker, and returns positioned runs backed by shared word blobs.
 */
ParagraphLayout layoutParagraph(FontContext &fontContext, Paragraph &paragraph,
                                FlowGeometry &geometry,
                                const ParagraphLayoutOptions &options = {});

/**
 * Lays a paragraph out as one unconstrained horizontal line whose baseline
 * begins at `baselineOrigin`.
 *
 * This is the ergonomic path for labels and captions: callers do not need to
 * construct a one-entry LineSetFlow or precompute the paragraph width.
 */
ParagraphLayout layoutSingleLine(FontContext &fontContext, Paragraph &paragraph,
                                 SkPoint baselineOrigin,
                                 const PathTextOptions &pathText = {});

} // namespace sigil::weave
