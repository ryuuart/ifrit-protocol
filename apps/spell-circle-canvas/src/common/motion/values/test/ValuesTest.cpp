/** @file
 * The values with no clock under them: the four forms an Animatable<T>
 * holds and the accessors each answers through, a live value shared by
 * every copy, a bound value read lazily through its stages (and a bound
 * value followed in turn), the Tween a described motion is — read at a
 * time with no engine, through keyframes, passes and a stagger resolved
 * for a child — the Transition, the curve comparator, and the arithmetic
 * over a clock reading.
 */

#include "support/StandsAlone.h"

#include <gtest/gtest.h>
#include <sigilmotion/bind/Binding.h>
#include <sigilmotion/ease/Ease.h>
#include <sigilmotion/schedule/Stagger.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilmotion/values/Time.h>
#include <sigilmotion/values/Transition.h>
#include <sigilmotion/values/Tween.h>

#include <chrono>
#include <cmath>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace sigil::motion;
using namespace std::chrono_literals;

// ---------------------------------------------------------------------------
// A live value: one cell, however many copies read it.

TEST(Values, EveryCopyOfALiveValueReadsAndWritesOneCell) {
  Animatable<float> wave = animatable(0.25f);
  Animatable<float> copy = wave;
  EXPECT_EQ(copy.form(), Animatable<float>::Form::Live);
  EXPECT_EQ(copy.identity(), wave.identity());
  EXPECT_FLOAT_EQ(copy.value(), 0.25f);

  // Assigning a number to a live value writes the cell, so every copy —
  // and every property handed one — reads the new number.
  wave = 0.75f;
  EXPECT_FLOAT_EQ(copy.value(), 0.75f);
  copy = 0.5f;
  EXPECT_FLOAT_EQ(wave.value(), 0.5f);
  EXPECT_EQ(wave.form(), Animatable<float>::Form::Live);
}

TEST(Values, ALiveValueIsEqualByItsCellAndNeverByItsNumber) {
  // Two live values holding the same number are two values: a slot that
  // is moving must never be pruned into a slot moving something else.
  const Animatable<float> wave = animatable(0.5f);
  const Animatable<float> copy = wave;
  const Animatable<float> other = animatable(0.5f);
  EXPECT_TRUE(copy == wave);
  EXPECT_TRUE(propertyEqual(copy, wave));
  EXPECT_FALSE(other == wave);
  // …and never equal to the constant it happens to hold either.
  EXPECT_FALSE(wave == Animatable<float>(0.5f));
}

TEST(Values, TheCellLivesAsLongAsAnyCopyDoes) {
  Animatable<float> wave = animatable(0.25f);
  const std::weak_ptr<detail::Cell<float>> cell = wave.cell();
  Animatable<float> copy = wave;
  // Replacing the value with another form lets go of the cell…
  wave = Animatable<float>(1.0f);
  EXPECT_EQ(wave.form(), Animatable<float>::Form::Constant);
  ASSERT_FALSE(cell.expired());
  EXPECT_FLOAT_EQ(copy.value(), 0.25f);
  // …and the last copy letting go of it frees it.
  copy = Animatable<float>{};
  EXPECT_TRUE(cell.expired());
}

TEST(Values, ANumberAssignedToAnyOtherFormMakesItThatConstant) {
  const Animatable<float> wave = animatable(3.0f);
  Animatable<float> shaped = bind(wave, {.to = {0.0f, 10.0f}});
  shaped = 4.0f;
  EXPECT_EQ(shaped.form(), Animatable<float>::Form::Constant);
  EXPECT_FLOAT_EQ(shaped.value(), 4.0f);
  // The source it followed is untouched.
  EXPECT_FLOAT_EQ(wave.value(), 3.0f);

  Animatable<float> described = animate({.to = 1.0f});
  described = 2.0f;
  EXPECT_EQ(described.form(), Animatable<float>::Form::Constant);
  EXPECT_EQ(described.described(), nullptr);
}

TEST(Values, ADefaultValueIsTheConstantZero) {
  const Animatable<float> empty;
  EXPECT_EQ(empty.form(), Animatable<float>::Form::Constant);
  ASSERT_NE(empty.constant(), nullptr);
  EXPECT_FLOAT_EQ(*empty.constant(), 0.0f);
  EXPECT_EQ(empty.binding(), nullptr);
  EXPECT_EQ(empty.identity(), nullptr);
  EXPECT_EQ(empty.cell(), nullptr);
  EXPECT_FALSE(empty.isRunning());
}

