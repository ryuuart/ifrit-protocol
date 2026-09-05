/** @file
 * The schedule: the five orderings and their determinism, the even
 * ladder and the amount-mode division, a cue table and what a short one
 * does, the nested cascade's compounded beat, the looping fold, the beat
 * a host reads back, and the field walk that demands every member of a
 * spread participate in its own equality.
 */

#include <gtest/gtest.h>
#include <sigilcore/comparable/Fields.h>
#include <sigilmotion/schedule/Schedule.h>

#include <algorithm>
#include <boost/pfr/core.hpp>
#include <string>
#include <vector>

using namespace sigil::motion;

namespace {

std::vector<float> orderOf(Spread::From from, uint32_t count,
                           uint32_t seed = 0) {
  std::vector<float> out;
  cascadeOrder(from, count, seed, out);
  return out;
}

/** One dealt ordering: which end a run opens from, and the rank the four
 *  units are dealt in when it does. The scattered ordering is absent
 *  because its ranks are a permutation rather than a shape — the two
 *  cases below say what it promises instead. */
struct Ordering {
  const char* name;
  Spread::From from;
  std::vector<float> ranks;
};

std::string orderingName(const testing::TestParamInfo<Ordering>& info) {
  return info.param.name;
}

struct CascadeOrdering : testing::TestWithParam<Ordering> {};

}  // namespace

TEST_P(CascadeOrdering, DealsTheRanksItsNameReads) {
  EXPECT_EQ(orderOf(GetParam().from, 4), GetParam().ranks);
}

INSTANTIATE_TEST_SUITE_P(
    Orderings, CascadeOrdering,
    testing::Values(
        Ordering{"FromStart", Spread::From::Start, {0, 1, 2, 3}},
        Ordering{"FromEnd", Spread::From::End, {3, 2, 1, 0}},
        Ordering{"FromCenter", Spread::From::Center, {3, 1, 1, 3}},
        Ordering{"FromEdges", Spread::From::Edges, {0, 2, 2, 0}}),
    orderingName);

TEST(Order, OneUnitNeverSpreads) {
  // Whichever end it claims to start from, the single member opens at 0.
  for (Spread::From from :
       {Spread::From::Start, Spread::From::Center, Spread::From::End,
        Spread::From::Random, Spread::From::Edges})
    EXPECT_EQ(orderOf(from, 1), (std::vector<float>{0}));
}

TEST(Order, TheScatterIsAPermutationAndIsRepeatable) {
  const std::vector<float> a = orderOf(Spread::From::Random, 8);
  EXPECT_EQ(a, orderOf(Spread::From::Random, 8));
  std::vector<float> ranks = a;
  std::sort(ranks.begin(), ranks.end());
  EXPECT_EQ(ranks, (std::vector<float>{0, 1, 2, 3, 4, 5, 6, 7}));
}

TEST(Order, ASeedDealsAnIndependentScatter) {
  EXPECT_NE(orderOf(Spread::From::Random, 8, 1),
            orderOf(Spread::From::Random, 8));
  EXPECT_NE(orderOf(Spread::From::Random, 8, 1),
            orderOf(Spread::From::Random, 8, 2));
}

TEST(Cascade, EachUnitOpensOneStepAfterTheOneBeforeIt) {
  const Spread spec{.eachMs = 100, .durationMs = 400};
  Cascade cascade;
  cascade.build(spec, 4, 0);
  EXPECT_FLOAT_EQ(cascade.startMs(0, 0), 0.0f);
  EXPECT_FLOAT_EQ(cascade.startMs(3, 0), 300.0f);
  EXPECT_FLOAT_EQ(cascade.totalMs, 700.0f);
  EXPECT_FLOAT_EQ(spec.spanMs(4), 700.0f);
}

TEST(Cascade, AmountModeKeepsTheTotalAndShrinksTheSpacing) {
  const Spread spec{.amountMs = 300, .durationMs = 400};
  // Every count past one answers the same span, because the amount IS
  // the spread.
  EXPECT_FLOAT_EQ(spec.spanMs(4), 700.0f);
  EXPECT_FLOAT_EQ(spec.spanMs(31), 700.0f);
  // …and a single unit has nothing to spread over.
  EXPECT_FLOAT_EQ(spec.spanMs(1), 400.0f);
  EXPECT_FLOAT_EQ(spec.spanMs(0), 400.0f);
}

