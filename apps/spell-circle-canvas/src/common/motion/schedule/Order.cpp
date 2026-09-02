/** @file
 * The five orderings, including the seeded permutation the scattered one
 * deals.
 */

#include <sigilcore/compute/Noise.h>
#include <sigilmotion/schedule/Order.h>

#include <algorithm>
#include <cmath>
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

}  // namespace sigil::motion
