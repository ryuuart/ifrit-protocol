#pragma once

/** @file
 * SigilCompose KIT — a node placed by the numbers a plate was measured
 * in, and the one line every plate draws.
 *
 * The coordinate systems themselves are SigilGeometry's
 * (`geometry::path::Frame`, the polar one; `geometry::path::Grid`, the
 * unit map). What is here is the ways a NODE is placed by them — a disc
 * about a centre, a pinned box at absolute coordinates, the disc a
 * frame's own radius names, and the two circles that disc is drawn as,
 * stroked and filled — and the line: a separator, a tick, a caret, a
 * whisker, which are one component at four thicknesses.
 */

#include <include/core/SkPoint.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/Shape.h>
#include <sigilcompose/core/SurfacePaint.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/path/Frame.h>

#include <concepts>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace sigil::compose::kit {

// ---------------------------------------------------------------------------
// Placement values — the arithmetic every polar plate spells out

/** A box of `radius` about `centre` — the polar-chart placement. Every
 *  inscribed-in-the-box generator (sector, arc, circle, star) otherwise
 *  needs `width(2r).height(2r).centerAt(c)` spelled out at its call
 *  site. */
inline Element disc(SkPoint centre, float radius) {
  return box()
      .width(Dimension(radius * 2))
      .height(Dimension(radius * 2))
      .centerAt(centre);
}

/** THE PINNED BOX: a node at absolute `(x, y)` of size @p w × @p h.
 *
 *  `box().left(Dimension(x)).top(Dimension(y)).width(Dimension(w)).height(Dimension(h))`
 * is the four-call spelling every plate that has no layout at all repeats — a
 *  transcribed interface, an engraved plate, a pattern card. Reconstructing
 *  a reference means quoting coordinates measured off it, and there is
 *  nothing for a flexbox to decide.
 *
 *  It is a peer of `disc()` rather than a layout scheme: it decides
 *  nothing, and every number is the caller's. */
inline Element at(float x, float y, float w, float h) {
  return box()
      .left(Dimension(x))
      .top(Dimension(y))
      .width(Dimension(w))
      .height(Dimension(h));
}
/** The same, onto an element that already exists — the overload a caller
 *  reaches for when the node is built elsewhere and only its position is
 *  the plate's business. */
inline Element at(Element e, float x, float y, float w, float h) {
  e.left(Dimension(x))
      .top(Dimension(y))
      .width(Dimension(w))
      .height(Dimension(h));
  return e;
}
/** The same two with the size stated as `Dimension`s — a height of
 *  `1.7_em` beside a width in px — for a pinned box measured in the font
 *  it will be set in. */
inline Element at(float x, float y, Dimension w, Dimension h) {
  return box().left(Dimension(x)).top(Dimension(y)).width(w).height(h);
}
inline Element at(Element e, float x, float y, Dimension w, Dimension h) {
  e.left(Dimension(x)).top(Dimension(y)).width(w).height(h);
  return e;
}

/** `disc` at @p frame: an Element sized and centred for a
 *  `shapes::circle()`/`sector()`/`arc()` outline at @p rNorm of the
 *  frame's radius.
 *
 *  IT TAKES A FRAME THAT ALREADY EXISTS, and the constraint is what
 *  makes that true rather than a matter of style. A `Frame` begins with
 *  a point and a radius, so `disc({x, y}, r)` initialises one just as
 *  readily as it initialises the SkPoint the centre overload wants, and
 *  a plain overload pair leaves that call ambiguous — which is a
 *  compile error at every site that writes a centre as a braced pair,
 *  the way every other Skia point is written. Deduction cannot see
 *  through a braced list, so this overload drops out of the set there
 *  and the pair means the point it reads as. Spell a frame LITERAL as
 *  `geometry::path::Frame{…}`. */
template <class FrameLike>
  requires std::same_as<std::remove_cvref_t<FrameLike>, geometry::path::Frame>
inline Element disc(const FrameLike& frame, float rNorm = 1.0f) {
  return disc(frame.centre, rNorm * frame.radius);
}

