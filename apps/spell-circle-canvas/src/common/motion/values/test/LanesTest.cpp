/** @file
 * The animation lanes: the family run in a lane list, retargeting the
 * fixed slots when one side of the diff lacks a lane, and dropping a
 * positional family whose shape changed.
 */

#include <gtest/gtest.h>
#include <sigilcore/reconcile/Lanes.h>
#include <sigilmotion/values/Keyframes.h>

#include <chrono>
#include <vector>

using namespace sigil::core;
using sigil::motion::Animatable;
using sigil::motion::AnimatedFloat;
using sigil::motion::AnimatedFloats;
using sigil::motion::Ticker;
using sigil::motion::Transitioned;

namespace {

enum class Family : uint8_t { Slot, Span };
using L = Lane<Family>;

Animatable<float> ramped(float to, int ms) {
  Transitioned<float> t;
  t.value = to;
  t.spec.duration = std::chrono::milliseconds(ms);
  t.spec.ease = &choreograph::easeNone;
  return t;
}

}  // namespace

TEST(Lanes, FamilyLanesIsTheContiguousRunOfOneFamily) {
  const Animatable<float> v = 1.0f;
  std::vector<L> lanes = {{&v, {Family::Slot, 0}, 0},
                          {nullptr, {Family::Slot, 1}, 0},
                          {&v, {Family::Span, 0}, 0},
                          {&v, {Family::Span, 1}, 0}};
  const std::span<const L> all(lanes);
  EXPECT_EQ(familyLanes(all, Family::Slot).size(), 2u);
  EXPECT_EQ(familyLanes(all, Family::Span).size(), 2u);
  EXPECT_EQ(familyLanes(all, Family::Span)[1].slot.index, 1u);
  EXPECT_EQ(familyLanes(std::span<const L>(), Family::Span).size(), 0u);
}

TEST(Lanes, RetargetSlotsRampsFromTheStandingValueWhenOneSideLacksTheLane) {
  Ticker ticker;
  std::unique_ptr<AnimatedFloat> anims[2];
  const Animatable<float> next = ramped(2.0f, 1000);
  std::vector<L> prev = {{nullptr, {Family::Slot, 0}, 1.0f},
                         {nullptr, {Family::Slot, 1}, 0.0f}};
  std::vector<L> now = {{&next, {Family::Slot, 0}, 1.0f},
                        {nullptr, {Family::Slot, 1}, 0.0f}};
  retargetSlots<Family>(ticker, anims, prev, now, {});
  ASSERT_TRUE(anims[0]);
  EXPECT_FALSE(anims[1]);  // neither side carried it: untouched
  EXPECT_EQ(anims[0]->value.value(), 1.0f);  // from the standing value
  ticker.tick(0.5);
  EXPECT_NEAR(anims[0]->value.value(), 1.5f, 1e-4f);
}

TEST(Lanes, RetargetFamilyDropsRunningMotionsWhenTheShapeChanges) {
  Ticker ticker;
  AnimatedFloats anims;
  const Animatable<float> a0 = 0.0f, a1 = ramped(1.0f, 1000);
  std::vector<L> prev = {{&a0, {Family::Span, 0}, 0}};
  std::vector<L> same = {{&a1, {Family::Span, 0}, 0}};
  retargetFamily<Family>(ticker, anims, prev, same, {});
  ASSERT_EQ(anims.size(), 1u);
  ASSERT_TRUE(anims[0]);
  EXPECT_TRUE(anims[0]->started);
  // Two lanes where there was one: the family's shape changed, so the
  // running motion drops rather than riding onto an endpoint that now
  // means something else.
  std::vector<L> grown = {{&a1, {Family::Span, 0}, 0},
                          {&a1, {Family::Span, 1}, 0}};
  retargetFamily<Family>(ticker, anims, same, grown, {});
  ASSERT_EQ(anims.size(), 2u);
  EXPECT_FALSE(anims[0]);
  EXPECT_FALSE(anims[1]);
}
