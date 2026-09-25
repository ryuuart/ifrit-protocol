/** @file
 * Where a child sits in a stagger: the named orderings, a distance across
 * a grid, the seeded permutation the scattered one deals, and the dense
 * ranking a caller-stated order is dealt in.
 */

#include <sigilcore/compute/Noise.h>
#include <sigilmotion/schedule/Stagger.h>

#include <algorithm>
#include <boost/unordered/unordered_flat_set.hpp>
#include <cmath>
#include <cstdio>
#include <limits>
#include <numeric>

namespace sigil::motion {

namespace {

/** The stateless splitmix64 of one key — the avalanche over the key
 *  offset by the gamma, used to order units rather than to shape a
 *  value. The same body every library that has to agree on a seeded draw
 *  reads, which is why it is called rather than transcribed. */
uint64_t mix64Value(uint64_t z) {
  return core::noise::mix64(z + core::noise::kMix64Gamma);
}

/** Once per distinct shape: a schedule is rebuilt every frame, and one
 *  mistyped table would otherwise scroll the same line past its author
 *  forever. */
void warnRankTableMismatch(size_t keyCount, size_t unitCount) {
  static thread_local boost::unordered_flat_set<uint64_t> seen;
  const uint64_t key = ((uint64_t)keyCount << 32u) | (uint32_t)unitCount;
  if (!seen.insert(key).second) return;
  std::fprintf(stderr,
               "SigilMotion: an order of %zu numbers against %zu units — "
               "%s\n",
               keyCount, unitCount,
               keyCount < unitCount
                   ? "every unit past the table's end opens last"
                   : "the numbers past the last unit are never read");
}

/** DENSE RANKS of a stated order: the step is how many DISTINCT smaller
 *  numbers there are, so ties open together and the step above them is the
 *  next one up. A table shorter than the run leaves the units past its end
 *  at the last step; a non-finite number sorts to the end. */
void ranks(const std::vector<float>& keys, uint32_t count,
           std::vector<float>& out) {
  out.assign(count, 0.0f);
  if (count == 0) return;
  if (keys.size() != count) warnRankTableMismatch(keys.size(), count);
  std::vector<uint32_t> indices;
  indices.reserve(count);
  for (uint32_t i = 0; i < count && i < keys.size(); ++i) indices.push_back(i);
  const auto keyOf = [&keys](uint32_t i) {
    const float key = keys[i];
    return std::isfinite(key) ? key : std::numeric_limits<float>::max();
  };
  std::stable_sort(
      indices.begin(), indices.end(),
      [&keyOf](uint32_t left, uint32_t right) { return keyOf(left) < keyOf(right); });
  float slot = 0.0f;
  for (size_t rank = 0; rank < indices.size(); ++rank) {
    if (rank > 0 && keyOf(indices[rank]) != keyOf(indices[rank - 1]))
      slot += 1.0f;
    out[indices[rank]] = slot;
  }
  const float last = indices.empty() ? 0.0f : slot;
  for (uint32_t i = (uint32_t)keys.size(); i < count; ++i) out[i] = last;
}

/** A distance across a `{columns, rows}` grid from the origin cell. */
void gridSteps(const StaggerOrigin& origin, uint32_t columns,
               StaggerAxis axis, uint32_t count, std::vector<float>& out) {
  const uint32_t rows = (count + columns - 1) / columns;
  float originColumn = 0.0f, originRow = 0.0f;
  if (origin.index) {
    originColumn = (float)(*origin.index % columns);
    originRow = (float)(*origin.index / columns);
  } else if (origin.place == StaggerFrom::Center ||
             origin.place == StaggerFrom::Edges) {
    originColumn = (float)(columns - 1) * 0.5f;
    originRow = (float)(rows - 1) * 0.5f;
  } else if (origin.place == StaggerFrom::Last) {
    originColumn = (float)(columns - 1);
    originRow = (float)(rows - 1);
  }
  out.assign(count, 0.0f);
  float farthest = 0.0f;
  for (uint32_t i = 0; i < count; ++i) {
    const float across = (float)(i % columns) - originColumn;
    const float down = (float)(i / columns) - originRow;
    out[i] = axis == StaggerAxis::Columns ? std::abs(across)
             : axis == StaggerAxis::Rows  ? std::abs(down)
                                          : std::sqrt(across * across + down * down);
    farthest = std::max(farthest, out[i]);
  }
  if (!origin.index && origin.place == StaggerFrom::Edges)
    for (float& step : out) step = farthest - step;
}

/** The named orderings over a list, in multiples of one step. Center and
 *  Edges span the same number of steps as First, so a run's total is the
 *  same whichever end it opens from. */
void listSteps(const StaggerOrigin& origin, uint32_t count, uint32_t seed,
               std::vector<float>& out) {
  out.assign(count, 0.0f);
  const float last = count > 1 ? (float)(count - 1) : 0.0f;
  if (origin.index) {
    for (uint32_t i = 0; i < count; ++i)
      out[i] = std::abs((float)i - (float)*origin.index);
    return;
  }
  switch (origin.place) {
    case StaggerFrom::First:
      for (uint32_t i = 0; i < count; ++i) out[i] = (float)i;
      break;
    case StaggerFrom::Last:
      for (uint32_t i = 0; i < count; ++i) out[i] = (float)(count - 1 - i);
      break;
    case StaggerFrom::Center:
      for (uint32_t i = 0; i < count; ++i)
        out[i] = std::abs((float)i - last * 0.5f) * 2.0f;
      break;
    case StaggerFrom::Edges:
      for (uint32_t i = 0; i < count; ++i)
        out[i] = last - std::abs((float)i - last * 0.5f) * 2.0f;
      break;
    case StaggerFrom::Random: {
      const uint64_t salt = seed ? mix64Value(seed) : 0ull;
      std::vector<uint32_t> indices(count);
      std::iota(indices.begin(), indices.end(), 0u);
      std::stable_sort(indices.begin(), indices.end(),
                       [count, salt](uint32_t left, uint32_t right) {
                         return mix64Value(left * 2654435761ull + count + salt) <
                                mix64Value(right * 2654435761ull + count + salt);
                       });
      for (uint32_t rank = 0; rank < count; ++rank)
        out[indices[rank]] = (float)rank;
      break;
    }
  }
}

}  // namespace

void staggerSteps(const StaggerOrigin& origin,
                  const std::array<uint32_t, 2>& grid, StaggerAxis axis,
                  uint32_t seed, const std::vector<float>& rankBy,
                  bool reverse, uint32_t count, std::vector<float>& out) {
  if (!rankBy.empty())
    ranks(rankBy, count, out);
  else if (grid[0] > 0 && origin.place != StaggerFrom::Random)
    gridSteps(origin, grid[0], axis, count, out);
  else
    listSteps(origin, count, seed, out);
  if (reverse && !out.empty()) {
    const float last = *std::max_element(out.begin(), out.end());
    for (float& step : out) step = last - step;
  }
}

StaggerStep staggerStep(const StaggerOrigin& origin,
                        const std::array<uint32_t, 2>& grid, StaggerAxis axis,
                        uint32_t seed, const std::vector<float>& rankBy,
                        bool reverse, Place place) {
  // Every child of a run asks the same question with the same counts, so
  // the run's steps are dealt once and read by index until it changes.
  struct Run {
    StaggerOrigin origin;
    std::array<uint32_t, 2> grid{};
    StaggerAxis axis = StaggerAxis::Both;
    uint32_t seed = 0;
    std::vector<float> rankBy;
    bool reverse = false;
    uint32_t count = 0;
    std::vector<float> steps;
    float last = 0.0f;
  };
  static thread_local Run run{.count = UINT32_MAX};
  const uint32_t count = (uint32_t)std::max<size_t>(place.count, 1);
  if (run.count != count || !(run.origin == origin) || run.grid != grid ||
      run.axis != axis || run.seed != seed || run.rankBy != rankBy ||
      run.reverse != reverse) {
    run = {origin, grid, axis, seed, rankBy, reverse, count, {}, 0.0f};
    staggerSteps(origin, grid, axis, seed, rankBy, reverse, count, run.steps);
    run.last = run.steps.empty()
                   ? 0.0f
                   : *std::max_element(run.steps.begin(), run.steps.end());
  }
  const size_t index = std::min<size_t>(place.index, run.steps.size() - 1);
  return {run.steps[index], run.last};
}

}  // namespace sigil::motion
