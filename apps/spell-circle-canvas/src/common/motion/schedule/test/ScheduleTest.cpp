/** @file
 * The stagger and the schedule it resolves into: the named orderings and
 * their determinism, a value resolved per child from each origin, across
 * a grid, along a curve, from a start and reversed, a range spread across
 * the siblings, a table of cues; and the schedule — the even ladder and
 * the range division, a cue table and what a short one does, the nested
 * schedule's compounded beat, the looping fold, the beat a host reads
 * back, a caller-stated order, and the field walk that demands every
 * option of a stagger participate in its own equality.
 */

#include "support/StandsAlone.h"

#include <gtest/gtest.h>
#include <sigilcore/comparable/Fields.h>
#include <sigilmotion/ease/Ease.h>
#include <sigilmotion/schedule/Schedule.h>
#include <sigilmotion/schedule/Stagger.h>

#include <algorithm>
#include <boost/pfr/core.hpp>
#include <chrono>
#include <cmath>
#include <string>
#include <vector>

using namespace sigil::motion;
using namespace std::chrono_literals;

namespace {

/** The steps a run of @p count siblings is dealt in from @p origin. */
std::vector<float> orderOf(StaggerOrigin origin, uint32_t count,
                           uint32_t seed = 0) {
  std::vector<float> out;
  staggerSteps(origin, {}, StaggerAxis::Both, seed, {}, false, count, out);
  return out;
}

/** A length of time as the float milliseconds a schedule's arithmetic
 *  runs in. */
float milliseconds(Duration length) { return (float)(length.count() * 1000.0); }

/** Every child's value of @p value in a run of @p count. */
template <typename V>
std::vector<V> perChild(const Staggered<V>& value, size_t count) {
  std::vector<V> out;
  for (size_t index = 0; index < count; ++index)
    out.push_back(value.at({index, count}));
  return out;
}

/** Every child's delay in a run of @p count, in milliseconds. */
std::vector<float> delaysOf(const Staggered<Duration>& delay, size_t count) {
  std::vector<float> out;
  for (const Duration each : perChild(delay, count))
    out.push_back(std::round(milliseconds(each) * 1000.0f) / 1000.0f);
  return out;
}

/** One dealt ordering: which end a run opens from, and the steps the four
 *  units are dealt in when it does. The scattered ordering is absent
 *  because its steps are a permutation rather than a shape — the two
 *  cases below say what it promises instead. */
struct Ordering {
  const char* name;
  StaggerFrom from;
  std::vector<float> steps;
};

std::string orderingName(const testing::TestParamInfo<Ordering>& info) {
  return info.param.name;
}

struct StaggerOrdering : testing::TestWithParam<Ordering> {};

}  // namespace

// ---------------------------------------------------------------------------
// The orderings.

TEST_P(StaggerOrdering, DealsTheStepsItsNameReads) {
  EXPECT_EQ(orderOf(GetParam().from, 4), GetParam().steps);
}

INSTANTIATE_TEST_SUITE_P(
    Orderings, StaggerOrdering,
    testing::Values(Ordering{"FromFirst", StaggerFrom::First, {0, 1, 2, 3}},
                    Ordering{"FromLast", StaggerFrom::Last, {3, 2, 1, 0}},
                    Ordering{"FromCenter", StaggerFrom::Center, {3, 1, 1, 3}},
                    Ordering{"FromEdges", StaggerFrom::Edges, {0, 2, 2, 0}}),
    orderingName);

TEST(Order, OneUnitNeverSpreads) {
  // Whichever end it claims to start from, the single member opens at 0.
  for (StaggerFrom from : {StaggerFrom::First, StaggerFrom::Center,
                           StaggerFrom::Last, StaggerFrom::Random,
                           StaggerFrom::Edges})
    EXPECT_EQ(orderOf(from, 1), (std::vector<float>{0}));
}

TEST(Order, TheScatterIsAPermutationAndIsRepeatable) {
  const std::vector<float> scatter = orderOf(StaggerFrom::Random, 8);
  EXPECT_EQ(scatter, orderOf(StaggerFrom::Random, 8));
  std::vector<float> steps = scatter;
  std::sort(steps.begin(), steps.end());
  EXPECT_EQ(steps, (std::vector<float>{0, 1, 2, 3, 4, 5, 6, 7}));
}