TEST(Values, AnIntegerLiteralIsTheNumberAndNotAPointer) {
  // The claim is about OVERLOAD RESOLUTION, so the compiler makes it, and
  // the LITERAL below is the whole test: a literal 0 is a null-pointer
  // constant as well as a number, and a slot that took anything a
  // pointer can be made from would leave `Animatable<float>(0)`
  // ambiguous. A consumer would then need an integral overload of its own
  // for every numeric property it has.
  static_assert(!std::is_constructible_v<Animatable<float>, void*>,
                "a pointer is not a value this slot holds");
  static_assert(!std::is_constructible_v<Animatable<float>, float*>,
                "a live value is made by animatable(), not by an address");

  const Animatable<float> zero(0);
  EXPECT_EQ(zero.form(), Animatable<float>::Form::Constant);
  ASSERT_NE(zero.constant(), nullptr);
  EXPECT_FLOAT_EQ(*zero.constant(), 0.0f);
}

// ---------------------------------------------------------------------------
// A bound value: a live number followed through stages, read when read.

TEST(Values, ABoundValueReadsItsSourceThroughItsStagesWhenItIsRead) {
  Animatable<float> phase = animatable(0.0f);
  const Animatable<float> pixels = bind(phase, {.to = {-70.0f, 170.0f}});
  EXPECT_EQ(pixels.form(), Animatable<float>::Form::Bound);
  EXPECT_FLOAT_EQ(pixels.value(), -70.0f);
  // Nothing is recomputed on a clock: the stages run on the source's
  // number at the moment the value is read, so it is never a frame late.
  phase = 0.5f;
  EXPECT_FLOAT_EQ(pixels.value(), 50.0f);
  phase = 1.0f;
  EXPECT_FLOAT_EQ(pixels.value(), 170.0f);
  // It is the source's identity, and exactly as live as the source.
  EXPECT_EQ(pixels.identity(), phase.identity());
  EXPECT_TRUE(pixels.isRunning());
  EXPECT_FALSE(bind(Animatable<float>(0.5f)).isRunning());
}

TEST(Values, ABoundValueFollowedInTurnComposesItsStages) {
  // bind(bind(x, a), b) reads b applied to a applied to x — a derived
  // value is itself a value to derive from, at any depth, and every level
  // is read on the same number.
  Animatable<float> source = animatable(0.5f);
  const Animatable<float> tens = bind(source, {.to = {0.0f, 10.0f}});
  const Animatable<float> composed =
      bind(tens, {.from = {0.0f, 10.0f}, .to = {100.0f, 200.0f}});
  EXPECT_FLOAT_EQ(tens.value(), 5.0f);
  EXPECT_FLOAT_EQ(composed.value(), 150.0f);
  source = 1.0f;
  EXPECT_FLOAT_EQ(composed.value(), 200.0f);
  EXPECT_EQ(composed.identity(), source.identity());
  ASSERT_NE(composed.source(), nullptr);
  EXPECT_EQ(composed.source()->form(), Animatable<float>::Form::Bound);
}

TEST(Values, ABoundValueRetainsItsSourceThroughEveryCopy) {
  Animatable<float> source = animatable(0.5f);
  const std::weak_ptr<detail::Cell<float>> cell = source.cell();
  Animatable<float> shaped = bind(source, {.to = {10.0f, 30.0f}});
  source = Animatable<float>{};
  ASSERT_FALSE(cell.expired());
  EXPECT_FLOAT_EQ(shaped.value(), 20.0f);
  Animatable<float> moved = std::move(shaped);
  EXPECT_FALSE(cell.expired());
  moved = 0.0f;
  EXPECT_TRUE(cell.expired());
}

TEST(Values, TwoBoundValuesAreEqualByTheirSourceAndTheirStages) {
  const Animatable<float> source = animatable(0.5f);
  const Animatable<float> shaped = bind(source, {.to = {10.0f, 30.0f}});
  EXPECT_TRUE(shaped == bind(source, {.to = {10.0f, 30.0f}}));
  EXPECT_FALSE(shaped == bind(source, {.to = {10.0f, 40.0f}}));
  EXPECT_FALSE(shaped == bind(animatable(0.5f), {.to = {10.0f, 30.0f}}));
  // A shaped value never takes the place of the bare one it follows.
  EXPECT_FALSE(shaped == source);
}

