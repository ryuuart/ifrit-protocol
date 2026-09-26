#pragma once

/** @file
 * @ingroup geometry-kit
 *
 * A figure's divisions as ONE path with N contours.
 *
 * Three stock values over `radial`, one idea: emit N marks into a single
 * outline rather than N drawn things. `ticks()` walks a division count
 * around a `PolarFrame`; `arcs()` walks the same count as CLOSED segments
 * of the ring itself; `chords()` walks a polygon's sides. Each has a box
 * form — a `Radial` whose frame comes from the box it is asked for — and
 * a frame form, the outline drawn on a frame the caller places.
 */

#include <sigilgeometry/kit/Radial.h>
#include <sigilgeometry/path/Frame.h>
#include <sigilgeometry/path/Radial.h>

#include <algorithm>
#include <functional>
#include <glm/vec2.hpp>

namespace sigil::geometry::shapes {

/** How far in and out one mark reaches, in normalised radius. */
struct Span {
  float inner = 0.92f;
  float outer = 1.0f;
  bool operator==(const Span&) const = default;
};

/** A RADIAL DIVISION LADDER, emitted as one path with N contours rather
 *  than N drawn things. Every angle is in the FRAME's units — `.from = 0`
 *  on a North/CW frame is 12 o'clock, on an East/CW frame 3 o'clock —
 *  which is the whole reason a `PolarFrame` exists: it carries where zero
 *  is and which way the angles run, so a ladder need not restate either.
 *  @trap One path has one style and one reveal window, so marks that fade
 *  or move individually still want their own keyed nodes. */
struct Ticks {
  /** How many marks. With `sweep = 360` and `closed = false` this is the
   *  division count and mark N would coincide with mark 0, so it is not
   *  emitted. */
  int divisions = 60;
  /** The frame's degrees of the first mark. */
  float from = 0.0f;
  /** Total span in frame degrees. 360 is a full ring. */
  float sweep = 360.0f;
  /** Emit `divisions + 1` marks, i.e. close the ladder at both ends. What
   *  a 0–90° altitude scale wants and a 60-minute ring does not. */
  bool closed = false;

  /** The ordinary mark. */
  Span mark{};
  /** Every `longEvery`-th mark (counting from index 0) takes `longMark`
   *  instead. 0 disables. */
  int longEvery = 0;
  Span longMark{0.88f, 1.0f};

  /** THE ESCAPE HATCH, for a ladder with more than two length classes.
   *  Return the span for mark @p i; the second argument is what the
   *  fields above would have given. A DEGENERATE span (`inner == outer`)
   *  skips the mark, and is the only way to. Null — the default — means
   *  "use the fields".
   *  @trap Equality cannot see a callable, so a Ticks carrying one
   *  compares unequal to everything and its node never prunes. */
  std::function<Span(int i, Span fromFields)> classify;

  /** HOW WIDE ONE MARK IS, in px across the radius. Zero — the default —
   *  emits the open radial LINE a ladder is stroked from; anything else
   *  emits a CLOSED rectangle on the same radius, which is the node, the
   *  lozenge and the bar of a ring that is filled. Px rather than
   *  degrees, because the mark that fattens with radius is `arcs()`.
   *  @trap Not a stroke width by another name: a closed mark is
   *  GEOMETRY, so it fills, unions and is clipped as the shape it is. */
  float markPx = 0.0f;

