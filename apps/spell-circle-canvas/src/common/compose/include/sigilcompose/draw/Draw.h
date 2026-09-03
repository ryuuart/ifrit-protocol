#pragma once

/** @file
 * The door between the tree and the pen, both ways: a node whose content
 * is a pen program, and a retained element painted inside a pen's loop.
 */

#include <include/core/SkRect.h>
#include <sigilcompose/core/Element.h>
#include <sigildraw/Pen.h>
#include <sigildraw/Retained.h>

#include <functional>
#include <string_view>

namespace sigil::compose {

/** A PEN PROGRAM: what a node runs each frame with a pen over its box. */
using PenProgram = std::function<void(draw::Pen&)>;

/** A NODE WHOSE CONTENT IS A PEN PROGRAM — `custom()` with a pen in
 *  place of the raw canvas, for the one node a declarative scene wants
 *  to draw imperatively.
 *
 *  The pen's `width` and `height` are the node's box, its transform
 *  starts at the box's corner, its clock is the composer's and its fonts
 *  are the composer's. The node is at `Cache::None`, so the program runs
 *  every frame; the pen it runs with lives with the node, so a style set
 *  in one frame holds in the next and a guest painted from it is
 *  retained. Like `custom()`, it sizes as an empty box does — give it
 *  dims, or make it `absolute().inset(0)`. */
Element draw(PenProgram program);
/** The PRUNABLE spelling: @p key is the program's identity, on the same
 *  contract as the keyed `custom()`. */
Element draw(std::string_view key, PenProgram program);

/** THE OTHER WAY THROUGH THE DOOR: `pen.element(element, box)` lands
 *  here. A composer is kept in the pen for the call site, @p element is
 *  reconciled against what that composer already holds — so its layout,
 *  its text shaping, its caches and its bindings carry from one frame
 *  to the next — and it is laid out at the box's size and painted at the
 *  box's corner in the pen's current space. Its clock is stepped by the
 *  pen's frame delta, so it advances on the frames it is painted and
 *  stands still on the frames it is not. A pen begun with no fonts
 *  paints nothing, since a composer shapes text with them. */
void paintRetained(draw::Pen& pen, const Element& element, const SkRect& box,
                   draw::Slot slot);

}  // namespace sigil::compose
