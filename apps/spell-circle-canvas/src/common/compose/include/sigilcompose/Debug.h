#pragma once

/** @file
 * SigilCompose debug assertions for GENERATED geometry.
 *
 * Studies in this library keep generating tilings, subdivisions, lattices
 * and pavings from closed-form rules, and then have no way to check the
 * rule. The Penrose paving study wrote four checks by hand — vertex angle
 * sums, arc-endpoint degree, tangent continuity, and coverage — and
 * reported the thing worth generalising:
 *
 *     the two CHEAP checks both pass on a subdivision that overlaps in
 *     one place and gaps in another.
 *
 * Area conservation passes because an overlap and a gap of equal area
 * cancel exactly. Containment passes because every child really is inside
 * its parent. Only point-sampled coverage catches it, and every agent
 * rebuilding a tiling was about to write that sampler again.
 *
 * These are for tests, sketches and `--verify` paths, not for the paint
 * loop: coverage() is O(samples × candidate pieces).
 */

#include "sigilcompose/Compose.h"
#include "sigilcompose/Console.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkPath.h>
#include <include/core/SkPicture.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <include/core/SkSurface.h>

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::compose::debug {

/** What a point-sampled coverage test found. A correct exact cover has
 *  `uncovered == 0 && doubled == 0`. */
struct Coverage {
  int samples = 0;   ///< points tested inside `region`
  int uncovered = 0; ///< in the region and in NO piece — a gap
  int doubled = 0;   ///< in the region and in MORE THAN ONE — an overlap
  /** Up to `witnesses` example points for each failure, so a caller can
   *  print or draw where it went wrong instead of just how often. */
  std::vector<SkPoint> uncoveredAt;
  std::vector<SkPoint> doubledAt;

  bool exact() const { return uncovered == 0 && doubled == 0; }
  float uncoveredFraction() const {
    return samples > 0 ? (float)uncovered / (float)samples : 0.0f;
  }
  float doubledFraction() const {
    return samples > 0 ? (float)doubled / (float)samples : 0.0f;
  }
};

/** Samples @p region on a `grid × grid` lattice and counts how many
 *  pieces contain each point.
 *
 *  Points are taken at CELL CENTRES, deliberately off the lattice a
 *  tiling is likely to be built on — sampling exactly on shared edges
 *  makes every boundary look doubled and tells you nothing. For the same
 *  reason a piece boundary passing exactly through a sample is a coin
 *  flip; raise `grid` rather than trusting a single small run.
 *
 *  @p grid of 128 is 16384 samples, which resolves a defect about
 *  1/128 of the region across. */
inline Coverage coverage(std::span<const SkPath> pieces, const SkRect &region,
                         int grid = 128, size_t witnesses = 8) {
  Coverage out;
  if (region.isEmpty() || grid < 2)
    return out;

  // Bounds first: SkPath::contains is not cheap, and most pieces are
  // nowhere near most samples.
  std::vector<SkRect> bounds;
  bounds.reserve(pieces.size());
  for (const SkPath &p : pieces)
    bounds.push_back(p.getBounds());

  const float dx = region.width() / (float)grid;
  const float dy = region.height() / (float)grid;
  for (int gy = 0; gy < grid; ++gy) {
    const float y = region.top() + ((float)gy + 0.5f) * dy;
    for (int gx = 0; gx < grid; ++gx) {
      const float x = region.left() + ((float)gx + 0.5f) * dx;
      int hits = 0;
      for (size_t i = 0; i < pieces.size() && hits < 2; ++i) {
        if (!bounds[i].contains(x, y))
          continue;
        if (pieces[i].contains(x, y))
          ++hits;
      }
      ++out.samples;
      if (hits == 0) {
        ++out.uncovered;
        if (out.uncoveredAt.size() < witnesses)
          out.uncoveredAt.push_back({x, y});
      } else if (hits > 1) {
        ++out.doubled;
        if (out.doubledAt.size() < witnesses)
          out.doubledAt.push_back({x, y});
      }
    }
  }
  return out;
}

/** Coverage over an arbitrary REGION rather than a rect.
 *
 *  An annulus, a sector, a plate — anything whose outline is not a box —
 *  cannot be tested against `region.bounds()` without counting the parts
 *  outside it as gaps. Note the reference region should be built from the
 *  SAME vertices as the pieces where it can be: testing a polyline
 *  tiling against a true circle reports the chord error as phantom gaps
 *  (it reported 62 of them, first try, on a ring of zodiac cells). */
inline Coverage coverage(std::span<const SkPath> pieces, const SkPath &region,
                         int grid = 128, size_t witnesses = 8) {
  Coverage out;
  const SkRect box = region.getBounds();
  if (box.isEmpty() || grid < 2)
    return out;

  std::vector<SkRect> bounds;
  bounds.reserve(pieces.size());
  for (const SkPath &p : pieces)
    bounds.push_back(p.getBounds());

  const float dx = box.width() / (float)grid;
  const float dy = box.height() / (float)grid;
  for (int gy = 0; gy < grid; ++gy) {
    const float y = box.top() + ((float)gy + 0.5f) * dy;
    for (int gx = 0; gx < grid; ++gx) {
      const float x = box.left() + ((float)gx + 0.5f) * dx;
      if (!region.contains(x, y))
        continue; // outside the region entirely — not a gap
      int hits = 0;
      for (size_t i = 0; i < pieces.size() && hits < 2; ++i) {
        if (!bounds[i].contains(x, y))
          continue;
        if (pieces[i].contains(x, y))
          ++hits;
      }
      ++out.samples;
      if (hits == 0) {
        ++out.uncovered;
        if (out.uncoveredAt.size() < witnesses)
          out.uncoveredAt.push_back({x, y});
      } else if (hits > 1) {
        ++out.doubled;
        if (out.doubledAt.size() < witnesses)
          out.doubledAt.push_back({x, y});
      }
    }
  }
  return out;
}

/** Every distinct endpoint in @p pieces, with how many pieces touch it,
 *  within @p tolerance. The chaining test for decorated tilings: on the
 *  Oxford Penrose paving every interior arc endpoint must have degree 2,
 *  or the stainless bands do not link up. */
struct VertexDegrees {
  std::vector<SkPoint> points;
  std::vector<int> degree;
  /** How many CLOSED contours were seen. A closed contour has no
   *  endpoints, so it contributes none — and saying so matters: a ring of
   *  72 closed sectors used to come back as 72 points of degree 1, which
   *  is neither right nor wrong, just meaningless. If this is nonzero and
   *  `points` is empty, the input is all closed and this test does not
   *  apply to it. */
  size_t closedContours = 0;
  /** Which merged point each piece's endpoints landed on, two per
   *  contour in order — the adjacency `components()` needs. */
  std::vector<std::pair<size_t, size_t>> edges;

  /** How many separate CONNECTED pieces the input is.
   *
   *  "Is this one piece of metal?" is the question a decorated tiling, a
   *  knot, a wire graph and an astrolabe rete all actually ask, and the
   *  degree list alone cannot answer it — a study hand-rolled union-find
   *  for exactly this. 1 means one piece. */
  size_t components() const {
    std::vector<size_t> parent(points.size());
    for (size_t i = 0; i < parent.size(); ++i)
      parent[i] = i;
    auto find = [&](size_t i) {
      while (parent[i] != i)
        i = parent[i] = parent[parent[i]];
      return i;
    };
    for (const auto &e : edges) {
      const size_t a = find(e.first), b = find(e.second);
      if (a != b)
        parent[a] = b;
    }
    size_t roots = 0;
    for (size_t i = 0; i < parent.size(); ++i)
      roots += find(i) == i;
    return roots;
  }

  /** Points with a degree not in [lo, hi] — the ones to look at. */
  std::vector<size_t> outside(int lo, int hi) const {
    std::vector<size_t> bad;
    for (size_t i = 0; i < degree.size(); ++i)
      if (degree[i] < lo || degree[i] > hi)
        bad.push_back(i);
    return bad;
  }
};

/** Collects the first and last point of every contour of every piece and
 *  merges those within @p tolerance. O(endpoints²) — fine for the
 *  thousands a tiling produces, not for a mesh. */
inline VertexDegrees endpointDegrees(std::span<const SkPath> pieces,
                                     float tolerance = 0.05f) {
  VertexDegrees out;
  auto note = [&](SkPoint p) -> size_t {
    for (size_t i = 0; i < out.points.size(); ++i)
      if (SkPoint::Distance(out.points[i], p) <= tolerance) {
        ++out.degree[i];
        return i;
      }
    out.points.push_back(p);
    out.degree.push_back(1);
    return out.points.size() - 1;
  };
  size_t contourStart = 0;
  bool haveStart = false;
  for (const SkPath &path : pieces) {
    SkPath::Iter iter(path, false);
    SkPoint pts[4];
    SkPoint first{0, 0}, last{0, 0};
    bool open = false;
    for (SkPath::Verb verb = iter.next(pts); verb != SkPath::kDone_Verb;
         verb = iter.next(pts)) {
      switch (verb) {
      case SkPath::kMove_Verb:
        if (open) {
          const size_t end = note(last);
          if (haveStart)
            out.edges.emplace_back(contourStart, end);
        }
        first = last = pts[0];
        open = true;
        contourStart = note(first);
        haveStart = true;
        break;
      case SkPath::kLine_Verb: last = pts[1]; break;
      case SkPath::kQuad_Verb:
      case SkPath::kConic_Verb: last = pts[2]; break;
      case SkPath::kCubic_Verb: last = pts[3]; break;
      case SkPath::kClose_Verb:
        // A closed contour has NO endpoints. Retract the point noted at
        // its moveTo rather than leaving a phantom degree-1 vertex.
        if (open && haveStart) {
          if (--out.degree[contourStart] == 0 &&
              contourStart + 1 == out.points.size()) {
            out.points.pop_back();
            out.degree.pop_back();
          }
          ++out.closedContours;
        }
        open = false;
        haveStart = false;
        break;
      default: break;
      }
    }
    if (open) {
      const size_t end = note(last);
      if (haveStart)
        out.edges.emplace_back(contourStart, end);
    }
    haveStart = false;
  }
  return out;
}

// ---------------------------------------------------------------------------
// Reading back what you actually drew

/** A rasterised element tree, for measuring your own output.
 *
 *  Three studies hand-rolled the same forty lines — `SkSurfaces::Raster`
 *  plus `snapshot()` plus `readPixels` — to check a claim against the
 *  pixels rather than against the description that produced them. That is
 *  the strongest shape a verification can take, and nothing in `Debug.h`
 *  supported it: everything here was path-level.
 *
 *  **The default colour type is F16, and that is the non-obvious half.**
 *  A slit-scan study measuring an intensity falloff found its outer
 *  streak at 1/120 of the apex, which N32 quantises to two levels — an
 *  8-bit read-back would have produced a confident wrong exponent rather
 *  than an obviously broken one. If you are measuring a RATIO, measure it
 *  in float. */
struct Raster {
  SkBitmap bitmap;

  bool valid() const { return !bitmap.isNull(); }
  int width() const { return bitmap.width(); }
  int height() const { return bitmap.height(); }
  /** Unpremultiplied linear-ish read. Out of bounds is transparent. */
  SkColor4f at(int x, int y) const {
    if (x < 0 || y < 0 || x >= bitmap.width() || y >= bitmap.height())
      return {0, 0, 0, 0};
    return bitmap.getColor4f(x, y);
  }
};

inline Raster rasterize(Element root, sigil::weave::FontContext &fonts,
                        SkISize size,
                        SkColorType colorType = kRGBA_F16_SkColorType,
                        SkColor4f background = {0, 0, 0, 0}) {
  Raster out;
  if (size.isEmpty())
    return out;
  const SkImageInfo info = SkImageInfo::Make(
      size.width(), size.height(), colorType, kPremul_SkAlphaType);
  sk_sp<SkSurface> surface = SkSurfaces::Raster(info);
  if (!surface)
    return out;
  surface->getCanvas()->clear(background);
  // snapshot() sizes by the root's CHILDREN, not the root's own dims —
  // the trap that produced a silently wrong exponent for one study. The
  // wrapper therefore carries EXPLICIT dims, so an `absolute().inset(0)`
  // child fills the surface instead of resolving against nothing.
  if (sk_sp<SkPicture> picture =
          snapshot(box()
                       .width((float)size.width())
                       .height((float)size.height())
                       .child(std::move(root)),
                   fonts, {(float)size.width(), (float)size.height()}))
    surface->getCanvas()->drawPicture(picture);
  out.bitmap.allocPixels(info);
  if (!surface->readPixels(out.bitmap.pixmap(), 0, 0))
    out.bitmap.reset();
  return out;
}

// ---------------------------------------------------------------------------
// Saying whether it was right — the half that was missing

/** One claim, its evidence, and its verdict.
 *
 *  Every proving plate in this corpus proves itself on screen, and **not one
 *  of them can be falsified by its own output**. `sigillum_aemeth` calls
 *  `debug::coverage` twice and then hand-formats a sentence about the
 *  result; `thunder_fulu` calls no `debug::` at all. Between them they make
 *  53 `fmt()` calls producing strings whose truth is not connected to the
 *  assertion — a plate that reads "RING GEOMETRY  EXACT" reads exactly the
 *  same whether it is or not.
 *
 *  `minard_1869.cpp:2580` independently invented the fix as a lambda, and
 *  that is the whole idea: the library supplied the MEASUREMENT and nothing
 *  supplied the REPORTING, so the two were joined by hand at every site and
 *  could drift apart at any of them. Here the printed verdict is computed
 *  from the same two values it prints.
 *
 *  The line saving is small (~130 lines over 5 studies). The point is that a
 *  study's claim stops being a sentence that happens to read well and
 *  becomes a value you can fail a build on — see `failures()`. */
struct Check {
  std::string label;
  std::string expected, actual; ///< already formatted, for printing
  bool pass = false;

  /** `  <label padded> <actual, right-aligned>   PASS`, or
   *  `… FAIL want <expected>` — `"  %-44s %8ld   %s"`, which is the format
   *  `minard_1869.cpp:2580` snprintf'd by hand, plus the half it could not
   *  print. Values right-align because a proving plate is a table and a
   *  ragged number column is unreadable at 8 pt.
   *
   *  The `want` clause is the point. Minard's version prints the computed
   *  number and the word DIFF, and a reader cannot act on that: it says
   *  something is wrong and not what would have been right.
   *
   *  Long labels are NOT truncated. `sigillum_aemeth.cpp:1719` documents
   *  losing four checks' units and one closing paren to a console column
   *  that silently clipped at 46 characters; a proving plate that hides
   *  half of a claim is worse than one that wraps. */
  std::string line(int labelWidth = 44, int valueWidth = 8) const {
    std::string out = "  " + label;
    if ((int)label.size() < labelWidth)
      out.append((size_t)labelWidth - label.size(), ' ');
    out += ' ';
    if ((int)actual.size() < valueWidth)
      out.append((size_t)valueWidth - actual.size(), ' ');
    out += actual;
    out += pass ? "   PASS" : "   FAIL want " + expected;
    return out;
  }
};

namespace detail {
inline std::string fmtLong(long v) {
  char buf[32];
  std::snprintf(buf, sizeof buf, "%ld", v);
  return buf;
}
inline std::string fmtDouble(double v) {
  char buf[48];
  std::snprintf(buf, sizeof buf, "%.6g", v);
  return buf;
}
} // namespace detail

/** Integer identity — the conservation check (`422,000 − 22,000 == 400,000`
 *  on Minard's own engraved numbers).
 *
 *  Constrained to integral types on purpose. A plain `long` parameter would
 *  swallow `check("r", 257.972, measured)` through an implicit truncation
 *  and report EXACT on two numbers that differ; requiring integers makes
 *  that a compile error and sends you to the tolerance overload, which is
 *  the only correct way to compare floats. */
template <std::integral T, std::integral U>
Check check(std::string label, T expected, U actual) {
  return {std::move(label), detail::fmtLong((long)expected),
          detail::fmtLong((long)actual), expected == actual};
}

/** Float agreement within @p tol. There is no default tolerance on purpose:
 *  a measured radius and a solved one agree to a number the study chose, and
 *  a library-chosen epsilon would be a claim the library is not entitled to
 *  make. */
inline Check check(std::string label, double expected, double actual,
                   double tol) {
  Check c{std::move(label), detail::fmtDouble(expected),
          detail::fmtDouble(actual), std::fabs(expected - actual) <= tol};
  c.expected += " \xc2\xb1 " + detail::fmtDouble(tol);
  return c;
}

inline Check check(std::string label, std::string_view expected,
                   std::string_view actual) {
  return {std::move(label), std::string(expected), std::string(actual),
          expected == actual};
}

/** The bare assertion, for a claim with no two numbers to compare
 *  ("every interior arc endpoint has degree 2"). */
inline Check check(std::string label, bool condition) {
  return {std::move(label), "true", condition ? "true" : "false", condition};
}

/** Append a check to a console ring, palette index chosen by the verdict.
 *
 *  The indices are parameters because the six hand-built plates do not agree
 *  on their palettes — `minard_1869` reads {dim, pass, fail, …} and
 *  `thunder_fulu` reads {dim, heading, pass, number, fail}. The defaults are
 *  minard's, being the plate that invented this. */
inline void report(console::LineRing &ring, const Check &c,
                   size_t passPalette = 1, size_t failPalette = 2,
                   int labelWidth = 44, int valueWidth = 8) {
  const std::string text = c.line(labelWidth, valueWidth);
  ring.append(std::u8string(text.begin(), text.end()),
              c.pass ? passPalette : failPalette);
}

/** How many of @p checks failed — a `--verify` exit code, and the thing that
 *  makes the plate's claims mean something off the screen. */
inline int failures(std::span<const Check> checks) {
  int n = 0;
  for (const Check &c : checks)
    n += c.pass ? 0 : 1;
  return n;
}

} // namespace sigil::compose::debug