TEST(Values, CopyingAShapedSlotDeepCopiesItsOutOfLineBlock) {
  // The fat forms keep their extra state out of line, so copying one must
  // deep-copy rather than alias — two slots that shared their stages would
  // move together the first time either was reshaped.
  const Animatable<float> cell = animatable(3.0f);
  const Animatable<float> shaped =
      bind(cell, {.from = {0.0f, 10.0f}, .to = {-70.0f, 170.0f}});
  Animatable<float> copy = shaped;
  ASSERT_NE(copy.binding(), nullptr);
  EXPECT_NE(copy.binding(), shaped.binding());
  EXPECT_EQ(copy.form(), Animatable<float>::Form::Bound);
  EXPECT_EQ(copy.identity(), shaped.identity());
  const Animatable<float> moved = std::move(copy);
  ASSERT_NE(moved.binding(), nullptr);
  EXPECT_EQ(moved.form(), Animatable<float>::Form::Bound);
}

namespace {
/** One of the four forms an Animatable<float> holds, which accessors are
 *  entitled to answer for it, and the number it reads as. */
struct FormCase {
  const char* name;
  Animatable<float> slot;
  Animatable<float>::Form form;
  bool constant, described, binding, identity, running;
  float read;
};

/** The live cell the two live forms read. */
const Animatable<float>& live() {
  static const Animatable<float> cell = animatable(3.0f);
  return cell;
}

std::string formName(const testing::TestParamInfo<FormCase>& info) {
  return info.param.name;
}

struct Forms : testing::TestWithParam<FormCase> {};
}  // namespace

TEST_P(Forms, AnswerOnlyThroughTheAccessorsTheirFormAllows) {
  // The form's numbering is public behaviour — a shaped value sorts AFTER
  // a bare live one rather than taking its place — and each accessor
  // answers for its own form and returns null for the rest. identity()
  // answers for BOTH live forms, so a consumer asking only "is this
  // driven live?" reads one accessor.
  const FormCase& form = GetParam();
  EXPECT_EQ(form.slot.form(), form.form);
  EXPECT_EQ(form.slot.constant() != nullptr, form.constant);
  EXPECT_EQ(form.slot.described() != nullptr, form.described);
  EXPECT_EQ(form.slot.binding() != nullptr, form.binding);
  EXPECT_EQ(form.slot.source() != nullptr, form.binding);
  EXPECT_EQ(form.slot.identity() != nullptr, form.identity);
  EXPECT_EQ(form.slot.isRunning(), form.running);
  EXPECT_NEAR(form.slot.value(), form.read, 1e-3f);
}

INSTANTIATE_TEST_SUITE_P(
    Held, Forms,
    testing::Values(
        FormCase{"constant", Animatable<float>{0.5f},
                 Animatable<float>::Form::Constant, true, false, false, false,
                 false, 0.5f},
        FormCase{"described",
                 animate({.from = 0.0f, .to = 1.0f, .duration = 400ms}),
                 Animatable<float>::Form::Described, false, true, false, false,
                 false, 1.0f},
        FormCase{"live", live(), Animatable<float>::Form::Live, false, false,
                 false, true, true, 3.0f},
        FormCase{"shaped",
                 bind(live(), {.from = {0.0f, 10.0f}, .to = {-70.0f, 170.0f}}),
                 Animatable<float>::Form::Bound, false, false, true, true, true,
                 -70.0f + 0.3f * 240.0f}),
    formName);

// ---------------------------------------------------------------------------
// The Tween: a motion, described, read with no engine.

