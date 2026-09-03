#pragma once

/** @file
 * SigilCompose typography — `TextPath`, a run of type whose BASELINE is a
 * path: the value `Element::onPath` takes.
 */

#include <sigilcompose/core/Shape.h>
#include <sigilmotion/values/Animatable.h>

namespace sigil::compose {

/** Text whose BASELINE is a path (`Element::onPath`).
 *
 *  The run is shaped once — real kerning, real ligatures, real advances —
 *  and then every glyph is placed by arc length along the resolved path
 *  and rotated to its tangent, through the same batched RSXform draw
 *  kinetic text uses (one draw per font+colour, never one per glyph).
 *
 *  The alternative, placing curved lettering by hand, costs one Element
 *  and one layout PER GLYPH and loses kerning, because each glyph is laid
 *  out alone. Ring labels, dial faces, seals, compass roses, mottoes and
 *  map lettering all want this instead. */
struct TextPath {
  /** The baseline, resolved against the node's laid-out box — any
   *  `shapes::` generator, or your own. EVERY contour is walked, in order,
   *  as one continuous arc-length coordinate, so a trajectory that the
   *  frame cut into several contours still carries its whole run.
   *
   *  "The node's box" means the TEXT NODE'S OWN box, not a parent's. The
   *  tempting `disc(c, R).child(text(...).onPath(...))` resolves the ring
   *  against the text's intrinsic size and silently collapses every label
   *  into a blob. Give the TEXT node the disc's width and height instead
   *  — the text leaf is the disc. */
  Shape path;
  /** WHERE ALONG the path the run sits, as a fraction of its length. With
   *  Align::Center this is the run's midpoint.
   *
   *  One float, so every `bind()`/`animate()` verb applies — and on a
   *  CLOSED baseline the fraction WRAPS, which is the infinite marquee: a
   *  phase output running 0→1 forever walks the whole run round the loop
   *  and back to where it started, with no seam and no relayout. On an
   *  open one the run simply slides, and glyphs pushed off either end are
   *  dropped rather than piled on the last point.
   *
   *  Moving it is PAINT-ONLY. The run is shaped and broken across the
   *  path's contours once; the phase re-places the glyphs it already
   *  placed, so a marquee costs a repaint and never a reflow. It is
   *  content volatility all the same — the glyphs move inside the node's
   *  own box — so the node's recording is refused while the phase runs and
   *  taken again once it provably holds still. */
  motion::Animatable<float> at = 0.0f;
  enum class Align { Start, Center, End };
  Align align = Align::Start;
  /** Perpendicular offset in px, positive to the LEFT of travel — which on
   *  a clockwise circle is outward. The path is the baseline, so this is
   *  how far off it the type rides. */
  float offset = 0.0f;
  /** Flip glyphs that would come out upside down, so lettering on the
   *  lower half of a ring reads right way up.
   *
   *  Default OFF, which is the engraver's convention: glyph-up points
   *  radially outward everywhere, so the bottom of a ring genuinely reads
   *  upside down. Modern signage flips; historical plates do not. */
  bool autoFlip = false;
  /** Which way a glyph faces.
   *
   *  `Tangent` is running lettering: the baseline lies ALONG the path,
   *  which is what a ring inscription or a motto wants. Note this already
   *  gives you "up points outward" on a circle — that is why a clock
   *  face's 6 comes out upside down, and why `autoFlip` exists.
   *
   *  `Radial` runs the baseline along the RADIUS instead, so the type
   *  radiates like a spoke — which is how an astrolabe limb, a compass
   *  rose and a radial axis label their divisions: you turn the
   *  instrument to read them. Without it each numeral costs one rotated
   *  Element, which is the same per-element cost onPath exists to avoid.
   *
   *  `Upright` leaves every glyph level regardless of where it sits —
   *  the convention a calendar ring or a modern gauge uses, and the one
   *  case neither of the others can reach.
   *
   *  The centre `Radial` radiates from is the resolved baseline's
   *  BOUNDING-BOX centre. That is the true centre for a full ring and
   *  silently wrong for an arc that does not span one — a quarter-arc's
   *  bbox centre is not its circle's centre — so give a partial arc a
   *  full-circle baseline and place the run on it with `at`. */
  enum class Orient { Tangent, Radial, Upright } orient = Orient::Tangent;
  /** Turn every glyph to its EXACT tangent instead of snapping the angle.
   *
   *  Snapping is the default because each distinct rotation is a distinct
   *  glyph-atlas strike: a curve whose glyphs turn continuously would
   *  re-rasterize every letter on every frame. The steps are far under a
   *  pixel of lean at label sizes on a ring whose letters sit further apart
   *  than that. Set it for STATIC artwork set large, where the steps show
   *  and nothing is paying per frame. */
  bool exactTangent = false;
};

}  // namespace sigil::compose
