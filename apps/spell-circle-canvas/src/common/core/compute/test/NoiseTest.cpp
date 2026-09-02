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

TEST(Noise, TheAvalancheIsPinnedToItsExactWords) {
  EXPECT_EQ(noise::mix64(1u), 0x5692161d100b05e5ull);
  EXPECT_EQ(noise::mix64(42u), 0xa759ea27d4727622ull);
  EXPECT_EQ(noise::mix64(0xdeadbeefu), 0x4e062702ec929eeaull);
  EXPECT_EQ(noise::mix64(0xffffffffffffffffull), 0xb4d055fcf2cbbd7bull);
  // Zero is the one input the avalanche leaves alone — every round of it
  // is a shift, an xor and a multiply, and all three fix zero. That is
  // what the gamma is for: a counter starting at 0 is offset off the
  // fixed point before it is ever mixed.
  EXPECT_EQ(noise::mix64(0u), 0ull);
  EXPECT_NE(noise::mix64(noise::kMix64Gamma), 0ull);
}

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

TEST(Noise, TheXorshiftStepIsPinnedToItsExactWords) {
  uint32_t state = 1u;
  EXPECT_EQ(noise::xorshiftNext(state), 270369u);
  EXPECT_EQ(noise::xorshiftNext(state), 67634689u);
  EXPECT_EQ(noise::xorshiftNext(state), 2647435461u);
  state = 0x9E3779B9u;
  EXPECT_EQ(noise::xorshiftNext(state), 1359758873u);
  state = 0x2545F491u;
  EXPECT_EQ(noise::xorshiftNext(state), 3777279546u);
}

TEST(Noise, TheXorshiftStreamAdvancesInPlaceAndReturnsWhatItAdvancedTo) {
  uint32_t carried = 42u, mirror = 42u;
  const uint32_t drawn = noise::xorshiftNext(carried);
  EXPECT_EQ(drawn, carried);
  EXPECT_NE(carried, mirror);
  // Zero is the fixed point every shift shares: a stream that starts
  // there never leaves, which is why the seed must be any other word.
  uint32_t zero = 0u;
  EXPECT_EQ(noise::xorshiftNext(zero), 0u);
}

TEST(Noise, XorshiftUnitIsPinnedToItsExactFloats) {
  uint32_t state = 0x9E3779B9u;
  EXPECT_EQ(bits(noise::xorshiftUnitNext(state)), 0x3ea2188cu);
  EXPECT_EQ(bits(noise::xorshiftUnitNext(state)), 0x3f602e55u);
  EXPECT_EQ(bits(noise::xorshiftUnitNext(state)), 0x3ef7731eu);
  state = 1u;
  for (int draw = 0; draw < 4096; ++draw) {
    const float u = noise::xorshiftUnitNext(state);
    EXPECT_GE(u, 0.0f);
    EXPECT_LT(u, 1.0f);
  }
}

TEST(Noise, TheMix64StreamIsTheCounterSteppedByTheGammaAndAvalanched) {
  noise::Mix64Stream stream(42u);
  // The first word IS mix64(seed + gamma)'s high half: the stream adds
  // before it mixes, so the seed itself is never handed out.
  EXPECT_EQ(stream.bits(),
            (uint32_t)(noise::mix64(42u + noise::kMix64Gamma) >> 32u));
  EXPECT_EQ(stream.bits(), 0x28efe333u);
  EXPECT_EQ(stream.bits(), 0x47526757u);
}

TEST(Noise, TheMix64StreamsUnitFloatsArePinnedAndStayInRange) {
  noise::Mix64Stream stream(42u);
  EXPECT_EQ(bits(stream.unit()), 0x3f3dd732u);
  EXPECT_EQ(bits(stream.unit()), 0x3e23bf8cu);
  EXPECT_EQ(bits(stream.unit()), 0x3e8ea4ceu);
  noise::Mix64Stream walk(1u);
  for (int draw = 0; draw < 4096; ++draw) {
    const float u = walk.unit();
    EXPECT_GE(u, 0.0f);
    EXPECT_LT(u, 1.0f);
    const float s = walk.signedUnit();
    EXPECT_GE(s, -1.0f);
    EXPECT_LT(s, 1.0f);
    const float r = walk.range(-3.0f, 5.0f);
    EXPECT_GE(r, -3.0f);
    EXPECT_LT(r, 5.0f);
  }
}

TEST(Noise, TwoMix64StreamsOnOneSeedDrawTheSameSequence) {
  noise::Mix64Stream a(0xdeadbeefcafef00dull), b(0xdeadbeefcafef00dull);
  for (int draw = 0; draw < 64; ++draw) EXPECT_EQ(a.bits(), b.bits());
}
