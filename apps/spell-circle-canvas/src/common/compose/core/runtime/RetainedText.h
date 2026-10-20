#pragma once

/** @file
 * Internal to the kernel — what a mounted text leaf retains of its text
 * between frames: the paragraph, the layout it came to and the size it
 * measured, where a frame chain left it, the rects its slots, anchors and
 * readings landed on, and the engine state dressed type keeps.
 */

#include <include/core/SkRect.h>
#include <sigilweave/layout/ParagraphLayout.h>
#include <sigilweave/paragraph/Paragraph.h>
#include <yoga/Yoga.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "TextState.h"
#include "description/ComposeInternal.h"

namespace sigil::compose::detail {

/** THE TEXT A NODE RETAINS: one part of the retained node, so every field
 *  is read off the instance by its own name. Empty on a node that is not
 *  text. */
// fields are grouped by what they belong to, not by size
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
struct RetainedText {
  std::optional<sigil::weave::Paragraph> paragraph;
  sigil::weave::ParagraphLayout textLayout;
  std::vector<sigil::weave::LineMetrics> lines;
  /// The same geometry for a VERTICAL passage, where a "line" is a column
  /// and lineMetrics() answers with nothing. Exactly one of the two lists is
  /// ever non-empty, and which one is the node's writing mode.
  std::vector<sigil::weave::ColumnMetrics> columns;
  float measuredForWidth = -1.0f;
  float measuredForHeight = -1.0f;
  YGSize measuredSize{0, 0};
  float measuredBaseline = 0.0f;  // first character's baseline, from the top
  // textThreadTo(): the word this frame's fill begins at — 0 for the head of a
  // chain, and whatever the frame before it left unplaced for every other.
  uint32_t threadCursor = 0;
  // …and the LINE this frame's first line is, counted from the story's
  // start. A story numbers its own lines: weave::selectors::line(40) is the
  // fortieth line of the story wherever it landed, so a chain that reflows
  // moves the selection with the text instead of addressing a different line in
  // every frame. 0 for the head, and for every text that is not a frame.
  uint32_t threadLineOffset = 0;
  // …and the MEASURE THE NEXT FRAME SETS IN, which the widow rule needs
  // because the lines it counts are the ones this frame will not hold.
  // Only the chain knows it; 0 says it is not known yet, which is the
  // first pass over a chain nobody has laid out.
  float threadNextMeasure = 0;
  // …and the lines the WHOLE chain placed, written to every frame once the
  // walk knows it. A cascade numbered over the story needs the story's own
  // count, and no single fill has it.
  uint32_t threadStoryLines = 0;
  // Whether some frame threads INTO this one, which is what makes the last
  // link of a chain a frame rather than an ordinary text leaf. A leaf's
  // height is an answer and it grows down the page; a FRAME is bounded by
  // its own depth. The links before the last are known by their own
  // textThreadTo(), and the last is known only by this.
  bool threadedInto = false;
  uint32_t contentRev = 0;     // bumped on text/exclusion change
  uint32_t measuredRev = ~0u;  // rev the cached measurement belongs to
  // weave::rich().slot(): the slot names in the order the content declares them
  // — which is the order weave matches its placeholder records in — and where
  // the finished layout put each one, in this node's own space. A child
  // keyed by one of these names takes that rect as its box.
  std::vector<std::string> textSlotKeys;
  std::vector<std::pair<std::string, SkRect>> textSlotRects;
  // textAttach(): the rect each anchored child's key resolves to, in this
  // node's own space, resolved once per layout from the placement the paragraph
  // produced. A key that resolved no glyphs is absent, and its child places
  // nothing.
  std::vector<std::pair<std::string, SkRect>> textMarkRects;
  // textAnnotation(): every reading, laid out where the base's units put it, in
  // this node's own space. Each is a small paragraph of its own with its own
  // placement, so the kernel draws it exactly as it draws the base.
  struct PlacedAnnotation {
    std::shared_ptr<sigil::weave::Paragraph> paragraph;
    sigil::weave::ParagraphLayout layout;
  };
  std::vector<PlacedAnnotation> textAnnotations;
  // WHERE THE GLYPHS THEMSELVES REACH, in this node's own space: the union
  // of the placed blobs' bounds, base and readings alike. A line's band is
  // its ascent over its descent, and the outlines a face draws are not
  // held to it — a comma's tail, a swash, an accent on a capital all stand
  // outside the box the layout measured. Anything that sizes a surface
  // from a text leaf reads this beside the box, because a surface cut to
  // the box alone loses the pixel that hangs past it, whole. Empty until a
  // layout has run, and rewritten by every layout after.
  SkRect textInk = SkRect::MakeEmpty();
  // weave::rich().add(text, styleName): each named run and the text it
  // occupies, in declaration order — what selectors::style resolves against.
  // Cleared and rebuilt with the paragraph, so the names a node answers for are
  // exactly the ones its current content declares.
  std::vector<detail::NamedRun> textNamedRuns;
  // The engine's state (TextState) — null until dressed type first asks
  // for it. Read through textStateOf().
  std::unique_ptr<TextState> textState;
};

}  // namespace sigil::compose::detail
