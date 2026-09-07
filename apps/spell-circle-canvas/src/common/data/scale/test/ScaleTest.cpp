/** Every case here asserts one thing the header promises, against a
 *  closed form worked out by hand rather than against whatever the code
 *  happens to answer. */

#include <gtest/gtest.h>
#include <sigildata/scale/Scale.h>

#include <cmath>
#include <string>
#include <vector>

using namespace sigil::data;

namespace {

TEST(DataScale, LinearMapsProportionallyAndInvertsBack) {
  const Scale x{.domain = {0, 100}, .range = {40, 760}};
  EXPECT_DOUBLE_EQ(40.0, x(0.0));
  EXPECT_DOUBLE_EQ(400.0, x(50.0));
  EXPECT_DOUBLE_EQ(760.0, x(100.0));
  EXPECT_DOUBLE_EQ(50.0, x.invert(400.0));
  EXPECT_DOUBLE_EQ(0.5, x.position(50.0));
}

TEST(DataScale, AReversedRangeRunsBackwards) {
  const Scale y{.domain = {0, 10}, .range = {500, 100}};
  EXPECT_DOUBLE_EQ(500.0, y(0.0));
  EXPECT_DOUBLE_EQ(100.0, y(10.0));
  EXPECT_DOUBLE_EQ(300.0, y(5.0));
  EXPECT_DOUBLE_EQ(5.0, y.invert(300.0));
}

TEST(DataScale, ADegenerateDomainAnswersTheMiddleOfTheRange) {
  const Scale flat{.domain = {7, 7}, .range = {0, 200}};
  EXPECT_DOUBLE_EQ(100.0, flat(7.0));
  EXPECT_DOUBLE_EQ(100.0, flat(1000.0));
}

TEST(DataScale, OverflowDecidesWhatHappensOutsideTheDomain) {
  const Scale extend{.domain = {0, 10}, .range = {0, 1}};
  EXPECT_DOUBLE_EQ(1.5, extend(15.0));

  const Scale clamp{
      .domain = {0, 10}, .range = {0, 1}, .overflow = Overflow::Clamp};
  EXPECT_DOUBLE_EQ(1.0, clamp(15.0));
  EXPECT_DOUBLE_EQ(0.0, clamp(-4.0));

  const Scale wrap{
      .domain = {0, 10}, .range = {0, 1}, .overflow = Overflow::Wrap};
  EXPECT_DOUBLE_EQ(0.5, wrap(15.0));
  EXPECT_DOUBLE_EQ(0.6, wrap(-4.0));

  const Scale pingPong{
      .domain = {0, 10}, .range = {0, 1}, .overflow = Overflow::PingPong};
  EXPECT_DOUBLE_EQ(0.5, pingPong(15.0));
  EXPECT_DOUBLE_EQ(0.5, pingPong(25.0));
  EXPECT_DOUBLE_EQ(0.4, pingPong(-4.0));
}

TEST(DataScale, LogIsProportionalToTheLogarithmInItsBase) {
  const Scale decades{
      .domain = {1, 1000}, .range = {0, 3}, .transform = Transform::Log};
  EXPECT_NEAR(0.0, decades(1.0), 1e-12);
  EXPECT_NEAR(1.0, decades(10.0), 1e-12);
  EXPECT_NEAR(2.0, decades(100.0), 1e-12);
  EXPECT_NEAR(std::pow(10.0, 1.5), decades.invert(1.5), 1e-9);

  const Scale octaves{.domain = {1, 8},
                      .range = {0, 3},
                      .transform = Transform::Log,
                      .base = 2.0};
  EXPECT_NEAR(2.0, octaves(4.0), 1e-12);
}

TEST(DataScale, SqrtIsPowAtAHalfAndIsWhatAnAreaReadsAs) {
  const Scale radius{
      .domain = {0, 1200}, .range = {0, 90}, .transform = Transform::Sqrt};
  EXPECT_DOUBLE_EQ(45.0, radius(300.0));  // a quarter of the area
  EXPECT_DOUBLE_EQ(90.0, radius(1200.0));
  EXPECT_NEAR(300.0, radius.invert(45.0), 1e-9);

  const Scale half{.domain = {0, 1200},
                   .range = {0, 90},
                   .transform = Transform::Pow,
                   .exponent = 0.5};
  EXPECT_DOUBLE_EQ(radius(300.0), half(300.0));
}

TEST(DataScale, PowRaisesTheDomainBeforeMapping) {
  const Scale squared{.domain = {0, 10},
                      .range = {0, 100},
                      .transform = Transform::Pow,
                      .exponent = 2.0};
  EXPECT_DOUBLE_EQ(25.0, squared(5.0));
  EXPECT_NEAR(5.0, squared.invert(25.0), 1e-9);
}

TEST(DataScale, SymlogSpreadsADomainThatHoldsZeroAndBothSigns) {
  const Scale s{
      .domain = {-100, 100}, .range = {0, 1}, .transform = Transform::Symlog};
  EXPECT_DOUBLE_EQ(0.5, s(0.0));
  EXPECT_DOUBLE_EQ(0.0, s(-100.0));
  EXPECT_DOUBLE_EQ(1.0, s(100.0));
  const double expected =
      (std::log1p(10.0) + std::log1p(100.0)) / (2.0 * std::log1p(100.0));
  EXPECT_NEAR(expected, s(10.0), 1e-12);
  EXPECT_NEAR(10.0, s.invert(s(10.0)), 1e-9);
}

TEST(DataScale, ALogDomainThatTouchesZeroHasNoAnswerAndNoLadder) {
  const Scale touching{
      .domain = {0, 100}, .range = {0, 1}, .transform = Transform::Log};
  EXPECT_TRUE(std::isnan(touching(10.0)));
  EXPECT_TRUE(std::isnan(touching(0.0)));
  EXPECT_TRUE(touching.ticks(5).empty());
  EXPECT_EQ(touching.domain, touching.nice(5).domain);

  const Scale crossing{
      .domain = {-10, 10}, .range = {0, 1}, .transform = Transform::Log};
  EXPECT_TRUE(std::isnan(crossing(1.0)));
  EXPECT_TRUE(crossing.ticks(5).empty());
}

TEST(DataScale, APropWithNoMappingReadsLikeALogDomainThatTouchesZero) {
  // Every transform whose own prop leaves it without a mapping answers
  // the same way: not a number, no ladder, and a domain left alone.
  const Scale flat{.domain = {-100, 100},
                   .range = {0, 1},
                   .transform = Transform::Symlog,
                   .threshold = 0.0};
  EXPECT_TRUE(std::isnan(flat(10.0)));
  EXPECT_TRUE(std::isnan(flat.invert(0.5)));
  EXPECT_TRUE(flat.ticks(5).empty());
  EXPECT_DOUBLE_EQ(0.0, flat.tickStep(5));
  EXPECT_EQ(flat.domain, flat.nice(5).domain);

  const Scale collapsed{.domain = {0, 10},
                        .range = {0, 100},
                        .transform = Transform::Pow,
                        .exponent = 0.0};
  EXPECT_TRUE(std::isnan(collapsed(5.0)));
  EXPECT_TRUE(std::isnan(collapsed.invert(50.0)));
  EXPECT_TRUE(collapsed.ticks(5).empty());
  EXPECT_EQ(collapsed.domain, collapsed.nice(5).domain);

  const Scale unit{.domain = {1, 1000},
                   .range = {0, 3},
                   .transform = Transform::Log,
                   .base = 1.0};
  EXPECT_TRUE(std::isnan(unit(10.0)));
  EXPECT_TRUE(unit.ticks(5).empty());

  // The neighbouring props still map, so the guard costs nothing that
  // has an answer.
  const Scale near{.domain = {-100, 100},
                   .range = {0, 1},
                   .transform = Transform::Symlog,
                   .threshold = 0.001};
  EXPECT_DOUBLE_EQ(0.5, near(0.0));
  EXPECT_FALSE(std::isnan(near(10.0)));
}

TEST(DataScale, ThresholdReadsItsCutsInTheOrderTheyAreGiven) {
  // A value's slot is how many leading cuts it is at or past, so a list
  // that does not ascend has slots that overlap and it is the caller
  // who sorts.
  const Scale jumbled{.domain = {0, 40},
                      .range = {0, 2},
                      .transform = Transform::Threshold,
                      .thresholds = {30, 10}};
  EXPECT_EQ(0, jumbled.slot(5.0));
  EXPECT_EQ(0, jumbled.slot(20.0));  // past the second cut, not the first
  EXPECT_EQ(2, jumbled.slot(35.0));
  EXPECT_EQ(std::vector<double>({30, 10}), jumbled.ticks());
}

TEST(DataScale, BandGivesEveryEntryAWidthAndPaddingEatsIntoIt) {
  const Scale plain{
      .range = {0, 100}, .transform = Transform::Band, .steps = 4};
  EXPECT_DOUBLE_EQ(25.0, plain.stepWidth());
  EXPECT_DOUBLE_EQ(25.0, plain.bandwidth());
  EXPECT_DOUBLE_EQ(0.0, plain(0.0));
  EXPECT_DOUBLE_EQ(75.0, plain(3.0));
  EXPECT_DOUBLE_EQ(3.0, plain.invert(80.0));

  const Scale gapped{.range = {0, 100},
                     .transform = Transform::Band,
                     .steps = 4,
                     .padding = 0.2};
  EXPECT_NEAR(100.0 / 3.8, gapped.stepWidth(), 1e-12);
  EXPECT_NEAR(100.0 / 3.8 * 0.8, gapped.bandwidth(), 1e-12);
  EXPECT_NEAR(0.0, gapped(0.0), 1e-12);

  const Scale inset{.range = {0, 100},
                    .transform = Transform::Band,
                    .steps = 4,
                    .outerPadding = 0.5};
  EXPECT_DOUBLE_EQ(20.0, inset.stepWidth());
  EXPECT_DOUBLE_EQ(10.0, inset(0.0));
}

TEST(DataScale, OrdinalIsTheIthOfNAndPointIsItWithOuterPadding) {
  const Scale ordinal{
      .range = {0, 1}, .transform = Transform::Ordinal, .steps = 5};
  EXPECT_DOUBLE_EQ(0.0, ordinal(0.0));
  EXPECT_DOUBLE_EQ(0.5, ordinal(2.0));
  EXPECT_DOUBLE_EQ(1.0, ordinal(4.0));
  EXPECT_DOUBLE_EQ(0.0, ordinal.bandwidth());

  const Scale point{.range = {0, 1},
                    .transform = Transform::Point,
                    .steps = 5,
                    .outerPadding = 1.0};
  EXPECT_NEAR(1.0 / 6.0, point.stepWidth(), 1e-12);
  EXPECT_NEAR(1.0 / 6.0, point(0.0), 1e-12);
  EXPECT_NEAR(5.0 / 6.0, point(4.0), 1e-12);
}

TEST(DataScale, ASingleEntryStandsAtTheStartOfItsRange) {
  const Scale one{
      .range = {0, 100}, .transform = Transform::Ordinal, .steps = 1};
  EXPECT_DOUBLE_EQ(0.0, one(0.0));
  EXPECT_DOUBLE_EQ(0.0, one.invert(50.0));
}

TEST(DataScale, QuantizeCutsTheDomainIntoEqualBands) {
  const Scale q{.domain = {0, 1},
                .range = {0, 4},
                .transform = Transform::Quantize,
                .steps = 5};
  EXPECT_EQ(0, q.slot(0.0));
  EXPECT_EQ(1, q.slot(0.25));
  EXPECT_EQ(4, q.slot(0.99));
  EXPECT_EQ(4, q.slot(1.5));  // held inside, not wrapped round
  EXPECT_EQ(0, q.slot(-3.0));
  EXPECT_DOUBLE_EQ(1.0, q(0.25));
  EXPECT_DOUBLE_EQ(0.2, q.invert(1.0));

  const std::vector<double> cuts = q.ticks();
  ASSERT_EQ(4u, cuts.size());
  EXPECT_DOUBLE_EQ(0.2, cuts[0]);
  EXPECT_DOUBLE_EQ(0.8, cuts[3]);
}

TEST(DataScale, ThresholdCutsWhereItIsToldAndItsFirstSlotIsUnbounded) {
  const Scale t{.domain = {0, 40},
                .range = {0, 3},
                .transform = Transform::Threshold,
                .thresholds = {10, 20, 30}};
  EXPECT_EQ(0, t.slot(5.0));
  EXPECT_EQ(1, t.slot(10.0));
  EXPECT_EQ(1, t.slot(19.9));
  EXPECT_EQ(3, t.slot(35.0));
  EXPECT_DOUBLE_EQ(3.0, t(35.0));
  EXPECT_EQ(std::vector<double>({10, 20, 30}), t.ticks());
  EXPECT_TRUE(std::isinf(t.invert(0.0)));
  EXPECT_DOUBLE_EQ(20.0, t.invert(2.0));
}

TEST(DataScale, TimeIsLinearInSecondsAndTicksOnClockUnits) {
  const Scale t{
      .domain = {0, 3600}, .range = {0, 1}, .transform = Transform::Time};
  EXPECT_DOUBLE_EQ(0.5, t(1800.0));
  EXPECT_DOUBLE_EQ(900.0, t.tickStep(6));
  EXPECT_EQ(std::vector<double>({0, 900, 1800, 2700, 3600}), t.ticks(6));
}

TEST(DataScale, TicksAreTheMultiplesOfAReadableStepInsideTheDomain) {
  const Scale ten{.domain = {0, 10}};
  const std::vector<double> whole = ten.ticks(10);
  ASSERT_EQ(11u, whole.size());
  EXPECT_DOUBLE_EQ(0.0, whole.front());
  EXPECT_DOUBLE_EQ(10.0, whole.back());
  EXPECT_DOUBLE_EQ(1.0, ten.tickStep(10));

  const Scale unit{.domain = {0, 1}};
  EXPECT_EQ(std::vector<double>({0.0, 0.2, 0.4, 0.6, 0.8, 1.0}), unit.ticks(5));

  // A request is a request: the readable step of 20 over [0, 100] gives
  // six ticks for a count of four, which is the point of asking for a
  // ladder rather than for a division.
  const Scale hundred{.domain = {0, 100}};
  EXPECT_EQ(std::vector<double>({0, 20, 40, 60, 80, 100}), hundred.ticks(4));

  const Scale ragged{.domain = {2.3, 17.6}};
  const std::vector<double> inside = ragged.ticks(5);
  ASSERT_FALSE(inside.empty());
  EXPECT_DOUBLE_EQ(4.0, inside.front());
  EXPECT_DOUBLE_EQ(16.0, inside.back());
}

TEST(DataScale, LogTicksAreTheMultiplesOfEachPowerWhileTheyFit) {
  const Scale wide{.domain = {1, 1000}, .transform = Transform::Log};
  const std::vector<double> many = wide.ticks(10);
  ASSERT_EQ(28u, many.size());
  EXPECT_DOUBLE_EQ(1.0, many.front());
  EXPECT_DOUBLE_EQ(9.0, many[8]);
  EXPECT_DOUBLE_EQ(10.0, many[9]);
  EXPECT_DOUBLE_EQ(100.0, many[18]);
  EXPECT_DOUBLE_EQ(1000.0, many.back());

  // Too many decades for the multiples to fit: the powers themselves.
  EXPECT_EQ(std::vector<double>({1, 10, 100, 1000}), wide.ticks(3));
}

TEST(DataScale, NiceRoundsTheDomainOutToTheEndsOfItsLadder) {
  const Scale ragged{.domain = {2.3, 17.6}};
  const Scale round = ragged.nice(5);
  EXPECT_DOUBLE_EQ(0.0, round.domain.low);
  EXPECT_DOUBLE_EQ(20.0, round.domain.high);
  EXPECT_EQ(round.ticks(5).front(), round.domain.low);
  EXPECT_EQ(round.ticks(5).back(), round.domain.high);

  const Scale reversed{.domain = {17.6, 2.3}};
  const Scale roundReversed = reversed.nice(5);
  EXPECT_DOUBLE_EQ(20.0, roundReversed.domain.low);
  EXPECT_DOUBLE_EQ(0.0, roundReversed.domain.high);
}

TEST(DataScale, NiceOnALogScaleRoundsToWholePowers) {
  const Scale ragged{.domain = {3, 7000}, .transform = Transform::Log};
  const Scale round = ragged.nice();
  EXPECT_DOUBLE_EQ(1.0, round.domain.low);
  EXPECT_DOUBLE_EQ(10000.0, round.domain.high);
}

TEST(DataScale, NiceLeavesADiscreteScaleAlone) {
  const Scale band{.range = {0, 100}, .transform = Transform::Band, .steps = 7};
  EXPECT_EQ(band, band.nice());
}

TEST(DataScale, ARangeThisFileCannotNameIsTheCallersInterpolator) {
  const Scale heat{.domain = {0, 40}, .overflow = Overflow::Clamp};
  const auto ramp = [](double t) {
    return std::string(t < 0.5 ? "cool" : "warm");
  };
  EXPECT_EQ("cool", heat.through(10.0, ramp));
  EXPECT_EQ("warm", heat.through(30.0, ramp));
  EXPECT_EQ("warm", heat.through(400.0, ramp));
}

TEST(DataScale, TwoScalesWithTheSamePropsAreTheSameValue) {
  const Scale a{.domain = {0, 5}, .range = {0, 1}};
  Scale b = a;
  EXPECT_EQ(a, b);
  b.overflow = Overflow::Clamp;
  EXPECT_NE(a, b);
}

}  // namespace
