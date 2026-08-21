#pragma once

/** @file
 * SigilCompose checks for GENERATED geometry — tilings, subdivisions,
 * lattices, pavings: the constructions whose correctness is a property of a
 * rule rather than of anything you can see at a glance.
 *
 * Why point-sampled coverage rather than something cheaper: the two obvious
 * cheap checks both PASS on a subdivision that overlaps in one place and
 * gaps in another. Total-area conservation passes because an overlap and a
 * gap of equal area cancel exactly, and containment passes because every
 * child really does lie inside its parent. Sampling the region is what
 * separates them, because it asks each point how many pieces claim it.
 *
 * This header is for tests, sketches and verification passes, not for the
 * paint loop: coverage() costs O(samples × candidate pieces), and the
 * rasterizing helpers allocate a surface per call.
 */

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

#include "sigilcompose/Compose.h"
#include "sigilcompose/Feed.h"

namespace sigil::compose::debug {

/** What a point-sampled coverage test found. A correct exact cover has
 *  `uncovered == 0 && doubled == 0`. */
struct Coverage {
  int samples = 0;    ///< points tested inside `region`
  int uncovered = 0;  ///< in the region and in NO piece — a gap
  int doubled = 0;    ///< in the region and in MORE THAN ONE — an overlap
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
inline Coverage coverage(std::span<const SkPath> pieces, const SkRect& region,
                         int grid = 128, size_t witnesses = 8) {
  Coverage out;
  if (region.isEmpty() || grid < 2) return out;

  // Bounds first: SkPath::contains is not cheap, and most pieces are
  // nowhere near most samples.
  std::vector<SkRect> bounds;
  bounds.reserve(pieces.size());
  for (const SkPath& p : pieces) bounds.push_back(p.getBounds());

  const float dx = region.width() / (float)grid;
  const float dy = region.height() / (float)grid;
  for (int gy = 0; gy < grid; ++gy) {
    const float y = region.top() + ((float)gy + 0.5f) * dy;
    for (int gx = 0; gx < grid; ++gx) {
      const float x = region.left() + ((float)gx + 0.5f) * dx;
      int hits = 0;
      for (size_t i = 0; i < pieces.size() && hits < 2; ++i) {
        if (!bounds[i].contains(x, y)) continue;
        if (pieces[i].contains(x, y)) ++hits;
      }
      ++out.samples;
      if (hits == 0) {
        ++out.uncovered;
        if (out.uncoveredAt.size() < witnesses)
          out.uncoveredAt.push_back({x, y});
      } else if (hits > 1) {
        ++out.doubled;
        if (out.doubledAt.size() < witnesses) out.doubledAt.push_back({x, y});
      }
    }
  }
  return out;
}

/** Coverage over an arbitrary REGION rather than a rect.
 *
 *  An annulus, a sector, a plate — anything whose outline is not a box —
 *  cannot be tested against `region.bounds()` without counting everything
 *  outside it as a gap.
 *
 *  Build the reference region from the SAME vertices as the pieces wherever
 *  you can. A tiling made of polylines tested against a true circle reports
 *  the chord error between them as a ring of phantom gaps, which looks
 *  exactly like a real defect. */
inline Coverage coverage(std::span<const SkPath> pieces, const SkPath& region,
                         int grid = 128, size_t witnesses = 8) {
  Coverage out;
  const SkRect box = region.getBounds();
  if (box.isEmpty() || grid < 2) return out;

  std::vector<SkRect> bounds;
  bounds.reserve(pieces.size());
  for (const SkPath& p : pieces) bounds.push_back(p.getBounds());

  const float dx = box.width() / (float)grid;
  const float dy = box.height() / (float)grid;
  for (int gy = 0; gy < grid; ++gy) {
    const float y = box.top() + ((float)gy + 0.5f) * dy;
    for (int gx = 0; gx < grid; ++gx) {
      const float x = box.left() + ((float)gx + 0.5f) * dx;
      if (!region.contains(x, y))
        continue;  // outside the region entirely — not a gap
      int hits = 0;
      for (size_t i = 0; i < pieces.size() && hits < 2; ++i) {
        if (!bounds[i].contains(x, y)) continue;
        if (pieces[i].contains(x, y)) ++hits;
      }
      ++out.samples;
      if (hits == 0) {
        ++out.uncovered;
        if (out.uncoveredAt.size() < witnesses)
          out.uncoveredAt.push_back({x, y});
      } else if (hits > 1) {
        ++out.doubled;
        if (out.doubledAt.size() < witnesses) out.doubledAt.push_back({x, y});
      }
    }
  }
  return out;
}

/** Every distinct endpoint in @p pieces, with how many pieces touch it,
 *  within @p tolerance. The chaining test for decorated tilings: when arcs
 *  drawn on the tiles are meant to link into continuous bands, every
 *  interior endpoint must have degree 2 and any other number is a break. */
struct VertexDegrees {
  std::vector<SkPoint> points;
  std::vector<int> degree;
  /** How many CLOSED contours were seen. A closed contour has no
   *  endpoints and contributes no points at all, which is why this count
   *  is reported separately: if it is nonzero and `points` is empty, the
   *  input is entirely closed and the degree test says nothing about it —
   *  a result that would otherwise read as a clean pass. */
  size_t closedContours = 0;
  /** Which merged point each piece's endpoints landed on, two per
   *  contour in order — the adjacency `components()` needs. */
  std::vector<std::pair<size_t, size_t>> edges;

