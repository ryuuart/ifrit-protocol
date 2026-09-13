#pragma once

/** @file
 * SigilCompose KIT — the ground a plate stands on: the board a placed
 * drawing is laid out over, at its own size, on its own ground, inside
 * its own mat.
 *
 * Every prop is the CONTENT and the ARRANGEMENT — how big, what is
 * behind, how much room stands between the edge and everything on it.
 * A board names no class and states no sheet, no font and no ink: what
 * a board holds is boxes, and every box property is the node's own, so
 * the sheet a page-less drawing resolves its classes through is stated
 * by its caller on the board it returns.
 */

#include <include/core/SkSize.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/SurfacePaint.h>

namespace sigil::compose::kit {

/** THE BOARD A DRAWING IS PLACED ON: the root of a plate that has no
 *  layout at all — a transcribed interface, an engraved card, a poster —
 *  where every child carries its own rect and nothing is a page.
 *
 *      kit::board({.size = {1440, 880}, .ground = Fill::color(kWall)})
 *          .styleSheet(look()).ink(kInk)
 *          .children({text(u8"…").rect(masthead), plate().rect(frame)})
 *
 *  It is a `stack`, so every child keeps the rect it was built with and
 *  they are painted in the order they were written — and an absolute
 *  child is placed against the board's own edge, so the room a drawing
 *  leaves at that edge is in the child's rect rather than in a distance
 *  the board holds. A board is not a page: it rules nothing off and
 *  writes no line, and what it holds is the caller's. */
struct Board {
  /** How big, px. An axis left at 0 stretches to the parent's own edge,
   *  which for the root a sketch renders is the canvas. */
  SkSize size{0, 0};
  /** Behind the whole board. Fill::none() (default) paints nothing, for
   *  a canvas the host already cleared. */
  SurfacePaint ground;
};

[[nodiscard]] Element board(const Board& plate);

}  // namespace sigil::compose::kit
