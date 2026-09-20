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
#include <array>
#include <cmath>
#include <cstddef>
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

/** Walk every strand from its start: do the knots it meets run over,
 *  under, over? That is what a plaited figure is, and the reading is the
 *  same whichever figure is under test — so the two cases that ask it ask
 *  it the same way. */
bool walksAlternating(size_t strandCount, const std::vector<Crossing>& knots,
                      const CrossingRule& rule) {
  for (size_t strand = 0; strand < strandCount; ++strand) {
    std::vector<std::pair<float, bool>> along;  // (arc length, this one over)
    for (const Crossing& knot : knots) {
      if (knot.a != strand && knot.b != strand) continue;
      const bool firstIsOver = rule.decide(knot) == Order::Over;
      along.push_back({knot.a == strand ? knot.alongA : knot.alongB,
                       knot.a == strand ? firstIsOver : !firstIsOver});
    }
    std::sort(along.begin(), along.end());
    for (size_t i = 1; i < along.size(); ++i)
      if (along[i].second == along[i - 1].second) return false;
  }
  return true;
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
  // A {7/2} HEPTAGRAM: seven chords, each skipping one vertex, two
  // crossings on each and seven in all. It is the smallest figure that
  // tells the two alternating rules apart — with two strands they agree,
  // and with three they can still agree by luck.
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
  CrossingRule plaited = crossing::alternateAlong();
  plaited.prepare(knots);
  EXPECT_TRUE(walksAlternating(chords.size(), knots, plaited));
  // …and the ordinal rule does not, because it alternates along ONE
  // strand's numbering and every other strand meets that numbering in
  // whatever order it happens to.
  EXPECT_FALSE(walksAlternating(chords.size(), knots, crossing::alternate()));

  // Unprepared it is list order, and preparing does not change what it
  // compares to: the table is a function of geometry, not of the author.
  EXPECT_TRUE(crossing::alternateAlong() == plaited);
  EXPECT_FALSE(crossing::alternateAlong() == crossing::alternate());
  // A pin beats it, the way a pin beats every rule under it.
  plaited.except(knots.front().index, Order::Under);
  EXPECT_EQ(plaited.decide(knots.front()), Order::Under);
}

// THE SAME STAR, DRAWN THE WAY A FIGURE IS ACTUALLY TRAVERSED: seven open
// chords in visiting order, so CONSECUTIVE strands share a vertex. The
// case above indexes its chords by starting vertex, which puts the shared
// vertices on non-adjacent index pairs and the genuine crossings on the
// adjacent ones — the arrangement that never asks whether a strand which
// STOPS at a meeting is passing through it. This one asks.
//
// A shared vertex has to be excluded BY RULE. Answering it from a sample
// taken a step either way cannot: at the vertex the strand that stops
// there has no sample past it, so the probe is the meeting itself, and
// the side it comes out on is the sign of a rounding. The placements
// below are the evidence — several of them are the arrangements where
// that rounding used to come out the wrong way and manufacture an eighth
// knot, which is why the case sweeps a figure rather than drawing one.
TEST(Crossings, ThePlaitedStarHoldsItsSevenKnotsWhereverItIsDrawn) {
  // The star as the sigil study builds it, down to the arithmetic: the
  // angle in degrees of a turn, converted once, measured clockwise from
  // twelve o'clock. Two roundings of one angle are two different figures
  // at this scale, and it is the study's figure that is under test.
  const auto star = [](float radius, SkPoint centre) {
    const auto vertex = [&](int step) {
      const float degToRad = 3.14159265358979f / 180.0f;
      const float angle =
          (float)((2 * step) % 7) * 360.0f / 7.0f * degToRad - 1.5707963f;
      return SkPoint{centre.fX + radius * std::cos(angle),
                     centre.fY + radius * std::sin(angle)};
    };
    std::vector<SkPath> strands;
    for (int i = 0; i < 7; ++i) {
      const SkPoint from = vertex(i), to = vertex((i + 1) % 7);
      strands.push_back(segment(from.fX, from.fY, to.fX, to.fY));
    }
    return strands;
  };
  // A knot is a property of the figure, not of where the figure sits: the
  // same star translated and scaled answers the same seven. The first is
  // the study's own radius and centre, at pixel scale and already moved.
  const std::pair<float, SkPoint> places[]{
      {0.777f * 618.0f, {0.882f * 618.0f, 0.882f * 618.0f}},
      {144.8f, {100.0f, 120.0f}},
      {358.1f, {311.3f, 427.9f}},
      {547.7f, {311.3f, 427.9f}},
      {618.8f, {100.0f, 120.0f}},
      {150.0f, {200.0f, 200.0f}}};
  for (const auto& [radius, centre] : places) {
    const std::vector<SkPath> strands = star(radius, centre);
    const std::vector<Crossing> knots = discoverCrossings(strands);
    ASSERT_EQ(knots.size(), 7u)
        << "a {7/2} star meets itself once per point, and its seven shared "
           "vertices are meetings rather than crossings; radius "
        << radius;
    // FOURTEEN PASSES: two per knot, and two on every strand.
    std::array<int, 7> passes{};
    for (const Crossing& knot : knots) {
      ++passes[knot.a];
      ++passes[knot.b];
      // No knot sits at a vertex, on either strand.
      EXPECT_GT(knot.alongA, 0.05f);
      EXPECT_LT(knot.alongA, 0.95f);
      EXPECT_GT(knot.alongB, 0.05f);
      EXPECT_LT(knot.alongB, 0.95f);
    }
    for (int met : passes) EXPECT_EQ(met, 2);
    CrossingRule plaited = crossing::alternateAlong();
    plaited.prepare(knots);
    EXPECT_TRUE(walksAlternating(strands.size(), knots, plaited));
  }
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

// THE FIGURE'S OWN SCALE decides how finely a strand is sampled and how
// near two reported meetings have to be to be one meeting. A fixed
// number of pixels is not a measure of anything: the same two grids, one
// a tenth the size, must answer the same number of crossings.
TEST(Crossings, ATinyFigureAnswersTheSameCrossingsAsALargeOne) {
  const auto grid = [](float side) {
    std::vector<SkPath> strands;
    for (int i = 1; i <= 3; ++i) {
      SkPathBuilder row;
      row.moveTo(0, side * (float)i / 4.0f);
      row.lineTo(side, side * (float)i / 4.0f);
      strands.push_back(row.detach());
      SkPathBuilder column;
      column.moveTo(side * (float)i / 4.0f, 0);
      column.lineTo(side * (float)i / 4.0f, side);
      strands.push_back(column.detach());
    }
    return strands;
  };
  const size_t large = discoverCrossings(grid(400.0f)).size();
  EXPECT_EQ(large, 9u);
  EXPECT_EQ(discoverCrossings(grid(40.0f)).size(), large);
  EXPECT_EQ(discoverCrossings(grid(6.0f)).size(), large);
}

}  // namespace
