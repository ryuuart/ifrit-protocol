/** @file
 * THE NEIGHBOURHOOD OPERATORS: the four whose definition reads points
 * the point being written does not own — the chain-order smoothing, the
 * spatial relaxation, the k-means clustering, and the attribute transfer
 * that gathers from another cloud entirely.
 *
 * None of them has a kernel, and the reason is the same one four times:
 * a kernel is a pure function of one point, and here one lane cannot be
 * both what is read and what is written. The mask is read the way every
 * other filter reads it — the result blended against what the lane held
 * before the pass.
 */

#include <sigilcore/compute/Chance.h>
#include <sigilcore/schedule/Parallel.h>

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <limits>
#include <vector>

#include "CookInternal.h"
#include "sigilgeometry/mesh/pop/Pop.h"
#include "sigilgeometry/path/Neighbours.h"

namespace sigil::geometry::mesh {

void runSmooth(Attrs& attrs, const pop::Smooth& op, size_t count,
               size_t grain) {
  // Chain-order op: double-buffered, read-old/write-new — the
  // shape a parallel pass would have too, and the reason there
  // is no kernel for it: a point reads two it does not own, so
  // one lane cannot be both what is read and what is written.
  // The mask blends the relaxed value against the old one
  // BEFORE the scratch write, so a masked point's neighbours
  // still see its old value this pass.
  std::vector<glm::vec4>& values = attrs.ensure(op.lane.name);
  const std::vector<glm::vec4>* mask =
      op.mask.empty() ? nullptr : &attrs.ensure(op.mask);
  for (int pass = 0; pass < op.iterations; ++pass) {
    std::vector<glm::vec4> next(count);
    core::schedule::parallelFor(count, grain, [&](size_t first, size_t last) {
      for (size_t i = first; i < last; ++i) {
        const size_t a = i == 0 ? 0 : i - 1;
        const size_t b = i + 1 < count ? i + 1 : i;
        const glm::vec4 mid = (values[a] + values[b]) * 0.5f;
        const glm::vec4 v = values[i];
        const glm::vec4 relaxed = v + (mid - v) * op.strength;
        float m = 1.0f;
        if (mask) {
          const float raw = (*mask)[i].x;
          m = raw < 0.0f ? 0.0f : (raw > 1.0f ? 1.0f : raw);
        }
        next[i] = m >= 1.0f ? relaxed : v + (relaxed - v) * m;
      }
    });
    values.swap(next);
  }
}

void runRelax(Attrs& attrs, const pop::Relax& op, size_t count) {
  // SPATIAL relaxation, over the one grid the whole tree's
  // proximity is answered by. The mask blends the settled
  // positions against where they started, which is the same
  // rule every other filter's mask is read by.
  std::vector<glm::vec4>& values = attrs.ensure("P");
  const std::vector<glm::vec4>* mask =
      op.mask.empty() ? nullptr : &attrs.ensure(op.mask);
  std::vector<glm::vec3> positions(count);
  for (size_t i = 0; i < count; ++i)
    positions[i] = {values[i].x, values[i].y, values[i].z};
  const std::vector<glm::vec3> before = positions;
  path::relax(positions,
              path::Relaxation{op.radius, op.iterations, op.strength});
  for (size_t i = 0; i < count; ++i) {
    float m = 1.0f;
    if (mask) {
      const float raw = (*mask)[i].x;
      m = raw < 0.0f ? 0.0f : (raw > 1.0f ? 1.0f : raw);
    }
    const glm::vec3 settled = before[i] + (positions[i] - before[i]) * m;
    values[i] = {settled.x, settled.y, settled.z, values[i].w};
  }
}

void runCluster(Attrs& attrs, const pop::Cluster& op, size_t count,
                size_t grain) {
  // K-MEANS in the metric `weights` names. The centres start at
  // points drawn from the set itself rather than at random
  // coordinates, so no centre begins somewhere the points are
  // not and no group starts empty.
  const std::vector<glm::vec4>& from = attrs.ensure(op.from.name);
  std::vector<glm::vec4>& to = attrs.ensure(op.to);
  const int groups =
      (int)std::min<size_t>((size_t)std::max(op.count, 1), count);
  const glm::vec4 w = op.weights;
  const auto keyOf = [&](size_t i) { return from[i] * w; };

  // The centres are seeded the way k-means++ seeds them: the
  // first at a point drawn from the set, and each one after it
  // at a point chosen with probability proportional to how far
  // it is from the nearest centre so far. Drawing all of them
  // at random instead puts two centres in one clump often
  // enough that a clustering nobody could defend comes back
  // for a set whose groups are obvious.
  std::vector<glm::vec4> centres;
  centres.reserve((size_t)groups);
  core::chance::Stream stream = core::chance::Stream::pcg(op.seed);
  centres.push_back(keyOf(stream.below(count)));
  std::vector<float> spread(count, 0.0f);
  for (int g = 1; g < groups; ++g) {
    double total = 0;
    for (size_t i = 0; i < count; ++i) {
      const glm::vec4 d = keyOf(i) - centres.back();
      const float squared = glm::dot(d, d);
      if (g == 1 || squared < spread[i]) spread[i] = squared;
      total += (double)spread[i];
    }
    if (!(total > 0)) {
      centres.push_back(keyOf(stream.below(count)));
      continue;
    }
    double pick = (double)stream.unit() * total;
    size_t at = count - 1;
    for (size_t i = 0; i < count; ++i) {
      pick -= (double)spread[i];
      if (pick <= 0) {
        at = i;
        break;
      }
    }
    centres.push_back(keyOf(at));
  }

  std::vector<int> owner(count, 0);
  for (int pass = 0; pass < std::max(op.iterations, 1); ++pass) {
    core::schedule::parallelFor(count, grain, [&](size_t first, size_t last) {
      for (size_t i = first; i < last; ++i) {
        const glm::vec4 key = keyOf(i);
        float best = std::numeric_limits<float>::infinity();
        int at = 0;
        for (int g = 0; g < groups; ++g) {
          const glm::vec4 d = key - centres[(size_t)g];
          const float squared = glm::dot(d, d);
          if (squared < best) {
            best = squared;
            at = g;
          }
        }
        owner[i] = at;
      }
    });
    std::vector<glm::vec4> sums((size_t)groups, glm::vec4(0));
    std::vector<int> tally((size_t)groups, 0);
    for (size_t i = 0; i < count; ++i) {
      sums[(size_t)owner[i]] += keyOf(i);
      ++tally[(size_t)owner[i]];
    }
    // A group nobody joined keeps the centre it had: moving it
    // to the origin would drag it somewhere the points are not.
    for (int g = 0; g < groups; ++g)
      if (tally[(size_t)g] > 0)
        centres[(size_t)g] = sums[(size_t)g] / (float)tally[(size_t)g];
  }
  for (size_t i = 0; i < count; ++i) to[i] = {(float)owner[i], 0, 0, 0};
}

void runTransfer(Attrs& attrs, const pop::Transfer& op, size_t count) {
  // A GATHER FROM ANOTHER CLOUD. The source is indexed once and
  // every destination point reads it, which is the whole reason
  // this is one operator and not a loop at a call site.
  if (!op.lane.empty() && op.radius > 0 && !op.source.positions.empty()) {
    const std::vector<glm::vec4>& positions = attrs.ensure("P");
    std::vector<glm::vec4>& lane = attrs.ensure(op.lane);
    const std::vector<glm::vec4>* mask =
        op.mask.empty() ? nullptr : &attrs.ensure(op.mask);
    const path::Neighbours index(op.source.positions);

    // The source lane read under the destination's own name: a
    // colour if the source carries one there, else a vector, else
    // a scalar in .x. A source that carries nothing under the
    // name transfers nothing.
    const std::vector<glm::vec4>* colours = op.source.colorIf(op.lane);
    const std::vector<glm::vec3>* vectors =
        colours ? nullptr : op.source.vectorIf(op.lane);
    const std::vector<float>* scalars =
        (colours || vectors) ? nullptr : op.source.scalarIf(op.lane);
    const auto sourceAt = [&](uint32_t i) -> glm::vec4 {
      if (colours) return (*colours)[i];
      if (vectors) {
        const glm::vec3 v = (*vectors)[i];
        return {v.x, v.y, v.z, 0};
      }
      if (scalars) return {(*scalars)[i], 0, 0, 0};
      return {0, 0, 0, 0};
    };

    if (colours || vectors || scalars) {
      const float blend = std::clamp(op.blendWidth, 0.0f, 1.0f);
      const int samples = std::max(op.maxSamples, 1);
      std::vector<uint32_t> found;
      for (size_t i = 0; i < count; ++i) {
        const glm::vec4 p = positions[i];
        const glm::vec3 here{p.x, p.y, p.z};
        found = index.nearest(here, samples);
        glm::vec4 gathered{0, 0, 0, 0};
        float weight = 0, nearestDistance = 0;
        for (size_t at = 0; at < found.size(); ++at) {
          const float distance = glm::length(index.point(found[at]) - here);
          if (distance > op.radius) break;
          if (at == 0) nearestDistance = distance;
          // One over the distance, with a coincident source
          // taking the whole weight rather than an infinite one.
          const float w = distance > 0 ? 1.0f / distance : 1.0e6f;
          gathered += sourceAt(found[at]) * w;
          weight += w;
        }
        if (weight <= 0) continue;
        gathered /= weight;

        // The taper: full strength until the blend band starts,
        // then back to what the destination already held.
        float strength = 1.0f;
        if (blend > 0) {
          const float inner = op.radius * (1.0f - blend);
          if (nearestDistance > inner)
            strength = 1.0f - (nearestDistance - inner) / (op.radius - inner);
          strength = std::clamp(strength, 0.0f, 1.0f);
        }
        if (mask) {
          const float raw = (*mask)[i].x;
          strength *= raw < 0.0f ? 0.0f : (raw > 1.0f ? 1.0f : raw);
        }
        lane[i] += (gathered - lane[i]) * strength;
      }
    }
  }
}

}  // namespace sigil::geometry::mesh
