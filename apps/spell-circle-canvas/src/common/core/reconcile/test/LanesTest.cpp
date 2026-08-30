/** @file
 * The animation lanes: resolving a lane for the frame, the family run in a
 * lane list, retargeting a ramp from its current value, snapping when the
 * target is plain, dropping a positional family whose shape changed, and
 * playing a mount entrance.
 */

#include <gtest/gtest.h>
#include <sigilcore/reconcile/Lanes.h>
#include <sigilmotion/values/Keyframes.h>

#include <chrono>
#include <vector>

using namespace sigil::core;
using sigil::motion::Animatable;
using sigil::motion::Ticker;
using sigil::motion::Transition;
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

Animatable<float> entrance(float from, float to, int ms) {
  Transitioned<float> t;
  t.value = to;
  t.from = from;
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

TEST(Lanes, ResolveFloatAtPrefersABindingThenARunningRampThenThePlainValue) {
  const Animatable<float> plain = 3.0f;
  EXPECT_EQ(resolveFloatAt(nullptr, plain), 3.0f);
  AnimatedFloat anim;
  anim.value = 7.0f;
  EXPECT_EQ(resolveFloatAt(&anim, plain), 3.0f);  // not started: ignored
  anim.started = true;
  EXPECT_EQ(resolveFloatAt(&anim, plain), 7.0f);
  choreograph::Output<float> out{9.0f};
  const Animatable<float> bound = &out;
  EXPECT_EQ(resolveFloatAt(&anim, bound), 9.0f);  // the binding wins
  EXPECT_EQ(resolveFloatAt(nullptr, ramped(5.0f, 100)), 5.0f);
}

TEST(Lanes, ATransitionRampsToTheTargetAndRetargetsFromWhereItIs) {
  Ticker ticker;
  std::unique_ptr<AnimatedFloat> anim;
  EXPECT_TRUE(transitionFloatAt(ticker, anim, 0.0f, ramped(10.0f, 1000), {}));
  ASSERT_TRUE(anim);
  EXPECT_TRUE(anim->started);
  EXPECT_EQ(anim->target, 10.0f);
  ticker.tick(0.5);
  EXPECT_NEAR(anim->value.value(), 5.0f, 1e-4f);
  // A patch to the same target leaves the motion alone.
  EXPECT_TRUE(transitionFloatAt(ticker, anim, ramped(10.0f, 1000),
                                ramped(10.0f, 1000), {}));
  ticker.tick(0.25);
  EXPECT_NEAR(anim->value.value(), 7.5f, 1e-4f);
  // A new target starts from the current value, not from the description.
  EXPECT_TRUE(transitionFloatAt(ticker, anim, ramped(10.0f, 1000),
                                ramped(0.0f, 1000), {}));
  EXPECT_NEAR(anim->value.value(), 7.5f, 1e-4f);
  ticker.tick(0.5);
  EXPECT_NEAR(anim->value.value(), 3.75f, 1e-4f);
}

TEST(Lanes, APlainTargetSnapsAndDisconnectsTheRamp) {
  Ticker ticker;
  std::unique_ptr<AnimatedFloat> anim;
  transitionFloatAt(ticker, anim, 0.0f, ramped(10.0f, 1000), {});
  ticker.tick(0.2);
  EXPECT_FALSE(transitionFloatAt(ticker, anim, ramped(10.0f, 1000), 4.0f, {}));
  EXPECT_FALSE(anim->started);
  EXPECT_FALSE(anim->value.isConnected());
  EXPECT_EQ(resolveFloatAt(anim.get(), 4.0f), 4.0f);
}

TEST(Lanes, ANodeDefaultTransitionRampsAPlainChange) {
  Ticker ticker;
  std::unique_ptr<AnimatedFloat> anim;
  Transition spec;
  spec.duration = std::chrono::milliseconds(1000);
  spec.ease = &choreograph::easeNone;
  EXPECT_TRUE(transitionFloatAt(ticker, anim, 0.0f, 8.0f, spec));
  ticker.tick(0.5);
  EXPECT_NEAR(anim->value.value(), 4.0f, 1e-4f);
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

TEST(Lanes, AMountEntrancePlaysFromToValueAfterTheExtraDelay) {
  Ticker ticker;
  std::unique_ptr<AnimatedFloat> anim;
  mountEntrance(ticker, anim, 5.0f, 0.0f);
  EXPECT_FALSE(anim);  // no entrance declared
  mountEntrance(ticker, anim, entrance(0.0f, 10.0f, 1000), 0.5f);
  ASSERT_TRUE(anim);
  EXPECT_EQ(anim->value.value(), 0.0f);
  ticker.tick(0.5);
  EXPECT_NEAR(anim->value.value(), 0.0f, 1e-4f);  // held for the carry
  ticker.tick(0.5);
  EXPECT_NEAR(anim->value.value(), 5.0f, 1e-4f);
}

TEST(Lanes, AWaypointEntrancePlaysItsSegmentsInTurn) {
  Ticker ticker;
  std::unique_ptr<AnimatedFloat> anim;
  Transitioned<float> t;
  t.value = 0.0f;
  t.spec.ease = &choreograph::easeNone;
  t.waypoints = {{std::chrono::milliseconds(0), 0.0f},
                 {std::chrono::milliseconds(1000), 10.0f},
                 {std::chrono::milliseconds(2000), 0.0f}};
  mountEntrance(ticker, anim, t, 0.0f);
  ASSERT_TRUE(anim);
  EXPECT_EQ(anim->target, 0.0f);
  ticker.tick(1.0);
  EXPECT_NEAR(anim->value.value(), 10.0f, 1e-4f);
  ticker.tick(0.5);
  EXPECT_NEAR(anim->value.value(), 5.0f, 1e-4f);
}
