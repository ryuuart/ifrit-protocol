#pragma once

/** @file
 * Hashes that identify rather than randomize: the FNV-1a fold a cache
 * key is accumulated with, and the stirring step that folds one more
 * word into a hash already in hand.
 *
 * These answer "is this the same thing as that", so their outputs land
 * in bucket numbers and cache keys. The constants are fixed by the
 * agreement between the places that compute them, not by a reference
 * document: two hashes of one thing must match each other, and changing
 * a constant re-buckets everything already keyed by it.
 *
 * The seeded mixers a jitter draws from are a different family and live
 * in Noise.h: those exist to look random, these exist to collide
 * rarely.
 */

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace sigil::core::hash {

/** The word a fold starts from.
 *
 *  Not FNV-1a's published offset basis: any odd 64-bit word serves as
 *  one, and this is the word every bucket in this project is computed
 *  from. It is load-bearing exactly because it is shared — a hash only
 *  has to agree with the other hashes of the same thing. */
inline constexpr uint64_t kFnvOffset = 1469598103934665603ull;

/** FNV-1a's multiplier. */
inline constexpr uint64_t kFnvPrime = 1099511628211ull;

/** FNV-1a over the eight bytes of @p value, least significant first.
 *
 *  Byte order is spelled out rather than taken from memory, so the fold
 *  is the same number on a big-endian machine as on a little-endian
 *  one. */
inline uint64_t fnv1a(uint64_t hash, uint64_t value) {
  for (unsigned byte = 0; byte < 8; ++byte) {
    hash ^= (value >> (byte * 8)) & 0xffull;
    hash *= kFnvPrime;
  }
  return hash;
}

/** FNV-1a over @p text's bytes, each read unsigned so a high-bit byte
 *  folds the same whether `char` is signed or not. */
inline uint64_t fnv1a(uint64_t hash, std::string_view text) {
  for (char c : text) {
    hash ^= (uint64_t)(unsigned char)c;
    hash *= kFnvPrime;
  }
  return hash;
}

/** Folds @p value into @p hash: the golden-ratio stir, which spreads a
 *  word that is nearly the same as the last one across the whole result
 *  instead of leaving the difference in the low bits. For accumulating
 *  a std::hash over the members of a key struct. */
inline size_t combine(size_t hash, uint32_t value) {
  hash ^= value + 0x9E3779B9u + (hash << 6u) + (hash >> 2u);
  return hash;
}

}  // namespace sigil::core::hash
