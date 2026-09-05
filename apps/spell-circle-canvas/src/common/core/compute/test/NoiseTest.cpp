/** @file
 * The seeded mixers, pinned to the words and floats they produce.
 *
 * These bodies exist so that a second implementation of them agrees to
 * the bit: a GPU kernel reproduces the PCG three word for word, and a
 * point cook, a jitter and a shader's CPU twin all have to draw the same
 * number for the same index. A body that drifts cannot be caught by a
 * property — "in range", "not equal to its neighbour" and "the same
 * twice" all survive a different mixer, and every one of them is still
 * true of the wrong stream. Only the exact outputs catch it, so exact
 * outputs are what this file asserts, floats compared as bits so a value
 * one ulp away fails.
 */

#include <gtest/gtest.h>
#include <sigilcore/compute/Noise.h>

#include <cstdint>
#include <cstring>
#include <functional>
#include <string>

namespace {

using namespace sigil::core;

uint32_t bits(float f) {
  uint32_t b = 0;
  std::memcpy(&b, &f, sizeof b);
  return b;
}

/** One draw a header states a range for: how to take the next value, and
 *  the interval it promised. `highIncluded` says whether the top of the
 *  range is reachable — a signed hash reaches 1, a unit float does not. */
struct Draw {
  const char* name;
  std::function<float(uint32_t)> next;
  float low;
  float high;
  bool highIncluded;
};

std::string drawName(const testing::TestParamInfo<Draw>& info) {
  return info.param.name;
}

struct Draws : testing::TestWithParam<Draw> {};

}  // namespace

TEST(Noise, TheAvalancheIsTheSameWordsInEveryImplementation) {
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

TEST(Noise, TheStatelessHashIsTheSameFloatsInEveryImplementation) {
  EXPECT_EQ(bits(noise::hash(0u, 0u)), 0xbf4464a2u);
  EXPECT_EQ(bits(noise::hash(7u, 1u)), 0x3f430f78u);
  EXPECT_EQ(bits(noise::hash(7u, 2u)), 0xbf70191au);
  EXPECT_EQ(bits(noise::hash(42u, 99u)), 0xbe242270u);
  EXPECT_EQ(bits(noise::hash(0xffffffffu, 0xffffffffu)), 0xbf4f494au);
}

TEST_P(Draws, LandInTheRangeTheHeaderStatesForThem) {
  Draw draw = GetParam();  // copied: a stream carries its state in here
  for (uint32_t i = 0; i < 4096; ++i) {
    const float v = draw.next(i);
    EXPECT_GE(v, draw.low) << "draw " << i;
    if (draw.highIncluded)
      EXPECT_LE(v, draw.high) << "draw " << i;
    else
      EXPECT_LT(v, draw.high) << "draw " << i;
  }
}

INSTANTIATE_TEST_SUITE_P(
    Ranges, Draws,
    testing::Values(
        Draw{"TheStatelessHash",
             [](uint32_t i) { return noise::hash(11u, i); }, -1.0f, 1.0f, true},
        Draw{"ThePcgUnit", [](uint32_t i) { return noise::pcgUnit(i); }, 0.0f,
             1.0f, false},
        Draw{"TheXorshiftUnit",
             [state = uint32_t{1}](uint32_t) mutable {
               return noise::xorshiftUnitNext(state);
             },
             0.0f, 1.0f, false},
        Draw{"TheMix64StreamsUnit",
             [stream = noise::Mix64Stream(1u)](uint32_t) mutable {
               return stream.unit();
             },
             0.0f, 1.0f, false},
        Draw{"TheMix64StreamsSignedUnit",
             [stream = noise::Mix64Stream(1u)](uint32_t) mutable {
               return stream.signedUnit();
             },
             -1.0f, 1.0f, false},
        Draw{"TheMix64StreamsNamedRange",
             [stream = noise::Mix64Stream(1u)](uint32_t) mutable {
               return stream.range(-3.0f, 5.0f);
             },
             -3.0f, 5.0f, false}),
    drawName);

TEST(Noise, EachPcgStepIsTheSameWordsInEveryImplementation) {
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

TEST(Noise, ThePcgUnitFloatIsTheSameFloatsInEveryImplementation) {
  EXPECT_EQ(bits(noise::pcgUnit(0u)), 0x3f3b2fe2u);
  EXPECT_EQ(bits(noise::pcgUnit(1u)), 0x3f3eea3cu);
  EXPECT_EQ(bits(noise::pcgUnit(42u)), 0x3f7432ffu);
  EXPECT_EQ(bits(noise::pcgUnit(0xdeadbeefu)), 0x3e2665c8u);
}

TEST(Noise, TheStreamCarriesItsStateAndItsFirstWordIsTheStatelessHash) {
  uint32_t state = 42u;
  EXPECT_EQ(noise::pcgNext(state), noise::pcgHash(42u));
  EXPECT_EQ(state, noise::pcgAdvance(42u));
  EXPECT_EQ(bits(noise::pcgUnitNext(state)), 0x3e8cbceeu);
  EXPECT_EQ(state, 1561666408u);
}

TEST(Noise, TheXorshiftStepIsTheSameWordsInEveryImplementation) {
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

TEST(Noise, TheXorshiftUnitFloatIsTheSameFloatsInEveryImplementation) {
  uint32_t state = 0x9E3779B9u;
  EXPECT_EQ(bits(noise::xorshiftUnitNext(state)), 0x3ea2188cu);
  EXPECT_EQ(bits(noise::xorshiftUnitNext(state)), 0x3f602e55u);
  EXPECT_EQ(bits(noise::xorshiftUnitNext(state)), 0x3ef7731eu);
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

TEST(Noise, TheMix64StreamsUnitFloatsAreTheSameFloatsInEveryImplementation) {
  noise::Mix64Stream stream(42u);
  EXPECT_EQ(bits(stream.unit()), 0x3f3dd732u);
  EXPECT_EQ(bits(stream.unit()), 0x3e23bf8cu);
  EXPECT_EQ(bits(stream.unit()), 0x3e8ea4ceu);
}

TEST(Noise, TwoMix64StreamsOnOneSeedDrawTheSameSequence) {
  noise::Mix64Stream a(0xdeadbeefcafef00dull), b(0xdeadbeefcafef00dull);
  for (int draw = 0; draw < 64; ++draw) EXPECT_EQ(a.bits(), b.bits());
}

TEST(Noise, ADifferentSeedIsADifferentStream) {
  // The seed is the first argument for a reason: two consumers drawing at
  // the same index must be able to differ by naming different seeds, or
  // every seeded figure in a scene is the same figure.
  for (uint32_t i = 0; i < 64; ++i)
    EXPECT_NE(noise::hash(7u, i), noise::hash(8u, i)) << "index " << i;
  EXPECT_NE(noise::pcgUnit(7u), noise::pcgUnit(8u));
}
