#pragma once

/** @file
 * @ingroup geometry-kit
 *
 * A figure's divisions as ONE path with N contours.
 *
 * Three generators, one idea: emit N marks into a single `SkPathBuilder`
 * rather than N drawn things. `ticks()` walks a division count around a
 * `PolarFrame`; `arcs()` walks the same count as CLOSED segments of the
 * ring itself; `chords()` walks a polygon's sides.
 */

#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Frame.h>

#include <algorithm>
#include <functional>
#include <vector>

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

/** The ladder as a path in the FRAME's parent space (absolute coordinates:
 *  `frame.centre` is where it says it is). */
inline SkPath ticks(const path::PolarFrame& frame, const Ticks& t) {
  SkPathBuilder b;
  const int n = std::max(0, t.divisions);
  if (n == 0) return b.detach();
  const int count = t.closed ? n + 1 : n;
  const float step = t.sweep / (float)n;
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

/** THE LADDER AS A SHAPE VALUE, with the frame taken from the node's own
 *  laid-out box: centre at the box centre, radius half the SHORTER side.
 *  `conventions` therefore supplies ONLY `zero`, `sense` and `originDeg`
 *  — its `centre` and `radius` are overwritten, so a frame passed here
 *  does not place the ladder. Comparable, so the node prunes, unless the
 *  Ticks carries a `classify` callable.
 *  @trap Half the shorter side keeps a ladder on an oblong box a circle
 *  rather than an ellipse `PolarFrame::fraction()` no longer matches. */
struct TicksShape {
  Ticks t;
  path::PolarFrame conventions;
  bool operator==(const TicksShape&) const = default;
  SkPath path(SkSize size) const {
    path::PolarFrame f = conventions;
    f.centre = {size.width() * 0.5f, size.height() * 0.5f};
    f.radius = std::min(size.width(), size.height()) * 0.5f;
    return ticks(f, t);
  }
  SkPath operator()(SkSize s) const { return path(s); }
};

/** The tick marks @p t describes, as a shape value that takes its
 *  centre and radius from the box it is asked for; @p conventions
 *  supplies only the angle zero and sense. */
inline TicksShape ticks(const Ticks& t, path::PolarFrame conventions = {}) {
  return TicksShape{t, conventions};
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

/** The segments as a path in the FRAME's parent space. */
inline SkPath arcs(const path::PolarFrame& frame, const Arcs& a) {
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
  path::PolarFrame conventions;
  bool operator==(const ArcsShape&) const = default;
  SkPath path(SkSize size) const {
    path::PolarFrame f = conventions;
    f.centre = {size.width() * 0.5f, size.height() * 0.5f};
    f.radius = std::min(size.width(), size.height()) * 0.5f;
    return arcs(f, a);
  }
  SkPath operator()(SkSize s) const { return path(s); }
};

/** The ring segments @p a describes, as a shape value that takes its
 *  centre and radius from the box it is asked for; @p conventions
 *  supplies only the angle zero and sense. */
inline ArcsShape arcs(const Arcs& a, path::PolarFrame conventions = {}) {
  return ArcsShape{a, conventions};
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
inline SkPath chords(const path::PolarFrame& frame, const Chords& c) {
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
  path::PolarFrame conventions;
  bool operator==(const ChordsShape&) const = default;
  SkPath path(SkSize size) const {
    path::PolarFrame f = conventions;
    f.centre = {size.width() * 0.5f, size.height() * 0.5f};
    f.radius = std::min(size.width(), size.height()) * 0.5f;
    return chords(f, c);
  }
  SkPath operator()(SkSize s) const { return path(s); }
};

/** The same chords as a shape value that takes its centre and radius
 *  from the box it is asked for; @p conventions supplies only the angle
 *  zero and sense. */
inline ChordsShape chords(const Chords& c, path::PolarFrame conventions = {}) {
  return ChordsShape{c, conventions};
}

}  // namespace sigil::geometry::shapes
