#pragma once

/** @file
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
#include <include/core/SkTextBlob.h>

#include <cstdint>
#include <vector>

namespace textflow {

class FontContext;

/** Specifies how text is aligned inside each available line interval. */
enum class TextAlignment : uint8_t { kStart, kCenter, kEnd, kJustify };

/** Selects the fast greedy breaker or optimal Knuth-Plass line breaking. */
enum class LineBreakStrategy : uint8_t { kGreedy, kKnuthPlass };

/** Overrides the paragraph's font-derived line metrics when non-zero. */
struct LineMetricsOptions {
  float height = 0;
  float ascent = 0;
};

/** Controls soft-hyphen handling independently from the break strategy. */
struct HyphenationOptions {
  bool enabled = true;
  /// Added as squared demerits by Knuth-Plass to discourage repeated
  /// discretionary hyphen breaks.
  float penalty = 50.0f;
};

/** Controls spacing when TextAlignment::kJustify is selected. */
struct JustificationOptions {
  /// Paragraph-final and hard-break-final lines use this alignment unless
  /// `justifyLastLine` requests full justification.
  TextAlignment lastLineAlignment = TextAlignment::kStart;
  bool justifyLastLine = false;

  /// CJK text has no spaces, so eligible zero-width ideographic gaps may be
  /// expanded up to `maxIdeographicExpansion * fontSize` per gap.
  bool expandIdeographicGaps = true;
  float maxIdeographicExpansion = 0.5f;

  /// Space elasticity, expressed as fractions of the natural space width.
  float spaceStretch = 0.5f;
  float spaceShrink = 0.333f;
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
  TextAlignment alignment = TextAlignment::kStart;
  LineBreakStrategy lineBreakStrategy = LineBreakStrategy::kGreedy;
  LineMetricsOptions lineMetrics;
  HyphenationOptions hyphenation;
  JustificationOptions justification;
  KnuthPlassOptions knuthPlass;
  OverflowOptions overflow;
  PathTextOptions pathText;
};

/// One draw call: a shared word blob translated to `origin`, or a fully
/// positioned RSXform blob (contour/rotated intervals) drawn at (0,0).
/// Placeholder runs carry no blob at all — just the flow position where the
/// caller should draw its inline object (see
/// ParagraphLayout::placeholderRects).
struct PositionedRun {
  sk_sp<SkTextBlob> blob;
  ShapedWordRef shaped; ///< glyph source (batched drawing, choreography)
  SkPoint origin = {0, 0};
  uint32_t styleIndex = 0; ///< paint lookup into Paragraph::spans()
  uint32_t wordIndex = 0;  ///< which Word produced this run
  int lineIndex = 0;
  bool transformed = false;  ///< RSXform blob (positions baked into the blob)
  int placeholderIndex = -1; ///< \>= 0: index into Paragraph::placeholders()
};

/** Positioned output of one paragraph layout pass. */
struct ParagraphLayout {
  std::vector<PositionedRun> runs;
  int lineCount = 0;
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
    SkRect rect = SkRect::MakeEmpty();
    int lineIndex = 0;
  };
  /** Returns rectangles for inline objects in the paragraph. */
  [[nodiscard]] std::vector<PlacedPlaceholder>
  placeholderRects(const Paragraph &paragraph) const;
};

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

} // namespace textflow