TEST(Values, ATweenNamingFromIsAnEntranceAndToAloneEasesOnChange) {
  const Animatable<float> entrance =
      animate({.from = 0.0f, .to = 1.0f, .duration = 400ms});
  const Tween<float>* described = entrance.described();
  ASSERT_NE(described, nullptr);
  EXPECT_TRUE(described->isEntrance());
  EXPECT_FLOAT_EQ(described->from->value(), 0.0f);
  EXPECT_FLOAT_EQ(described->rest(), 1.0f);
  EXPECT_EQ(described->duration.value(), 400ms);
  EXPECT_TRUE(described->keyframes.empty());

  const Tween<float> change{.to = 0.4f};
  EXPECT_FALSE(change.isEntrance());  // no entrance: eases on change only
  EXPECT_FLOAT_EQ(change.rest(), 0.4f);
  EXPECT_EQ(change.duration.value(), 250ms);
  EXPECT_EQ(change.delay.value(), 0ms);

  const Tween<float> path{
      .from = 40.0f,
      .keyframes = {{.to = -20.0f, .duration = 200ms},
                    {.to = 10.0f, .duration = 100ms},
                    {.to = 0.0f, .duration = 100ms}}};
  EXPECT_TRUE(path.isEntrance());
  EXPECT_FLOAT_EQ(path.rest(), 0.0f);  // the last keyframe is where it rests

  // The degenerate ask is still DETERMINATE — value-initialized, not
  // whatever was on the stack.
  EXPECT_FLOAT_EQ(Tween<float>{}.rest(), 0.0f);
}

TEST(Values, ATweenReadAtATimeHoldsItsDelayThenEases) {
  const Tween<float> ramp{.from = 0.0f,
                          .to = 10.0f,
                          .duration = 1s,
                          .delay = 200ms,
                          .ease = ease::linear};
  EXPECT_FLOAT_EQ(ramp.at(0ms), 0.0f);
  EXPECT_FLOAT_EQ(ramp.at(200ms), 0.0f);  // the delay holds `from`
  EXPECT_NEAR(ramp.at(700ms), 5.0f, 1e-4f);
  EXPECT_FLOAT_EQ(ramp.at(1200ms), 10.0f);
  EXPECT_FLOAT_EQ(ramp.at(5s), 10.0f);  // past the end it rests
  // A tween with no `from` starts where it rests: nothing to move from.
  EXPECT_FLOAT_EQ((Tween<float>{.to = 3.0f}.at(100ms)), 3.0f);
}

TEST(Values, AKeyframeWithNoDurationTakesAnEqualShareOfTheTween) {
  // Undurationed keyframes share `duration / keyframes.size()` — two
  // steps over a second are half a second each.
  const Tween<float> even{.from = 0.0f,
                          .keyframes = {{.to = 10.0f}, {.to = 0.0f}},
                          .duration = 1s,
                          .ease = ease::linear};
  EXPECT_NEAR(even.at(250ms), 5.0f, 1e-4f);
  EXPECT_NEAR(even.at(500ms), 10.0f, 1e-4f);
  EXPECT_NEAR(even.at(750ms), 5.0f, 1e-4f);
  EXPECT_NEAR(even.at(1s), 0.0f, 1e-4f);

  // A stated duration is kept, and the share is still the tween's
  // duration over the number of keyframes — not what is left over.
  const Tween<float> mixed{.from = 0.0f,
                           .keyframes = {{.to = 10.0f, .duration = 200ms},
                                         {.to = 0.0f}},
                           .duration = 1s,
                           .ease = ease::linear};
  EXPECT_NEAR(mixed.at(100ms), 5.0f, 1e-4f);
  EXPECT_NEAR(mixed.at(200ms), 10.0f, 1e-4f);
  EXPECT_NEAR(mixed.at(450ms), 5.0f, 1e-4f);
  EXPECT_NEAR(mixed.at(700ms), 0.0f, 1e-4f);

  // A keyframe's own curve wins over the tween's.
  const Tween<float> curved{
      .from = 0.0f,
      .keyframes = {{.to = 1.0f, .duration = 1s, .ease = ease::inQuad}},
      .ease = ease::linear};
  EXPECT_NEAR(curved.at(500ms), 0.25f, 1e-4f);
}

TEST(Values, ATweenRepeatsAsItsLoopSaysAndAlternatesEveryOtherPass) {
  const Tween<float> bounce{.from = 0.0f,
                            .to = 10.0f,
                            .duration = 1s,
                            .ease = ease::linear,
                            .loop = 1,
                            .alternate = true};
  EXPECT_NEAR(bounce.at(500ms), 5.0f, 1e-4f);
  // The second pass runs backwards.
  EXPECT_NEAR(bounce.at(1250ms), 7.5f, 1e-4f);
  EXPECT_NEAR(bounce.at(1750ms), 2.5f, 1e-4f);
  // Past the last pass it rests where that pass ended: back at the start.
  EXPECT_FLOAT_EQ(bounce.at(3s), 0.0f);

  // For ever, the passes keep coming.
  const Tween<float> forever{.from = 0.0f,
                             .to = 10.0f,
                             .duration = 1s,
                             .ease = ease::linear,
                             .loop = -1};
  EXPECT_NEAR(forever.at(Duration(10.25)), 2.5f, 1e-3f);
}