  /** How many separate CONNECTED pieces the input is; 1 means it is all
   *  one piece.
   *
   *  "Is this a single piece of metal?" is what a decorated tiling, a knot
   *  or a wire graph is usually asking, and the degree list alone cannot
   *  answer it — every endpoint can have the right degree while the figure
   *  falls into two disjoint loops. This unions the `edges` adjacency to
   *  find out. */
  size_t components() const {
    std::vector<size_t> parent(points.size());
    for (size_t i = 0; i < parent.size(); ++i) parent[i] = i;
    auto find = [&](size_t i) {
      while (parent[i] != i) i = parent[i] = parent[parent[i]];
      return i;
    };
    for (const auto& e : edges) {
      const size_t a = find(e.first), b = find(e.second);
      if (a != b) parent[a] = b;
    }
    size_t roots = 0;
    for (size_t i = 0; i < parent.size(); ++i) roots += find(i) == i;
    return roots;
  }

  /** Points with a degree not in [lo, hi] — the ones to look at. */
  std::vector<size_t> outside(int lo, int hi) const {
    std::vector<size_t> bad;
    for (size_t i = 0; i < degree.size(); ++i)
      if (degree[i] < lo || degree[i] > hi) bad.push_back(i);
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
  for (const SkPath& path : pieces) {
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
            if (haveStart) out.edges.emplace_back(contourStart, end);
          }
          first = last = pts[0];
          open = true;
          contourStart = note(first);
          haveStart = true;
          break;
        case SkPath::kLine_Verb:
          last = pts[1];
          break;
        case SkPath::kQuad_Verb:
        case SkPath::kConic_Verb:
          last = pts[2];
          break;
        case SkPath::kCubic_Verb:
          last = pts[3];
          break;
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
        default:
          break;
      }
    }
    if (open) {
      const size_t end = note(last);
      if (haveStart) out.edges.emplace_back(contourStart, end);
    }
    haveStart = false;
  }
  return out;
}

// ---------------------------------------------------------------------------
// Reading back what you actually drew

/** A rasterised element tree, for checking a claim against the PIXELS
 *  rather than against the description that produced them — the strongest
 *  form a verification here can take, since it tests what was actually
 *  drawn.
 *
 *  **The default colour type is F16, and that is the non-obvious half.**
 *  An 8-bit read-back quantises a faint value to a handful of levels, so a
 *  falloff measured near the dark end returns a confidently wrong number
 *  instead of an obviously broken one. If you are measuring a RATIO,
 *  measure it in float. */
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

