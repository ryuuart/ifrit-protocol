#pragma once

/** @file
 * SigilCompose PIXEL STYLES — the hard-edged interface vocabulary a
 * bitmap-era panel is dressed with, as value decorations beside the
 * blurred ones in `LayerStyles.h`: the BEVEL PAIR (a light edge and a
 * dark edge, raised or sunken as one value), corner BRACKETS standing
 * off a box, a TICK RAIL along one of its edges, SCANLINES over it, and
 * the STIPPLE a colour is laid through.
 *
 * Every one is made of strokes and rectangles on the pixel lattice and
 * never of a blur or a shader, which is what the era looked like: a 1 px
 * highlight and a 1 px shadow, a reticle's four L's, a ruler nobody
 * reads, the raster of the tube the interface was shot on. Every one is
 * a VALUE with defaulted equality, so a panel wearing them prunes and
 * caches like any other static decoration.
 *
 * ATTACHMENT IS THE CONTRACT, as for every decoration: `.overlay()` is
 * the slot a bevel wants — over the fill, under the content and the
 * children — because a bevel put in `.background()` is painted and then
 * covered by the surface it was meant to sit on, and one in
 * `.foreground()` rides over the panel's own label. Brackets, rails and
 * scanlines are usually foregrounds.
 */

#include <include/core/SkBlendMode.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <sigilcompose/core/Paint.h>
#include <sigilgeometry/kit/Corners.h>
#include <sigilgeometry/path/Edges.h>

#include <algorithm>
#include <cstdint>

namespace sigil::compose::styles {

/** WHERE THE TWO BANDS MEET at the two corners they collide on — the
 *  top-right and the bottom-left of a raised pair, where one band arrives
 *  along the top or the left and the other along the right or the bottom.
 *  (The other two corners belong to one band outright and have nothing to
 *  decide.)
 *
 *  Every toolkit that drew a bevel picked one of these three and drew
 *  every control in it, which is why the choice is a token rather than a
 *  per-mark argument. */
enum class BevelCorner {
  /** The near band runs the full width and the far band lies under it —
   *  the square step, and what a stroked pair does by itself. */
  Square,
  /** The bands meet on the 45° diagonal, with the pixel ON the diagonal
   *  taken by the NEAR band. */
  Mitre,
  /** The same diagonal one pixel over: the corner pixel taken by the FAR
   *  band. This is what the inner ring of a doubled frame wants — two
   *  rings that both claim the corner leave a bright notch in a groove
   *  that is meant to read as continuous. */
  MitreFar,
};

/** WHERE A BAND ENDS at a corner the mask left the other band out of.
 *
 *  It is a different question from `BevelCorner`, which decides how two
 *  bands that are BOTH drawn divide the corner between them. This one
 *  only arises where a ring is drawn on some of its edges: the band
 *  arrives at a corner and there is nothing to meet.
 *
 *  With every edge drawn the two answers are the same mark. */
enum class BevelEnds : uint8_t {
  /** The band runs into the corner the whole ring would have made — the
   *  corner mode's own mitre, or its square step — so a masked ring is
   *  the full ring with bands left out. */
  Mitred,
  /** The band is cut flush there, one missing band's depth short, so a
   *  half ring stops at the edge it dresses and the corner it does not
   *  own is left to the face. This is the doubled edge a key wears on
   *  its two shaded sides: a shadow that turns the corner and stops. */
  Sliced,
};

/** THE BEVEL PAIR: a light edge on the top and left of the outline and a
 *  dark edge on the bottom and right, each a stroke kept inside the
 *  silhouette — the raised panel of the beveled-desktop era.
 *  SUNKEN IS NOT A SECOND DRAWING: the same two tones on the far edges,
 *  which is what a well, a trough and a pressed button have always been,
 *  and why it is one bool on one value rather than a twin type.
 *
 *  UNDER `Square` the edges are the outline's sub-contours facing each
 *  box edge, so the pair follows a chamfered panel onto its chamfers. The
 *  two vertical edges are painted first and the two horizontal ones over
 *  them, so the top-right corner belongs to the top edge and the
 *  bottom-left to the bottom.
 *
 *  UNDER A MITRE the mark is the box's RING, split on the two diagonals
 *  and clipped inside the outline. A diagonal is a corner's idea and a
 *  corner is a box's, so a mitre says nothing about a chamfer or a round;
 *  a rounded panel wearing one gets the ring its box describes with the
 *  round cut out of it.
 *
 *  The widths are separate because the era's panels were not symmetric:
 *  a 3 px lift over a 2 px drop is one common spelling, 1 px over 1 px
 *  the other. */
struct BevelPair {
  SkColor4f light = {1, 1, 1, 0.6f};
  SkColor4f dark = {0, 0, 0, 0.5f};
  float lightWidth = 1.0f;
  float darkWidth = 1.0f;
  /** Light on the far edges, dark on the near ones. */
  bool sunken = false;
  /** The edge classification's sampling length in px. */
  float step = 3.0f;
  BevelCorner corner = BevelCorner::Square;
  /** ON is the smooth edge a vector-era panel has. OFF is the lattice a
   *  toolkit drew on: the ring's coordinates are rounded to whole pixels
   *  and every mark is hard, which is the only way a 1 px highlight
   *  reads as a line rather than as a blur. */
  bool antiAlias = true;
  /** WHICH SIDES THE PAIR IS DRAWN ON. `All` is the ring every panel
   *  wears. A subset is a HALF RING — a deepened shadow on the two
   *  shaded sides with no light opposite it, which is what a key's
   *  doubled edge and a one-sided rule are, and what a stroke sliced to
   *  the same edges cannot be, since it carries no tone of its own for
   *  the side it turns onto. A band a tone would not have landed on is
   *  simply not drawn: the mask selects sides, never tones. */
  geometry::path::Edge edges = geometry::path::Edge::All;
  /** How a band ends where the mask left its neighbour out; nothing at
   *  all under a full mask. */
  BevelEnds ends = BevelEnds::Mitred;

