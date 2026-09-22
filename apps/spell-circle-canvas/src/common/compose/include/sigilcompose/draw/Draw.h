#pragma once

/** @file
 * @ingroup compose-draw
 *
 * The door between the tree and the pen, both ways: a node whose content
 * is a pen program, and a retained element painted inside a pen's loop.
 */

#include <include/core/SkRect.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcompose/core/Operator.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcore/callable/Callable.h>
#include <sigildraw/Pen.h>
#include <sigildraw/Retained.h>

#include <functional>
#include <string_view>

namespace sigil::compose {

/** A PEN PROGRAM: what a node runs each frame with a pen over its box.
 *
 *  IT NAMES ONLY THE PARAMETERS IT READS, as a paint program does: the pen
 *  and the paint context are both offered, so `[](draw::Pen& pen) {…}`,
 *  `[](draw::Pen& pen, const PaintContext& ctx) {…}` and `[] {…}` are all
 *  pen programs and nothing spells a parameter in order to ignore it. The
 *  context is what a program needs to hand a path to the decoration
 *  grammar — the node's own size, its outline, its content scale, the ink
 *  and font in force — which the pen alone cannot answer. */
using PenProgram = sigil::core::Callable<void(draw::Pen&, const PaintContext&)>;

/** A NODE WHOSE CONTENT IS A PEN PROGRAM — `custom()` with a pen in
 *  place of the raw canvas, for the one node a declarative scene wants
 *  to draw imperatively.
 *
 *  The pen's `width` and `height` are the node's box, its transform
 *  starts at the box's corner, its clock is the composer's and its fonts
 *  are the composer's. Its `mouseX`, `mouseY`, `mouseIsPressed` and the
 *  keys are what the host fed `Composer::setPointer` and
 *  `Composer::setKey`, the pointer in the node's own box. The node is at
 * @p caching, which is `Cache::None` by default — the program runs every
 * frame; the pen it runs with lives with the node, so a style set in one
 * frame holds in the next and a guest painted from it is retained. A
 * program that is drawn ONCE says `Cache::Texture` HERE rather than
 * chaining it after, because what the drawing IS is not a correction to
 * the verb. IT COMES BACK COVERING — `Element::cover()`, the box it
 * stands in — because a canvas fills its box by nature; a pen given a box
 * of its own says so with `inset()`, `rect()` or a size. */
Element pen(PenProgram program, Cache caching = Cache::None);
/** The PRUNABLE spelling: @p key is the program's identity, on the same
 *  contract as the keyed `custom()`. */
Element pen(std::string_view key, PenProgram program,
            Cache caching = Cache::None);

/** A NODE WHOSE PEN PAINTS ONTO A CANVAS THAT IS KEPT — `pen()`'s
 *  counterpart for a program that builds a picture up over frames.
 *
 *  `pen` REPAINTS FROM NOTHING EVERY FRAME; `graphics` KEEPS WHAT EARLIER
 *  FRAMES DREW. Each frame the program is handed the pen of a
 *  `draw::Graphics` the size of the node's box, what it draws lands on a
 *  surface that stands between frames, and that surface is put down on the
 *  node's canvas — so a trail, a slow accumulation and a picture drawn
 *  once all work here and none of them can work in `pen()`. The graphics
 *  is resized when the box changes, and a resize keeps the pixels.
 *
 *  p5's LOOP WORDS THEREFORE MEAN SOMETHING, and the program's own pen is
 *  where they are read: `noLoop()` stops running the program while the
 *  surface goes on being put down, `redraw()` runs it once more, and
 *  `frameRate(fps)` runs it at most that often. The node is `custom()` at
 *  @p caching like `pen()`, `Cache::None` by default, so the blit happens
 *  every frame whatever the program is doing; and like `pen()` it comes
 *  back covering the box it stands in.
 *
 *  The pen's clock, fonts, ink, font and input are the node's, exactly
 *  as in `pen()` — except that `frameCount` counts the program's RUNS and
 *  `deltaTime` is the time since the last one, as p5 keeps them for a
 *  program under `noLoop` or a requested rate. The canvas is formed no
 *  coarser than the composer's `setBakeDensity`, so a picture a host
 *  means to photograph finer than it steps it is drawn finer from the
 *  first frame rather than magnified at the still. */
Element graphics(PenProgram program, Cache caching = Cache::None);
/** The PRUNABLE spelling: @p key is the program's identity, on the same
 *  contract as the keyed `custom()`. */
Element graphics(std::string_view key, PenProgram program,
                 Cache caching = Cache::None);

/** THE OTHER WAY THROUGH THE DOOR: `pen.element(element, box)` lands
 *  here. A composer is kept in the pen for the call site, @p element is
 *  reconciled against what that composer already holds — so its layout,
 *  its text shaping, its caches and its bindings carry from one frame
 *  to the next — and it is laid out at the box's size and painted at the
 *  box's corner in the pen's current space. Its clock is stepped by the
 *  pen's frame delta, so it advances on the frames it is painted and
 *  stands still on the frames it is not. A pen begun with no fonts
 *  paints nothing, since a composer shapes text with them.
 *
 *  THE GUEST CASCADES FROM THE PEN: the ink and the font the pen was told
 *  it inherits are what the composer's root inherits, so a tree painted
 *  inside a pen program begins in the same colour and the same type the
 *  pen's own verbs do. */
void paintRetained(draw::Pen& pen, const Element& element, const SkRect& box,
                   draw::Slot slot);

/** A SCOPE PROGRAM: what a pen draws over a node's scope — every settled
 *  node under it, with its key, facts, bounds and outline — the one door
 *  through which a program reads where things landed. It is handed the
 *  scope as it stood when layout settled; attaching to it draws nothing. */
using ScopeProgram = std::function<void(draw::Pen&, const Scope&)>;

/** THE IMPERATIVE DOOR OF THE OPERATOR FAMILY: an adding operator that
 *  attaches ONE pen over the scope's box and hands @p program the pen
 *  and the scope, so a wire whose weight is a fact on the card, a label
 *  stamped along a route, a bulk overlay over a thousand nodes, are
 *  drawn by hand from the same table `connect::ByLane` reads.
 *
 *      box().children({…}).operators({drawWith(drawTraffic).zIndex(-1)})
 *
 *  What it costs, against an operator that attaches elements: the output
 *  is pixels, so nothing downstream reads it — no later operator, no
 *  sheet, no hit test; and a program is a callable with no equality, so
 *  an unkeyed operator compares equal to nothing and its node is
 *  described afresh every frame. The KEYED spelling vouches for the
 *  program's identity, as the keyed `pen()` does — equal keys assert
 *  equal programs — so the operator and the pen it attaches both prune
 *  while the scope they read is unchanged, and only the program's own
 *  run is paid for. */
Operator drawWith(ScopeProgram program);
Operator drawWith(std::string_view key, ScopeProgram program);

}  // namespace sigil::compose
