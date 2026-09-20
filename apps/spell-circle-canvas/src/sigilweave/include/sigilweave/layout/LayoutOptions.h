#pragma once

/** @file
 * @ingroup weave-layout
 *
 * WHAT A CALLER TELLS THE LAYOUT STAGE, in one value: the settings
 * grouped by the stage that reads them, and the blocks that override
 * them one paragraph at a time. Every field is defaulted and every
 * nested group is inert unless its stage runs; settings that belong to
 * the geometry stay on the geometry.
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

/** Groups the settings of paragraph layout by the stage that reads them.
 * Every member is defaulted and the common path sets only `alignment`;
 * each nested group is inert unless its stage runs. The top-level
 * alignment, justification, hyphenation and tab stops are the WHOLE
 * LAYOUT'S answer, and `blocks` overrides them block by block.
 */
struct ParagraphLayoutOptions {
  /// AN INPUT OF THIS LAYOUT IS MOVING — a bound measure, an animating
  /// frame, a text whose content changes frame to frame — so this layout
  /// is one of a run of them: break decisions for a block in a UNIFORM
  /// measure are kept and reused, keyed on the words and the whole-pixel
  /// measure, and the block is broken against the measure alone rather
  /// than the frame's supply of lines. A settled layout sets nothing here.
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

  /// THE MEASURE THE NEXT FRAME OF THE CHAIN SETS IN, which only whoever
  /// holds the chain knows. 0 says nothing is known and the widow count
  /// is taken at the measure this frame's last line was set in — exact
  /// for a chain of equal frames, off by the difference for one that
  /// changes width. Every other keep is settled from lines this frame
  /// placed and never reads it.
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
  /// table gives no class of its own; 0 leaves the face's own setting.
  float tsume = 0;

  /// One entry per BLOCK — the text between two mandatory breaks — in
  /// block order. A block past the end of this list, and every block when
  /// it is empty, is set in `blockDefault`.
  std::vector<ParagraphStyle> blocks;
  /// What a block the list does not reach is set in: the fields above
  /// alone by default, or whatever the passage inherits when a host has
  /// resolved that for it.
  ParagraphStyle blockDefault;

  bool operator==(const ParagraphLayoutOptions&) const = default;
};

}  // namespace sigil::weave