TEST(Order, ASeedDealsAnIndependentScatter) {
  EXPECT_NE(orderOf(StaggerFrom::Random, 8, 1), orderOf(StaggerFrom::Random, 8));
  EXPECT_NE(orderOf(StaggerFrom::Random, 8, 1),
            orderOf(StaggerFrom::Random, 8, 2));
}

TEST(Order, AnIndexOriginSpreadsOutwardFromThatChild) {
  EXPECT_EQ(orderOf(1, 5), (std::vector<float>{1, 0, 1, 2, 3}));
}

// ---------------------------------------------------------------------------
// A value resolved per child.

TEST(Stagger, APlainValueIsTheSameForEveryChild) {
  const Staggered<Duration> plain = 120ms;
  EXPECT_FALSE(plain.isStaggered());
  EXPECT_EQ(plain.form(), Staggered<Duration>::Form::Value);
  EXPECT_EQ(delaysOf(plain, 3), (std::vector<float>{120, 120, 120}));
}

TEST(Stagger, EachOriginDealsTheStepFromItsOwnChild) {
  EXPECT_EQ(delaysOf(stagger(100ms), 5),
            (std::vector<float>{0, 100, 200, 300, 400}));
  EXPECT_EQ(delaysOf(stagger(100ms, {.from = StaggerFrom::Last}), 5),
            (std::vector<float>{400, 300, 200, 100, 0}));
  // Center and Edges span as many steps as First, so a run's total is
  // the same whichever end it opens from.
  EXPECT_EQ(delaysOf(stagger(100ms, {.from = StaggerFrom::Center}), 5),
            (std::vector<float>{400, 200, 0, 200, 400}));
  EXPECT_EQ(delaysOf(stagger(100ms, {.from = StaggerFrom::Edges}), 5),
            (std::vector<float>{0, 200, 400, 200, 0}));
  EXPECT_EQ(delaysOf(stagger(100ms, {.from = 1}), 5),
            (std::vector<float>{100, 0, 100, 200, 300}));

  // The scatter is a permutation of the First ladder, the same every
  // time it is read.
  const Staggered<Duration> scattered =
      stagger(100ms, {.from = StaggerFrom::Random});
  std::vector<float> delays = delaysOf(scattered, 5);
  EXPECT_EQ(delays, delaysOf(scattered, 5));
  std::sort(delays.begin(), delays.end());
  EXPECT_EQ(delays, (std::vector<float>{0, 100, 200, 300, 400}));
}

TEST(Stagger, AValueThatIsNotATimeStepsTheSameWay) {
  EXPECT_EQ(perChild(stagger(10.0f), 4), (std::vector<float>{0, 10, 20, 30}));
  // A child alone is the first child.
  EXPECT_FLOAT_EQ(stagger(10.0f).value(), 0.0f);
}

TEST(Stagger, ARangeSpreadsFromTheFirstChildsValueToTheLasts) {
  EXPECT_EQ(perChild(stagger({0.0f, 360.0f}), 5),
            (std::vector<float>{0, 90, 180, 270, 360}));
  // The range is dealt by the origin like a step is: from the middle,
  // the middle child takes the first value and both ends the last.
  EXPECT_EQ(perChild(stagger({0.0f, 360.0f}, {.from = StaggerFrom::Center}), 5),
            (std::vector<float>{360, 180, 0, 180, 360}));
  // One child has nothing to spread across.
  EXPECT_FLOAT_EQ(stagger({10.0f, 20.0f}).at({0, 1}), 10.0f);
  // A range written in milliseconds is a range of durations.
  const Staggered<Duration> delay = stagger({0ms, 600ms});
  EXPECT_EQ(delaysOf(delay, 4), (std::vector<float>{0, 200, 400, 600}));
}

TEST(Stagger, AGridMeasuresEachChildsDistanceAcrossIt) {
  // Three by three from the middle cell: the centre opens first, the four
  // edge cells one step later, the corners a diagonal later.
  const Staggered<Duration> ripple =
      stagger(10ms, {.from = StaggerFrom::Center, .grid = {3, 3}});
  EXPECT_NEAR(milliseconds(ripple.at({4, 9})), 0.0f, 1e-4f);
  EXPECT_NEAR(milliseconds(ripple.at({1, 9})), 10.0f, 1e-4f);
  EXPECT_NEAR(milliseconds(ripple.at({3, 9})), 10.0f, 1e-4f);
  EXPECT_NEAR(milliseconds(ripple.at({0, 9})), 10.0f * std::sqrt(2.0f), 1e-3f);

  // Along one axis, only that axis's distance counts.
  const Staggered<Duration> columns = stagger(
      10ms,
      {.from = StaggerFrom::Center, .grid = {3, 3}, .axis = StaggerAxis::Columns});
  EXPECT_NEAR(milliseconds(columns.at({0, 9})), 10.0f, 1e-4f);
  EXPECT_NEAR(milliseconds(columns.at({1, 9})), 0.0f, 1e-4f);
  EXPECT_NEAR(milliseconds(columns.at({7, 9})), 0.0f, 1e-4f);
}