TEST(Values, ATweenResolvesEveryStaggeredFieldForTheChildAtAPlace) {
  const Tween<float> fan{.to = stagger(10.0f),
                         .duration = stagger({200ms, 500ms}),
                         .delay = stagger(40ms)};
  EXPECT_TRUE(fan.isStaggered());
  const Tween<float> third = fan.resolved({2, 4});
  EXPECT_FALSE(third.isStaggered());
  EXPECT_FLOAT_EQ(third.rest(), 20.0f);
  EXPECT_NEAR(third.delay.value().count(), 0.080, 1e-9);
  EXPECT_NEAR(third.duration.value().count(), 0.400, 1e-9);
  // A child alone is the first child.
  EXPECT_FLOAT_EQ(fan.rest(), 0.0f);
  // What a stagger does not touch is carried over.
  EXPECT_TRUE(easeEqual(third.easing(), fan.easing()));
  EXPECT_FALSE((Tween<float>{.to = 1.0f}).isStaggered());
}

TEST(Values, TwoTweensAreEqualByEveryField) {
  const Tween<float> base{.from = 0.0f, .to = 1.0f, .duration = 300ms};
  EXPECT_TRUE(tweenEqual(base, Tween<float>{.from = 0.0f, .to = 1.0f,
                                            .duration = 300ms}));
  Tween<float> moved = base;
  moved.composition = Composition::Blend;
  EXPECT_FALSE(tweenEqual(base, moved));
  moved = base;
  moved.delay = stagger(10ms);
  EXPECT_FALSE(tweenEqual(base, moved));
  moved = base;
  moved.ease = ease::outBack(2.4f);
  EXPECT_FALSE(tweenEqual(base, moved));
  // An empty curve is read as the default one it stands for.
  moved = base;
  moved.ease = {};
  EXPECT_TRUE(tweenEqual(base, moved));
  // Two described slots compare by their tweens.
  EXPECT_TRUE(animate(base) == animate(Tween<float>(base)));
  EXPECT_FALSE(animate(base) == animate({.to = 1.0f}));
}

TEST(Values, ATweenReadsAnyValueThatAddsAndScales) {
  struct Point {
    float x = 0.0f, y = 0.0f;
    Point operator+(const Point& other) const { return {x + other.x, y + other.y}; }
    Point operator-(const Point& other) const { return {x - other.x, y - other.y}; }
    Point operator*(float amount) const { return {x * amount, y * amount}; }
    bool operator==(const Point&) const = default;
  };
  const Tween<Point> glide{.from = Point{0.0f, 0.0f},
                           .to = Point{10.0f, -4.0f},
                           .duration = 1s,
                           .ease = ease::linear};
  const Point half = glide.at(500ms);
  EXPECT_NEAR(half.x, 5.0f, 1e-4f);
  EXPECT_NEAR(half.y, -2.0f, 1e-4f);
  const Animatable<Point> slot = animate<Point>(glide);
  EXPECT_EQ(slot.value(), (Point{10.0f, -4.0f}));
}

TEST(Values, ATweenReadWithNoEngineDrivesABoundValue) {
  // The value half of SigilMotion on its own: a sketch that holds no
  // engine reads a tween at its own clock, writes the number into a live
  // value, and a shaped binding turns it into pixels the way a property
  // downstream would read it.
  Animatable<float> phase = animatable(0.0f);
  const Tween<float> swing{
      .from = 0.0f, .to = 1.0f, .duration = 500ms, .ease = ease::outBack()};
  const Animatable<float> pixels = bind(phase, {.to = {-70.0f, 170.0f}});
  EXPECT_NEAR(pixels.value(), -70.0f, 1e-3f);

  phase = swing.at(100ms);  // still climbing
  EXPECT_GT(pixels.value(), -70.0f);
  EXPECT_LT(pixels.value(), 170.0f);

  phase = swing.at(250ms);  // outBack is already past its target
  EXPECT_GT(pixels.value(), 170.0f);

  phase = swing.at(650ms);  // past the end
  EXPECT_NEAR(pixels.value(), 170.0f, 1e-3f);
}