TEST(Cascade, ACueTableStatesEveryDelayAndTheStepIsIgnored) {
  Spread spec{.durationMs = 180};
  spec.cueMs = {0, 340, 720, 1180};
  spec.eachMs = 9999;  // said nothing: the table states the delays
  Cascade cascade;
  cascade.build(spec, 4, 0);
  EXPECT_FLOAT_EQ(cascade.startMs(2, 0), 720.0f);
  EXPECT_FLOAT_EQ(cascade.totalMs, 1360.0f);
}

TEST(Cascade, AShortCueTablePilesItsTailOnTheLastTime) {
  Spread spec{.durationMs = 100};
  spec.cueMs = {0, 50};
  Cascade cascade;
  cascade.build(spec, 5, 0);
  EXPECT_FLOAT_EQ(cascade.startMs(2, 0), 50.0f);
  EXPECT_FLOAT_EQ(cascade.startMs(4, 0), 50.0f);
}

TEST(Cascade, ANestedCascadeOwnsTheBeat) {
  Spread spec{.eachMs = 200, .durationMs = 9999};
  spec.then(Spread{.eachMs = 30, .durationMs = 120});
  Cascade cascade;
  cascade.build(spec, 3, 4);
  // The beat is the inner ladder's own extent, not the outer duration.
  EXPECT_FLOAT_EQ(cascade.beatMs, 120.0f + 30.0f * 3.0f);
  // A start compounds the two levels.
  EXPECT_FLOAT_EQ(cascade.startMs(2, 3), 400.0f + 90.0f);
}

TEST(Cascade, LocalTimeClampsAtBothEnds) {
  const Spread spec{.eachMs = 100, .durationMs = 400};
  Cascade cascade;
  cascade.build(spec, 4, 0);
  EXPECT_FLOAT_EQ(cascade.localTime(0.0f, 3, 0), 0.0f);
  EXPECT_FLOAT_EQ(cascade.localTime(1.0f, 3, 0), 1.0f);
  // Unit 3 opens at 300 of 700; halfway through the master is 350.
  EXPECT_FLOAT_EQ(cascade.localTime(0.5f, 3, 0), 50.0f / 400.0f);
}

TEST(Cascade, ALoopingCascadeSpansItsPeriodAndFoldsEveryUnit) {
  const Spread spec{.eachMs = 100, .durationMs = 200, .loopMs = 400};
  Cascade cascade;
  cascade.build(spec, 4, 0);
  EXPECT_FLOAT_EQ(cascade.totalMs, 400.0f);
  EXPECT_FLOAT_EQ(spec.spanMs(4), 400.0f);
  // Master 0 and master 1 name the same instant of the cycle, so a
  // wrapping phase crosses its own seam with no jump.
  for (uint32_t i = 0; i < 4; ++i)
    EXPECT_FLOAT_EQ(cascade.localTime(0.0f, i, 0),
                    cascade.localTime(1.0f, i, 0));
  // A unit whose start is past the period lands at start mod period
  // rather than waiting: every unit is always somewhere in its cycle.
  EXPECT_GT(cascade.localTime(0.0f, 3, 0), 0.0f);
}

TEST(Cascade, TheBeatReadBackAgreesWithTheTwoAccessors) {
  const Spread spec{.eachMs = 100, .durationMs = 400};
  Cascade cascade;
  cascade.build(spec, 4, 0);
  const Beat beat = cascade.beat(0.5f, 3, 0);
  EXPECT_EQ(beat.unitIndex, 3u);
  EXPECT_FLOAT_EQ(beat.startMs, cascade.startMs(3, 0));
  EXPECT_FLOAT_EQ(beat.localT, cascade.localTime(0.5f, 3, 0));
  EXPECT_TRUE(beat.active);
  // Begun and not finished is the whole of "running": a clamped local
  // time reads 0 before the beat opens and 1 forever after it closes.
  EXPECT_FALSE(cascade.beat(0.0f, 3, 0).active);
  EXPECT_FALSE(cascade.beat(1.0f, 3, 0).active);
}

TEST(Spread, EqualityReadsEveryFieldAndDescendsIntoTheNesting) {
  Spread a{.eachMs = 100, .durationMs = 400};
  Spread b = a;
  EXPECT_TRUE(a == b);
  b.seed = 3;
  EXPECT_FALSE(a == b);
  b = a;
  a.then(Spread{.eachMs = 10});
  EXPECT_FALSE(a == b);
  b.then(Spread{.eachMs = 10});
  EXPECT_TRUE(a == b);
  b.then(Spread{.eachMs = 11});
  EXPECT_FALSE(a == b);
}

