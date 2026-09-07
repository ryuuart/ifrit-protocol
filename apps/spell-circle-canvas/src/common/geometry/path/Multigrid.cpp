#include <boost/unordered/unordered_flat_map.hpp>
#include <cmath>
#include <cstdint>
#include <numbers>

#include "sigilgeometry/path/Lattice.h"

namespace sigil::geometry::path {
namespace {

/** The weld: corners quantised into buckets a tolerance across, each new
 *  corner measured against the nine buckets it could have a neighbour in.
 *  A single bucket would part two corners a hair either side of a
 *  boundary, which in a tiling is exactly the pair that must be one. */
class CornerWeld {
 public:
  CornerWeld(std::vector<glm::dvec2>& into, double tolerance)
      : points_(into), cell_(tolerance > 0 ? tolerance : 1e-12) {}

  int index(glm::dvec2 p) {
    const int64_t cx = (int64_t)std::floor(p.x / cell_);
    const int64_t cy = (int64_t)std::floor(p.y / cell_);
    for (int64_t dy = -1; dy <= 1; ++dy) {
      for (int64_t dx = -1; dx <= 1; ++dx) {
        auto it = buckets_.find(key(cx + dx, cy + dy));
        if (it == buckets_.end()) continue;
        for (int i : it->second) {
          const glm::dvec2 q = points_[(size_t)i];
          if (std::abs(q.x - p.x) <= cell_ && std::abs(q.y - p.y) <= cell_)
            return i;
        }
      }
    }
    const int made = (int)points_.size();
    points_.push_back(p);
    buckets_[key(cx, cy)].push_back(made);
    return made;
  }

 private:
  static int64_t key(int64_t x, int64_t y) {
    return (int64_t)((uint64_t)x * 0x9E3779B97F4A7C15ull ^ (uint64_t)y);
  }
  std::vector<glm::dvec2>& points_;
  double cell_;
  boost::unordered_flat_map<int64_t, std::vector<int>> buckets_;
};

}  // namespace

std::vector<MultigridFamily> multigridRing(int count, double offset,
                                           double spacing) {
  std::vector<MultigridFamily> out;
  if (count < 2) return out;
  // An even count spread over a whole turn puts family j and family
  // j + count/2 on the same lines, so the ring closes on a half turn
  // there and on a whole turn where the count is odd.
  const double turn =
      (count % 2 == 0) ? std::numbers::pi : 2.0 * std::numbers::pi;
  out.reserve((size_t)count);
  for (int j = 0; j < count; ++j)
    out.push_back({turn * (double)j / (double)count, spacing, offset});
  return out;
}

MultigridTiling multigrid(std::span<const MultigridFamily> families,
                          const MultigridOptions& options) {
  MultigridTiling out;
  const int n = (int)families.size();
  if (n < 2 || options.radius <= 0 || options.maxRhombs <= 0) return out;

  std::vector<glm::dvec2> zeta((size_t)n);
  for (int j = 0; j < n; ++j) {
    const MultigridFamily& f = families[(size_t)j];
    if (!(f.spacing > 0)) return out;
    zeta[(size_t)j] = {std::cos(f.normal), std::sin(f.normal)};
  }

  // THE REACH, CARRIED BACK THROUGH THE DUAL MAP. A crossing at x lands
  // its rhomb near Mx, where M is the sum of each family's normal against
  // itself over its spacing; so a tiling reaching `radius` is answered by
  // the crossings inside radius/λ, λ the smaller of M's two eigenvalues.
  // The ceilings that count the lines add half a spacing each, which is
  // the constant beside it.
  double mxx = 0, mxy = 0, myy = 0, drift = 0;
  for (int j = 0; j < n; ++j) {
    const glm::dvec2 z = zeta[(size_t)j];
    const double w = 1.0 / families[(size_t)j].spacing;
    mxx += z.x * z.x * w;
    mxy += z.x * z.y * w;
    myy += z.y * z.y * w;
    drift += std::abs(families[(size_t)j].offset) + 1.0;
  }
  const double trace = mxx + myy;
  const double det2 = mxx * myy - mxy * mxy;
  const double disc = std::max(0.0, trace * trace - 4.0 * det2);
  const double lambda = 0.5 * (trace - std::sqrt(disc));
  if (!(lambda > 1e-12)) return out;  // the families do not span the plane
  const double reach = (options.radius + drift) / lambda;

  std::vector<int> bound((size_t)n);
  for (int j = 0; j < n; ++j) {
    const MultigridFamily& f = families[(size_t)j];
    bound[(size_t)j] =
        (int)std::ceil(reach / f.spacing + std::abs(f.offset)) + 2;
  }

  CornerWeld weld(out.vertices, options.tolerance);
  const double radius2 = options.radius * options.radius;

  for (int r = 0; r < n; ++r) {
    for (int s = r + 1; s < n; ++s) {
      const glm::dvec2 zr = zeta[(size_t)r], zs = zeta[(size_t)s];
      const double det = zr.x * zs.y - zr.y * zs.x;
      if (std::abs(det) < 1e-12) continue;  // the two families are parallel
      const double sr = families[(size_t)r].spacing;
      const double ss = families[(size_t)s].spacing;
      const double gr = families[(size_t)r].offset;
      const double gs = families[(size_t)s].offset;

      for (int kr = -bound[(size_t)r]; kr <= bound[(size_t)r]; ++kr) {
        for (int ks = -bound[(size_t)s]; ks <= bound[(size_t)s]; ++ks) {
          if ((int)out.rhombs.size() >= options.maxRhombs) return out;
          // The one point where line kr of family r meets line ks of
          // family s.
          const double a = ((double)kr - gr) * sr;
          const double b = ((double)ks - gs) * ss;
          const glm::dvec2 x{(a * zs.y - b * zr.y) / det,
                             (zr.x * b - zs.x * a) / det};

          // How many lines of every family stand between it and the
          // origin. The two that made the crossing are exact by
          // construction, and reading them back through the ceiling would
          // be the one place a hair of arithmetic could move a rhomb.
          glm::dvec2 z{0, 0};
          for (int j = 0; j < n; ++j) {
            const MultigridFamily& f = families[(size_t)j];
            const int k = (j == r) ? kr
                          : (j == s)
                              ? ks
                              : (int)std::ceil((zeta[(size_t)j].x * x.x +
                                                zeta[(size_t)j].y * x.y) /
                                                   f.spacing +
                                               f.offset);
            z = z + zeta[(size_t)j] * (double)k;
          }

          const glm::dvec2 c[4] = {z, z + zr, z + zr + zs, z + zs};
          bool near = false;
          for (const glm::dvec2& p : c)
            near = near || (p.x * p.x + p.y * p.y) <= radius2;
          if (!near) continue;

          MultigridRhomb rhomb;
          rhomb.families[0] = r;
          rhomb.families[1] = s;
          rhomb.lines[0] = kr;
          rhomb.lines[1] = ks;
          for (int i = 0; i < 4; ++i) rhomb.corners[i] = weld.index(c[i]);
          out.rhombs.push_back(rhomb);
        }
      }
    }
  }
  return out;
}

}  // namespace sigil::geometry::path
