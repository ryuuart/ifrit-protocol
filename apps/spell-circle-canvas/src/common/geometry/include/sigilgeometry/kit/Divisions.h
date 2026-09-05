#pragma once

/** @file
 * A figure's divisions as ONE path with N contours.
 *
 * Three generators, one idea: emit N marks into a single `SkPathBuilder`
 * rather than N drawn things. `ticks()` walks a division count around a
 * `Frame`; `arcs()` walks the same count as CLOSED segments of the ring
 * itself; `chords()` walks a polygon's sides.
 *
 * ## Why one path and not N of them
 *
 * A divider ladder is static geometry with one style. As N drawn things it
 * costs N of everything a consumer spends per item, for a drawing that
 * never changes — and a radial mark's box is usually the full diameter of
 * the figure, so those are N items whose bounds are the whole plate. As
 * one path it is one recording, one stroke, and one arc-length coordinate
 * over all the marks.
 *
 * **The one case that must stay N nodes is per-mark animation.** Marks
 * that fade or move individually need their own keyed nodes, because one
 * path has one style and one reveal window. If every mark shares those,
 * use this; if each mark has its own phase, do not.
 *
 * ## What this is not
 *
 * Not a linear tick ladder along an edge: a consumer that stamps marks at
 * a pitch along any path already has the straight case. These two are the
 * radial and polygonal cases, where the mark positions come from an angle
 * convention rather than an arc length.
 */

#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Frame.h>

#include <algorithm>
#include <functional>

namespace sigil::geometry::shapes {

/** How far in and out one mark reaches, in normalised radius. */
struct Span {
  float inner = 0.92f;
  float outer = 1.0f;
  bool operator==(const Span&) const = default;
};

/** A radial division ladder.
 *
 *      // 72 divisions, every sixth reaching further in. One node
 *      // instead of a loop.
 *      box().rect(fig.box())
 *           .shape(shapes::ticks({.divisions = 72,
 *                                .mark = {.inner = 0.96f, .outer = 1.0f},
 *                                .longEvery = 6,
 *                                .longMark = {.inner = 0.91f, .outer = 1.0f}}))
 *           .stroke(stroke(1.0f, Fill::color(brass)));
 *
 *  Every angle is in the FRAME's units — `.from = 0` on a North/CW frame
 *  is 12 o'clock, on an East/CW frame it is 3 o'clock. That is the whole
 *  reason `Frame` exists; see `kit/Frame.h`. */
struct Ticks {
  /** How many marks. With `sweep = 360` and `closed = false` this is the
   *  division count and mark N would coincide with mark 0, so it is not
   *  emitted. */
  int divisions = 60;
  /** Frame degrees of the first mark. */
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

  /** The escape hatch, for a ladder with more than two length classes —
   *  three alternating lengths, say, which no long/short pair expresses.
   *  Return the span for mark @p i; the second argument is what the fields
   *  above would have given, so a classifier can defer to them.
   *
   *  **Return a degenerate span (`inner == outer`) to SKIP a mark.** That
   *  is the only way to skip one, and it is what a ladder drawn in two
   *  passes at two stroke weights needs: the light pass must leave a hole
   *  where the heavy pass goes, or both passes stack on the same mark and
   *  it prints darker than either weight.
   *
   *  Null (the default) means "use the fields". A `std::function` here has
   *  no reconciler consequence through the SkPath overload — that path is
   *  built immediately and never stored. Through the SHAPE overload it is
   *  the one member equality cannot see, so a Ticks carrying a classifier
   *  compares unequal to EVERYTHING, including a copy of itself: its node
   *  never prunes and re-records on every describe. */
  std::function<Span(int i, Span fromFields)> classify;

