#pragma once

/** @file
 * @ingroup layout
 *
 * WHERE THE LINES BREAK and what decides it: the alignment, the choice of
 * breaker, the metrics that override the font's, where a word may be
 * hyphenated, and the tolerances the optimizing breaker weighs.
 */

#include <cstdint>
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

/** Where a word may be broken, and which of those breaks a line may take.
 *
 * The two halves are decided at different stages and that is the whole of
 * the split. Where a break MAY fall is segmentation: a soft hyphen already
 * in the text, plus whatever `patterns` finds inside a word under
 * `limits`, and all of that is a fact about the text that the whole layout
 * shares. Which of those opportunities a line actually TAKES is a break
 * decision — the three fields under `limits` — so a block may state its
 * own and the breaker reads the block's.
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
  /// author typed. Null leaves discretionary hyphens the only opportunity,
  /// which is what a text that says nothing gets. The kit ships Liang
  /// pattern sets (kit/Hyphenation.h); a caller's own implementation is a
  /// peer of them. Compared by identity, because two hyphenators that are
  /// not the same object cannot be shown to answer the same way.
  const Hyphenator* patterns = nullptr;

  /// Which of a word's break points become opportunities at all — a fact
  /// about the word, so it is settled during segmentation and the whole
  /// layout shares it (paragraph/Hyphenation.h).
  HyphenationLimits limits;

  /// Most lines in a row that may end in a hyphen; 0 lifts the limit.
  int consecutiveLimit = 0;
  /// The band at the ragged edge inside which a line is already square
  /// enough, px; 0 lifts it. A line whose last WHOLE word ends inside the
  /// band is left ragged, because a word broken to reach further is a
  /// hyphen the page did not need — so the question is asked of the line
  /// WITHOUT the break, and both breakers ask it the same way. A word that
  /// is the whole line is still broken: there is nothing else on the line
  /// for the zone to measure. Ragged setting only — a justified line
  /// shows its slack in the gaps rather than at the edge.
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
  /// The longest the optimizing breaker may spend on ONE BLOCK before it
  /// gives up and lets the greedy breaker fill that block instead, in
  /// microseconds; 0 lifts the limit, which is what a layout that says
  /// nothing gets.
  ///
  /// It is a DEGRADE AND NOT A POLICY. The composer is meant to run on
  /// moving text — that is what it is for — and this is the floor under a
  /// frame that meets a block it cannot compose in time: one frame set
  /// greedily, counted in ParagraphLayout::degradedBlocks, rather than a
  /// frame that arrives late. A layout that reports degrades every frame
  /// is asking for a longer budget or a shorter block.
  float budgetMicroseconds = 0.0f;
  bool operator==(const KnuthPlassOptions&) const = default;
};

}  // namespace sigil::weave
