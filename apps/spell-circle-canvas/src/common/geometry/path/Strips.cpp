#include <include/core/SkPathBuilder.h>
#include <include/core/SkRect.h>

#include <algorithm>
#include <boost/unordered/unordered_flat_map.hpp>
#include <cmath>
#include <cstdint>
#include <vector>

#include "sigilgeometry/path/Operations.h"

namespace sigil::geometry::path::operations {
namespace {

constexpr float kDegrees = 57.295779513f;

glm::vec2 unitOf(glm::vec2 v) {
  const float l = std::sqrt(v.x * v.x + v.y * v.y);
  return l > 1e-9f ? glm::vec2{v.x / l, v.y / l} : glm::vec2{0, 0};
}
float cross(glm::vec2 a, glm::vec2 b) { return a.x * b.y - a.y * b.x; }
/** The normal a quarter turn to the LEFT of travel. */
glm::vec2 leftOf(glm::vec2 u) { return {-u.y, u.x}; }

/** One end of one piece, standing at a node. */
struct End {
  int piece = 0;
  int which = 0;  // 0 the `from` end, 1 the `to` end
  glm::vec2 at{0, 0};
  glm::vec2 out{0, 0};  // away from the node, along the piece
  float half = 0;
  float angle = 0;
  // Filled in once the node is read: the corner toward the end after it
  // round the node, the corner toward the one before it, and the node.
  glm::vec2 next{0, 0}, prev{0, 0};
  bool wedge = false;  // whether the node point is a corner of the outline
};

/** WHERE ONE END'S RAIL MEETS ONE SEAM. The seam runs out of the node
 *  along the bisector of the two directions that share it, and the rail
 *  stands a half-width to the given side of the piece's own axis; the
 *  corner is where they cut. `side` is +1 for the rail left of travel
 *  and -1 for the rail right of it. */
glm::vec2 seamCorner(const End& e, glm::vec2 other, float side, Join join,
                     float miterLimit) {
  const glm::vec2 u = e.out;
  glm::vec2 b = e.out + other;
  if (std::sqrt(b.x * b.x + b.y * b.y) < 1e-6f) {
    // The two run exactly against each other: the seam is the square cut
    // across them both.
    b = leftOf(u) * side;
  } else {
    b = unitOf(b);
    if (cross(u, b) * side < 0) b = -b;
  }
  const float turn = cross(u, b);
  if (std::abs(turn) < 1e-6f) return e.at + leftOf(u) * (e.half * side);
  float reach = side * e.half / turn;
  const float limit = (join == Join::Miter) ? miterLimit * e.half : e.half;
  reach = std::clamp(reach, -limit, limit);
  return e.at + b * reach;
}

/** The ends of every piece, welded into nodes and put in order round
 *  each one, with each end's two corners solved. */
std::vector<End> readEnds(std::span<const Strip> pieces,
                          const StripOptions& options) {
  std::vector<End> ends;
  ends.reserve(pieces.size() * 2);
  for (size_t i = 0; i < pieces.size(); ++i) {
    const Strip& s = pieces[i];
    const glm::vec2 u = unitOf(s.to - s.from);
    if (u.x == 0 && u.y == 0) continue;
    const float half = std::abs(s.width) * 0.5f;
    ends.push_back({(int)i, 0, s.from, u, half, std::atan2(u.y, u.x)});
    ends.push_back({(int)i, 1, s.to, -u, half, std::atan2(-u.y, -u.x)});
  }

  // The weld: ends quantised into buckets a tolerance across, each end
  // measured against the nine buckets a neighbour could be in, so a pair
  // a hair either side of a bucket boundary is still one node.
  const float cell = options.tolerance > 0 ? options.tolerance : 1e-6f;
  boost::unordered_flat_map<int64_t, std::vector<int>> nodes;
  const auto key = [](int64_t x, int64_t y) {
    return (int64_t)((uint64_t)x * 0x9E3779B97F4A7C15ull ^ (uint64_t)y);
  };
  std::vector<int> owner(ends.size(), -1);
  std::vector<std::vector<int>> members;
  for (size_t e = 0; e < ends.size(); ++e) {
    const int64_t cx = (int64_t)std::floor(ends[e].at.x / cell);
    const int64_t cy = (int64_t)std::floor(ends[e].at.y / cell);
    int found = -1;
    for (int64_t dy = -1; dy <= 1 && found < 0; ++dy) {
      for (int64_t dx = -1; dx <= 1 && found < 0; ++dx) {
        auto it = nodes.find(key(cx + dx, cy + dy));
        if (it == nodes.end()) continue;
        for (int m : it->second) {
          const glm::vec2 d = ends[(size_t)m].at - ends[e].at;
          if (std::abs(d.x) <= cell && std::abs(d.y) <= cell) {
            found = owner[(size_t)m];
            break;
          }
        }
      }
    }
    if (found < 0) {
      found = (int)members.size();
      members.emplace_back();
    }
    owner[e] = found;
    members[(size_t)found].push_back((int)e);
    nodes[key(cx, cy)].push_back((int)e);
  }

  for (std::vector<int>& node : members) {
    std::sort(node.begin(), node.end(), [&](int a, int b) {
      return ends[(size_t)a].angle < ends[(size_t)b].angle;
    });
    const size_t count = node.size();
    for (size_t k = 0; k < count; ++k) {
      End& e = ends[(size_t)node[k]];
      if (count == 1) {
        const glm::vec2 n = leftOf(e.out) * e.half;
        e.next = e.at + n;
        e.prev = e.at - n;
        e.wedge = false;
        continue;
      }
      const glm::vec2 after = ends[(size_t)node[(k + 1) % count]].out;
      const glm::vec2 before = ends[(size_t)node[(k + count - 1) % count]].out;
      e.next = seamCorner(e, after, 1.0f, options.join, options.miterLimit);
      e.prev = seamCorner(e, before, -1.0f, options.join, options.miterLimit);
      // The node is a corner of the outline only where the two seams are
      // different lines; two ends meeting share one seam and the end is
      // planed flat across it.
      const glm::vec2 a = unitOf(e.next - e.at), b = unitOf(e.prev - e.at);
      e.wedge = std::sqrt((a.x + b.x) * (a.x + b.x) +
                          (a.y + b.y) * (a.y + b.y)) > 1e-3f;
    }
  }
  return ends;
}

/** The end walked from its `prev` corner to its `next` corner, which is
 *  the way round the outline that crosses the end. */
void crossEnd(SkPathBuilder& b, const End& e, Join join) {
  if (join == Join::Round) {
    const glm::vec2 r0 = e.prev - e.at, r1 = e.next - e.at;
    const float radius = std::sqrt(r0.x * r0.x + r0.y * r0.y);
    if (radius > 1e-6f) {
      const float a0 = std::atan2(r0.y, r0.x) * kDegrees;
      const float a1 = std::atan2(r1.y, r1.x) * kDegrees;
      const float outward = std::atan2(-e.out.y, -e.out.x) * kDegrees;
      float sweep = a1 - a0;
      while (sweep < 0) sweep += 360.0f;
      float toOutward = outward - a0;
      while (toOutward < 0) toOutward += 360.0f;
      if (toOutward > sweep) sweep -= 360.0f;
      const SkRect oval = SkRect::MakeLTRB(e.at.x - radius, e.at.y - radius,
                                           e.at.x + radius, e.at.y + radius);
      b.arcTo(oval, a0, sweep, false);
      return;
    }
  }
  if (e.wedge) b.lineTo(e.at.x, e.at.y);
  b.lineTo(e.next.x, e.next.y);
}

}  // namespace

std::vector<SkPath> stripOutlines(std::span<const Strip> pieces,
                                  const StripOptions& options) {
  std::vector<SkPath> out((size_t)pieces.size());
  const std::vector<End> ends = readEnds(pieces, options);
  std::vector<const End*> byPiece(pieces.size() * 2, nullptr);
  for (const End& e : ends) byPiece[(size_t)e.piece * 2 + (size_t)e.which] = &e;

  for (size_t i = 0; i < pieces.size(); ++i) {
    const End* a = byPiece[i * 2];
    const End* z = byPiece[i * 2 + 1];
    if (!a || !z) continue;
    SkPathBuilder b;
    // Out along the rail left of travel, across the far end, back along
    // the rail right of it, across the near end.
    b.moveTo(a->next.x, a->next.y);
    b.lineTo(z->prev.x, z->prev.y);
    crossEnd(b, *z, options.join);
    b.lineTo(a->prev.x, a->prev.y);
    crossEnd(b, *a, options.join);
    b.close();
    out[i] = b.detach();
  }
  return out;
}

SkPath strips(std::span<const Strip> pieces, const StripOptions& options) {
  return unite(stripOutlines(pieces, options));
}

std::vector<StripLap> stripLaps(std::span<const Strip> pieces,
                                const StripOptions& options) {
  std::vector<StripLap> out;
  const int n = (int)pieces.size();
  for (int i = 0; i < n; ++i) {
    const glm::vec2 di = pieces[(size_t)i].to - pieces[(size_t)i].from;
    const float li = std::sqrt(di.x * di.x + di.y * di.y);
    if (li < 1e-6f) continue;
    for (int j = i + 1; j < n; ++j) {
      const glm::vec2 dj = pieces[(size_t)j].to - pieces[(size_t)j].from;
      const float lj = std::sqrt(dj.x * dj.x + dj.y * dj.y);
      if (lj < 1e-6f) continue;
      const float det = di.x * dj.y - di.y * dj.x;
      const glm::vec2 ui = di / li, uj = dj / lj;
      const float sine = std::abs(cross(ui, uj));
      if (sine < 1e-6f || det == 0) continue;  // parallel: they never cross
      const glm::vec2 r = pieces[(size_t)j].from - pieces[(size_t)i].from;
      const float t = (r.x * dj.y - r.y * dj.x) / det;
      const float u = (r.x * di.y - r.y * di.x) / det;
      // An end standing on the other piece is a MEETING, which the
      // joinery mitres; a lap is a crossing with stock on both sides of
      // it on both pieces.
      const float mi = options.tolerance / std::max(li, 1.0f);
      const float mj = options.tolerance / std::max(lj, 1.0f);
      if (t < mi || t > 1 - mi || u < mj || u > 1 - mj) continue;

      StripLap lap;
      lap.pieces[0] = i;
      lap.pieces[1] = j;
      lap.at = pieces[(size_t)i].from + di * t;
      lap.along[0] = ui;
      lap.along[1] = uj;
      lap.at01[0] = t;
      lap.at01[1] = u;
      const float wi = std::abs(pieces[(size_t)i].width);
      const float wj = std::abs(pieces[(size_t)j].width);
      lap.halfSpan[0] = std::min(options.lapLimit * wj, wj * 0.5f / sine);
      lap.halfSpan[1] = std::min(options.lapLimit * wi, wi * 0.5f / sine);
      out.push_back(lap);
    }
  }
  return out;
}

}  // namespace sigil::geometry::path::operations
