/** @file
 * The seeded stream and the shapes drawn from it.
 *
 * Two kinds of claim, and the file is organised by them. A SEQUENCE
 * claim pins exact words: these bodies exist so that a scatter re-rolls
 * identically on another machine and so a hand-carried mixer can be
 * replaced by a stream without the picture moving, and neither survives
 * a drifted word. A DISTRIBUTION claim pins a moment within a tolerance,
 * because the shape of a distribution is what a caller asked for and a
 * mean that is right to three places over a hundred thousand draws is
 * the only evidence that the shape is the one named.
 */

#include <gtest/gtest.h>
#include <sigilcore/compute/Chance.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <span>
#include <vector>

namespace {

using namespace sigil::core;
using chance::Stream;

/** The mean and the variance of a run of draws, in double so the
 *  tolerance a case states is about the distribution and not about the
 *  accumulation. */
struct Moments {
  double mean = 0;
  double variance = 0;
};

Moments momentsOf(const std::vector<double>& values) {
  const double mean = std::accumulate(values.begin(), values.end(), 0.0) /
                      (double)values.size();
  double sum = 0;
  for (double v : values) sum += (v - mean) * (v - mean);
  return {mean, sum / (double)values.size()};
}

template <typename Shape>
Moments drawMoments(Stream stream, const Shape& shape, int count) {
  std::vector<double> values;
  values.reserve((size_t)count);
  for (int i = 0; i < count; ++i)
    values.push_back((double)stream.sample(shape));
  return momentsOf(values);
}

}  // namespace

// ---- the sequences --------------------------------------------------------

TEST(Stream, IsTheMixerItNamesWordForWord) {
  // The point of the stream is that it is not a fourth mixer: a caller
  // replacing a hand-carried state with one of these keeps its picture.
  Stream pcg = Stream::pcg(7);
  uint32_t pcgState = 7;
  Stream mix = Stream::mix64(7);
  noise::Mix64Stream mixState(7);
  Stream xorshift = Stream::xorshift(7);
  uint32_t xorshiftState = 7;
  for (int i = 0; i < 64; ++i) {
    EXPECT_EQ(pcg.bits(), noise::pcgNext(pcgState)) << "draw " << i;
    EXPECT_EQ(mix.bits(), mixState.bits()) << "draw " << i;
    EXPECT_EQ(xorshift.bits(), noise::xorshiftNext(xorshiftState))
        << "draw " << i;
  }
}

TEST(Stream, AnswersTheSameWordsInEveryImplementation) {
  Stream stream = Stream::pcg(2026);
  const std::array<uint32_t, 6> expected = {0x2c708824u, 0x40031d3cu,
                                            0x6efead76u, 0xf4b986dbu,
                                            0xdd2c6787u, 0xa407145eu};
  for (size_t i = 0; i < expected.size(); ++i)
    EXPECT_EQ(stream.bits(), expected[i]) << "draw " << i;
}

TEST(Stream, TheUnitDrawIsTheWordSqueezedThroughAFloatsExactBits) {
  Stream words = Stream::pcg(2026);
  Stream units = Stream::pcg(2026);
  for (int i = 0; i < 4096; ++i) {
    const uint32_t word = words.bits();
    const float unit = units.unit();
    EXPECT_FLOAT_EQ(unit, (float)(word >> 8u) * (1.0f / 16777216.0f));
    // The range the header states, and the reason it is not
    // `pcgUnitNext`: that one can round up to exactly 1.
    EXPECT_GE(unit, 0.0f);
    EXPECT_LT(unit, 1.0f);
  }
}

TEST(Stream, ACopyDrawsTheSameContinuationAndReseedRewindsTheRun) {
  Stream stream = Stream::pcg(11);
  for (int i = 0; i < 5; ++i) stream.bits();
  Stream copy = stream;
  for (int i = 0; i < 16; ++i) EXPECT_EQ(stream.bits(), copy.bits());

  Stream fresh = Stream::pcg(11);
  stream.reseed(11);
  EXPECT_EQ(stream.drawn(), 0u);
  for (int i = 0; i < 16; ++i) EXPECT_EQ(stream.bits(), fresh.bits());
}

TEST(Stream, TheXorshiftStateThatFixesItselfIsNeverTheOneItStartsFrom) {
  // All three shifts fix zero, so a stream seeded there would answer
  // zero for ever; one is the nearest state that does not.
  Stream stream = Stream::xorshift(0);
  EXPECT_NE(stream.bits(), 0u);
  EXPECT_NE(stream.bits(), 0u);
}