  /** HOW WIDE ONE MARK IS, in px across the radius. Zero — the default —
   *  emits the open radial LINE a ladder is stroked from; anything else
   *  emits a CLOSED rectangle standing on the same radius, which is the
   *  node, the lozenge and the bar of a ring that is filled rather than
   *  stroked.
   *
   *  The difference is not a stroke width by another name. A stroked line
   *  is a mark the paint decides the weight of; a closed mark is
   *  GEOMETRY, so it fills, it takes a gradient across its own width, it
   *  unions with its neighbours, and the ring it belongs to can be
   *  clipped, offset or measured as the shape it is. Px rather than
   *  degrees because a node ring reads as marks of one size, not as
   *  wedges that fatten with radius — the wedge is `arcs()`. */
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

/** The ladder as a path in the FRAME's parent space (absolute coordinates:
 *  `frame.centre` is where it says it is). */
inline SkPath ticks(const path::Frame& frame, const Ticks& t) {
  SkPathBuilder b;
  const int n = std::max(0, t.divisions);
  if (n == 0) return b.detach();
  const int count = t.closed ? n + 1 : n;
  const float step = n > 0 ? t.sweep / (float)n : 0.0f;
  for (int i = 0; i < count; ++i) {
    Span s = (t.longEvery > 0 && i % t.longEvery == 0) ? t.longMark : t.mark;
    if (t.classify) s = t.classify(i, s);
    if (s.inner == s.outer) continue;
    const float deg = t.from + step * (float)i;
    const SkPoint inner = frame.at(deg, s.inner);
    const SkPoint outer = frame.at(deg, s.outer);
    if (t.markPx <= 0.0f) {
      b.moveTo(inner);
      b.lineTo(outer);
      continue;
    }
    // A closed mark: the same radial run, given a width across it. The
    // offset is perpendicular to the frame's own outward direction, so a
    // mark stands square to its radius whatever the frame's conventions
    // are.
    const SkVector out = frame.dir(deg);
    const SkVector across{-out.fY * t.markPx * 0.5f, out.fX * t.markPx * 0.5f};
    b.moveTo(inner.fX + across.fX, inner.fY + across.fY);
    b.lineTo(outer.fX + across.fX, outer.fY + across.fY);
    b.lineTo(outer.fX - across.fX, outer.fY - across.fY);
    b.lineTo(inner.fX - across.fX, inner.fY - across.fY);
    b.close();
  }
  return b.detach();
}

/** The ladder as a SHAPE VALUE, with the frame taken from the node's own
 *  laid-out box: centre at the box centre, radius = half the SHORTER side.
 *
 *  `conventions` therefore supplies ONLY `zero`, `sense` and `originDeg`.
 *  Its `centre` and `radius` are overwritten, so a frame passed here does
 *  not place the ladder — the node's box does.
 *
 *  Half the shorter side, not half the width, so a ladder on a non-square
 *  box stays a circle instead of silently becoming an ellipse whose
 *  `Frame::fraction()` no longer matches. Give it a square box
 *  (`Frame::box()`, `kit::disc`) and the question does not arise.
 *
 *  Comparable, so the node prunes — unless the Ticks carries a `classify`
 *  callable, which equality cannot see and which therefore makes the whole
 *  value compare unequal to everything. */
struct TicksShape {
  Ticks t;
  path::Frame conventions;
  bool operator==(const TicksShape&) const = default;
  SkPath path(SkSize size) const {
    path::Frame f = conventions;
    f.centre = {size.width() * 0.5f, size.height() * 0.5f};
    f.radius = std::min(size.width(), size.height()) * 0.5f;
    return ticks(f, t);
  }
  SkPath operator()(SkSize s) const { return path(s); }
};

inline TicksShape ticks(const Ticks& t, path::Frame conventions = {}) {
  return TicksShape{t, conventions};
}

// ---------------------------------------------------------------------------
// arcs — the ring's own divisions as N closed segments.

/** A ring of CLOSED ARC SEGMENTS: N wedges of the annulus between two
 *  radii, each `spanDeg` wide, dealt round the frame the way `ticks()`
 *  deals its marks.
 *
 *  This is the curved sibling of a `Ticks` carrying a `markPx`, and the
 *  two answer different pictures. A tick's mark is a straight bar of one
 *  width — a node, dealt round a ring, reading as marks of one size. An
 *  arc's mark follows the ring, so its edges are the ring's own arcs and
 *  it fattens with radius the way a segment of a dial does. A segmented
 *  progress ring, a fan of sectors, a broken annulus: each is one path
 *  here rather than N drawn things.
 *
 *  Every angle is in the FRAME's units, exactly as `ticks()` reads them,
 *  and `spanDeg` is a WIDTH, so it takes the frame's sign but not its
 *  origin. A span wider than the pitch makes neighbours overlap, which
 *  a fill with a non-zero winding closes into a solid ring — say the
 *  pitch, not more, unless that is the drawing. */
struct Arcs {
  /** How many segments, dealt as `ticks()` deals marks: with a full
   *  sweep and `closed = false`, segment N would coincide with segment 0
   *  and is not emitted. */
  int divisions = 12;
  /** Frame degrees of the first segment's CENTRE. */
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

/** The segments as a path in the FRAME's parent space. */
inline SkPath arcs(const path::Frame& frame, const Arcs& a) {
  SkPathBuilder b;
  const int n = std::max(0, a.divisions);
  if (n == 0 || a.spanDeg == 0.0f || a.mark.inner == a.mark.outer)
    return b.detach();
  const int count = a.closed ? n + 1 : n;
  const float step = a.sweep / (float)n;
  const SkRect outer = frame.box(a.mark.outer);
  const SkRect inner = frame.box(a.mark.inner);
  for (int i = 0; i < count; ++i) {
    const float centre = a.from + step * (float)i;
    const float start = centre - a.spanDeg * 0.5f;
    const float end = centre + a.spanDeg * 0.5f;
    // Out along the far edge, in across the end, back along the near one:
    // one contour whose two curved sides are the ring's own arcs rather
    // than a polyline that would show its facets under a stroke.
    b.arcTo(outer, frame.skiaDeg(start), frame.skiaSweep(a.spanDeg), true);
    b.lineTo(frame.at(end, a.mark.inner));
    b.arcTo(inner, frame.skiaDeg(end), frame.skiaSweep(-a.spanDeg), false);
    b.close();
  }
  return b.detach();
}

/** `arcs` as a SHAPE VALUE, frame from the laid-out box — the same rule
 *  as `ticks` and `chords`: centre at the box centre, radius half the
 *  shorter side, and the `conventions` frame's own centre and radius
 *  ignored. Fully comparable, since `Arcs` has no callable member. */
struct ArcsShape {
  Arcs a;
  path::Frame conventions;
  bool operator==(const ArcsShape&) const = default;
  SkPath path(SkSize size) const {
    path::Frame f = conventions;
    f.centre = {size.width() * 0.5f, size.height() * 0.5f};
    f.radius = std::min(size.width(), size.height()) * 0.5f;
    return arcs(f, a);
  }
  SkPath operator()(SkSize s) const { return path(s); }
};

inline ArcsShape arcs(const Arcs& a, path::Frame conventions = {}) {
  return ArcsShape{a, conventions};
}

// ---------------------------------------------------------------------------
// chords — a polygon's sides (or a star polygon's) as N open contours.

/** The n vertices of a regular n-gon on a frame, as chord endpoints, wound
 *  so that consecutive contours run the same way round.
 *
 *  **What it is for, and nothing else in the library does this.** With
 *  `step = 1` and `closed = false` the sides come out as *n separate open
 *  contours of one path*, and `TextPath` walks every contour of a baseline
 *  in order as ONE arc-length coordinate. So side *k*'s midpoint is at
 *  exactly `(k + 0.5) / n` of the whole run, and an inscription around a
 *  polygon becomes one text node on one outline instead of n runs a caller
 *  has to place. `shapes::polygon(n)` cannot do this: it emits one CLOSED
 *  contour, so a per-side coordinate does not exist.
 *
 *  The winding decides which way glyphs on that baseline face — clockwise
 *  on screen puts glyph-up radially outward, the engraver's convention,
 *  the same choice `shapes::circle` documents. It comes from the frame's
 *  `sense` rather than from an argument here.
 *
 *  @p inset shortens each chord by that many px at BOTH ends — the gap an
 *  engraver leaves at a vertex so the corner ornament reads. A chord
 *  shorter than twice the inset is dropped entirely. */
struct Chords {
  int sides = 7;
  /** 1 = the polygon's sides. 2 = a {n/2} star polygon's chords, and so
   *  on. Coprime with `sides` gives one closed traversal; otherwise it
   *  gives `gcd(sides, step)` separate rings, which is the correct
   *  {6/2} hexagram (two triangles) rather than an error. */
  int step = 1;
  /** rNorm of the vertices. */
  float radius = 1.0f;
  /** Frame degrees of vertex 0. */
  float from = 0.0f;
  /** px trimmed off each end of every chord. */
  float inset = 0.0f;
  /** true joins the chords into closed contours (a star outline you can
   *  fill); false leaves each chord its own OPEN contour, which is the
   *  addressable-per-side form TextPath wants. */
  bool closed = false;

