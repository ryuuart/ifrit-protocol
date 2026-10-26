#pragma once

/** @file
 * @ingroup compose-kit
 *
 * CHROME: the dressed edge of a control, as tokens.
 *
 * THE BEVEL is the whole of it. Every interface toolkit that made a
 * rectangle look like a thing you could press drew one: an edge lit from
 * a direction, a light tone on the two sides facing the light and a
 * shadow tone on the two facing away, some depth in pixels, and — for the
 * toolkits that wanted a thicker frame than one bevel makes — a second
 * bevel a gap inside the first. `Bevel` is that as ONE value, and the
 * eras below are token sets of it: what separates Motif from a Flash
 * panel from a skin from a stamped plate is the tones, the depth, the
 * corner and whether the edge is drawn or moulded, not four families of
 * painters.
 *
 * The tones flow through the cascade. Each tone is a colour term — a
 * colour, the ink in force or `var(name)` — read where the bevel paints,
 * and a subtree that sets the custom properties `bevel-light` and
 * `bevel-shadow` dresses every bevel inside it that takes its tones in
 * force, with nothing said at the use site but the shape:
 *
 *     window.var("bevel-light", hi).var("bevel-shadow", lo);
 *     …
 *     panel.overlay(kit::bevels::inherited(kit::bevels::motif(hi, lo)));
 *     outline.overlay(kit::bevels::motif(var("rule"), var("rule"), 1));
 */

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/PixelStyles.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/Shape.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Paint.h>

#include <optional>

namespace sigil::compose::kit {

/** THE SECOND RING of a doubled bevel: the same edge again, @p gap px
 *  further in. Unset on a `Bevel` draws one ring, which is every plain
 *  button; set, it is the thicker frame a window, a group box and a
 *  menu separator wear.
 *
 *  `inverted` turns the inner ring the other way up, which is what makes
 *  a GROOVE rather than a step: a raised ring around a sunken one reads
 *  as a line scored into the surface, and it is the only way a toolkit
 *  with two tones drew a divider. */
struct BevelInner {
  /** Px inside the outer ring. 0 puts the two rings edge to edge. */
  float gap = 1;
  ColorTerm light = {1, 1, 1, 0.6f};
  ColorTerm shadow = {0, 0, 0, 0.5f};
  /** Px of the lit edge. */
  float depth = 1;
  /** Px of the shadowed edge; 0 takes `depth`. */
  float shadowDepth = 0;
  bool inverted = false;
  /** WHICH SIDES this ring is drawn on, and how its bands end where the
   *  mask left their neighbour out. A ring on two sides is a HALF ring —
   *  the doubled edge a key wears on its shaded sides alone, where a
   *  second full ring would light the top of a 3 px control — and
   *  `BevelEnds::Sliced` stops it flush at the corners it does not own
   *  rather than running it into the outer ring's. */
  geometry::path::Edge edges = geometry::path::Edge::All;
  styles::BevelEnds ends = styles::BevelEnds::Mitred;
  bool operator==(const BevelInner&) const = default;
};

/** THE BEVEL: a control's lit edge as one token set, and a VALUE
 *  decoration in its own right — attach it with `.overlay()`, which is
 *  the slot that puts it over the fill and under the content. A bevel in
 *  `.background()` is covered by the surface it was meant to sit on and
 *  one in `.foreground()` rides over the panel's own label.
 *
 *  SUNKEN IS THE SAME TOKENS THE OTHER WAY UP — the two tones on the far
 *  edges, which is what a well, a trough and a pressed button have always
 *  been. It is one bool because a toolkit's pressed button was the
 *  toolkit's raised button with two colours swapped, and a look that
 *  states the pair twice can drift.
 *
 *  SOFTNESS is the one thing that changes the KIND of mark: at 0 the edge
 *  is drawn on the pixel lattice, hard, which is what a toolkit that
 *  owned every pixel did; above 0 it is a blurred plane pair lit from
 *  `angleDegrees`, which is what a stamped, moulded or embossed surface
 *  wants. The tones and the depth mean the same thing on both sides of
 *  that line, so a look moves between them by one number. */
struct Bevel {
  /** The lit tone and the shaded one: each a colour, the ink in force or
   *  a custom property, read where the ring paints. */
  ColorTerm light = {1, 1, 1, 0.6f};
  ColorTerm shadow = {0, 0, 0, 0.5f};
  /** Px of the lit edge. */
  float depth = 1;
  /** Px of the shadowed edge; 0 takes `depth`. */
  float shadowDepth = 0;
  bool sunken = false;
  styles::BevelCorner corner = styles::BevelCorner::Square;
  /** Off is the lattice a bitmap toolkit drew on. Ignored above 0
   *  softness, where the mark is a blur. */
  bool antiAlias = true;
  /** WHICH SIDES the ring is drawn on, and how its bands end where the
   *  mask left their neighbour out — the same pair of properties the inner
   *  ring carries, for the outer one. Ignored above 0 softness, where the
   *  mark is a blurred plane pair rather than four bands. */
  geometry::path::Edge edges = geometry::path::Edge::All;
  styles::BevelEnds ends = styles::BevelEnds::Mitred;
  /** Px the edge is blurred over; 0 is the drawn edge. */
  float softness = 0;
  /** Where the light stands, in degrees, for a softened edge. */
  float angleDegrees = 120;
  std::optional<BevelInner> inner;
  /** THE TONES IN FORCE: where set, the outer ring's light and shadow are
   *  the custom properties `bevel-light` and `bevel-shadow` in force where
   *  the bevel paints, each a colour, and `light` and `shadow` above are
   *  the fallback where no ancestor set one. An inner ring whose tones
   *  restate the outer pair — an etched groove's, which is the same pair
   *  the other way up — takes the pair in force with it; an inner ring
   *  with tones of its own keeps them. */
  bool tonesInForce = false;