TEST(Sequences, TheRadicalInverseIsTheDigitsMirroredAboutThePoint) {
  Stream base2 = Stream::halton(2);
  Stream sobol = Stream::sobol();
  // Halton in base two and the first Sobol dimension are the same
  // numbers reached two ways: a division, and the index's bits reversed.
  const std::array<uint32_t, 5> halves = {0x00000000u, 0x80000000u, 0x40000000u,
                                          0xc0000000u, 0x20000000u};
  for (size_t i = 0; i < halves.size(); ++i) {
    EXPECT_EQ(sobol.bits(), halves[i]) << "term " << i;
    EXPECT_EQ(base2.bits(), halves[i]) << "term " << i;
  }

  Stream base3 = Stream::halton(3);
  const std::array<uint32_t, 4> thirds = {0x00000000u, 0x55555555u, 0xaaaaaaaau,
                                          0x1c71c71cu};
  for (size_t i = 0; i < thirds.size(); ++i)
    EXPECT_EQ(base3.bits(), thirds[i]) << "term " << i;
}

TEST(Sequences, TheGoldenRecurrenceIsTheCounterSteppedByTheGoldenWord) {
  Stream stream = Stream::golden(0);
  const std::array<uint32_t, 4> expected = {0x9e3779b9u, 0x3c6ef372u,
                                            0xdaa66d2cu, 0x78dde6e5u};
  for (size_t i = 0; i < expected.size(); ++i)
    EXPECT_EQ(stream.bits(), expected[i]) << "term " << i;
}

TEST(Sequences, EachStratifiedDrawLandsInItsOwnCellAndTheLadderRepeats) {
  const int strata = 8;
  Stream stream = Stream::stratified(strata, 9);
  for (int lap = 0; lap < 3; ++lap)
    for (int cell = 0; cell < strata; ++cell) {
      const float drawn = stream.unit();
      EXPECT_GE(drawn, (float)cell / (float)strata) << "lap " << lap;
      EXPECT_LT(drawn, (float)(cell + 1) / (float)strata) << "lap " << lap;
    }
}

TEST(Sequences, ALowDiscrepancyRunFillsTheIntervalMoreEvenlyThanAMixerDoes) {
  // What the sequence sources exist for, stated as the only thing that
  // tells them apart from a mixer: the largest gap left after n terms.
  auto largestGap = [](Stream stream, int count) {
    std::vector<double> drawn;
    drawn.reserve((size_t)count);
    for (int i = 0; i < count; ++i) drawn.push_back((double)stream.unit());
    std::sort(drawn.begin(), drawn.end());
    double gap = drawn.front();
    for (size_t i = 1; i < drawn.size(); ++i)
      gap = std::max(gap, drawn[i] - drawn[i - 1]);
    return std::max(gap, 1.0 - drawn.back());
  };
  const int count = 256;
  EXPECT_LT(largestGap(Stream::halton(2), count),
            largestGap(Stream::pcg(3), count));
  EXPECT_LT(largestGap(Stream::golden(0), count),
            largestGap(Stream::pcg(3), count));
}

// ---- the shapes -----------------------------------------------------------

TEST(Shapes, TheUniformDrawIsFlatAcrossTheIntervalItNames) {
  const Moments moments =
      drawMoments(Stream::pcg(5), chance::Uniform{-2.0f, 6.0f}, 200000);
  // A flat run over [a, b) has mean (a+b)/2 and variance (b-a)^2/12.
  EXPECT_NEAR(moments.mean, 2.0, 0.02);
  EXPECT_NEAR(moments.variance, 64.0 / 12.0, 0.05);

  Stream stream = Stream::pcg(5);
  for (int i = 0; i < 4096; ++i) {
    const float drawn = stream.sample(chance::Uniform{-2.0f, 6.0f});
    EXPECT_GE(drawn, -2.0f);
    EXPECT_LT(drawn, 6.0f);
  }
}

TEST(Shapes, TheGaussianDrawCarriesTheMeanAndDeviationItNames) {
  const Moments moments =
      drawMoments(Stream::pcg(1), chance::Gaussian{3.0f, 2.0f}, 200000);
  EXPECT_NEAR(moments.mean, 3.0, 0.02);
  EXPECT_NEAR(std::sqrt(moments.variance), 2.0, 0.02);

  // The shape of it, not only its width: about two thirds of a normal
  // run lies within one deviation and about a twentieth beyond two.
  Stream stream = Stream::pcg(1);
  int within = 0, beyond = 0;
  const int count = 100000;
  for (int i = 0; i < count; ++i) {
    const double z = (double)stream.sample(chance::Gaussian{0.0f, 1.0f});
    if (std::abs(z) <= 1.0) ++within;
    if (std::abs(z) > 2.0) ++beyond;
  }
  EXPECT_NEAR((double)within / count, 0.6827, 0.01);
  EXPECT_NEAR((double)beyond / count, 0.0455, 0.005);
}

