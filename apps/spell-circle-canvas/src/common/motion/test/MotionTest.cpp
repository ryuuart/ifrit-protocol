// SigilMotion: the frame clock's pause/scale/clamp behavior, the Ticker
// actually driving Choreograph motions to completion, and the animation
// VALUES (Transition / ease:: / animate() / bind()) standing up with no
// renderer under them.

#include <sigilmotion/Animation.h>
#include <sigilmotion/FrameClock.h>
#include <sigilmotion/Ticker.h>

#include <gtest/gtest.h>

// POSITIVE CONTROL for the "SigilMotion alone" pin below. The whole point
// of moving the animation values out of <sigilcompose/Compose.h> is that a
// consumer can drive them without a drawing library, and a test asserting
// that is worthless if compose happens to be on the include path anyway —
// it would pass for the wrong reason. motion_test links SigilMotion and
// gtest only, so compose's headers must be UNREACHABLE from this TU. If
// SigilMotion ever grows a link edge that drags them in, this stops the
// build instead of quietly hollowing out the pin.
#if __has_include(<sigilcompose/Compose.h>)
#error "motion_test can see SigilCompose headers — the SigilMotion-alone \
pin is no longer proving anything (ROADMAP §37)."
#endif

using namespace sigil::motion;
namespace ch = choreograph;
using namespace std::chrono_literals;

TEST(FrameClockTest, FirstTickIsZeroThenDeltas) {
  FrameClock clock;
  EXPECT_EQ(clock.tick(10.0), 0.0);
  EXPECT_NEAR(clock.tick(10.016), 0.016, 1e-9);
  EXPECT_NEAR(clock.elapsed(), 0.016, 1e-9);
}

TEST(FrameClockTest, ClampsStallsAndScalesTime) {
  FrameClock clock({.maxDelta = 0.25});
  clock.tick(0.0);
  EXPECT_NEAR(clock.tick(5.0), 0.25, 1e-9); // suspended app: clamped

  clock.setTimeScale(0.5);
  EXPECT_NEAR(clock.tick(5.1), 0.05, 1e-9); // half speed
}

TEST(FrameClockTest, PauseFreezesElapsed) {
  FrameClock clock;
  clock.tick(0.0);
  clock.tick(0.1);
  clock.setPaused(true);
  EXPECT_EQ(clock.tick(0.2), 0.0);
  EXPECT_NEAR(clock.elapsed(), 0.1, 1e-9);
  clock.setPaused(false);
  // The paused span was consumed while paused — no catch-up jump.
  EXPECT_NEAR(clock.tick(0.3), 0.1, 1e-9);
}

TEST(TickerTest, DrivesMotionToCompletionAndSettles) {
  Ticker ticker;
  ch::Output<float> value = 0.0f;
  ticker.timeline().apply(&value).then<ch::RampTo>(10.0f, 1.0f);

  EXPECT_TRUE(ticker.active());
  ticker.tick(0.5);
  EXPECT_NEAR(value.value(), 5.0f, 1e-4);
  EXPECT_TRUE(ticker.active());

  ticker.tick(0.6); // past the end
  EXPECT_NEAR(value.value(), 10.0f, 1e-4);
  EXPECT_FALSE(ticker.active()); // finished motions self-remove
}

TEST(TickerTest, SteppablesReportAndRetire) {
  Ticker ticker;
  double accumulated = 0.0;
  ticker.add([&accumulated](double dt) {
    accumulated += dt;
    return accumulated < 1.0;
  });

  EXPECT_TRUE(ticker.tick(0.4));
  EXPECT_TRUE(ticker.tick(0.4));
  EXPECT_FALSE(ticker.tick(0.4)); // crossed 1.0 → retired
  EXPECT_FALSE(ticker.active());
}

// ---------------------------------------------------------------------------
// Animation values (<sigilmotion/Animation.h>) — moved out of SigilCompose
// 2026-07-29. These tests exist to prove the values are usable through
// SigilMotion ALONE: no Skia, no Yoga, no compose kernel is linked into
// motion_test, and the #error guard at the top of this file keeps it that
// way. Compose's own coverage of the same types (its reconciler compare,
// the Composer running them per frame) stays in compose_test.

