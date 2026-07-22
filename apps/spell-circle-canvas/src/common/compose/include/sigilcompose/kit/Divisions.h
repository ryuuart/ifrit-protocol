#pragma once

/** @file
 * SigilCompose KIT — a figure's divisions as ONE path with N contours.
 *
 * Two generators, one idea: emit N marks into a single `SkPathBuilder`
 * rather than N Elements. `ticks()` walks a division count around a
 * `Frame`; `chords()` walks a polygon's sides.
 *
 * ## Why one path and not N nodes — this is the part worth reading
 *
 * The corpus does it both ways and the split is not stylistic:
 *
 * | site | marks | spelling |
 * |---|---:|---|
 * | `thaumonomicon.cpp:1319-1327` | 72 | one `SkPathBuilder`, `i % 6` long |
 * | `ds2_bench.cpp:250-262` | n | one `SkPathBuilder`, `i % 3` three lengths |
 * | `sigillum_aemeth.cpp:1928-1945` | 40 | one path, 40 contours, one trim window |
 * | `chevreul_circle.cpp:1160-1181` | 72 | **72 Elements**, each a full-diameter box |
 * | `dunhuang_star_chart.cpp:1743-1756` | 28 | **28 Elements**, each a full-diameter box |
 * | `nightingale_coxcomb.cpp:212-222` | 1 at a time | `spoke(rFrac, bearing)` per call |
 * | `chaucer_astrolabe.cpp:1975-1990` | 181 | one Element each, into an instance pool |
 *
 * The one-path form is not merely shorter. A divider ladder is static
 * geometry with one style, so as N nodes it costs N layouts, N
 * reconciliations and N pictures for a drawing that never changes — and
 * `chevreul_circle`'s 72 dividers each carry a box the full diameter of
 * the figure, which is 72 nodes whose bounds are the whole plate. As one
 * path it is one node, one picture, one stroke.
 *
 * **The one case that must stay N nodes** is per-mark animation:
 * `sigillum_aemeth`'s forty ring letters fade individually off a solver's
 * output, so they are forty keyed nodes and correctly so
 * (`EXTRACT.md §5.3`). The DIVIDERS between them are one path with forty
 * contours and one trim window, which is exactly this component. If every
 * mark shares one style and one reveal, use this; if each mark has its own
 * phase, do not.
 *
 * ## What this is NOT
 *
 * Not a linear tick ladder along an edge. `astral_tome.cpp:777-796` does
 * that in twelve lines with `brushes::ScatterBrush{.spacing = pitch,
 * .alignToPath = true}`, which is already the right tool and already
 * ships. `EXTRACT.md §5.7` rejected a tick-ladder component on that
 * evidence and it was right about the linear case. The radial case it did
 * not examine, and the radial case has six hand-rolls.
 */

#include "sigilcompose/Shapes.h"
#include "sigilcompose/kit/Frame.h"

#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>

#include <algorithm>
#include <functional>

namespace sigil::compose::kit {

/** How far in and out one mark reaches, in normalised radius. */
struct Span {
  float inner = 0.92f;
  float outer = 1.0f;
  bool operator==(const Span &) const = default;
};

/** A radial division ladder.
 *
 *      // thaumonomicon's brass rose: 72 divisions, every sixth reaching
 *      // further in. One node instead of a loop.
 *      box().rect(fig.box())
 *           .outline(kit::ticks({.divisions = 72,
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
   *  instead. 0 disables. `thaumonomicon` uses 6 of 72, `chaucer` 5 of
   *  every 2° step, `ds2_bench` 3. */
  int longEvery = 0;
  Span longMark{0.88f, 1.0f};

  /** The escape hatch, and it is needed: `ds2_bench.cpp:255-258` has
   *  THREE length classes (`i % 3` → 1.0 / 0.86 / 0.93), which no
   *  long/short pair can express. Return the span for mark @p i; the
   *  second argument is what the fields above would have given, so a
   *  classifier can defer to them.
   *
   *  **Return a degenerate span (`inner == outer`) to SKIP a mark.** That
   *  is how a ladder drawn in two passes at two stroke weights comes out
   *  right, and it is not optional: `chaucer_astrolabe.cpp:1970-1987`
   *  rules its altitude scale every 2° with every fifth heavier and
   *  longer, so the light pass must leave a hole where the heavy pass
   *  goes. Draw both and the fifths composite two translucent strokes and
   *  print darker than the plate ever did.
   *
   *  Null (the default) means "use the fields". A `std::function` here is
   *  free of reconciler consequence because this component returns an
   *  SkPath immediately — it is not stored in a node and never
   *  participates in equality. (Contrast `outline()`, whose `OutlineFn`
   *  IS stored and therefore never prunes — `ROADMAP.md §3`.) */
  std::function<Span(int i, Span fromFields)> classify;
};

