/** @file
 * The seeded mixers, pinned to the words and floats they produce.
 *
 * Stored renders are seeded through these functions, and a GPU kernel
 * reproduces the PCG three word for word. A body that drifts therefore
 * cannot be caught by a property — "in range", "not equal to its
 * neighbour" and "the same twice" all survive a different mixer. Only
 * the exact outputs catch it, so exact outputs are what this file
 * asserts, floats compared as bits so a value one ulp away fails.
 */

#include <gtest/gtest.h>
#include <sigilcore/compute/Noise.h>

#include <cstdint>
#include <cstring>

namespace {

using namespace sigil::core;

uint32_t bits(float f) {
  uint32_t b = 0;
  std::memcpy(&b, &f, sizeof b);
  return b;
}

}  // namespace

TEST(Noise, HashIsPinnedToItsExactFloats) {
  EXPECT_EQ(bits(noise::hash(0u, 0u)), 0xbf4464a2u);
  EXPECT_EQ(bits(noise::hash(7u, 1u)), 0x3f430f78u);
  EXPECT_EQ(bits(noise::hash(7u, 2u)), 0xbf70191au);
  EXPECT_EQ(bits(noise::hash(42u, 99u)), 0xbe242270u);
  EXPECT_EQ(bits(noise::hash(0xffffffffu, 0xffffffffu)), 0xbf4f494au);
}

TEST(Noise, HashLandsInItsStatedRange) {
  for (uint32_t i = 0; i < 4096; ++i) {
    const float v = noise::hash(11u, i);
    EXPECT_GE(v, -1.0f);
    EXPECT_LE(v, 1.0f);
  }
}

TEST(Noise, ThePcgStepsArePinned) {
  EXPECT_EQ(noise::pcgAdvance(0u), 2891336453u);
  EXPECT_EQ(noise::pcgAdvance(1u), 3639132858u);
  EXPECT_EQ(noise::pcgAdvance(42u), 4234014391u);
  EXPECT_EQ(noise::pcgAdvance(0xdeadbeefu), 3644678912u);

  EXPECT_EQ(noise::pcgMix(0u), 0u);
  EXPECT_EQ(noise::pcgMix(1u), 277803675u);
  EXPECT_EQ(noise::pcgMix(42u), 2522215345u);
  EXPECT_EQ(noise::pcgMix(0xdeadbeefu), 4130710537u);

  EXPECT_EQ(noise::pcgHash(0u), 129708002u);
  EXPECT_EQ(noise::pcgHash(1u), 2831084092u);
  EXPECT_EQ(noise::pcgHash(42u), 1223963391u);
  EXPECT_EQ(noise::pcgHash(0xdeadbeefu), 1730779506u);
}

TEST(Noise, PcgHashIsTheAdvanceThenTheMix) {
  for (uint32_t x : {0u, 1u, 42u, 7u, 0xdeadbeefu})
    EXPECT_EQ(noise::pcgHash(x), noise::pcgMix(noise::pcgAdvance(x)));
}

TEST(Noise, PcgUnitIsPinnedToItsExactFloats) {
  EXPECT_EQ(bits(noise::pcgUnit(0u)), 0x3f3b2fe2u);
  EXPECT_EQ(bits(noise::pcgUnit(1u)), 0x3f3eea3cu);
  EXPECT_EQ(bits(noise::pcgUnit(42u)), 0x3f7432ffu);
  EXPECT_EQ(bits(noise::pcgUnit(0xdeadbeefu)), 0x3e2665c8u);
  for (uint32_t x = 0; x < 4096; ++x) {
    EXPECT_GE(noise::pcgUnit(x), 0.0f);
    EXPECT_LT(noise::pcgUnit(x), 1.0f);
  }
}

TEST(Noise, TheStreamCarriesItsStateAndItsFirstWordIsTheStatelessHash) {
  uint32_t state = 42u;
  EXPECT_EQ(noise::pcgNext(state), noise::pcgHash(42u));
  EXPECT_EQ(state, noise::pcgAdvance(42u));
  EXPECT_EQ(bits(noise::pcgUnitNext(state)), 0x3e8cbceeu);
  EXPECT_EQ(state, 1561666408u);
}