TEST(AnimationValues, TransitionSurvivesAnEmptyEase) {
  // `{360ms, {}, 220ms}` — the obvious way to name a delay and keep the
  // house curve — leaves `ease` an EMPTY std::function. Reading it raw
  // throws bad_function_call on the first frame; easing() is the fix.
  const Transition named{360ms, {}, 220ms};
  EXPECT_EQ(named.duration, 360ms);
  EXPECT_EQ(named.delay, 220ms);
  EXPECT_FALSE((bool)named.ease);
  EXPECT_TRUE((bool)named.easing());
  EXPECT_NEAR(named.easing()(0.5f), choreograph::easeOutQuad(0.5f), 1e-6f);

  const Transition spec{200ms, ease::outBack()};
  EXPECT_GT(spec.easing()(0.8f), 1.0f); // overshoot, then settle
  EXPECT_NEAR(spec.easing()(1.0f), 1.0f, 1e-5f);
}

TEST(AnimationValues, AnimateBuildersDescribeEntranceChangeAndPath) {
  const Transitioned<float> entrance = animate(from(0.0f).to(1.0f), {400ms});
  EXPECT_FLOAT_EQ(entrance.value, 1.0f);
  ASSERT_TRUE(entrance.from.has_value());
  EXPECT_FLOAT_EQ(*entrance.from, 0.0f);
  EXPECT_EQ(entrance.spec.duration, 400ms);
  EXPECT_TRUE(entrance.waypoints.empty());

  const Transitioned<float> change = animate(to(0.4f), {180ms});
  EXPECT_FLOAT_EQ(change.value, 0.4f);
  EXPECT_FALSE(change.from.has_value()); // no entrance: ramp on change only

  const Transitioned<float> path = animate(
      through({{0ms, 40.f}, {200ms, -20.f}, {300ms, 10.f}, {400ms, 0.f}}));
  EXPECT_EQ(path.waypoints.size(), 4u);
  ASSERT_TRUE(path.from.has_value());
  EXPECT_FLOAT_EQ(*path.from, 40.f);   // first waypoint is the entrance
  EXPECT_FLOAT_EQ(path.value, 0.f);    // last is the settled value
  EXPECT_EQ(path.spec.duration, 400ms);

  // The degenerate ask must still be DETERMINATE — value-initialized, not
  // whatever was on the stack.
  const Transitioned<float> empty = animate(through({}));
  EXPECT_FLOAT_EQ(empty.value, 0.0f);
}

TEST(AnimationValues, BoundChainComposesInCallOrder) {
  ch::Output<float> phase = 0.0f;

  const BoundFloat named = bind(&phase).source(0, 100).target(-70, 170).value();
  const BoundFloat manual =
      bind(&phase).source(0, 100).scale(240).offset(-70).value();
  for (float v : {0.f, 25.f, 50.f, 100.f})
    EXPECT_NEAR(named.apply(v), manual.apply(v), 1e-4f);
  EXPECT_NEAR(named.apply(0.f), -70.f, 1e-4f);
  EXPECT_NEAR(named.apply(100.f), 170.f, 1e-4f);

  // Order matters and reads the way it looks.
  EXPECT_NEAR(bind(&phase).scale(240).offset(-70).value().apply(0.5f),
              0.5f * 240.f - 70.f, 1e-4f);
  EXPECT_NEAR(bind(&phase).offset(-70).scale(240).value().apply(0.5f),
              (0.5f - 70.f) * 240.f, 1e-4f);

  // window() is source() that also clamps, so a curve downstream never
  // sees a value outside its domain.
  const BoundFloat w = bind(&phase).window(0.2f, 0.4f).value();
  const BoundFloat s = bind(&phase).source(0.2f, 0.4f).value();
  EXPECT_NEAR(w.apply(0.3f), s.apply(0.3f), 1e-4f);
  EXPECT_NEAR(w.apply(0.9f), 1.0f, 1e-4f);
  EXPECT_GT(s.apply(0.9f), 1.0f);

  EXPECT_NEAR(bind(&phase).quantize(5).value().apply(0.31f), 0.25f, 1e-4f);
  EXPECT_NEAR(bind(&phase).invert().value().apply(0.25f), 0.75f, 1e-4f);
  EXPECT_NEAR(bind(&phase).clamp(0, 1).value().apply(4.0f), 1.0f, 1e-4f);
}