TEST(Stagger, ACurveCrowdsTheStepsAndAStartOffsetsThemAll) {
  // Evenly spaced distances run through the curve: an ease-in bunches the
  // early children.
  EXPECT_EQ(delaysOf(stagger(100ms, {.ease = ease::inQuad}), 5),
            (std::vector<float>{0, 25, 100, 225, 400}));
  EXPECT_EQ(delaysOf(stagger(100ms, {.start = 50ms}), 3),
            (std::vector<float>{50, 150, 250}));
  EXPECT_EQ(delaysOf(stagger(100ms, {.reverse = true}), 3),
            (std::vector<float>{200, 100, 0}));
}

TEST(Stagger, ACueTableIsReadByIndexAndPilesItsTailOnTheLastEntry) {
  const Staggered<Duration> table = cues({0ms, 340ms, 720ms});
  EXPECT_EQ(table.form(), Staggered<Duration>::Form::Table);
  EXPECT_TRUE(table.isStaggered());
  EXPECT_EQ(delaysOf(table, 5), (std::vector<float>{0, 340, 720, 720, 720}));
}

TEST(Stagger, ACallerStatedOrderReplacesTheOrigin) {
  // Non-empty, `rankBy` replaces `from`: the order is the numbers', dealt
  // smallest first, ties together.
  const Staggered<Duration> byRadius =
      stagger(100ms, {.from = StaggerFrom::Last, .rankBy = {30, 10, 20, 40}});
  EXPECT_EQ(delaysOf(byRadius, 4), (std::vector<float>{200, 0, 100, 300}));
}

TEST(Stagger, TwoStaggersAreEqualByEveryPart) {
  EXPECT_EQ(stagger(100ms), stagger(100ms));
  EXPECT_NE(stagger(100ms), stagger(101ms));
  EXPECT_NE(stagger(100ms), stagger({0ms, 100ms}));
  EXPECT_NE(stagger(100ms, {.ease = ease::inQuad}),
            stagger(100ms, {.ease = ease::outQuad}));
  // Two different orders must never compare equal: the node that holds
  // one prunes, and it would keep beating to the old ladder for ever.
  EXPECT_NE(stagger(100ms, {.rankBy = {1.0f, 2.0f}}),
            stagger(100ms, {.rankBy = {2.0f, 1.0f}}));
  EXPECT_EQ(stagger(100ms, {.rankBy = {1.0f, 2.0f}}),
            stagger(100ms, {.rankBy = {1.0f, 2.0f}}));
  EXPECT_NE(cues({0ms, 340ms}), cues({0ms, 350ms}));
}

// ---------------------------------------------------------------------------
// THE FIELD WALK — the runtime half of the pin beside
// StaggerOptions::operator==.
//
// A stagger is a comparable value, and its equality is what lets the node
// holding one prune. An option left out fails INVISIBLY: two different
// staggers compare equal, the node prunes, and it keeps beating to the old
// ladder for ever. This perturbs every field the type DECLARES rather than
// a list someone remembered to extend.

namespace {

void perturb(StaggerOrigin& origin) { origin = StaggerFrom::Last; }
void perturb(std::array<uint32_t, 2>& grid) { grid[0] += 1u; }
void perturb(StaggerAxis& axis) { axis = StaggerAxis::Rows; }
void perturb(Easing& curve) { curve = ease::inQuad; }
void perturb(Duration& length) { length += 1ms; }
void perturb(bool& flag) { flag = !flag; }
void perturb(uint32_t& number) { number += 1u; }
void perturb(std::vector<float>& order) { order.push_back(1.0f); }

}  // namespace