  bool operator==(const Bevel&) const = default;
  /** How far in from the outline the marks reach. */
  float reach() const;
  /** Each ring is clipped inside the shape it dresses; an inner ring set
   *  outside the outer one, at a negative gap, stands past the outline,
   *  and the node keeps its box. */
  bool staysOnOutline() const { return !inner || inner->gap >= 0; }
  void paint(draw::Pen& pen, const PaintContext& ctx) const;
};

/** @p element DRESSED in @p bevel — the whole bevel, each ring in the slot it
 *  belongs in, which is the one call a panel needs.
 *
 *  The two rings do NOT go in the same slot. The outer one is an OVERLAY:
 *  it stands on the node's own edge, over the fill and under the content,
 *  which is where an edge belongs. The inner one is a FOREGROUND: a
 *  second ring a gap in from the edge stands at the padding line, and a
 *  frame there is a border the content sits INSIDE rather than an edge
 *  the content rides over — put it under the content and the first label
 *  long enough to reach the padding erases it.
 *
 *  Attaching the value yourself with `.overlay(b)` paints both rings in
 *  that one slot, which is what a node with no content wants. */
Element& bevelled(Element& element, const Bevel& bevel);

/** THE ERA TOKEN SETS: the SHAPE of each toolkit's bevel, taking the two
 *  tones, because how a toolkit derived its tones from a face is that
 *  toolkit's own arithmetic and belongs where the face is. */
namespace bevels {

/** @p shape with its outer ring's tones, and an inner ring's that restate
 *  them, read from the properties in force — `bevel-light` and
 *  `bevel-shadow` — and @p shape's own tones where no ancestor set them,
 *  so a panel outside any era is still drawn. */
Bevel inherited(Bevel shape = {});

/** THE TWO-TONE SHADOW of the Motif toolkits: @p depth px on the pixel
 *  lattice, mitred, drawn in the colour set of the window it is in. Every
 *  control in a CDE session is this, at 1 or 2 px, raised or sunken. */
Bevel motif(ColorTerm light, ColorTerm shadow, float depth = 2,
            bool sunken = false);

/** THE ETCHED GROOVE the same toolkits scored a separator with: half the
 *  depth twice, the inner ring the other way up, and the corner pixel to
 *  the far band on both rings so the groove closes. */
Bevel motifEtched(ColorTerm light, ColorTerm shadow, float depth = 2,
                  bool sunken = false);

/** THE CHAMFERED PANEL of a Flash-era component kit: an asymmetric pair
 *  over an antialiased silhouette — the lift wider than the drop, both
 *  derived from the face — and, doubled, the same pair again fainter
 *  @p gap px in. Pass a gap of 0 for the single-bevel panel. */
Bevel flash(material::Color face, float gap = 0);

/** THE SKIN'S PART: @p depth of light and the same of shadow, square,
 *  stated tones — a bitmap skin's chrome was pixels someone placed, so
 *  neither tone is derived from anything and every part of the window,
 *  from the title bar to the smallest key, is this one value. */
Bevel skin(ColorTerm light, ColorTerm shadow, float depth = 1,
           bool sunken = false);

/** THE STAMPED PLATE: a moulded edge rather than a drawn one — the
 *  blurred plane pair a pressed, cast or embossed surface has, lit from
 *  the upper left. @p sunken is the recess punched into the same plate.
 */
Bevel plate(ColorTerm light, ColorTerm shadow, float depth = 1.4f,
            float softness = 1.8f, bool sunken = false);

}  // namespace bevels

}  // namespace sigil::compose::kit
