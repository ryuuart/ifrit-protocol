// SigilMotionValues: Transition / ease:: / animate() / quantizeTime and
// the Animatable<T> slot standing up with no renderer under them, and
// driven by the Ticker to show the value type is enough on its own.

#include <gtest/gtest.h>
#include <sigilmotion/FrameClock.h>
#include <sigilmotion/Ticker.h>
#include <sigilmotion/Values.h>

#include <algorithm>
#include <cmath>
#include <vector>

// POSITIVE CONTROL for the "SigilMotion alone" tests below. Those claim a
// consumer can drive these values without linking a drawing library, and
// the claim would pass for the wrong reason if a drawing library happened
// to be on the include path anyway. This target links SigilMotion and
// gtest only, so a rendering library's headers must be UNREACHABLE here.
// If SigilMotion grows a link edge that drags them in, the build stops
// rather than quietly hollowing the tests out.
#if __has_include(<sigilcompose/Compose.h>)
#error \
    "motion_values_test can see a drawing library's headers — the tests \
below no longer prove that SigilMotion stands alone."
#endif

using namespace sigil::motion;
namespace ch = choreograph;
using namespace std::chrono_literals;

// ---------------------------------------------------------------------------
// Animation values (<sigilmotion/Values.h>). These prove the values are
// usable through SigilMotion ALONE: no drawing library, no layout engine
// and no scene kernel is linked here, and the #error guard at the top of
// this file keeps it that way. A consumer's own coverage — how it stores
// these values and resolves them per frame — belongs in that consumer's
// tests.
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
  EXPECT_GT(spec.easing()(0.8f), 1.0f);  // overshoot, then settle
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
  EXPECT_FALSE(change.from.has_value());  // no entrance: ramp on change only

  const Transitioned<float> path = animate(
      through({{0ms, 40.f}, {200ms, -20.f}, {300ms, 10.f}, {400ms, 0.f}}));
  EXPECT_EQ(path.waypoints.size(), 4u);
  ASSERT_TRUE(path.from.has_value());
  EXPECT_FLOAT_EQ(*path.from, 40.f);  // first waypoint is the entrance
  EXPECT_FLOAT_EQ(path.value, 0.f);   // last is the settled value
  EXPECT_EQ(path.spec.duration, 400ms);

  // The degenerate ask must still be DETERMINATE — value-initialized, not
  // whatever was on the stack.
  const Transitioned<float> empty = animate(through({}));
  EXPECT_FLOAT_EQ(empty.value, 0.0f);
}

TEST(AnimationValues, QuantizeTimeIsTheCanonicalFloorArithmetic) {
  // motion::quantizeTime against the hand-written floor(t*N)/N it stands
  // in for, bit-exact in BOTH precisions — the template keeps each call
  // site's own type rather than promoting to double.
  for (double t : {0.0, 0.081, 1.0 / 6.0, 2.499999, 13.37, 1000.05}) {
    EXPECT_EQ(quantizeTime(t, 6.0), std::floor(t * 6.0) / 6.0);
    EXPECT_EQ(quantizeTime(t, 8.0), std::floor(t * 8.0) / 8.0);
    const float ft = (float)t;
    EXPECT_EQ(quantizeTime(ft, 8.0f), std::floor(ft * 8.0f) / 8.0f);
  }
  // hz <= 0 answers the input unchanged: the spelling of "continuous".
  EXPECT_EQ(quantizeTime(1.234, 0.0), 1.234);
  EXPECT_EQ(quantizeTime(1.234, -5.0), 1.234);
  // …and the value HOLDS between steps, which is the whole point.
  EXPECT_EQ(quantizeTime(0.10, 6.0), quantizeTime(0.16, 6.0));
  EXPECT_NE(quantizeTime(0.16, 6.0), quantizeTime(0.17, 6.0));
}

TEST(AnimationValues, TickerDrivesABoundChainWithNoRenderer) {
  // The clock half and the value half of SigilMotion working together,
  // with nothing else linked. A choreograph Output rides the Ticker; a
  // shaped binding turns its [0,1] phase into pixels the way a property
  // slot downstream would read it.
  Ticker ticker;
  ch::Output<float> phase = 0.0f;
  const Transition spec{500ms, ease::outBack()};
  ticker.timeline().apply(&phase).then<ch::RampTo>(
      1.0f, (float)spec.duration.count() / 1000.0f, spec.easing());

  const BoundFloat toPixels = bind(&phase).target(-70.f, 170.f).value();
  EXPECT_NEAR(toPixels.apply(phase.value()), -70.f, 1e-3f);

  ASSERT_TRUE(ticker.active());
  ticker.tick(0.10);  // t = 0.2 — still climbing
  const float early = toPixels.apply(phase.value());
  EXPECT_GT(early, -70.f);
  EXPECT_LT(early, 170.f);

  ticker.tick(0.15);  // t = 0.5 — outBack is already past its target
  EXPECT_GT(toPixels.apply(phase.value()), 170.f);

  ticker.tick(0.40);  // past the end
  EXPECT_NEAR(toPixels.apply(phase.value()), 170.f, 1e-3f);
  EXPECT_FALSE(ticker.active());
}

TEST(AnimationValues, AnimatableHoldsAllFourFormsWithNoKernel) {
  // Animatable<T>, the property SLOT: nothing here needs a reconciler, a
  // canvas or a scene node.
  ch::Output<float> live = 3.0f;

  const Animatable<float> plain = 0.5f;
  const Animatable<float> ramped = animate(from(0.0f).to(1.0f), {400ms});
  const Animatable<float> bound = &live;
  const Animatable<float> shaped = bind(&live).source(0, 10).target(-70, 170);

  // The discriminant's numbering is public behaviour: a shaped binding
  // sorts AFTER a bare one rather than taking its place.
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

  // binding() answers for BOTH bound forms, so a consumer asking only
  // "is this driven live?" reads one accessor; boundMap() tells the two
  // apart when it matters.
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
  // value type is enough on its own. A real consumer's resolution is
  // context-aware — inherited transitions, staggering, mount entrances
  // against its own frame state — which is why SigilMotion deliberately
  // ships no resolve surface of its own.
  const auto readNow = [](const Animatable<float>& a, float fallback) {
    if (const float* p = a.plain()) return *p;
    if (const choreograph::Output<float>* out = a.binding())
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

  ticker.tick(0.6);  // past the end
  EXPECT_NEAR(readNow(width, -1.f), 240.f, 1e-3f);
  EXPECT_FALSE(ticker.active());

  EXPECT_FLOAT_EQ(readNow(Animatable<float>{0.25f}, -1.f), 0.25f);
}
