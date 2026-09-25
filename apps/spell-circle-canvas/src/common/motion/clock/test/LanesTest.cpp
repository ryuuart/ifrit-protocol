/** @file
 * The lanes: the family run in a lane list, retargeting the fixed slots
 * when one side of the diff lacks a lane, resolving a staggered lane for
 * the place its node stands at, and dropping a positional family whose
 * shape changed.
 */

#include "support/StandsAlone.h"

#include <gtest/gtest.h>
#include <sigilmotion/advanced/Held.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilmotion/ease/Ease.h>
#include <sigilmotion/values/Animatable.h>

#include <chrono>
#include <memory>
#include <span>
#include <vector>

#include "support/Ramps.h"

using namespace sigil::motion;
using namespace std::chrono_literals;
using sigil::motion::test::ramped;

namespace {

enum class Family : uint8_t { Slot, Span };
using FamilyLane = Lane<Family>;

/** Moves @p engine one stated frame of @p step past where it stands. */
void stepBy(Engine& engine, Duration step) {
  engine.advance(engine.elapsed() + step);
}

}  // namespace

TEST(Lanes, FamilyLanesIsTheContiguousRunOfOneFamily) {
  const Animatable<float> value = 1.0f;
  std::vector<FamilyLane> lanes = {{&value, {Family::Slot, 0}, 0},
                                   {nullptr, {Family::Slot, 1}, 0},
                                   {&value, {Family::Span, 0}, 0},
                                   {&value, {Family::Span, 1}, 0}};
  const std::span<const FamilyLane> all(lanes);
  EXPECT_EQ(familyLanes(all, Family::Slot).size(), 2u);
  EXPECT_EQ(familyLanes(all, Family::Span).size(), 2u);
  EXPECT_EQ(familyLanes(all, Family::Span)[1].slot.index, 1u);
  EXPECT_EQ(familyLanes(std::span<const FamilyLane>(), Family::Span).size(), 0u);
}

TEST(Lanes, RetargetFixedRampsFromTheStandingValueWhenOneSideLacksTheLane) {
  Engine engine;
  std::unique_ptr<HeldMotion> held[2];
  const Animatable<float> next = ramped(2.0f, 1s);
  std::vector<FamilyLane> previous = {{nullptr, {Family::Slot, 0}, 1.0f},
                                      {nullptr, {Family::Slot, 1}, 0.0f}};
  std::vector<FamilyLane> now = {{&next, {Family::Slot, 0}, 1.0f},
                                 {nullptr, {Family::Slot, 1}, 0.0f}};
  retargetFixed<Family>(engine, held, previous, now, {});
  ASSERT_TRUE(held[0]);
  EXPECT_FALSE(held[1]);  // neither side carried it: untouched
  EXPECT_EQ(held[0]->value(), 1.0f);  // from the standing value
  stepBy(engine, 500ms);
  EXPECT_NEAR(held[0]->value(), 1.5f, 1e-4f);
}

TEST(Lanes, RetargetFixedResolvesAStaggeredLaneForItsNodesPlace) {
  Engine engine;
  std::unique_ptr<HeldMotion> held[1];
  const Animatable<float> next = animate({.to = 2.0f,
                                          .duration = 1s,
                                          .delay = stagger(100ms),
                                          .ease = ease::linear});
  std::vector<FamilyLane> previous = {{nullptr, {Family::Slot, 0}, 1.0f}};
  std::vector<FamilyLane> now = {{&next, {Family::Slot, 0}, 1.0f}};
  // The third of three siblings holds for two steps of the stagger.
  retargetFixed<Family>(engine, held, previous, now, {}, {2, 3});
  ASSERT_TRUE(held[0]);
  stepBy(engine, 200ms);
  EXPECT_NEAR(held[0]->value(), 1.0f, 1e-4f);
  stepBy(engine, 500ms);
  EXPECT_NEAR(held[0]->value(), 1.5f, 1e-4f);
}

TEST(Lanes, RetargetPositionalDropsRunningMotionsWhenTheShapeChanges) {
  Engine engine;
  HeldMotions held;
  const Animatable<float> resting = 0.0f, rising = ramped(1.0f, 1s);
  std::vector<FamilyLane> previous = {{&resting, {Family::Span, 0}, 0}};
  std::vector<FamilyLane> same = {{&rising, {Family::Span, 0}, 0}};
  retargetPositional<Family>(engine, held, previous, same, {});
  ASSERT_EQ(held.size(), 1u);
  ASSERT_TRUE(held[0]);
  EXPECT_TRUE(held[0]->started);
  // Two lanes where there was one: the family's shape changed, so the
  // running motion drops rather than riding onto an endpoint that now
  // means something else.
  std::vector<FamilyLane> grown = {{&rising, {Family::Span, 0}, 0},
                                   {&rising, {Family::Span, 1}, 0}};
  retargetPositional<Family>(engine, held, same, grown, {});
  ASSERT_EQ(held.size(), 2u);
  EXPECT_FALSE(held[0]);
  EXPECT_FALSE(held[1]);
}