/** The ladder as a path in the FRAME's parent space (absolute coordinates:
 *  `frame.centre` is where it says it is). */
inline SkPath ticks(const Frame &frame, const Ticks &t) {
  SkPathBuilder b;
  const int n = std::max(0, t.divisions);
  if (n == 0)
    return b.detach();
  const int count = t.closed ? n + 1 : n;
  const float step = n > 0 ? t.sweep / (float)n : 0.0f;
  for (int i = 0; i < count; ++i) {
    Span s = (t.longEvery > 0 && i % t.longEvery == 0) ? t.longMark : t.mark;
    if (t.classify)
      s = t.classify(i, s);
    if (s.inner == s.outer)
      continue;
    const float deg = t.from + step * (float)i;
    b.moveTo(frame.at(deg, s.inner));
    b.lineTo(frame.at(deg, s.outer));
  }
  return b.detach();
}

/** The ladder as an `OutlineFn`, with the frame taken from the node's own
 *  laid-out box: centre at the box centre, radius = half the SHORTER side.
 *
 *  Half the shorter side, not half the width, so a ladder on a non-square
 *  box stays a circle instead of silently becoming an ellipse whose
 *  `Frame::fraction()` no longer matches — the failure mode documented on
 *  that method. Give it a square box (`Frame::box()`, `util::disc`) and
 *  the question does not arise.
 *
 *  @p conventions supplies `zero`, `sense` and `originDeg`; its `centre`
 *  and `radius` are ignored and replaced. */
inline shapes::OutlineFn ticks(const Ticks &t, Frame conventions = {}) {
  return [t, conventions](SkSize size) {
    Frame f = conventions;
    f.centre = {size.width() * 0.5f, size.height() * 0.5f};
    f.radius = std::min(size.width(), size.height()) * 0.5f;
    return ticks(f, t);
  };
}

// ---------------------------------------------------------------------------
// chords — a polygon's sides (or a star polygon's) as N open contours.

/** The n vertices of a regular n-gon on a frame, as chord endpoints, wound
 *  so that consecutive contours run the same way round.
 *
 *  Three uses, two files:
 *  `sigillum_aemeth.cpp:418-436` (`heptChords`, step 1, inset, OPEN);
 *  `sigillum_aemeth.cpp:1068-1082` (the {7/2} traversal, step 2, closed);
 *  `thaumonomicon.cpp:1331-1341` (a {5/2} pentagram and a {6/2} hexagram
 *  as chord segments in one path).
 *
 *  **What it is for, and nothing else in the library does this.** With
 *  `step = 1` and `closed = false` the sides come out as *n separate open
 *  contours of one path*, and `TextPath` walks every contour of a baseline
 *  in order as ONE arc-length coordinate (`Compose.h:554-558`). So side
 *  *k*'s midpoint is at exactly `(k + 0.5) / n` of the whole run, and 49
 *  letters around a heptagon become one text node on one outline instead
 *  of seven runs a caller has to place. `shapes::polygon(n)` cannot: it
 *  emits one CLOSED contour, so a per-side coordinate does not exist.
 *
 *  The winding decides which way the glyphs face — clockwise on screen
 *  puts glyph-up radially outward, which is the engraver's convention
 *  (`Shapes.h:154-167` documents the same choice for `circle`). It comes
 *  from the frame's `sense`, so it is one field rather than an argument
 *  nobody chose.
 *
 *  @p inset shortens each chord by that many px at BOTH ends — the gap an
 *  engraver leaves at a vertex so the corner ornament reads. */
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
};

inline SkPath chords(const Frame &frame, const Chords &c) {
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
      if (seen[(size_t)start])
        continue;
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
      if (len <= 2 * c.inset)
        continue;
      const SkVector u{d.fX / len, d.fY / len};
      a = {a.fX + u.fX * c.inset, a.fY + u.fY * c.inset};
      z = {z.fX - u.fX * c.inset, z.fY - u.fY * c.inset};
    }
    b.moveTo(a);
    b.lineTo(z);
  }
  return b.detach();
}

/** `chords` as an `OutlineFn`, frame from the laid-out box — same rule as
 *  `ticks`: centre at the box centre, radius half the shorter side. */
inline shapes::OutlineFn chords(const Chords &c, Frame conventions = {}) {
  return [c, conventions](SkSize size) {
    Frame f = conventions;
    f.centre = {size.width() * 0.5f, size.height() * 0.5f};
    f.radius = std::min(size.width(), size.height()) * 0.5f;
    return chords(f, c);
  };
}

} // namespace sigil::compose::kit
