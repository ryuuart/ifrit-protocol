/** @file
 * The region, the three spreads, and the relaxation that turns any of
 * them into blue noise.
 */
#include "sigilgeometry/path/Scatter.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <glm/ext/vector_int2.hpp>
#include <glm/geometric.hpp>
#include <numbers>
#include <utility>

#include "sigilgeometry/path/Neighbours.h"

namespace sigil::geometry::path {
namespace {

using core::chance::Stream;

/** How many draws a rejection loop may make per point it wants before it
 *  gives up on the region. A region far smaller than its own bounding box
 *  — a thin diagonal sliver, a hairline ring — would otherwise never
 *  finish, and answering fewer points than asked for is a better failure
 *  than not answering. */
constexpr int kAttemptsPerPoint = 64;

/** Bridson's constant: how many candidates are thrown around an active
 *  point before it is retired. Thirty is the number the algorithm is
 *  written with, and below about twenty the packing visibly loosens. */
constexpr int kPoissonCandidates = 30;

/** THE REGION'S CONTAINMENT TEST, BUILT ONCE FOR A RUN OF QUERIES.
 *
 *  `Region::contains` walks every edge of every ring, which is the right
 *  answer for one question and the wrong shape for a hundred thousand of
 *  them: filling an outline asks the same shape about point after point,
 *  and the walk is what a fill spends all of its time in. The edges are
 *  therefore bucketed by SCANLINE ROW — each edge listed in every row its
 *  y range crosses — so a query crosses only the edges that could possibly
 *  cross it. The rule is the same even-odd rule `containsEvenOdd`
 *  answers, and the two agree point for point; only the number of edges
 *  looked at differs.
 *
 *  It is not a `Region` member because a region is a plain value that
 *  compares and copies, and this is a table built from one. */
class Inside {
 public:
  /** Held BY REFERENCE, so the region has to outlive the table. */
  Inside(Region&&) = delete;
  explicit Inside(const Region& region) : m_region(&region) {
    const SkRect box = region.bounds();
    size_t edges = 0;
    for (const Polyline& ring : region.rings)
      if (ring.points.size() >= 3) edges += ring.points.size();
    if (edges < 64 || box.isEmpty() || !(box.height() > 0)) return;

    // About four edges per row: fewer rows and a query walks a crowd, more
    // and an edge is listed in rows it barely touches.
    m_rows = (int)std::clamp<size_t>(edges / 4, 1, 8192);
    m_top = box.fTop;
    m_rowsPerUnit = (float)m_rows / box.height();
    m_buckets.resize((size_t)m_rows);
    for (const Polyline& ring : region.rings) {
      if (ring.points.size() < 3) continue;
      for (size_t i = 0; i < ring.points.size(); ++i) {
        const glm::vec2 from = ring.points[i];
        const glm::vec2 to = ring.points[(i + 1) % ring.points.size()];
        if (from.y == to.y) continue;  // horizontal edges cross no ray
        const int lo = row(std::min(from.y, to.y));
        const int hi = row(std::max(from.y, to.y));
        for (int at = lo; at <= hi; ++at)
          m_buckets[(size_t)at].push_back({from, to});
      }
    }
  }

  [[nodiscard]] bool operator()(glm::vec2 point) const {
    if (m_buckets.empty()) return m_region->contains(point);
    bool inside = false;
    for (const Edge& edge : m_buckets[(size_t)row(point.y)]) {
      const bool straddles = (edge.from.y > point.y) != (edge.to.y > point.y);
      if (!straddles) continue;
      const float t = (point.y - edge.from.y) / (edge.to.y - edge.from.y);
      if (point.x < edge.from.x + t * (edge.to.x - edge.from.x))
        inside = !inside;
    }
    return inside;
  }

 private:
  struct Edge {
    glm::vec2 from;
    glm::vec2 to;
  };
  [[nodiscard]] int row(float y) const {
    return std::clamp((int)((y - m_top) * m_rowsPerUnit), 0, m_rows - 1);
  }