TEST(Stagger, EveryOptionParticipatesInEquality) {
  using Options = StaggerOptions<Duration>;
  static const char* const kNames[] = {"from",  "grid",    "axis", "ease",
                                       "start", "reverse", "seed", "rankBy"};
  static_assert(sigil::core::kFieldCount<Options> == std::size(kNames),
                "name a new option here as well as in operator==");
  const Options base;
  [&]<std::size_t... Field>(std::index_sequence<Field...>) {
    (([&] {
       Options moved = base;
       perturb(boost::pfr::get<Field>(moved));
       EXPECT_FALSE(base == moved) << "option " << kNames[Field] << " is unread";
     }()),
     ...);
  }(std::make_index_sequence<sigil::core::kFieldCount<Options>>{});
}

// ---------------------------------------------------------------------------
// The schedule: a stagger resolved against a frame's counts.

TEST(Schedule, EachUnitOpensOneStepAfterTheOneBeforeIt) {
  const Timing timing{.delay = stagger(100ms), .duration = 400ms};
  const Schedule schedule(timing, 4);
  EXPECT_FLOAT_EQ(milliseconds(schedule.start(0)), 0.0f);
  EXPECT_FLOAT_EQ(milliseconds(schedule.start(3)), 300.0f);
  EXPECT_FLOAT_EQ(schedule.totalMs, 700.0f);
  EXPECT_FLOAT_EQ(milliseconds(schedule.total()), 700.0f);
  EXPECT_FLOAT_EQ(milliseconds(timing.span(4)), 700.0f);
}

TEST(Schedule, ARangeKeepsTheTotalAndShrinksTheSpacing) {
  const Timing timing{.delay = stagger({0ms, 300ms}), .duration = 400ms};
  // Every count past one answers the same span, because the range IS the
  // spread.
  EXPECT_FLOAT_EQ(milliseconds(timing.span(4)), 700.0f);
  EXPECT_FLOAT_EQ(milliseconds(timing.span(31)), 700.0f);
  // …and a single unit has nothing to spread over.
  EXPECT_FLOAT_EQ(milliseconds(timing.span(1)), 400.0f);
  EXPECT_FLOAT_EQ(milliseconds(timing.span(0)), 400.0f);
}

TEST(Schedule, ACueTableStatesEveryDelay) {
  const Timing timing{.delay = cues({0ms, 340ms, 720ms, 1180ms}),
                      .duration = 180ms};
  const Schedule schedule(timing, 4);
  EXPECT_FLOAT_EQ(milliseconds(schedule.start(2)), 720.0f);
  EXPECT_FLOAT_EQ(schedule.totalMs, 1360.0f);
}

TEST(Schedule, AShortCueTablePilesItsTailOnTheLastTime) {
  const Schedule schedule({.delay = cues({0ms, 50ms}), .duration = 100ms}, 5);
  EXPECT_FLOAT_EQ(milliseconds(schedule.start(2)), 50.0f);
  EXPECT_FLOAT_EQ(milliseconds(schedule.start(4)), 50.0f);
}

TEST(Schedule, ANestedScheduleOwnsTheBeat) {
  const Timing timing{
      .delay = stagger(200ms), .duration = 120ms, .within = stagger(30ms)};
  const Schedule schedule(timing, 3, 4);
  // The beat is the inner ladder's own extent.
  EXPECT_FLOAT_EQ(schedule.beatMs, 120.0f + 30.0f * 3.0f);
  // A start compounds the two levels.
  EXPECT_FLOAT_EQ(milliseconds(schedule.start(2, 3)), 400.0f + 90.0f);
}

TEST(Schedule, LocalProgressClampsAtBothEnds) {
  const Schedule schedule({.delay = stagger(100ms), .duration = 400ms}, 4);
  EXPECT_FLOAT_EQ(schedule.localProgress(0.0f, 3), 0.0f);
  EXPECT_FLOAT_EQ(schedule.localProgress(1.0f, 3), 1.0f);
  // Unit 3 opens at 300 of 700; halfway through the master is 350.
  EXPECT_FLOAT_EQ(schedule.localProgress(0.5f, 3), 50.0f / 400.0f);
}