/** ONE CIRCLE STROKED AND NOT FILLED, of @p radius about @p centre: a
 *  rule of a limb, a struck construction circle, a declination circle, an
 *  orbit.
 *
 *      kit::ring(centre, r, stroke(1.2f, Fill::color(kInk)))
 *
 *  It is `disc` with the three verbs of pure ceremony that always follow
 *  it written once — the circle's own silhouette, a fill of none and the
 *  pen — because a box of radius r about a point is not yet a circle. */
inline Element ring(SkPoint centre, float radius, Decoration pen) {
  return disc(centre, radius)
      .shape(geometry::shapes::circle())
      .fill(Fill::none())
      .stroke(std::move(pen));
}

/** ONE FILLED CIRCLE of @p radius about @p centre: a pole, a star, a
 *  crossing, a marked point of a construction. */
inline Element dot(SkPoint centre, float radius, SurfacePaint fill) {
  return disc(centre, radius)
      .shape(geometry::shapes::circle())
      .fill(std::move(fill));
}

/** WHAT IS IN IT STANDS IN THE MIDDLE, both ways — `alignItems(Center)`
 *  and `justify(Center)`, which always travel together and say one thing
 *  between them: an icon in its cell, a glyph in its key, a figure in its
 *  well, a picture in the room it was given.
 *
 *      kit::centred(glyph()).width(24).height(24)
 *      kit::centred().row().gap(6).children({a, b})
 *
 *  It decides nothing else: the box it returns is an ordinary one, so the
 *  size, the ground and the direction are the caller's. */
[[nodiscard]] inline Element centred() {
  return box().alignItems(Align::Center).justify(Justify::Center);
}
/** The same round one child, which is what most of them hold. */
[[nodiscard]] inline Element centred(Element child) {
  return centred().children({std::move(child)});
}

// ---------------------------------------------------------------------------
// The one line

/** ONE LINE: a mark of `thickness` running `length`, in the ink in force
 *  unless a fill is stated.
 *
 *      kit::line({.fill = Fill::color(kRule)})       // a hairline across
 *      kit::line({.length = Dimension(13), .thickness = 3, .column = true})
 *
 *  A HAIRLINE IS ITS DEFAULT THICKNESS and a tick is the same call at
 *  another one, which is why there is one name here and not two: a
 *  separator, a rule under a head, a tick on a scale, a caret and a
 *  whisker are all a box of one small dimension in the ink. */
struct Line {
  /** Along the line. Auto (default) stretches it across the flow it
   *  stands in, which is what a separator between stacked things wants; a
   *  line that must fill the flow's OWN axis — the rule that fills what a
   *  name and a note leave between them — says `Element::grow` on what
   *  comes back. */
  Dimension length;
  /** Across the line, px. */
  float thickness = 1.0f;
  /** false (default) runs the line ACROSS; true runs it DOWN. */
  bool column = false;
  /** Fill::none() (default) is `Fill::currentInk()`, so a line under a
   *  recoloured ancestor is recoloured with it. */
  Fill fill;
  /** Held off at BOTH ends, px — the separator that stops short of the
   *  edges it runs between. */
  float inset = 0.0f;

  /** A SECOND RAIL HELD OFF THIS ONE — the double rule a masthead, a
   *  colophon and a specimen sheet's row are ruled with, which is one
   *  heavy line with a hairline or a dotted companion beside it rather
   *  than two lines a caller places.
   *
   *  Unset (default) is the single rule. A pair is drawn as ONE node
   *  whose two rails share one route, so the companion's dashes register
   *  against the first rail rather than drifting off it. */
  struct Companion {
    float thickness = 0.7f;
    /** Between the two rails, px, on the side the pair reads from — the
     *  side a masthead's hairline stands on is under the heavy rule. */
    float gap = 4.0f;
    /** Fill::none() (default) is the first rail's own. */
    Fill fill;
    /** Dash on/off intervals, px; empty is solid. */
    std::vector<SkScalar> dash;
  };
  std::optional<Companion> pair;
};

/** THE LINE, or the pair of rails `Line::pair` asks for. */
[[nodiscard]] Element line(const Line& mark);

}  // namespace sigil::compose::kit