inline Raster rasterize(Element root, sigil::weave::FontContext& fonts,
                        SkISize size,
                        SkColorType colorType = kRGBA_F16_SkColorType,
                        SkColor4f background = {0, 0, 0, 0}) {
  Raster out;
  if (size.isEmpty()) return out;
  const SkImageInfo info = SkImageInfo::Make(size.width(), size.height(),
                                             colorType, kPremul_SkAlphaType);
  sk_sp<SkSurface> surface = SkSurfaces::Raster(info);
  if (!surface) return out;
  surface->getCanvas()->clear(background);
  // snapshot() sizes by the root's CHILDREN and ignores the root's own
  // dimensions, so the wrapper carries EXPLICIT dims and an explicit
  // canvas size: without them an `absolute().inset(0)` child resolves
  // against nothing and the read-back is of an empty surface.
  if (sk_sp<SkPicture> picture =
          snapshot(box()
                       .width((float)size.width())
                       .height((float)size.height())
                       .child(std::move(root)),
                   fonts, {(float)size.width(), (float)size.height()}))
    surface->getCanvas()->drawPicture(picture);
  out.bitmap.allocPixels(info);
  if (!surface->readPixels(out.bitmap.pixmap(), 0, 0)) out.bitmap.reset();
  return out;
}

// ---------------------------------------------------------------------------
// Saying whether it was right

/** One claim, its evidence, and its verdict.
 *
 *  The point is that the printed line is COMPUTED from the same two values
 *  it reports. A hand-formatted caption saying "RING GEOMETRY EXACT" reads
 *  identically whether the geometry is exact or not, because the sentence
 *  and the measurement are joined only by whoever typed them; here they
 *  cannot drift apart. And because the verdict is a value rather than a
 *  string, a set of them can fail a build — see `failures()`. */
struct Check {
  std::string label;
  std::string expected, actual;  ///< already formatted, for printing
  bool pass = false;

  /** `  <label padded> <actual, right-aligned>   PASS`, or
   *  `… FAIL want <expected>` — the shape of `"  %-44s %8ld   %s"`. Values
   *  right-align because a column of results is a table, and a ragged
   *  number column is hard to scan at small type.
   *
   *  The `want` clause matters: a failure that prints only the computed
   *  number says something is wrong without saying what would have been
   *  right, which a reader cannot act on.
   *
   *  Long labels are NOT truncated — they push the value column right
   *  instead. A clipped label silently loses the units or the qualifier at
   *  the end of a claim, which is worse than a line that wraps. */
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
}  // namespace detail

/** Integer identity — the conservation check, where two counts must agree
 *  exactly.
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
 *  how closely a measured value and a solved one must agree is a property
 *  of the construction being checked, and an epsilon picked here would be a
 *  claim this header is not entitled to make. */
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

/** Append a check to a feed of text rows, the row's style name chosen by the
 *  verdict.
 *
 *  The two names are parameters because the style set a plate reads is the
 *  caller's — nothing here knows what a given study calls its passing ink.
 *  The defaults are a convention; a set that registers neither name sets
 *  both rows in its base style, which is legible but says nothing. */
inline void report(feed::TextRing& ring, const Check& c,
                   std::string passStyle = "pass",
                   std::string failStyle = "fail", int labelWidth = 44,
                   int valueWidth = 8) {
  const std::string text = c.line(labelWidth, valueWidth);
  ring.append({std::u8string(text.begin(), text.end()),
               c.pass ? std::move(passStyle) : std::move(failStyle)});
}

/** How many of @p checks failed — an exit code for a verification run, and
 *  what makes the claims mean something away from the screen. */
inline int failures(std::span<const Check> checks) {
  int n = 0;
  for (const Check& c : checks) n += c.pass ? 0 : 1;
  return n;
}

}  // namespace sigil::compose::debug