TEST(Shapes, TheGaussianSpareIsSpentBeforeAnotherPairIsDrawn) {
  // The polar method makes two normals at a time; the second is held on
  // the stream that paid for it, so two streams at the same place stay
  // at the same place.
  Stream first = Stream::pcg(21);
  Stream second = Stream::pcg(21);
  for (int i = 0; i < 32; ++i)
    EXPECT_FLOAT_EQ(first.sample(chance::Gaussian{}),
                    second.sample(chance::Gaussian{}));
}

TEST(Shapes, TheExponentialWaitIsMemorylessWithTheMeanItsRateNames) {
  const Moments moments =
      drawMoments(Stream::pcg(2), chance::Exponential{2.0f}, 200000);
  // Mean 1/rate, and a variance of the same number squared.
  EXPECT_NEAR(moments.mean, 0.5, 0.01);
  EXPECT_NEAR(moments.variance, 0.25, 0.02);

  Stream stream = Stream::pcg(2);
  for (int i = 0; i < 4096; ++i)
    EXPECT_GE(stream.sample(chance::Exponential{2.0f}), 0.0f);
  EXPECT_FLOAT_EQ(stream.sample(chance::Exponential{0.0f}), 0.0f);
}

TEST(Shapes, AWeightedChoiceIsTakenInProportionToItsShareOfTheTotal) {
  const std::array<float, 4> weights = {1.0f, 0.0f, 3.0f, 4.0f};
  Stream stream = Stream::pcg(4);
  std::array<int, 4> taken = {0, 0, 0, 0};
  const int count = 80000;
  for (int i = 0; i < count; ++i) {
    const auto chosen = stream.sample(chance::Weighted{weights});
    ASSERT_TRUE(chosen.has_value());
    ++taken[*chosen];
  }
  EXPECT_NEAR((double)taken[0] / count, 0.125, 0.01);
  EXPECT_EQ(taken[1], 0);
  EXPECT_NEAR((double)taken[2] / count, 0.375, 0.01);
  EXPECT_NEAR((double)taken[3] / count, 0.5, 0.01);
}

TEST(Shapes, ADistributionWithNoPositiveWeightInItHasNoAnswer) {
  const std::array<float, 3> none = {0.0f, -1.0f, 0.0f};
  Stream stream = Stream::pcg(4);
  EXPECT_FALSE(stream.sample(chance::Weighted{none}).has_value());
  EXPECT_FALSE(stream.sample(chance::Weighted{}).has_value());
}

TEST(Shuffle, IsAPermutationAndEveryPlaceIsReachedAboutEquallyOften) {
  std::array<int, 6> items = {0, 1, 2, 3, 4, 5};
  Stream stream = Stream::pcg(8);
  std::array<std::array<int, 6>, 6> landed = {};
  const int count = 60000;
  for (int i = 0; i < count; ++i) {
    std::iota(items.begin(), items.end(), 0);
    chance::shuffle(stream, std::span<int>(items));
    std::array<bool, 6> seen = {};
    for (size_t place = 0; place < items.size(); ++place) {
      ASSERT_GE(items[place], 0);
      ASSERT_LT(items[place], 6);
      EXPECT_FALSE(seen[(size_t)items[place]]) << "value twice";
      seen[(size_t)items[place]] = true;
      ++landed[(size_t)items[place]][place];
    }
  }
  for (const auto& places : landed)
    for (int hits : places) EXPECT_NEAR((double)hits / count, 1.0 / 6.0, 0.01);
}

TEST(Reservoir, HoldsEveryItemWhileTheRunIsShorterThanItIs) {
  chance::Reservoir reservoir(4);
  Stream stream = Stream::pcg(6);
  for (size_t i = 0; i < 4; ++i) {
    const auto slot = reservoir.offer(stream);
    ASSERT_TRUE(slot.has_value());
    EXPECT_EQ(*slot, i);
  }
  EXPECT_EQ(reservoir.seen(), 4u);
  ASSERT_EQ(reservoir.kept().size(), 4u);
  for (size_t i = 0; i < 4; ++i) EXPECT_EQ(reservoir.kept()[i], i);
}

