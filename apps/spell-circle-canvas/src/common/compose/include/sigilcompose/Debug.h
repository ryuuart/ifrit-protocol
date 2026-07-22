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

#include <include/core/SkPath.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>

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
      case SkPath::kClose_Verb: open = false; break;
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

} // namespace sigil::compose::debug