// ---------------------------------------------------------------------------
// The Transition and the curve comparator.

TEST(Values, TransitionSurvivesAnEmptyEase) {
  // `{360ms, 220ms, {}}` — the obvious way to name a delay and keep the
  // house curve — leaves `ease` an EMPTY std::function. Reading it raw
  // throws bad_function_call on the first frame; easing() is the fix.
  const Transition named{360ms, 220ms, {}};
  EXPECT_EQ(named.duration, 360ms);
  EXPECT_EQ(named.delay, 220ms);
  EXPECT_FALSE((bool)named.ease);
  EXPECT_TRUE((bool)named.easing());
  EXPECT_NEAR(named.easing()(0.5f), ease::outQuad(0.5f), 1e-6f);
  EXPECT_EQ(named.composition, Composition::Replace);

  const Transition spec{.duration = 200ms, .ease = ease::outBack()};
  EXPECT_GT(spec.easing()(0.8f), 1.0f);  // overshoot, then settle
  EXPECT_NEAR(spec.easing()(1.0f), 1.0f, 1e-5f);

  // The same rule on a tween.
  const Tween<float> tween{.to = 1.0f, .ease = {}};
  EXPECT_NEAR(tween.easing()(0.5f), ease::outQuad(0.5f), 1e-6f);
}

TEST(Values, AShapedCurveComparesEqualAtTheSameSettings) {
  // A curve built by binding a shape parameter into a lambda compares
  // equal to nothing, so every value holding one re-patches forever.
  // ease::Curve keeps the shape and the numbers where they can be read
  // back, so two calls at the same argument are the same curve.
  EXPECT_TRUE(easeEqual(ease::outBack(), ease::outBack()));
  EXPECT_FALSE(easeEqual(ease::outBack(1.7f), ease::outBack(2.4f)));
  EXPECT_TRUE(
      easeEqual(ease::outElastic(1.0f, 0.3f), ease::outElastic(1.0f, 0.3f)));
  EXPECT_FALSE(
      easeEqual(ease::outElastic(1.0f, 0.3f), ease::outElastic(1.0f, 0.5f)));
  // A CSS curve is its four control numbers.
  EXPECT_TRUE(easeEqual(ease::cubicBezier(0.25f, 0.1f, 0.25f, 1.0f),
                        ease::cubicBezier(0.25f, 0.1f, 0.25f, 1.0f)));
  EXPECT_FALSE(easeEqual(ease::cubicBezier(0.25f, 0.1f, 0.25f, 1.0f),
                         ease::cubicBezier(0.4f, 0.0f, 0.2f, 1.0f)));
  // Two DIFFERENT shapes at the same numbers are different curves.
  EXPECT_FALSE(easeEqual(ease::outBack(1.7f), ease::inBack(1.7f)));
  // A plain function pointer compares by its address, and a capturing
  // lambda compares to nothing.
  EXPECT_TRUE(easeEqual(&ease::smoothstep, &ease::smoothstep));
  const float factor = 2.0f;
  Easing captured = [factor](float progress) { return progress * factor; };
  EXPECT_FALSE(easeEqual(captured, captured));
  // …and it evaluates the curve it says it is.
  EXPECT_FLOAT_EQ(ease::cubicBezier(0.25f, 0.1f, 0.25f, 1.0f)(0.0f), 0.0f);
  EXPECT_FLOAT_EQ(ease::cubicBezier(0.25f, 0.1f, 0.25f, 1.0f)(1.0f), 1.0f);
  EXPECT_GT(ease::cubicBezier(0.25f, 0.1f, 0.25f, 1.0f)(0.5f), 0.5f);
  // A transition that only differs by a curve's SETTING must not prune.
  EXPECT_TRUE(transitionEqual({.ease = ease::outBack(1.7f)},
                              {.ease = ease::outBack(1.7f)}));
  EXPECT_FALSE(transitionEqual({.ease = ease::outBack(1.7f)},
                               {.ease = ease::outBack(3.0f)}));
  EXPECT_FALSE(transitionEqual({}, {.composition = Composition::Blend}));
}

// ---------------------------------------------------------------------------
// The arithmetic over a clock reading.

