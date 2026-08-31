#pragma once

/** @file
 * SigilCompose KIT — instruments for looking at a cascade while you author
 * it: `trackMeter`, the schedule drawn, and `restGhost`, the same text at
 * rest under the moving copy. A cascade is an invisible remap and a
 * deviation has nothing on screen to be measured against, so both draw
 * what is otherwise only inferable.
 */

#include <include/core/SkColor.h>
#include <include/core/SkRect.h>
#include <sigilcompose/core/Composer.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/Text.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::compose::kit {

// The two below are instruments, on the same terms as everything else in
// this header: for a sketch under the eye of whoever is tuning it, for a
// test that has to see the schedule, and NOT for the paint loop of
// something that ships. Both are describe-time — they read a resolved
// layout and hand back ordinary elements — so neither costs anything on a
// frame that does not build one.

/** THE SCHEDULE, DRAWN: one cell per beat of track @p trackIndex on the
 *  keyed text node, at that beat's own laid-out rect, filled left to right
 *  by that beat's local progress.
 *
 *  Every cascade is otherwise invisible — it numbers units, spreads them
 *  and tells nobody — so tuning one means watching letters and guessing.
 *  This is `Composer::beatsOf` drawn without an intermediate: a beat that
 *  has not opened shows bed alone, one running shows its own fraction, one
 *  finished is full, and the pitch between cells is the cascade's real
 *  pitch, uneven where a cue table made it uneven.
 *
 *  THE RECTS ARE IN THE COMPOSER'S SPACE, because that is the space
 *  `beatsOf` answers in. Put the result over the whole composition —
 *  `root.child(kit::trackMeter(...).absolute().inset(0))` — and the cells
 *  land on the type wherever it is. It is read at DESCRIBE time from the
 *  layout the last draw left standing, so a moving cascade wants a
 *  re-describe per frame to move with it; that is the cost, and it is why
 *  this is an instrument and not a component.
 *
 *  An unknown key, a node that is not text and a track index past the
 *  node's list all give an EMPTY overlay, silently, exactly as `beatsOf`
 *  does. */
[[nodiscard]] inline Element trackMeter(const Composer& composer,
                                        std::string_view key, size_t trackIndex,
                                        SkColor4f fill,
                                        SkColor4f bed = {1, 1, 1, 0.10f}) {
  Element overlay = positioned();
  const std::vector<Beat> beats = composer.beatsOf(key, trackIndex);
  for (size_t i = 0; i < beats.size(); ++i) {
    const SkRect& rect = beats[i].rect;
    const std::string cell = "beat" + std::to_string(i);
    overlay.child(box()
                      .key(cell)
                      .left(rect.left())
                      .top(rect.top())
                      .width(rect.width())
                      .height(rect.height())
                      .fill(Fill::color(bed))
                      .child(box()
                                 .key(cell + "-t")
                                 .left(0)
                                 .top(0)
                                 .width(rect.width() *
                                        std::clamp(beats[i].localT, 0.0f, 1.0f))
                                 .height(rect.height())
                                 .fill(Fill::color(fill))));
  }
  return overlay;
}

/** THE SAME TEXT AT REST, UNDER THE MOVING COPY — what a deformation is
 *  measured against.
 *
 *  A track's deviation is per glyph and lives only in the draw, so the
 *  undeformed letter is nowhere on screen to compare with: a squash reads
 *  as a squash only beside the shape it squashed. This returns a box
 *  holding two copies of @p moving — a ghost set in @p colour, carrying no
 *  tracks and pinned at the box's origin, and @p moving itself in the flow,
 *  which is what sizes the box. Drop it in where the text was:
 *
 *      box().child(kit::restGhost(
 *          text(u8"RUBBERBAND", set).key("word").fx({…}), rest))
 *
 *  The ghost takes the moving copy's key with `-rest` after it (a keyless
 *  original leaves the ghost keyless too), so both are addressable and both
 *  prune. Everything that could make the two disagree about where a letter
 *  belongs is left alone — same content, same style, same width, same
 *  layout — and only the tracks and the ink differ.
 *
 *  IT GHOSTS THE TYPE AND NOTHING ELSE. A text node's children are its
 *  `Element::mark`s and its `RichText::slot` mounts, and both are already
 *  drawn once; the ghost carries neither, so nothing appears twice under
 *  one key. A slot's reserved space is content and stays, which is what
 *  keeps the two copies' letters in the same places.
 *
 *  TEXT ONLY. Anything else has its rest position on screen already, so a
 *  ghost of it would be a second copy of a thing that never moved; that
 *  warns once and hands @p moving back unchanged. */
[[nodiscard]] inline Element restGhost(Element moving, SkColor4f colour) {
  return detail::textAtRest(std::move(moving), colour);
}

}  // namespace sigil::compose::kit