TEST(Reservoir, GivesEveryItemOfARunTheSameChanceOfBeingHeld) {
  // The claim that makes it worth having: after n offers, each of the n
  // items is in the reservoir with probability keep/n, whatever n was —
  // and n is not known when the first item is offered.
  const size_t keep = 3, length = 20;
  Stream stream = Stream::pcg(12);
  std::vector<int> held(length, 0);
  const int runs = 60000;
  for (int run = 0; run < runs; ++run) {
    chance::Reservoir reservoir(keep);
    for (size_t i = 0; i < length; ++i) reservoir.offer(stream);
    EXPECT_EQ(reservoir.kept().size(), keep);
    for (size_t index : reservoir.kept()) ++held[index];
  }
  for (size_t i = 0; i < length; ++i)
    EXPECT_NEAR((double)held[i] / runs, (double)keep / (double)length, 0.01)
        << "item " << i;
}

TEST(Chance, IsOneSeedThatReRollsEveryNamedStreamTogether) {
  const chance::Chance sheet{.seed = 1848};
  // Two named uses are independent of each other, and adding a third
  // leaves both alone — which is what makes the name the key.
  Stream stars = sheet.stream("stars");
  Stream grain = sheet.stream("grain");
  bool differed = false;
  for (int i = 0; i < 16; ++i) differed |= stars.bits() != grain.bits();
  EXPECT_TRUE(differed);

  Stream again = sheet.stream("stars");
  Stream first = sheet.stream("stars");
  for (int i = 0; i < 16; ++i) EXPECT_EQ(again.bits(), first.bits());

  const chance::Chance rerolled{.seed = 1849};
  Stream moved = rerolled.stream("stars");
  Stream held = sheet.stream("stars");
  differed = false;
  for (int i = 0; i < 16; ++i) differed |= moved.bits() != held.bits();
  EXPECT_TRUE(differed);
}

TEST(Chance, CarriesTheSourceSoALookIsChosenOnceAndComparesExactly) {
  const chance::Chance halton{
      .seed = 5, .source = chance::Source::Halton, .parameter = 3};
  Stream carried = halton.stream();
  Stream named = Stream::halton(3, 5);
  for (int i = 0; i < 16; ++i) EXPECT_EQ(carried.bits(), named.bits());

  // Every member is a plain number, so the token compares exactly and a
  // memo keyed on it can be skipped.
  EXPECT_EQ(halton,
            (chance::Chance{
                .seed = 5, .source = chance::Source::Halton, .parameter = 3}));
  EXPECT_NE(halton,
            (chance::Chance{
                .seed = 5, .source = chance::Source::Halton, .parameter = 2}));
}

TEST(ChanceStream, ANormalDrawTerminatesOnEverySourceTheHeaderNames) {
  // The polar method rejects the pairs outside the unit disc, and a
  // source that answers one number for ever offers the same rejected
  // pair for ever. Every source named by `Source` must still answer.
  const chance::Stream sources[] = {
      chance::Stream::pcg(7),        chance::Stream::mix64(7),
      chance::Stream::xorshift(7),   chance::Stream::halton(2),
      chance::Stream::halton(3, 5),  chance::Stream::sobol(),
      chance::Stream::golden(7),     chance::Stream::stratified(0),
      chance::Stream::stratified(1), chance::Stream::stratified(8, 3)};
  for (chance::Stream stream : sources) {
    for (int i = 0; i < 64; ++i) {
      const float drawn = stream.normal();
      EXPECT_TRUE(std::isfinite(drawn))
          << "source " << (int)stream.source() << " draw " << i;
    }
  }
}

TEST(ChanceStream, ASeedWiderThanTheStateStillWalksItsOwnRun) {
  // The two 32-bit mixers step a 32-bit word. Two seeds an even 2^32
  // apart would walk one run if the high half were dropped.
  chance::Stream low = chance::Stream::pcg(7);
  chance::Stream high = chance::Stream::pcg(7 + (1ull << 32));
  EXPECT_NE(low.bits(), high.bits());
  chance::Stream lowX = chance::Stream::xorshift(7);
  chance::Stream highX = chance::Stream::xorshift(7 + (1ull << 32));
  EXPECT_NE(lowX.bits(), highX.bits());
  // A seed that fits in 32 bits is where it always was.
  chance::Stream plain = chance::Stream::pcg(7);
  uint32_t state = 7;
  EXPECT_EQ(plain.bits(), noise::pcgNext(state));
}
