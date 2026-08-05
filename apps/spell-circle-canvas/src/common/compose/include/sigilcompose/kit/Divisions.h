#pragma once

/** @file
 * SigilCompose KIT — a figure's divisions as ONE path with N contours.
 *
 * Two generators, one idea: emit N marks into a single `SkPathBuilder`
 * rather than N Elements. `ticks()` walks a division count around a
 * `Frame`; `chords()` walks a polygon's sides.
 *
 * ## Why one path and not N nodes
 *
 * A divider ladder is static geometry with one style. As N nodes it costs
 * N layouts, N reconciliations and N recordings for a drawing that never
 * changes — and a radial mark's box is usually the full diameter of the
 * figure, so those are N nodes whose bounds are the whole plate. As one
 * path it is one node, one recording, one stroke, and one arc-length
 * coordinate over all the marks.
 *
 * **The one case that must stay N nodes is per-mark animation.** Marks
 * that fade or move individually need their own keyed nodes, because one
 * path has one style and one reveal window. If every mark shares those,
 * use this; if each mark has its own phase, do not.
 *
 * ## What this is not
 *
 * Not a linear tick ladder along an edge. A brush scatter
 * (`brush::Scatter{.spacing = pitch, .alignToPath = true}`) already stamps
 * marks at a pitch along any path and is the right tool for the straight
 * case. These two are the radial and polygonal cases, where the mark
 * positions come from an angle convention rather than an arc length.
 */

#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>

#include <algorithm>
#include <functional>

#include "sigilcompose/Shapes.h"
#include "sigilcompose/kit/Frame.h"

namespace sigil::compose::kit {

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
 *           .shape(kit::ticks({.divisions = 72,
 *                                .mark = {.inner = 0.96f, .outer = 1.0f},
 *                                .longEvery = 6,
 *                                .longMark = {.inner = 0.91f, .outer = 1.0f}}))
 *           .stroke(util::stroke(1.0f, Fill::color(brass)));
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

  /** Field-wise, with the classifier conservative: any classify present
   *  means "not provably the same ladder". */
  bool operator==(const Ticks& o) const {
    if (classify || o.classify) return false;
    return divisions == o.divisions && from == o.from && sweep == o.sweep &&
           closed == o.closed && mark == o.mark && longEvery == o.longEvery &&
           longMark == o.longMark;
  }
};

/** The ladder as a path in the FRAME's parent space (absolute coordinates:
 *  `frame.centre` is where it says it is). */
inline SkPath ticks(const Frame& frame, const Ticks& t) {
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
    b.moveTo(frame.at(deg, s.inner));
    b.lineTo(frame.at(deg, s.outer));
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
 *  (`Frame::box()`, `util::disc`) and the question does not arise.
 *
 *  Comparable, so the node prunes — unless the Ticks carries a `classify`
 *  callable, which equality cannot see and which therefore makes the whole
 *  value compare unequal to everything. */
struct TicksShape {
  Ticks t;
  Frame conventions;
  bool operator==(const TicksShape&) const = default;
  SkPath path(SkSize size) const {
    Frame f = conventions;
    f.centre = {size.width() * 0.5f, size.height() * 0.5f};
    f.radius = std::min(size.width(), size.height()) * 0.5f;
    return ticks(f, t);
  }
  SkPath operator()(SkSize s) const { return path(s); }
};

inline TicksShape ticks(const Ticks& t, Frame conventions = {}) {
  return TicksShape{t, conventions};
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

inline SkPath chords(const Frame& frame, const Chords& c) {
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
  Frame conventions;
  bool operator==(const ChordsShape&) const = default;
  SkPath path(SkSize size) const {
    Frame f = conventions;
    f.centre = {size.width() * 0.5f, size.height() * 0.5f};
    f.radius = std::min(size.width(), size.height()) * 0.5f;
    return chords(f, c);
  }
  SkPath operator()(SkSize s) const { return path(s); }
};

inline ChordsShape chords(const Chords& c, Frame conventions = {}) {
  return ChordsShape{c, conventions};
}

}  // namespace sigil::compose::kit