  bool operator==(const BevelPair&) const = default;
  /** The same pair the other way up. */
  [[nodiscard]] BevelPair inverted() const {
    BevelPair other = *this;
    other.sunken = !sunken;
    return other;
  }
  /** How wide the mark is, inside the outline. */
  float reach() const { return std::max(lightWidth, darkWidth); }

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

/** A bevel pair in two stated tones, @p width px each. */
inline BevelPair bevelPair(SkColor4f light, SkColor4f dark, float width = 1.0f,
                           bool sunken = false) {
  return BevelPair{light, dark, width, width, sunken};
}

/** A bevel pair DERIVED from the face it sits on: the light edge is
 *  @p base lightened by @p lift (added per channel, clamped at one) and
 *  the dark edge is @p base with @p drop of its brightness taken away.
 *  Two operations rather than one because a highlight walks toward white
 *  and saturates there, while a shadow keeps the face's hue. A face near
 *  white has nowhere to lighten to and wants its tones stated
 *  outright. */
inline BevelPair bevelPair(SkColor4f base, float lift, float drop,
                           float width = 1.0f, bool sunken = false) {
  return BevelPair{material::skia::lighten(base, lift),
                   material::skia::scale(base, 1.0f - drop), width, width,
                   sunken};
}

/** CORNER BRACKETS standing off a box: an L of @p arm px at each selected
 *  corner, @p gap px inside the edge — the reticle, the selection frame,
 *  the crop mark of an interface that boxes nothing.
 *
 *  They are drawn on the node's BOX, not its outline: a bracket at a gap
 *  from the edge is a box's idea. Marks that belong on a silhouette's own
 *  corners — the brackets of a chamfered frame, on its chamfers — are the
 *  span claim `spans::corners(arm)` and follow the shape. A negative gap
 *  stands the brackets outside the box, and the decoration declares that
 *  reach. */
struct Brackets {
  SkColor4f color = {1, 1, 1, 1};
  float arm = 18.0f;
  float width = 2.0f;
  float gap = 0.0f;
  geometry::shapes::Corner corners = geometry::shapes::Corner::All;
  /** Off (the default) keeps every mark on whole pixels, which is what a
   *  1 px bracket on a screen-shot interface is. */
  bool antiAlias = false;

  bool operator==(const Brackets&) const = default;
  float bleed() const { return std::max(0.0f, -gap); }

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

inline Brackets brackets(
    SkColor4f color, float arm = 18.0f, float width = 2.0f, float gap = 0.0f,
    geometry::shapes::Corner corners = geometry::shapes::Corner::All) {
  return Brackets{color, arm, width, gap, corners};
}

/** A TICK RAIL along one edge of the box: a mark every @p pitch px,
 *  @p minor px long, every @p majorEvery-th one @p major px long — the
 *  ruler, the meter edge, the readout nobody reads that makes a panel
 *  denser than anything on it.
 *
 *  The ladder is linear: a mark's position is a distance along the edge,
 *  and the first stands @p phase pitches in, so a half-pitch phase (the
 *  default) keeps the end marks off the corners. Each mark occupies
 *  `[d, d + width)` across the run, which at an integer pitch and width
 *  lands on whole pixels. `edge` is a set: a rail on two edges is one
 *  value. The radial ladder is SigilGeometry's `shapes::ticks`, which
 *  states the angle convention this rail has no use for. */
struct TickRail {
  SkColor4f color = {1, 1, 1, 0.5f};
  float pitch = 8.0f;
  float minor = 4.0f;
  float major = 9.0f;
  float width = 1.0f;
  /** Every n-th mark, counting from the first, is a major one; 0 makes
   *  every mark minor. */
  int majorEvery = 4;
  /** Where the first mark stands, in pitches. */
  float phase = 0.5f;
  geometry::path::Edge edge = geometry::path::Edge::Top;
  bool antiAlias = false;