TEST(AnimationValues, TickerDrivesABoundChainWithNoRenderer) {
  // The pin: the clock half and the value half of SigilMotion, working
  // together, with nothing else linked. A choreograph Output rides the
  // Ticker; a shaped binding turns its [0,1] phase into pixels the way a
  // property slot downstream would read it.
  Ticker ticker;
  ch::Output<float> phase = 0.0f;
  const Transition spec{500ms, ease::outBack()};
  ticker.timeline().apply(&phase).then<ch::RampTo>(
      1.0f, (float)spec.duration.count() / 1000.0f, spec.easing());

  const BoundFloat toPixels = bind(&phase).target(-70.f, 170.f).value();
  EXPECT_NEAR(toPixels.apply(phase.value()), -70.f, 1e-3f);

  ASSERT_TRUE(ticker.active());
  ticker.tick(0.10); // t = 0.2 — still climbing
  const float early = toPixels.apply(phase.value());
  EXPECT_GT(early, -70.f);
  EXPECT_LT(early, 170.f);

  ticker.tick(0.15); // t = 0.5 — outBack is already past its target
  EXPECT_GT(toPixels.apply(phase.value()), 170.f);

  ticker.tick(0.40); // past the end
  EXPECT_NEAR(toPixels.apply(phase.value()), 170.f, 1e-3f);
  EXPECT_FALSE(ticker.active());
}

TEST(AnimationValues, AnimatableHoldsAllFourFormsWithNoKernel) {
  // Animatable<T> — the property SLOT — moved with the values it holds
  // (ROADMAP §37). Nothing here needs a reconciler, a canvas or a node.
  ch::Output<float> live = 3.0f;

  const Animatable<float> plain = 0.5f;
  const Animatable<float> ramped = animate(from(0.0f).to(1.0f), {400ms});
  const Animatable<float> bound = &live;
  const Animatable<float> shaped = bind(&live).source(0, 10).target(-70, 170);

  // The discriminant, in the order the pre-compaction std::variant had:
  // a shaped binding sorts AFTER a bare one rather than replacing it.
  EXPECT_EQ(plain.index(), 0);
  EXPECT_EQ(ramped.index(), 1);
  EXPECT_EQ(bound.index(), 2);
  EXPECT_EQ(shaped.index(), 3);

  ASSERT_NE(plain.plain(), nullptr);
  EXPECT_FLOAT_EQ(*plain.plain(), 0.5f);
  EXPECT_EQ(plain.transitioned(), nullptr);
  EXPECT_EQ(plain.binding(), nullptr);
  EXPECT_EQ(plain.boundMap(), nullptr);

  ASSERT_NE(ramped.transitioned(), nullptr);
  EXPECT_FLOAT_EQ(ramped.transitioned()->value, 1.0f);
  EXPECT_EQ(ramped.plain(), nullptr);

  // binding() answers for BOTH bound forms — the one accessor every
  // "bound ⇒ read it live" branch goes through — while boundMap() tells
  // them apart.
  EXPECT_EQ(bound.binding(), &live);
  EXPECT_EQ(shaped.binding(), &live);
  EXPECT_EQ(bound.boundMap(), nullptr);
  ASSERT_NE(shaped.boundMap(), nullptr);
  EXPECT_NEAR(shaped.boundMap()->apply(live.value()), -70.f + 0.3f * 240.f,
              1e-3f);

  // The out-of-line Extra block survives copies: copying a fat form must
  // deep-copy, not alias.
  Animatable<float> copy = shaped;
  ASSERT_NE(copy.boundMap(), nullptr);
  EXPECT_NE(copy.boundMap(), shaped.boundMap());
  EXPECT_EQ(copy.index(), 3);
  const Animatable<float> moved = std::move(copy);
  ASSERT_NE(moved.boundMap(), nullptr);
  EXPECT_EQ(moved.index(), 3);
}

TEST(AnimationValues, AnimatableDrivenByTheTickerWithNoKernel) {
  // A CONSUMER-SIDE resolve, written here in five lines, to show the
  // value type is enough on its own. Compose's real resolution is
  // context-aware (node transitions, stagger, mount entrances against a
  // PaintContext) and stays in compose — SigilMotion deliberately ships
  // no resolve surface.
  const auto readNow = [](const Animatable<float> &a, float fallback) {
    if (const float *p = a.plain())
      return *p;
    if (const choreograph::Output<float> *out = a.binding())
      return a.boundMap() ? a.boundMap()->apply(out->value()) : out->value();
    return fallback;
  };

  Ticker ticker;
  ch::Output<float> hp = 0.0f;
  ticker.timeline().apply(&hp).then<ch::RampTo>(100.0f, 1.0f);

  const Animatable<float> width = bind(&hp).source(0, 100).target(0, 240);
  EXPECT_NEAR(readNow(width, -1.f), 0.f, 1e-3f);

  ticker.tick(0.5);
  EXPECT_NEAR(readNow(width, -1.f), 120.f, 1e-2f);

  ticker.tick(0.6); // past the end
  EXPECT_NEAR(readNow(width, -1.f), 240.f, 1e-3f);
  EXPECT_FALSE(ticker.active());

  EXPECT_FLOAT_EQ(readNow(Animatable<float>{0.25f}, -1.f), 0.25f);
}