  /** Field-wise, with the classifier conservative: any classify present
   *  means "not provably the same ladder". */
  bool operator==(const Ticks& o) const {
    if (classify || o.classify) return false;
    return divisions == o.divisions && from == o.from && sweep == o.sweep &&
           closed == o.closed && mark == o.mark && longEvery == o.longEvery &&
           longMark == o.longMark && markPx == o.markPx;
  }
};

/** The ladder as an outline in the FRAME's parent space (absolute
 *  coordinates: `frame.centre` is where it says it is). */
path::Outline ticks(const path::PolarFrame& frame, const Ticks& t);

/** The ladder as general options: a `radial` with no joining and a mark
 *  at every division, cycled so every `longEvery`-th is the long one. */
path::RadialOptions radialOf(const Ticks& t, path::PolarFrame conventions = {});

/** THE LADDER AS A SHAPE VALUE, with the frame taken from the box it is
 *  asked for: centre at the box centre, radius half the SHORTER side.
 *  @p conventions therefore supplies ONLY `zero`, `sense` and
 *  `originDeg`. Comparable, so the node prunes, unless @p t carries a
 *  `classify` callable.
 *  @trap Half the shorter side keeps a ladder on an oblong box a circle
 *  rather than an ellipse `PolarFrame::fraction()` no longer matches. */
inline Radial ticks(const Ticks& t, path::PolarFrame conventions = {}) {
  return shapes::radial(t.divisions, radialOf(t, conventions));
}

// ---------------------------------------------------------------------------
// arcs — the ring's own divisions as N closed segments.

/** A RING OF CLOSED ARC SEGMENTS: N wedges of the annulus between two
 *  radii, each `spanDeg` wide, dealt round the frame the way `ticks()`
 *  deals its marks. The curved sibling of a `Ticks` carrying a `markPx`:
 *  an arc's mark follows the ring, so it fattens with radius the way a
 *  segment of a dial does. Every angle is in the FRAME's units, and
 *  `spanDeg` is a WIDTH, taking the frame's sign but not its origin.
 *  @trap A span wider than the pitch overlaps its neighbours, which a
 *  non-zero winding fill closes into a solid ring. */
struct Arcs {
  /** How many segments, dealt as `ticks()` deals marks: with a full
   *  sweep and `closed = false`, segment N would coincide with segment 0
   *  and is not emitted. */
  int divisions = 12;
  /** The frame's degrees of the first segment's CENTRE. */
  float from = 0.0f;
  /** Total span in frame degrees the divisions are dealt over. */
  float sweep = 360.0f;
  /** Emit `divisions + 1` segments — the closed ladder a scale wants and
   *  a full ring does not. */
  bool closed = false;
  /** How far in and out one segment reaches, in normalised radius. Equal
   *  radii emit nothing: a segment with no thickness is not a figure, and
   *  the degenerate contour would fill as nothing and stroke as a
   *  doubled arc. */
  Span mark{0.88f, 1.0f};
  /** Each segment's angular width, in frame degrees. */
  float spanDeg = 20.0f;

  bool operator==(const Arcs&) const = default;
};

/** The segments as an outline in the FRAME's parent space. */
path::Outline arcs(const path::PolarFrame& frame, const Arcs& a);

/** The segments as general options: a `radial` with no joining and a
 *  segment mark at every division. */
path::RadialOptions radialOf(const Arcs& a, path::PolarFrame conventions = {});

/** The ring segments @p a describes, as a shape value that takes its
 *  centre and radius from the box it is asked for — the same rule as
 *  `ticks`; @p conventions supplies only the angle zero and sense. */
inline Radial arcs(const Arcs& a, path::PolarFrame conventions = {}) {
  return shapes::radial(a.divisions, radialOf(a, conventions));
}

// ---------------------------------------------------------------------------
// chords — a polygon's sides (or a star polygon's) as N open contours.

/** THE N VERTICES OF A REGULAR N-GON on a frame, as chord endpoints,
 *  wound so that consecutive contours run the same way round. With
 *  `step = 1` and `closed = false` the sides come out as n SEPARATE OPEN
 *  contours of one path, which is the addressable-per-side form a text
 *  baseline walks as one arc-length coordinate; `shapes::polygon(n)`
 *  emits one closed contour and cannot say it. The winding, and so which
 *  way glyphs on that baseline face, comes from the frame's `sense`.
 *  @trap `inset` reaches the OPEN form alone: a closed traversal has no
 *  chord ends to trim. */
struct Chords {
  int sides = 7;
  /** 1 = the polygon's sides. 2 = a {n/2} star polygon's chords, and so
   *  on. Coprime with `sides` gives one closed traversal; otherwise it
   *  gives `gcd(sides, step)` separate rings, which is the correct
   *  {6/2} hexagram (two triangles) rather than an error. */
  int step = 1;
  /** rNorm of the vertices. */
  float radius = 1.0f;
  /** The frame's degrees of vertex 0. */
  float from = 0.0f;
  /** px trimmed off each end of every chord. */
  float inset = 0.0f;
  /** true joins the chords into closed contours (a star outline you can
   *  fill); false leaves each chord its own OPEN contour, which is the
   *  addressable-per-side form TextPath wants. */
  bool closed = false;

  bool operator==(const Chords&) const = default;
};

/** The chords @p c describes, drawn on @p frame: one open contour per
 *  side, or joined into closed contours when @p c asks. */
path::Outline chords(const path::PolarFrame& frame, const Chords& c);

/** The chords as general options: a `radial` joined chord by chord, or
 *  into loops when `closed`. */
path::RadialOptions radialOf(const Chords& c, path::PolarFrame conventions = {});

/** The same chords as a shape value that takes its centre and radius
 *  from the box it is asked for; @p conventions supplies only the angle
 *  zero and sense. */
inline Radial chords(const Chords& c, path::PolarFrame conventions = {}) {
  return shapes::radial(std::max(2, c.sides), radialOf(c, conventions));
}

}  // namespace sigil::geometry::shapes
