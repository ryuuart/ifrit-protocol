/** @file
 * Transition, ease::, animate(), quantizeTime and the Animatable<T> slot
 * standing up with no renderer under them, and driven by the Ticker to
 * show the value type is enough on its own.
 */

#include <gtest/gtest.h>
#include <sigilmotion/clock/FrameClock.h>
#include <sigilmotion/clock/Ticker.h>
#include <sigilmotion/values/Values.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "support/StandsAlone.h"

using namespace sigil::motion;
namespace ch = choreograph;
using namespace std::chrono_literals;

// ---------------------------------------------------------------------------
// The values themselves: what a Transition, an animate() builder and an
// Animatable<T> slot promise on their own. A consumer's own coverage —
// how it stores these values and resolves them per frame — belongs in
// that consumer's tests.
TEST(Values, TransitionSurvivesAnEmptyEase) {
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

TEST(Values, AnimateBuildersDescribeEntranceChangeAndPath) {
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

TEST(Values, QuantizeTimeIsTheCanonicalFloorArithmetic) {
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

TEST(Values, TheTickerDrivesABoundValueWithNothingElseLinked) {
  // The clock half and the value half of SigilMotion working together,
  // with nothing else linked: a choreograph Output rides the Ticker, and
  // a shaped binding turns its phase into pixels the way a property slot
  // downstream would read it. The read is written out here in five lines
  // to show the value type is enough on its own — a real consumer's
  // resolution is context-aware, which is why SigilMotion ships no
  // resolve surface of its own.
  const auto readNow = [](const Animatable<float>& a, float fallback) {
    if (const float* p = a.plain()) return *p;
    if (const choreograph::Output<float>* out = a.binding())
      return a.boundMap() ? a.boundMap()->apply(out->value()) : out->value();
    return fallback;
  };

  Ticker ticker;
  ch::Output<float> phase = 0.0f;
  const Transition spec{500ms, ease::outBack()};
  ticker.timeline().apply(&phase).then<ch::RampTo>(
      1.0f, (float)spec.duration.count() / 1000.0f, spec.easing());

  const Animatable<float> toPixels = bind(&phase).target(-70.f, 170.f);
  EXPECT_NEAR(readNow(toPixels, -1.f), -70.f, 1e-3f);

  ticker.tick(0.10);  // t = 0.2 — still climbing
  const float early = readNow(toPixels, -1.f);
  EXPECT_GT(early, -70.f);
  EXPECT_LT(early, 170.f);

  ticker.tick(0.15);  // t = 0.5 — outBack is already past its target
  EXPECT_GT(readNow(toPixels, -1.f), 170.f);

  ticker.tick(0.40);  // past the end
  EXPECT_NEAR(readNow(toPixels, -1.f), 170.f, 1e-3f);

  EXPECT_FLOAT_EQ(readNow(Animatable<float>{0.25f}, -1.f), 0.25f);
}

namespace {
/** One of the four forms an Animatable<float> holds, its discriminant,
 *  which accessors are entitled to answer for it, and the number the
 *  entitled one answers with. */
struct Form {
  const char* name;
  Animatable<float> slot;
  int index;
  bool plain, transitioned, binding, boundMap;
  float read;
};

/** The live cell the two bound forms name. It outlives every parameter
 *  because a binding holds the ADDRESS of a cell somebody else owns. */
ch::Output<float>& live() {
  static ch::Output<float> cell = 3.0f;
  return cell;
}

std::string formName(const testing::TestParamInfo<Form>& info) {
  return info.param.name;
}

struct Forms : testing::TestWithParam<Form> {};
}  // namespace

TEST_P(Forms, AnswerOnlyThroughTheAccessorsTheirDiscriminantAllows) {
  // The discriminant's numbering is public behaviour — a shaped binding
  // sorts AFTER a bare one rather than taking its place — and each
  // accessor answers for its own form and returns null for the rest.
  // binding() is the exception on purpose: it answers for BOTH bound
  // forms, so a consumer asking only "is this driven live?" reads one
  // accessor, and boundMap() tells the two apart when it matters.
  const Form& form = GetParam();
  EXPECT_EQ(form.slot.index(), form.index);
  EXPECT_EQ(form.slot.plain() != nullptr, form.plain);
  EXPECT_EQ(form.slot.transitioned() != nullptr, form.transitioned);
  EXPECT_EQ(form.slot.binding() != nullptr, form.binding);
  EXPECT_EQ(form.slot.boundMap() != nullptr, form.boundMap);

  // Read back through the entitled accessor, in the reading order a
  // consumer follows: a plain value, then a spec, then a live cell shaped
  // by its map when it has one.
  float got = 0.0f;
  if (const float* p = form.slot.plain())
    got = *p;
  else if (const Transitioned<float>* t = form.slot.transitioned())
    got = t->value;
  else if (const ch::Output<float>* out = form.slot.binding())
    got = form.slot.boundMap() ? form.slot.boundMap()->apply(out->value())
                               : out->value();
  EXPECT_NEAR(got, form.read, 1e-3f);
}

INSTANTIATE_TEST_SUITE_P(
    Held, Forms,
    testing::Values(
        Form{"plain", Animatable<float>{0.5f}, 0, true, false, false, false,
             0.5f},
        Form{"transitioned",
             Animatable<float>{animate(from(0.0f).to(1.0f), {400ms})}, 1, false,
             true, false, false, 1.0f},
        Form{"bound", Animatable<float>{&live()}, 2, false, false, true, false,
             3.0f},
        Form{"shaped",
             Animatable<float>{bind(&live()).source(0, 10).target(-70, 170)}, 3,
             false, false, true, true, -70.f + 0.3f * 240.f}),
    formName);

TEST(Values, CopyingAShapedSlotDeepCopiesItsOutOfLineBlock) {
  // The fat forms keep their extra state out of line, so copying one must
  // deep-copy rather than alias — two slots that shared a map would move
  // together the first time either was reshaped.
  ch::Output<float> cell = 3.0f;
  const Animatable<float> shaped = bind(&cell).source(0, 10).target(-70, 170);
  Animatable<float> copy = shaped;
  ASSERT_NE(copy.boundMap(), nullptr);
  EXPECT_NE(copy.boundMap(), shaped.boundMap());
  EXPECT_EQ(copy.index(), 3);
  const Animatable<float> moved = std::move(copy);
  ASSERT_NE(moved.boundMap(), nullptr);
  EXPECT_EQ(moved.index(), 3);
}
