#pragma once

/** @file
 * SigilCompose KIT — the ground a plate stands on: the board a placed
 * drawing is laid out over, and the titled region a page divides itself
 * into.
 *
 * Every prop is the CONTENT and the ARRANGEMENT — how big, what is
 * behind, what the words are, how much room stands between them. A board
 * names no class at all; a panel names the class each of its three lines
 * is set in and the `weave::StyleSheet` in force where it lands says what
 * that class is.
 */

#include <include/core/SkSize.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/SurfacePaint.h>
#include <sigilcompose/core/Utf8.h>
#include <sigilcompose/kit/Part.h>
#include <sigilcompose/kit/Specimen.h>

#include <optional>

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

// ---------------------------------------------------------------------------
// The titled region

/** The leaf a panel's eyebrow defaults to: @p text in the class
 *  `eyebrow`, so the sheet in force sets it. */
[[nodiscard]] inline Element panelEyebrow(const Utf8& text) {
  return compose::text(text).styleClass("eyebrow");
}

/** THE TITLED REGION A PAGE DIVIDES ITSELF INTO: an eyebrow over a
 *  title, a note ranged at the far edge of the head, a hairline under the
 *  head where the region rules, and the content standing in the panel's
 *  own well.
 *
 *      kit::panel({.eyebrow = "LOADOUT", .note = "3 / 8",
 *                  .rule = Fill::color(kKeyline),
 *                  .body = Well{.width = 260, .ground = Fill::color(kPanel),
 *                               .padding = 12, .corners = 5,
 *                               .keyline = Fill::color(kKeyline)}},
 *                 slots())
 *
 *  ITS THREE LINES ARE SET IN THE CLASSES `eyebrow`, `title` and
 *  `captionNote`, of the `weave::StyleSheet` in scope here, and nothing
 *  else is said about their type; each is a PART, so a panel whose title
 *  must stand otherwise hands in its own leaf and everything under the
 *  panel keeps its registers.
 *
 *  THE HEAD IS A `kit::sheet`'s, which is why a rule under it stands in
 *  the middle of the distance to the content rather than adding to it:
 *  ruling a panel moves nothing in it. A panel is not a card, a plate or
 *  a frame — a region with no eyebrow is the same component with one
 *  fewer line. */
struct Panel {
  Utf8 eyebrow;
  Utf8 title;
  /** Ranged at the far edge of the head's last line — the title's where
   *  there is one, the eyebrow's where there is not — so a count, a
   *  bound or a state stands opposite the words that name the region. */
  Utf8 note;
  /** The hairline under the head. Fill::none() (default) rules none. */
  Fill rule;
  float ruleWidth = 1.0f;
  /** Between the head and the content, px. A rule bisects it. */
  float gap = 12.0f;
  /** Between the eyebrow and the title, px. */
  float titleGap = 4.0f;
  /** THE PANEL'S OWN CELL — its size, ground, padding, corners and
   *  keyline. Unset leaves the head and the content bare, for a caller
   *  that grounds the region itself. */
  std::optional<Well> body;
  /** THE THREE LINES, as functions of their text and then of this panel;
   *  a part takes the parameters it names. Empty means the default. The
   *  plain names are the words themselves, so each part carries `Line`. */
  Part<Utf8, Panel> eyebrowLine = panelEyebrow;
  Part<Utf8, Panel> titleLine = sheetTitle;
  Part<Utf8, Panel> noteLine = captionNote;
};

/** THE REGION, with @p content standing under its head. */
[[nodiscard]] Element panel(const Panel& region, Element content);

}  // namespace sigil::compose::kit
