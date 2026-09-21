#pragma once

/** @file
 * @ingroup compose-core
 *
 * How a passage is SET, as verbs: the blocks it is styled in, the room
 * around its lines, where its first baseline sits, what becomes of the
 * room left over, and what happens when it overflows.
 */

#include <sigilcompose/core/Declarations.h>
#include <sigilcompose/core/SurfacePaint.h>
#include <sigilcompose/core/Utf8.h>
#include <sigilweave/layout/ParagraphLayout.h>
#include <sigilweave/style/Style.h>

#include <span>
#include <string_view>
#include <vector>

namespace sigil::compose {

/** THE TEXT PROPERTIES. Each is a property of the SETTING rather than
 *  of the words, which is why a style sheet may state one and why a
 *  node that holds no text quietly has nothing to apply it to. */
template <class Derived>
class TextStyleVerbs {
 public:
  /** How each BLOCK of this passage is set — one entry per block, in
   *  block order, a block being the text between two hard breaks. A
   *  block past the end of the list is set by the leaf's own fields
   *  alone. */
  Derived& paragraphs(std::vector<sigil::weave::ParagraphStyle> blocks);
  /** The same, by NAME: one class per block, resolved through the block
   *  half of the sheet in force where the leaf LANDS and laid over the
   *  block in force there. The two spellings are alternatives, and the
   *  last one written stands. */
  Derived& paragraphs(std::span<const std::string_view> names);
  /** THIS PASSAGE'S OPENING SET LARGE — a versal sized so its cap
   *  height spans the lines it is given, seated on the baseline it
   *  sinks to, with the lines under it wrapping the notch it cuts. It
   *  applies to the FIRST block. */
  Derived& initialLetter(sigil::weave::InitialLetter initial);
  /** WHERE THE FIRST BASELINE SITS below the top of this leaf's box —
   *  the first line's own ascent (the default), its cap height, its
   *  x-height, its whole pitch, or @p offset outright. Every later
   *  baseline follows at its own block's pitch. */
  Derived& firstBaseline(sigil::weave::FrameOptions::FirstBaseline rule,
                         float offset = 0);
  /** What becomes of the room left over down this leaf's box — nothing
   *  (the default), half above and half below, all above, or spread
   *  BETWEEN the lines as extra leading, at most
   *  @p maximumInterlineSpacing per gap. */
  Derived& distribute(sigil::weave::FrameOptions::Distribute rule,
                      float maximumInterlineSpacing = 0);
  /** ROOM BESIDE EVERY LINE of this passage, over and above the
   *  leading — `before` above a line and right of a column, `after`
   *  below one and left. It is a layout input: the room is in the strut
   *  before anything is broken. */
  Derived& reserve(sigil::weave::ReservedBand band);
  /** AN INPUT OF THIS PASSAGE IS MOVING — a measure that animates, a
   *  frame that grows, content that changes frame to frame — so this
   *  layout is one of a run of them. Nothing infers it. @p candidates
   *  is the floor under a frame the optimizing breaker cannot finish;
   *  0 is no floor. */
  Derived& live(bool on = true, int candidates = 0);
  /** The marker appended to the last line when the text overflows its
   *  geometry. Empty disables it. */
  Derived& ellipsis(Utf8 marker);
  /** Use at most this many lines — CSS line-clamp. The rest reports as
   *  overflow, and `ellipsis()`, when set, lands on the clamped line.
   *  0 is unclamped. */
  Derived& maxTextLines(int lines);
  /** Paint the GLYPHS with this material, mapped to TEXT-METRIC space:
   *  the material's unit square lands with x across the widest line and
   *  y from the first line's cap top to the last line's baseline. An
   *  EMPTY paint clears the override. */
  Derived& textFill(SurfacePaint paint);
  /** Stroke the GLYPHS, under the fill — engraved display type, an
   *  outlined label, a caption that has to survive over an image. Not
   *  `stroke()`, which dresses the node's box. */
  Derived& textStroke(float width, SurfacePaint paint);
  /** Flow this paragraph around the keyed node, with @p margin px of
   *  standoff. A target that declares a silhouette is subtracted by
   *  that outline; one that declares none is subtracted by its box.
   *  Call repeatedly to weave around several. */
  Derived& contentFlowAround(std::string_view key, float margin = 0.0f);

 private:
  Derived& self() { return static_cast<Derived&>(*this); }
  detail::ElementNode* declarations() {
    return detail::NodeAccess::declarations(self());
  }
};

}  // namespace sigil::compose