TEST(Schedule, ALoopingScheduleSpansItsPeriodAndFoldsEveryUnit) {
  const Timing timing{
      .delay = stagger(100ms), .duration = 200ms, .loop = 400ms};
  const Schedule schedule(timing, 4);
  EXPECT_FLOAT_EQ(schedule.totalMs, 400.0f);
  EXPECT_FLOAT_EQ(milliseconds(timing.span(4)), 400.0f);
  // Master 0 and master 1 name the same instant of the cycle, so a
  // wrapping phase crosses its own seam with no jump.
  for (uint32_t unit = 0; unit < 4; ++unit)
    EXPECT_FLOAT_EQ(schedule.localProgress(0.0f, unit),
                    schedule.localProgress(1.0f, unit));
  // A unit whose start is past the period lands at start mod period
  // rather than waiting: every unit is always somewhere in its cycle.
  EXPECT_GT(schedule.localProgress(0.0f, 3), 0.0f);
}

TEST(Schedule, TheBeatReadBackAgreesWithTheTwoAccessors) {
  const Schedule schedule({.delay = stagger(100ms), .duration = 400ms}, 4);
  const Beat beat = schedule.beat(0.5f, 3);
  EXPECT_EQ(beat.unitIndex, 3u);
  EXPECT_EQ(beat.start, schedule.start(3));
  EXPECT_FLOAT_EQ(beat.localProgress, schedule.localProgress(0.5f, 3));
  EXPECT_TRUE(beat.running);
  // Begun and not finished is the whole of "running": a clamped local
  // progress reads 0 before the beat opens and 1 for ever after it closes.
  EXPECT_FALSE(schedule.beat(0.0f, 3).running);
  EXPECT_FALSE(schedule.beat(1.0f, 3).running);
}

TEST(Schedule, TheLadderAgreesWithTheStaggerItResolves) {
  // One arithmetic: the start a schedule computes for a unit is the delay
  // the stagger resolves for that child — through a curve and from an
  // origin alike.
  for (const Staggered<Duration>& delay :
       {stagger(100ms), stagger(100ms, {.ease = ease::inQuad}),
        stagger(100ms, {.from = StaggerFrom::Center}),
        stagger(100ms, {.start = 50ms})}) {
    const Schedule schedule({.delay = delay, .duration = 200ms}, 5);
    for (uint32_t unit = 0; unit < 5; ++unit)
      EXPECT_NEAR(milliseconds(schedule.start(unit)),
                  milliseconds(delay.at({unit, 5})), 1e-3f)
          << "unit " << unit;
  }
}

TEST(Schedule, RankByDealsTheLadderInTheOrderTheCallerStates) {
  // A bloom that spreads outward is ordered by radius, not by index. The
  // spacing, the shape and the duration all still apply — only the
  // running order changes.
  const Schedule dealt(
      {.delay = stagger(100ms, {.rankBy = {30.0f, 10.0f, 20.0f, 40.0f}}),
       .duration = 200ms},
      4);
  EXPECT_FLOAT_EQ(milliseconds(dealt.start(1)), 0.0f);  // nearest opens first
  EXPECT_FLOAT_EQ(milliseconds(dealt.start(2)), 100.0f);
  EXPECT_FLOAT_EQ(milliseconds(dealt.start(0)), 200.0f);
  EXPECT_FLOAT_EQ(milliseconds(dealt.start(3)), 300.0f);

  // TIES OPEN TOGETHER, and the slot after them is the next one up: a
  // ring of equal radii is one beat, not a dealt-out run.
  const Schedule tied(
      {.delay = stagger(100ms, {.rankBy = {5.0f, 5.0f, 9.0f, 5.0f}}),
       .duration = 200ms},
      4);
  EXPECT_FLOAT_EQ(milliseconds(tied.start(0)), 0.0f);
  EXPECT_FLOAT_EQ(milliseconds(tied.start(1)), 0.0f);
  EXPECT_FLOAT_EQ(milliseconds(tied.start(3)), 0.0f);
  EXPECT_FLOAT_EQ(milliseconds(tied.start(2)), 100.0f);
}

TEST(Schedule, TwoTimingsAreEqualByEveryField) {
  const Timing base{.delay = stagger(100ms), .duration = 400ms};
  Timing moved = base;
  EXPECT_EQ(base, moved);
  moved.delay = stagger(100ms, {.seed = 3});
  EXPECT_NE(base, moved);
  moved = base;
  moved.loop = 1s;
  EXPECT_NE(base, moved);
  moved = base;
  moved.within = stagger(10ms);
  EXPECT_NE(base, moved);
  Timing other = base;
  other.within = stagger(10ms);
  EXPECT_EQ(moved, other);
  other.within = stagger(11ms);
  EXPECT_NE(moved, other);
}