  const Region* m_region;
  std::vector<std::vector<Edge>> m_buckets;
  int m_rows = 0;
  float m_top = 0;
  float m_rowsPerUnit = 0;
};

/** The next base a Halton stream may take for a second axis: the smallest
 *  prime above the one given. Two Halton sequences share structure unless
 *  their bases are co-prime, and consecutive primes are the pair every
 *  implementation of it uses.
 *
 *  Above the table the answer is the next number co-prime to the base,
 *  which still fills the rectangle rather than a line, but is not itself
 *  prime and so is a worse sequence than any base inside the table. */
uint32_t nextBase(uint32_t base) {
  static constexpr uint32_t kPrimes[] = {
      2,   3,   5,   7,   11,  13,  17,  19,  23,  29,  31,  37,
      41,  43,  47,  53,  59,  61,  67,  71,  73,  79,  83,  89,
      97,  101, 103, 107, 109, 113, 127, 131, 137, 139, 149, 151,
      157, 163, 167, 173, 179, 181, 191, 193, 197, 199};
  for (const uint32_t prime : kPrimes)
    if (prime > base) return prime;
  return base % 2 == 0 ? base + 1 : base + 2;
}

/** THE TWO STREAMS A PLANAR SCATTER DRAWS FROM, one per axis.
 *
 *  Two axes cannot share one run of words: every point would land on the
 *  diagonal. A mixer's second axis is therefore the same source seeded
 *  through one avalanche of the first seed. A Halton stream instead needs
 *  a DIMENSION per axis, which is a second base co-prime to the first, and
 *  that is what makes a Halton scatter fill a rectangle evenly rather than
 *  along a line. The other sequence sources carry only one dimension, so
 *  their second axis is the same sequence entered at a different term —
 *  even on each axis, and correlated between them. */
std::pair<Stream, Stream> axes(const Distribution& distribution) {
  if (distribution.source == core::chance::Source::Halton) {
    const uint32_t base =
        distribution.parameter < 2 ? 2u : distribution.parameter;
    return {Stream::of(distribution.source, distribution.seed, base),
            Stream::of(distribution.source, distribution.seed, nextBase(base))};
  }
  return {
      Stream::of(distribution.source, distribution.seed,
                 distribution.parameter),
      Stream::of(distribution.source, distribution.seed ^ 0x9e3779b97f4a7c15ull,
                 distribution.parameter)};
}

/** The count a distribution asks for, from whichever number it holds
 *  fixed. A spacing becomes a count through the area a square of that
 *  side covers, which is the same conversion in both directions. */
int wantedCount(const Distribution& distribution, float area) {
  double count = 0;
  switch (distribution.rate) {
    case Rate::Count:
      count = distribution.amount;
      break;
    case Rate::Density:
      count = (double)distribution.amount * (double)area;
      break;
    case Rate::Spacing: {
      const double spacing = distribution.amount;
      count = spacing > 0 ? (double)area / (spacing * spacing) : 0;
      break;
    }
  }
  const double capped =
      std::min(count, (double)std::max(distribution.maxPoints, 0));
  return capped > 0 ? (int)capped : 0;
}

/** The spacing a distribution asks for. A count becomes a spacing through
 *  the same conversion read the other way: the side of the square each
 *  point would own if they shared the area equally. */
float wantedSpacing(const Distribution& distribution, float area) {
  switch (distribution.rate) {
    case Rate::Spacing:
      return distribution.amount;
    case Rate::Density:
      return distribution.amount > 0 ? 1.0f / std::sqrt(distribution.amount)
                                     : 0.0f;
    case Rate::Count:
      break;
  }
  const float count = distribution.amount;
  if (!(count > 0) || !(area > 0)) return 0;
  return std::sqrt(area / count);
}

std::vector<glm::vec2> randomPoints(const Region& region, const Inside& inside,
                                    const Distribution& distribution,
                                    int count) {
  const SkRect box = region.bounds();
  std::vector<glm::vec2> points;
  if (count <= 0 || box.isEmpty()) return points;
  points.reserve((size_t)count);
  auto [streamX, streamY] = axes(distribution);
  const long long budget = (long long)count * kAttemptsPerPoint;
  for (long long attempt = 0; attempt < budget && (int)points.size() < count;
       ++attempt) {
    const glm::vec2 candidate{streamX.range(box.fLeft, box.fRight),
                              streamY.range(box.fTop, box.fBottom)};
    if (inside(candidate)) points.push_back(candidate);
  }
  return points;
}

std::vector<glm::vec2> latticePoints(const Region& region, const Inside& inside,
                                     const Distribution& distribution,
                                     float spacing, int cap) {
  std::vector<glm::vec2> points;
  const SkRect box = region.bounds();
  if (!(spacing > 0) || box.isEmpty() || cap <= 0) return points;

  // EVERY POINT SITS AT THE CENTRE OF A CELL IT OWNS, and the cells cover
  // the bounding box: a hundred-unit side at a pitch of ten is ten cells,
  // not eleven points with the two outermost sitting exactly on the
  // boundary where inside and outside are the same place. It is also what
  // makes the count of a lattice over a rect exactly the count a spacing
  // asks for.
  const int columns = std::max((int)std::floor(box.width() / spacing), 1);
  const int rows = std::max((int)std::floor(box.height() / spacing), 1);
  // The WORK is bounded, not the answer: a thin diagonal sliver has far
  // more cells in its bounding box than points in the shape, and giving
  // up on it entirely would answer nothing where fewer points is the
  // right answer. Cells are counted as they are visited and the walk
  // stops when it has looked at as many as the cap allows attempts for.
  const double budget = (double)cap * (double)kAttemptsPerPoint;
  double visited = 0;

  // Whatever the cells do not cover is split between the two margins, so a
  // side that is not a whole multiple of the pitch is centred rather than
  // left short on one edge.
  const float insetX = (box.width() - (float)columns * spacing) * 0.5f;
  const float insetY = (box.height() - (float)rows * spacing) * 0.5f;
  const float jitter = std::clamp(distribution.jitter, 0.0f, 1.0f);
  auto [streamX, streamY] = axes(distribution);
  for (int row = 0; row < rows; ++row)
    for (int column = 0; column < columns; ++column) {
      glm::vec2 candidate{box.fLeft + insetX + ((float)column + 0.5f) * spacing,
                          box.fTop + insetY + ((float)row + 0.5f) * spacing};
      if (jitter > 0) {
        // Both draws are taken whatever the candidate turns out to be, so
        // the lattice a seed answers does not depend on which cells the
        // region happened to keep.
        candidate.x += streamX.range(-0.5f, 0.5f) * jitter * spacing;
        candidate.y += streamY.range(-0.5f, 0.5f) * jitter * spacing;
      }
      if ((int)points.size() >= cap || ++visited > budget) return points;
      if (inside(candidate)) points.push_back(candidate);
    }
  return points;
}

/** Bridson's poisson-disc fill: a candidate is accepted only where no
 *  placed point is within the radius, and candidates are thrown in the
 *  annulus around points already placed, so the front grows outward
 *  instead of the whole box being darted at.
 *
 *  The background grid here is its own, and deliberately not a
 *  `Neighbours`: at a cell of radius / sqrt(2) at most one point can fall
 *  in a cell, which makes the separation test a fixed read of twenty-five
 *  cells — and, more to the point, an accepted point must be visible to
 *  the very next test, which a snapshot index is by contract not. */
std::vector<glm::vec2> poissonPoints(const Region& region, const Inside& inside,
                                     const Distribution& distribution,
                                     float radius, int cap) {
  std::vector<glm::vec2> points;
  const SkRect box = region.bounds();
  if (!(radius > 0) || box.isEmpty() || cap <= 0) return points;

  const float cell = radius / std::numbers::sqrt2_v<float>;
  const int columns = (int)std::floor(box.width() / cell) + 1;
  const int rows = (int)std::floor(box.height() / cell) + 1;
  if ((double)columns * (double)rows > 4.0e8) return points;
  std::vector<int> occupant((size_t)columns * (size_t)rows, -1);

  Stream stream = Stream::of(distribution.source, distribution.seed,
                             distribution.parameter);
  auto cellOf = [&](glm::vec2 p) {
    return glm::ivec2{
        std::clamp((int)((p.x - box.fLeft) / cell), 0, columns - 1),
        std::clamp((int)((p.y - box.fTop) / cell), 0, rows - 1)};
  };
  auto free = [&](glm::vec2 candidate) {
    const glm::ivec2 at = cellOf(candidate);
    for (int y = std::max(at.y - 2, 0); y <= std::min(at.y + 2, rows - 1); ++y)
      for (int x = std::max(at.x - 2, 0); x <= std::min(at.x + 2, columns - 1);
           ++x) {
        const int index = occupant[(size_t)y * (size_t)columns + (size_t)x];
        if (index < 0) continue;
        if (glm::length(points[(size_t)index] - candidate) < radius)
          return false;
      }
    return true;
  };
  auto place = [&](glm::vec2 point) {
    const glm::ivec2 at = cellOf(point);
    occupant[(size_t)at.y * (size_t)columns + (size_t)at.x] =
        (int)points.size();
    points.push_back(point);
  };

  // The seed point: the first draw that lands inside the region.
  const long long seedBudget = (long long)kAttemptsPerPoint * 64;
  for (long long attempt = 0; attempt < seedBudget; ++attempt) {
    const glm::vec2 candidate{stream.range(box.fLeft, box.fRight),
                              stream.range(box.fTop, box.fBottom)};
    if (inside(candidate)) {
      place(candidate);
      break;
    }
  }
  if (points.empty()) return points;

  std::vector<int> active{0};
  while (!active.empty() && (int)points.size() < cap) {
    const size_t pick = stream.below(active.size());
    const glm::vec2 from = points[(size_t)active[pick]];
    bool grew = false;
    for (int candidateIndex = 0; candidateIndex < kPoissonCandidates;
         ++candidateIndex) {
      const float angle = stream.range(0.0f, 2.0f * std::numbers::pi_v<float>);
      // Uniform over the annulus between one and two radii: the square
      // root is what stops the candidates crowding the inner edge.
      const float distance = radius * std::sqrt(stream.range(1.0f, 4.0f));
      const glm::vec2 candidate{from.x + distance * std::cos(angle),
                                from.y + distance * std::sin(angle)};
      if (!box.contains(candidate.x, candidate.y)) continue;
      if (!inside(candidate)) continue;
      if (!free(candidate)) continue;
      active.push_back((int)points.size());
      place(candidate);
      grew = true;
      break;
    }
    if (!grew) active.erase(active.begin() + (long)pick);
  }
  return points;
}

}  // namespace

// ---------------------------------------------------------------------------
// Region

Region Region::of(const SkPath& path, float tolerance) {
  return Region{flatten(path, tolerance)};
}

Region Region::of(SkRect rect) {
  Polyline ring;
  ring.closed = true;
  ring.points = {{rect.fLeft, rect.fTop},
                 {rect.fRight, rect.fTop},
                 {rect.fRight, rect.fBottom},
                 {rect.fLeft, rect.fBottom}};
  return Region{{std::move(ring)}};
}

Region Region::disc(glm::vec2 centre, float radius, int segments) {
  Polyline ring;
  ring.closed = true;
  const int count = std::max(segments, 3);
  ring.points.reserve((size_t)count);
  for (int i = 0; i < count; ++i) {
    const float angle =
        2.0f * std::numbers::pi_v<float> * (float)i / (float)count;
    ring.points.push_back({centre.x + radius * std::cos(angle),
                           centre.y + radius * std::sin(angle)});
  }
  return Region{{std::move(ring)}};
}

Region Region::of(std::span<const Polyline> rings) {
  return Region{{rings.begin(), rings.end()}};
}

SkRect Region::bounds() const { return path::bounds(rings); }

bool Region::contains(glm::vec2 point) const {
  return containsEvenOdd(rings, point);
}

float Region::area() const {
  // THE EVEN-ODD AREA, and it must be read the way `contains` is read
  // rather than from the windings: a ring inside an odd number of others
  // is a hole and subtracts, whatever direction either was drawn in. A
  // ring is placed by one of its own points, since rings of a region do
  // not cross.
  float total = 0;
  for (const Polyline& ring : rings) {
    if (ring.points.size() < 3) continue;
    int depth = 0;
    for (const Polyline& other : rings) {
      if (&other == &ring || other.points.size() < 3) continue;
      if (other.contains(ring.points.front())) ++depth;
    }
    const float signed_ = std::abs(ring.signedArea());
    total += depth % 2 == 0 ? signed_ : -signed_;
  }
  return std::abs(total);
}

// ---------------------------------------------------------------------------
// The scatter

std::vector<glm::vec2> sample(const Region& region,
                              const Distribution& distribution) {
  const float area = region.area();
  const int count = wantedCount(distribution, area);
  const float spacing = wantedSpacing(distribution, area);
  // A count asked for is a cap on the spreads that cannot hit it exactly:
  // a lattice and a poisson fill answer what their pitch fits, and must
  // not answer MORE than the number requested.
  const int cap = distribution.rate == Rate::Count
                      ? count
                      : std::max(distribution.maxPoints, 0);

  const Inside inside(region);
  std::vector<glm::vec2> points;
  switch (distribution.spread) {
    case Spread::Random:
      points = randomPoints(region, inside, distribution, count);
      break;
    case Spread::Lattice:
      points = latticePoints(region, inside, distribution, spacing, cap);
      break;
    case Spread::Poisson:
      points = poissonPoints(region, inside, distribution, spacing, cap);
      break;
  }

  if (distribution.relaxIterations > 0 && points.size() > 1 && spacing > 0) {
    // The push radius is the spacing the points were asked for: pushing
    // further than that would spread a set that is already as even as its
    // own count allows, and pushing less would leave the clumps.
    std::vector<glm::vec3> lifted;
    lifted.reserve(points.size());
    for (const glm::vec2 point : points) lifted.emplace_back(point, 0.0f);
    const SkRect box = region.bounds();
    relax(lifted, Relaxation{spacing, distribution.relaxIterations, 0.5f},
          [&](glm::vec3 moved) {
            const glm::vec2 flat{moved.x, moved.y};
            // While the relaxation runs, a point pushed out of the shape
            // is held inside the bounding box so that it goes on pushing
            // its neighbours instead of wandering off.
            if (inside(flat)) return glm::vec3(flat, 0.0f);
            return glm::vec3(std::clamp(moved.x, box.fLeft, box.fRight),
                             std::clamp(moved.y, box.fTop, box.fBottom), 0.0f);
          });
    // A point that FINISHED outside the shape keeps the place it started
    // rather than the edge it was held at: piling the overflow onto the
    // boundary is the one thing a relaxed scatter must not do.
    for (size_t i = 0; i < points.size(); ++i) {
      const glm::vec2 moved{lifted[i].x, lifted[i].y};
      if (inside(moved)) points[i] = moved;
    }
  }
  return points;
}

Distribution uniform(int count, uint64_t seed) {
  return Distribution{.spread = Spread::Random,
                      .rate = Rate::Count,
                      .amount = (float)count,
                      .seed = seed};
}

Distribution poisson(float radius, uint64_t seed) {
  return Distribution{.spread = Spread::Poisson,
                      .rate = Rate::Spacing,
                      .amount = radius,
                      .seed = seed};
}

Distribution blueNoise(int count, uint64_t seed, int iterations) {
  return Distribution{.spread = Spread::Random,
                      .rate = Rate::Count,
                      .amount = (float)count,
                      .seed = seed,
                      .relaxIterations = iterations};
}

Distribution grid(float spacing) {
  return Distribution{.spread = Spread::Lattice,
                      .rate = Rate::Spacing,
                      .amount = spacing,
                      .jitter = 0.0f};
}

Distribution jittered(float spacing, uint64_t seed, float jitter) {
  return Distribution{.spread = Spread::Lattice,
                      .rate = Rate::Spacing,
                      .amount = spacing,
                      .seed = seed,
                      .jitter = jitter};
}

}  // namespace sigil::geometry::path