// ---------------------------------------------------------------------------
// THE FIELD WALK — the runtime half of the pin beside Spread::operator==.
//
// A spread is a comparable value, and its equality is what lets the node
// holding one prune. A field left out fails INVISIBLY: two different
// cascades compare equal, the node prunes, and it keeps beating to the old
// ladder forever. The compile-time half (the kFieldCount assert beside the
// comparator) makes ADDING a field a build failure; this makes deciding
// what to do about it mechanical, because it perturbs every field the type
// DECLARES rather than a list someone remembered to extend.

namespace {

void perturb(float& v) { v += 1.0f; }
void perturb(uint32_t& v) { v += 1u; }
void perturb(std::vector<float>& v) { v.push_back(1.0f); }
void perturb(Spread::From& v) { v = Spread::From::End; }
void perturb(choreograph::EaseFn& v) { v = &choreograph::easeInQuad; }
void perturb(std::shared_ptr<const Spread>& v) {
  v = std::make_shared<const Spread>();
}

}  // namespace

TEST(Spread, EveryFieldParticipatesInEquality) {
  static const char* const kNames[] = {
      "eachMs",     "amountMs", "cueMs",        "rankBy", "durationMs",
      "loopMs",     "from",     "seed",         "distribution", "inner"};
  static_assert(sigil::core::kFieldCount<Spread> == std::size(kNames),
                "name a new field here as well as in operator==");
  const Spread base;
  [&]<std::size_t... I>(std::index_sequence<I...>) {
    (([&] {
       Spread moved = base;
       perturb(boost::pfr::get<I>(moved));
       EXPECT_FALSE(base == moved) << "field " << kNames[I] << " is unread";
     }()),
     ...);
  }(std::make_index_sequence<sigil::core::kFieldCount<Spread>>{});
}

TEST(Cascade, RankByDealsTheLadderInTheOrderTheCallerStates) {
  // A bloom that spreads outward is ordered by radius, not by index. The
  // spacing, the shape and the duration all still apply — only the
  // running order changes.
  Cascade dealt;
  dealt.build({.eachMs = 100,
               .rankBy = {30.0f, 10.0f, 20.0f, 40.0f},
               .durationMs = 200},
              4, 1);
  EXPECT_FLOAT_EQ(dealt.startMs(1, 0), 0.0f);    // nearest opens first
  EXPECT_FLOAT_EQ(dealt.startMs(2, 0), 100.0f);
  EXPECT_FLOAT_EQ(dealt.startMs(0, 0), 200.0f);
  EXPECT_FLOAT_EQ(dealt.startMs(3, 0), 300.0f);

  // TIES OPEN TOGETHER, and the slot after them is the next one up: a
  // ring of equal radii is one beat, not a dealt-out run.
  Cascade tied;
  tied.build({.eachMs = 100,
              .rankBy = {5.0f, 5.0f, 9.0f, 5.0f},
              .durationMs = 200},
             4, 1);
  EXPECT_FLOAT_EQ(tied.startMs(0, 0), 0.0f);
  EXPECT_FLOAT_EQ(tied.startMs(1, 0), 0.0f);
  EXPECT_FLOAT_EQ(tied.startMs(3, 0), 0.0f);
  EXPECT_FLOAT_EQ(tied.startMs(2, 0), 100.0f);

  // A cue table already states the whole order, so it wins.
  Cascade cued;
  cued.build({.eachMs = 100,
              .cueMs = {0, 50, 500, 90},
              .rankBy = {9.0f, 8.0f, 7.0f, 6.0f},
              .durationMs = 200},
             4, 1);
  EXPECT_FLOAT_EQ(cued.startMs(2, 0), 500.0f);
}

TEST(Spread, RankByIsReadByEquality) {
  // Two different orders must never compare equal: the node that holds
  // one prunes, and it would keep beating to the old ladder forever.
  EXPECT_NE((Spread{.rankBy = {1.0f, 2.0f}}), (Spread{.rankBy = {2.0f, 1.0f}}));
  EXPECT_EQ((Spread{.rankBy = {1.0f, 2.0f}}), (Spread{.rankBy = {1.0f, 2.0f}}));
}

TEST(Spread, CuesSetsTheTableOnASpreadAlreadyInHand) {
  Spread spec = Spread{.durationMs = 180}.cues({0, 340, 720});
  EXPECT_EQ(spec.cueMs, (std::vector<float>{0, 340, 720}));
  EXPECT_FLOAT_EQ(spec.durationMs, 180.0f);
  // Same value as writing the field, which is what makes it sugar.
  EXPECT_EQ(spec, (Spread{.cueMs = {0, 340, 720}, .durationMs = 180}));
}
