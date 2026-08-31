#pragma once

/** @file
 * @ingroup layout
 *
 * What a caller tells the layout stage, grouped by the stage that reads it:
 * alignment and the choice of breaker, the line metrics that override the
 * font's, soft-hyphen handling, justification elasticity, the Knuth-Plass
 * tolerances, the overflow ellipsis and line clamp, tab stops, and the
 * tangent snapping text on a path draws with. Every field is defaulted and
 * every nested group is inert unless its stage runs. Settings that belong
 * to the geometry stay on the geometry (ExclusionFlow::setMinIntervalWidth,
 * for instance).
 */

#include <cstdint>
#include <string>
#include <vector>

namespace sigil::weave {

/** Specifies how text is aligned inside each available line interval. */
enum class TextAlignment : uint8_t { kStart, kCenter, kEnd, kJustify };

/** Selects the fast greedy breaker or optimal Knuth-Plass line breaking. */
enum class LineBreakStrategy : uint8_t { kGreedy, kKnuthPlass };

/** Overrides the paragraph's font-derived line metrics when non-zero. */
struct LineMetricsOptions {
  float height = 0;  ///< line height, px; 0 keeps the font-derived value
  float ascent = 0;  ///< baseline offset below the line top, px; 0 keeps
                     ///< the font-derived value
};

/** Controls soft-hyphen handling independently from the break strategy. */
struct HyphenationOptions {
  /// False removes the break opportunity, not just the hyphen glyph: the
  /// halves either side of a soft hyphen fuse into one unbreakable word
  /// during segmentation, so the word wraps or overflows whole. Reaching
  /// the paragraph is what makes that happen — see
  /// Paragraph::setSoftHyphenBreaks, which layoutParagraph sets from here.
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
  bool justifyLastLine = false;  ///< stretch final lines to full measure too

  /// CJK text has no spaces, so eligible zero-width ideographic gaps may be
  /// expanded up to `maxIdeographicExpansion * fontSize` per gap.
  bool expandIdeographicGaps = true;
  float maxIdeographicExpansion = 0.5f;  ///< per-gap cap, fraction of fontSize

  /// Space elasticity, expressed as fractions of the natural space width.
  float spaceStretch = 0.5f;
  float spaceShrink = 0.333f;  ///< maximum shrink per space, as a fraction
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
  std::vector<float> positions;  ///< explicit stops, ascending px from the
                                 ///< interval start
  float interval = 0;            ///< repeat spacing past the last explicit
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
 * Groups the settings of paragraph layout by the stage that reads them.
 *
 * Every member is defaulted, and the common path sets only `alignment`. Each
 * nested group is inert unless its stage runs: `justification` applies under
 * kJustify, `knuthPlass` under kKnuthPlass, `tabStops` only when a word
 * carries a tab, `pathText` only when runs are transformed.
 */
struct ParagraphLayoutOptions {
  TextAlignment alignment = TextAlignment::kStart;  ///< per-interval placement
  /// Greedy is the fast default; Knuth-Plass trades speed for even spacing.
  LineBreakStrategy lineBreakStrategy = LineBreakStrategy::kGreedy;
  LineMetricsOptions lineMetrics;  ///< non-zero fields override font metrics
  HyphenationOptions hyphenation;  ///< soft-hyphen breaks and their penalty
  JustificationOptions justification;  ///< only used under kJustify
  KnuthPlassOptions knuthPlass;        ///< only used under kKnuthPlass
  OverflowOptions overflow;            ///< ellipsis marker and line clamping
  TabStopOptions tabStops;   ///< empty/zero → tabs measure as shaped spaces
  PathTextOptions pathText;  ///< draw-time only, never affects breaking
};

}  // namespace sigil::weave
