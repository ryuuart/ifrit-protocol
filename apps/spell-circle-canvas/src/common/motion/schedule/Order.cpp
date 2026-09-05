/** @file
 * The five orderings, including the seeded permutation the scattered one
 * deals, and the dense ranking a caller-stated order is dealt in.
 */

#include <sigilcore/compute/Noise.h>
#include <sigilmotion/schedule/Order.h>

#include <boost/unordered/unordered_flat_set.hpp>

#include <algorithm>
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
}  // namespace

void cascadeOrder(Spread::From from, uint32_t count, uint32_t seed,
                  std::vector<float>& order) {
  order.assign(count, 0.0f);
  // A cascade of ONE is a cascade with no spread, whichever end it claims
  // to start from: every shape below must put that single member at 0.
  const float last = count > 1 ? (float)(count - 1) : 0.0f;
  switch (from) {
    case Spread::From::Start:
      for (uint32_t i = 0; i < count; ++i) order[i] = (float)i;
      break;
    case Spread::From::End:
      for (uint32_t i = 0; i < count; ++i) order[i] = (float)(count - 1 - i);
      break;
    case Spread::From::Center:
      for (uint32_t i = 0; i < count; ++i)
        order[i] = std::abs((float)i - last * 0.5f) * 2.0f;
      break;
    case Spread::From::Edges:
      for (uint32_t i = 0; i < count; ++i)
        order[i] = last - std::abs((float)i - last * 0.5f) * 2.0f;
      break;
    case Spread::From::Random: {
      // Rank each unit by a hash of its index: deterministic, so the same
      // run scatters the same way on every frame and after a rebuild.
      // The seed salts that key AFTER a mix of its own, so seeds 1 and 2
      // deal permutations as independent as any two; seed 0 contributes
      // NOTHING to the key, which is what keeps the default scatter the
      // count-keyed one, bit for bit.
      const uint64_t salt = seed ? mix64Value(seed) : 0ull;
      std::vector<uint32_t> indices(count);
      std::iota(indices.begin(), indices.end(), 0u);
      std::stable_sort(indices.begin(), indices.end(),
                       [count, salt](uint32_t a, uint32_t b) {
                         return mix64Value(a * 2654435761ull + count + salt) <
                                mix64Value(b * 2654435761ull + count + salt);
                       });
      for (uint32_t rank = 0; rank < count; ++rank)
        order[indices[rank]] = (float)rank;
      break;
    }
  }
}

namespace {
/** Once per distinct shape, like the cue table's warning: a cascade is
 *  rebuilt every frame, and one mistyped table would otherwise scroll the
 *  same line past its author forever. */
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
}  // namespace

void cascadeRanks(const std::vector<float>& keys, uint32_t count,
                  std::vector<float>& out) {
  out.assign(count, 0.0f);
  if (count == 0) return;
  if (keys.size() != count) warnRankTableMismatch(keys.size(), count);

  std::vector<uint32_t> indices;
  indices.reserve(count);
  for (uint32_t i = 0; i < count && i < keys.size(); ++i) indices.push_back(i);
  // A non-finite key has no place in an order, so it goes to the end
  // rather than poisoning the comparison it takes part in.
  const auto keyOf = [&keys](uint32_t i) {
    const float k = keys[i];
    return std::isfinite(k) ? k : std::numeric_limits<float>::max();
  };
  std::stable_sort(indices.begin(), indices.end(),
                   [&keyOf](uint32_t a, uint32_t b) {
                     return keyOf(a) < keyOf(b);
                   });

  // DENSE: a slot is how many distinct smaller keys there are, so equal
  // keys open together and the next distinct key is the next slot.
  float slot = 0.0f;
  for (size_t rank = 0; rank < indices.size(); ++rank) {
    if (rank > 0 && keyOf(indices[rank]) != keyOf(indices[rank - 1]))
      slot += 1.0f;
    out[indices[rank]] = slot;
  }
  // Units the table did not reach open last, as they do under a short cue
  // table.
  const float last = indices.empty() ? 0.0f : slot;
  for (uint32_t i = (uint32_t)keys.size(); i < count; ++i) out[i] = last;
}

}  // namespace sigil::motion
