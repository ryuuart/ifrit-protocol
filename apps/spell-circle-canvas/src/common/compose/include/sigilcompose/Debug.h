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

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkPath.h>
#include <include/core/SkPicture.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <include/core/SkSurface.h>

#include <algorithm>
#include <span>
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

} // namespace sigil::compose::debug