  bool operator==(const Chords&) const = default;
};

inline SkPath chords(const path::Frame& frame, const Chords& c) {
  SkPathBuilder b;
  const int n = std::max(2, c.sides);
  const int step = std::max(1, c.step);
  const float pitch = 360.0f / (float)n;
  auto vertex = [&](int k) {
    return frame.at(c.from + pitch * (float)((k % n + n) % n), c.radius);
  };
  if (c.closed) {
    // Walk k, k+step, k+2·step … until it returns to k; repeat for every
    // ring the step generates. gcd(n, step) rings, n/gcd vertices each.
    std::vector<bool> seen((size_t)n, false);
    for (int start = 0; start < n; ++start) {
      if (seen[(size_t)start]) continue;
      int k = start;
      bool first = true;
      do {
        seen[(size_t)k] = true;
        const SkPoint p = vertex(k);
        first ? b.moveTo(p) : b.lineTo(p);
        first = false;
        k = (k + step) % n;
      } while (k != start);
      b.close();
    }
    return b.detach();
  }
  for (int k = 0; k < n; ++k) {
    SkPoint a = vertex(k), z = vertex(k + step);
    if (c.inset > 0) {
      const SkVector d{z.fX - a.fX, z.fY - a.fY};
      const float len = std::hypot(d.fX, d.fY);
      if (len <= 2 * c.inset) continue;
      const SkVector u{d.fX / len, d.fY / len};
      a = {a.fX + u.fX * c.inset, a.fY + u.fY * c.inset};
      z = {z.fX - u.fX * c.inset, z.fY - u.fY * c.inset};
    }
    b.moveTo(a);
    b.lineTo(z);
  }
  return b.detach();
}

/** `chords` as a SHAPE VALUE, frame from the laid-out box — same rule as
 *  `ticks`: centre at the box centre, radius half the shorter side, and
 *  the `conventions` frame's own centre and radius ignored. Fully
 *  comparable, since Chords has no callable member, so it always
 *  prunes. */
struct ChordsShape {
  Chords c;
  path::Frame conventions;
  bool operator==(const ChordsShape&) const = default;
  SkPath path(SkSize size) const {
    path::Frame f = conventions;
    f.centre = {size.width() * 0.5f, size.height() * 0.5f};
    f.radius = std::min(size.width(), size.height()) * 0.5f;
    return chords(f, c);
  }
  SkPath operator()(SkSize s) const { return path(s); }
};

inline ChordsShape chords(const Chords& c, path::Frame conventions = {}) {
  return ChordsShape{c, conventions};
}

}  // namespace sigil::geometry::shapes
