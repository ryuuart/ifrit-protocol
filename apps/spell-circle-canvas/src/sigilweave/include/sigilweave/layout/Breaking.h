#pragma once

/** @file
 * @ingroup weave-layout
 *
 * WHERE THE LINES BREAK and what decides it: the alignment, the choice of
 * breaker, the metrics that override the font's, where a word may be
 * hyphenated, and the tolerances the optimizing breaker weighs.
 */

#include <cstdint>
#include <memory>
#include <optional>

#include "sigilweave/paragraph/Hyphenation.h"

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
  bool operator==(const LineMetricsOptions&) const = default;
};

/** Where a word may be broken, and which of those breaks a line may
 * take. Where a break MAY fall is segmentation, and so a fact about the
 * text the whole layout shares; which of those opportunities a line
 * actually TAKES is a break decision, and so a block may state its own.
 */
struct HyphenationOptions {
  /// False removes the break opportunity, not just the hyphen glyph: the
  /// halves either side of a soft hyphen fuse into one unbreakable word
  /// during segmentation, so the word wraps or overflows whole, and
  /// `patterns` is not consulted at all. Reaching the paragraph is what
  /// makes that happen — see Paragraph::setSoftHyphenBreaks, which
  /// layoutParagraph sets from here.
  bool enabled = true;
  /// Added as squared demerits by Knuth-Plass to discourage repeated
  /// discretionary hyphen breaks.
  float penalty = 50.0f;

  /// Where inside a word a break may fall, beyond the soft hyphens the
  /// author typed; empty leaves those the only opportunity. The kit ships
  /// Liang pattern sets, and a caller's own implementation is a peer of
  /// them. HELD, NOT BORROWED: the options and the paragraph keep it.
  /// @trap Compared by IDENTITY: two hyphenators that are not the same
  /// object cannot be shown to answer the same way.
  std::shared_ptr<const Hyphenator> patterns;

  /// Which of a word's break points become opportunities at all — a fact
  /// about the word, so it is settled during segmentation and the whole
  /// layout shares it (paragraph/Hyphenation.h).
  HyphenationLimits limits;

  /// Most lines in a row that may end in a hyphen; 0 lifts the limit.
  int consecutiveLimit = 0;
  /// The band at the ragged edge inside which a line is already square
  /// enough, px; 0 lifts it. The question is asked of the line WITHOUT
  /// the break, and a word that is the whole line is still broken.
  /// @silent the block is justified, a justified line showing its slack
  /// in the gaps rather than at the edge.
  float zone = 0;
  /// Whether the last word of a block may be broken.
  bool lastWordOfBlock = true;

  bool operator==(const HyphenationOptions&) const = default;
};

/** Advanced tuning used only by LineBreakStrategy::kKnuthPlass. */
struct KnuthPlassOptions {
  /// Maximum TeX-style badness before the breaker uses its forced-fit path.
  float tolerance = 4000.0f;
  /// Intervals narrower than this are ignored so the algorithm never has to
  /// force a word into exclusion-shape slivers.
  float minimumIntervalWidth = 0.0f;
  /// How many BREAK CANDIDATES the optimizing breaker may weigh for ONE
  /// BLOCK before it gives up and lets the greedy breaker fill that
  /// block; 0 lifts the floor. One candidate is one candidate LINE. It is
  /// COUNTED AND NOT TIMED, so the same block at the same measure meets
  /// or misses it every time, and it is a DEGRADE rather than a policy,
  /// counted in `ParagraphLayout::degradedBlocks`.
  int candidates = 0;
  bool operator==(const KnuthPlassOptions&) const = default;
};

}  // namespace sigil::weave