TEST(Values, TheFlashEnvelopeRisesThenFallsToItsFloor) {
  // Nothing before the event, a linear rise to exactly 1 at the crest,
  // and an exponential fall towards the resting floor.
  EXPECT_FLOAT_EQ(flash(-100ms, 40ms, 340ms), 0.0f);
  EXPECT_FLOAT_EQ(flash(0ms, 40ms, 340ms), 0.0f);
  EXPECT_FLOAT_EQ(flash(20ms, 40ms, 340ms), 0.5f);
  EXPECT_FLOAT_EQ(flash(40ms, 40ms, 340ms), 1.0f);
  EXPECT_LT(flash(400ms, 40ms, 340ms), 1.0f);
  EXPECT_GT(flash(400ms, 40ms, 340ms), 0.0f);
  // The floor is where the fall is headed, and the crest is 1 whatever it
  // is — so a flare and a lamp can be mixed without rescaling.
  EXPECT_FLOAT_EQ(flash(40ms, 40ms, 340ms, 0.3f), 1.0f);
  EXPECT_GT(flash(4s, 40ms, 340ms, 0.3f), 0.29f);
  EXPECT_LT(flash(4s, 40ms, 340ms, 0.3f), 0.31f);
  EXPECT_LT(flash(4s, 40ms, 340ms), 0.001f);
  // Its fall alone IS decay, which is what makes the two one vocabulary.
  EXPECT_FLOAT_EQ(flash(500ms, 0ms, 340ms), decay(500ms, 340ms));
  // The degenerate settings answer rather than dividing by zero.
  EXPECT_FLOAT_EQ(flash(0ms, 0ms, 340ms), 1.0f);
  EXPECT_FLOAT_EQ(flash(1s, 0ms, 0ms), 1.0f);
  EXPECT_FLOAT_EQ(decay(1s, 0ms), 0.0f);
  EXPECT_FLOAT_EQ(decay(0ms, 340ms), 1.0f);
}

TEST(Values, QuantizeTimeIsTheCanonicalFloorArithmetic) {
  // motion::quantizeTime against the hand-written floor(t*N)/N it stands
  // in for, bit-exact in BOTH precisions — the template keeps each call
  // site's own type rather than promoting to double.
  for (double seconds : {0.0, 0.081, 1.0 / 6.0, 2.499999, 13.37, 1000.05}) {
    EXPECT_EQ(quantizeTime(seconds, 6.0), std::floor(seconds * 6.0) / 6.0);
    EXPECT_EQ(quantizeTime(seconds, 8.0), std::floor(seconds * 8.0) / 8.0);
    const float narrow = (float)seconds;
    EXPECT_EQ(quantizeTime(narrow, 8.0f), std::floor(narrow * 8.0f) / 8.0f);
    // The Duration form is the same number, in seconds.
    EXPECT_EQ(quantizeTime(Duration(seconds), 6.0).count(),
              std::floor(seconds * 6.0) / 6.0);
  }
  // rate <= 0 answers the input unchanged: the spelling of "continuous".
  EXPECT_EQ(quantizeTime(1.234, 0.0), 1.234);
  EXPECT_EQ(quantizeTime(1.234, -5.0), 1.234);
  // …and the value HOLDS between steps, which is the whole point.
  EXPECT_EQ(quantizeTime(0.10, 6.0), quantizeTime(0.16, 6.0));
  EXPECT_NE(quantizeTime(0.16, 6.0), quantizeTime(0.17, 6.0));
}

TEST(Values, TheStepIndexAndThePhaseReadADuration) {
  // The step a posterised clock is on is the count, not the seconds.
  EXPECT_EQ(stepIndex(1s, 8.0), 8);
  EXPECT_EQ(stepIndex(1100ms, 8.0), 8);
  EXPECT_EQ(stepIndex(1130ms, 8.0), 9);
  EXPECT_EQ(stepIndex(5s, 0.0), 0);  // continuous is on no step

  // A phase wraps into [0, 1), forwards for a negative time, and a
  // period of nothing is 0 rather than a NaN.
  EXPECT_NEAR(phase(2500ms, 1s), 0.5f, 1e-6f);
  EXPECT_NEAR(phase(-250ms, 1s), 0.75f, 1e-6f);
  EXPECT_FLOAT_EQ(phase(3s, 0s), 0.0f);
}
