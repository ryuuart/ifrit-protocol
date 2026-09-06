#pragma once

/** @file
 * @ingroup layout
 *
 * WHAT A CALLER TELLS THE LAYOUT STAGE, in one value: the settings grouped
 * by the stage that reads them, and the blocks that override them one
 * paragraph at a time. Every group is a header of its own beside this one
 * — breaking, justification, overflow, tab stops, the frame, mojikumi and
 * the paragraph style — and this file is what a caller hands to
 * `layoutParagraph`.
 *
 * Every field is defaulted and every nested group is inert unless its
 * stage runs. Settings that belong to the geometry stay on the geometry
 * (`ExclusionFlow::setMinimumIntervalWidth`, for instance).
 */

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "sigilweave/layout/Breaking.h"
#include "sigilweave/layout/Frame.h"
#include "sigilweave/layout/InitialLetter.h"
#include "sigilweave/layout/Justification.h"
#include "sigilweave/layout/Mojikumi.h"
#include "sigilweave/layout/Overflow.h"
#include "sigilweave/layout/ParagraphStyle.h"
#include "sigilweave/layout/TabStops.h"

namespace sigil::weave {

/**
 * Groups the settings of paragraph layout by the stage that reads them.
 *
 * Every member is defaulted, and the common path sets only `alignment`.
 * Each nested group is inert unless its stage runs: `justification`
 * applies under kJustify, `knuthPlass` under kKnuthPlass, `tabStops` only
 * when a word carries a tab, `pathText` only when runs are transformed.
 *
 * The top-level `alignment`, `justification`, `hyphenation` and `tabStops`
 * are the WHOLE LAYOUT'S answer, and a block that states none of its own
 * is set by them. `blocks` overrides them block by block.
 */
struct ParagraphLayoutOptions {
  /// AN INPUT OF THIS LAYOUT IS MOVING — a bound measure, an animating
  /// frame, a text whose content changes frame to frame — so this layout
  /// is one of a run of them rather than an answer someone asked for once.
  ///
  /// It changes two things and nothing else. The break decisions of a
  /// block set in a UNIFORM measure are kept and reused, keyed on the words
  /// and on the measure taken to the whole pixel below it, so a measure
  /// already seen costs no break decision at all and a measure between two
  /// seen ones is set in the narrower of them. And the block is broken
  /// against the measure alone rather than against the frame's supply of
  /// lines, so a frame that only grows or shrinks in DEPTH changes which
  /// lines it holds and never where they break.
  ///
  /// A settled layout sets nothing here and is answered exactly as it has
  /// always been answered.
  bool live = false;
  TextAlignment alignment = TextAlignment::kStart;  ///< per-interval placement
  /// Greedy is the fast default; Knuth-Plass trades speed for even spacing.
  LineBreakStrategy lineBreakStrategy = LineBreakStrategy::kGreedy;
  LineMetricsOptions lineMetrics;  ///< non-zero fields override font metrics
  HyphenationOptions hyphenation;  ///< where words may break, and which take
  JustificationOptions justification;  ///< only used under kJustify
  KnuthPlassOptions knuthPlass;        ///< only used under kKnuthPlass
  OverflowOptions overflow;            ///< ellipsis marker and line clamping
  TabStopOptions tabStops;   ///< empty/zero → tabs measure as shaped spaces
  PathTextOptions pathText;  ///< draw-time only, never affects breaking
  FrameOptions frame;        ///< first baseline and vertical distribution
  /// Room beside every line for what is set alongside the type; a block
  /// may reserve more.
  ReservedBand reserved;

  /// THE MEASURE THE NEXT FRAME OF THE CHAIN SETS IN, for the one keep
  /// that has to count lines this frame will not hold. The widow rule asks
  /// how many lines the remainder takes, and the remainder is set in the
  /// NEXT frame's measure, which this fill has no other way to learn: only
  /// whoever holds the chain knows what comes after. 0 says nothing is
  /// known and the count is taken at the measure this frame's last line
  /// was set in, which is exact for a chain of equal frames and off by the
  /// difference for one that changes width. Every other keep is settled
  /// from lines this frame placed and never reads it.
  float nextMeasure = 0;

  /// Which characters may not stand at a line's edge (kinsoku shori). A
  /// prohibition is settled during SEGMENTATION — the boundary is simply
  /// not opened — so no breaker knows the rule and both of them obey it.
  KinsokuTable kinsoku;
  /// How far a character may hang past the measure (optical margin
  /// alignment; burasagari down a column). Empty leaves every line squared
  /// on its advances, which is what a text that says nothing gets.
  HangingTable hanging;
  /// How much room stands between two adjacent full-width characters, by
  /// the class of each. Empty leaves every gap the width the shaper gave
  /// it, which is what a text that says nothing gets — and costs nothing,
  /// since a layout with no table asks no question about any gap.
  MojikumiTable mojikumi;
  /// How much of its own advance a full-width character gives up so it
  /// sets closer to its neighbours — tsume, as a fraction of the em,
  /// removed from the gap after every full-width character the mojikumi
  /// table gives no class of its own. 0 leaves the face's own setting.
  /// It is applied where mojikumi is applied and stops where that stops:
  /// at the gaps between words.
  float tsume = 0;

  /// One entry per BLOCK — the text between two mandatory breaks — in
  /// block order. A block past the end of this list, and every block when
  /// it is empty, is set by the fields above alone.
  std::vector<ParagraphStyle> blocks;

  bool operator==(const ParagraphLayoutOptions&) const = default;
};

}  // namespace sigil::weave
