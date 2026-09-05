/** @file
 * Where a set of paths cross, and who goes over: only proper crossings are
 * knots, they are numbered along the lower-indexed strand, and the rule
 * that decides each one is a value a pin can override.
 */

#include <gtest/gtest.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkRect.h>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "sigilgeometry/path/Crossings.h"

using namespace sigil::geometry::path;

namespace {

// ---------------------------------------------------------------------------
// Crossings.

namespace {
SkPath segment(float x0, float y0, float x1, float y1) {
  SkPathBuilder b;
  b.moveTo(x0, y0);
  b.lineTo(x1, y1);
  return b.detach();
}
}  // namespace

TEST(Crossings, OnlyProperCrossingsAreReported) {
  // An X: one crossing, at the middle of both strands.
  const std::vector<SkPath> x{segment(0, 0, 100, 100), segment(0, 100, 100, 0)};
  const std::vector<Crossing> found = discoverCrossings(x);
  ASSERT_EQ(found.size(), 1u);
  EXPECT_EQ(found[0].a, 0u);
  EXPECT_EQ(found[0].b, 1u);
  EXPECT_NEAR(found[0].at.fX, 50.0f, 1.0f);
  EXPECT_NEAR(found[0].alongA, 0.5f, 0.02f);

  // A shared endpoint is a MEETING, not a crossing — otherwise every
  // polygon corner would be a knot.
  EXPECT_TRUE(
      discoverCrossings({segment(0, 0, 50, 50), segment(50, 50, 100, 0)})
          .empty());
  // Coincident strands never cross: that is what a stack of layers is.
  EXPECT_TRUE(discoverCrossings({segment(0, 0, 100, 0), segment(0, 0, 100, 0)})
                  .empty());
  // Fewer than two strands cannot cross.
  EXPECT_TRUE(discoverCrossings({segment(0, 0, 100, 100)}).empty());
}

TEST(Crossings, TheyAreNumberedAlongTheLowerIndexedStrand) {
  const std::vector<SkPath> ladder{segment(0, 50, 300, 50),
                                   segment(200, 0, 200, 100),
                                   segment(100, 0, 100, 100)};
  const std::vector<Crossing> found = discoverCrossings(ladder);
  ASSERT_EQ(found.size(), 2u);
  // Sorted by position on strand 0, then numbered — so the crossing at
  // x = 100 is index 0 even though its strand was added last.
  EXPECT_EQ(found[0].index, 0u);
  EXPECT_NEAR(found[0].at.fX, 100.0f, 1.0f);
  EXPECT_EQ(found[1].index, 1u);
  EXPECT_NEAR(found[1].at.fX, 200.0f, 1.0f);
}

TEST(CrossingRule, ListOrderDecidesUnlessAnotherRuleOrAPinDoes) {
  const auto knot = [](size_t index, size_t a, size_t b) {
    Crossing c;
    c.index = index;
    c.a = a;
    c.b = b;
    return c;
  };
  // The default: `b` is later in the list, so `a` passes under.
  EXPECT_EQ(CrossingRule().decide(knot(0, 0, 1)), Order::Under);
  // alternate() IS sequence({Over, Under}) — two names, one machine.
  EXPECT_TRUE(crossing::alternate() ==
              crossing::sequence({Order::Over, Order::Under}));
  EXPECT_EQ(crossing::alternate().decide(knot(0, 0, 1)), Order::Over);
  EXPECT_EQ(crossing::alternate().decide(knot(1, 0, 1)), Order::Under);
  // Dominance, cycles legal — the impossible braid.
  const CrossingRule cyclic = crossing::pairs({{0, 1}, {1, 2}, {2, 0}});
  EXPECT_EQ(cyclic.decide(knot(0, 0, 1)), Order::Over);
  EXPECT_EQ(cyclic.decide(knot(0, 1, 2)), Order::Over);
  EXPECT_EQ(cyclic.decide(knot(0, 0, 2)), Order::Under);
  // A pin beats the rule beneath it, and re-pinning replaces rather than
  // stacks — there is one `.crossing` field and this is how it takes
  // exceptions.
  CrossingRule pinned = crossing::alternate();
  pinned.except(0, Order::Under).except(0, Order::Over);
  EXPECT_EQ(pinned.decide(knot(0, 0, 1)), Order::Over);
  EXPECT_FALSE(pinned == crossing::alternate());
}

TEST(CrossingRule, AlternateAlongPlaitsEveryStrandAndAlternateDoesNot) {
  // A {7/2} HEPTAGRAM: seven chords, each skipping one vertex, four
  // crossings on each. It is the smallest figure that tells the two
  // alternating rules apart — with two strands they agree, and with
  // three they can still agree by luck.
  constexpr int kPoints = 7;
  constexpr int kStep = 2;
  std::vector<SkPath> chords;
  for (int i = 0; i < kPoints; ++i) {
    const auto at = [&](int k) {
      const float a = 2.0f * 3.14159265f * (float)k / (float)kPoints - 1.5708f;
      return SkPoint{200 + 150 * std::cos(a), 200 + 150 * std::sin(a)};
    };
    const SkPoint from = at(i), to = at((i + kStep) % kPoints);
    chords.push_back(segment(from.x(), from.y(), to.x(), to.y()));
  }
  const std::vector<Crossing> knots = discoverCrossings(chords);
  ASSERT_EQ(knots.size(), (size_t)kPoints)
      << "a {7/2} star meets itself once per point, around the inner "
         "heptagon";

  // THE CLAIM: walk any chord from its start and the crossings you meet
  // run over, under, over, under. That is what a plaited star is.
  const auto walksAlternating = [&](const CrossingRule& rule) {
    for (size_t strand = 0; strand < chords.size(); ++strand) {
      std::vector<std::pair<float, bool>> along;  // (arc length, this one over)
      for (const Crossing& k : knots) {
        if (k.a != strand && k.b != strand) continue;
        const bool aIsOver = rule.decide(k) == Order::Over;
        along.push_back({k.a == strand ? k.alongA : k.alongB,
                         k.a == strand ? aIsOver : !aIsOver});
      }
      std::sort(along.begin(), along.end());
      for (size_t i = 1; i < along.size(); ++i)
        if (along[i].second == along[i - 1].second) return false;
    }
    return true;
  };

  CrossingRule plaited = crossing::alternateAlong();
  plaited.prepare(knots);
  EXPECT_TRUE(walksAlternating(plaited));
  // …and the ordinal rule does not, because it alternates along ONE
  // strand's numbering and every other strand meets that numbering in
  // whatever order it happens to.
  EXPECT_FALSE(walksAlternating(crossing::alternate()));

  // Unprepared it is list order, and preparing does not change what it
  // compares to: the table is a function of geometry, not of the author.
  EXPECT_TRUE(crossing::alternateAlong() == plaited);
  EXPECT_FALSE(crossing::alternateAlong() == crossing::alternate());
  // A pin beats it, the way a pin beats every rule under it.
  plaited.except(knots.front().index, Order::Under);
  EXPECT_EQ(plaited.decide(knots.front()), Order::Under);
}

TEST(CrossingPatch, TheLensIsBoundedByTheKnotsOwnTerritory) {
  const SkPath a = segment(0, 50, 200, 50);
  const SkPath b = segment(100, 0, 100, 100);
  const SkPath lens = crossingPatch(a, 8.0f, b, 8.0f, {100, 50}, 20.0f);
  ASSERT_FALSE(lens.isEmpty());
  EXPECT_TRUE(lens.getBounds().contains(SkRect::MakeLTRB(99, 49, 101, 51)));
  EXPECT_LE(lens.getBounds().width(), 41.0f);
  // Non-overlapping input still answers: a disc at the point, inside the
  // radius the caller allowed.
  const SkPath far =
      crossingPatch(a, 1.0f, segment(0, 900, 10, 900), 1.0f, {5, 900}, 6.0f);
  EXPECT_FALSE(far.isEmpty());
  EXPECT_LE(far.getBounds().width(), 13.0f);
}

}  // namespace