  bool operator==(const TickRail&) const = default;

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

inline TickRail tickRail(
    SkColor4f color, float pitch = 8.0f, float minor = 4.0f, float major = 9.0f,
    int majorEvery = 4, geometry::path::Edge edge = geometry::path::Edge::Top) {
  return TickRail{color, pitch, minor, major, 1.0f, majorEvery, 0.5f, edge};
}

/** SCANLINES over the outline: a band @p on px tall every @p period px,
 *  in @p color through @p blend, clipped inside the shape — the raster of
 *  the tube, laid over a panel as the last thing on it.
 *
 *  Hard rows, deliberately: a 3 px period with a 1 px band is the house
 *  spelling of an interface shot off a monitor, and a raised-cosine beam
 *  is a different picture (`material::field::crtOverlay`, which also
 *  carries the tube's corner falloff). `kPlus` in a tint is the phosphor
 *  reading; source-over in a low black alpha is the print reading.
 *  @p phase slides the rows, in px. */
struct Scanlines {
  SkColor4f color = {0, 0, 0, 0.2f};
  float period = 4.0f;
  float on = 2.0f;
  float phase = 0.0f;
  SkBlendMode blend = SkBlendMode::kSrcOver;

  bool operator==(const Scanlines&) const = default;
  /** The CRT reading composites with the picture beneath the rows; only
   *  the print reading (source-over black) draws over itself alone. */
  bool blends() const { return blend != SkBlendMode::kSrcOver; }

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

inline Scanlines scanlines(SkColor4f color, float period = 4.0f,
                           float on = 2.0f,
                           SkBlendMode blend = SkBlendMode::kSrcOver) {
  return Scanlines{color, period, on, 0.0f, blend};
}

/** THE STIPPLE: one colour laid through a repeating 1-BIT MASK on the
 *  pixel lattice, clipped inside the outline — the insensitive control of
 *  a toolkit that had no alpha, the 50 % dither a ramp was made of before
 *  there were gradients, the screen a monochrome printer shaded with.
 *
 *  THE MASK IS BITS, NOT AN IMAGE, and must be. A decoration is compared
 *  by value and an image inside one compares by POINTER, so a node whose
 *  stipple carries a tile cut where it is asked for never compares equal
 *  to itself and is re-recorded on every describe, forever. Bits compare
 *  like the numbers they are, and the tile they name is cut once for the
 *  process.
 *
 *  Bit `y * size + x`, counting from the low bit, is the cell at (x, y):
 *  set takes the colour, clear leaves what is under it. `size` is at most
 *  eight, which is every screen a bitmap toolkit ever shipped.
 *
 *  It is a TINT through a mask rather than a `Pattern`, and that is the
 *  distinction: a Pattern bakes its colours into its tile, so a themed
 *  stipple would need one tile per palette per colour, where one mask
 *  serves every colour it is ever drawn in. */
struct Stipple {
  SkColor4f color = {0, 0, 0, 1};
  /** The 50 % checkerboard: cells (0,0) and (1,1) of a 2 × 2 lattice. */
  uint64_t bits = 0b1001;
  int size = 2;
  /** Px per cell. Whole numbers keep the mask on the lattice. */
  float cell = 1.0f;

  bool operator==(const Stipple&) const = default;

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

/** The 50 % checkerboard in @p color — `stipple(x, y) = (x + y) & 1`,
 *  which is the one every toolkit greyed a dead control out with. */
inline Stipple stipple(SkColor4f color, float cell = 1.0f) {
  return Stipple{color, 0b1001, 2, cell};
}

/** A stipple of @p on cells in every `size × size` block, filled in the
 *  order a Bayer threshold matrix fills them — the ordered dither, one
 *  tone of it. @p size is 2, 4 or 8; @p on runs from 0 (nothing) to
 *  `size * size` (solid). */
Stipple dither(SkColor4f color, int on, int size = 4, float cell = 1.0f);

}  // namespace sigil::compose::styles
