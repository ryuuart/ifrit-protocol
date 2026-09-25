/** @file
 * The two comparators an identity prune reads a binding through: a curve
 * compared by its shape and its settings, and the whole binding compared
 * field by field.
 */

#include "support/StandsAlone.h"

#include <gtest/gtest.h>
#include <sigilmotion/bind/Binding.h>
#include <sigilmotion/ease/Ease.h>

#include <functional>

using namespace sigil::motion;

namespace {

/** A binding with every field carrying something other than its default,
 *  so a comparator that skipped a field would have to skip a field that
 *  is actually set. */
Binding furnished() {
  return Binding{
      .from = {0.25f, 0.75f},
      .clampFrom = true,
      .alternate = true,
      .envelope = envelope::trapezoid(0.1f, 0.3f, 0.7f, 0.9f),
      .ease = ease::inQuad,
      .quantize = 5,
      .reverse = true,
      .to = {-1.5f, 1.5f},
      .wrap = 2.0f,
      .wiggle = {.amount = 4.0f,
                 .frequency = 6.0f,
                 .seed = 11u,
                 .octaves = 3,
                 .falloff = 0.4f},
      .clamp = {-2.0f, 2.0f}};
}

}  // namespace

TEST(BindingEquality, ACurveComparesByItsShapeAndItsSettings) {
  // A plain function is its pointer, a shaped curve is its shape and its
  // numbers, and a capturing lambda is unequal to everything — including
  // to itself, because a std::function holding one cannot be read back.
  EXPECT_TRUE(easeEqual(ease::inQuad, ease::inQuad));
  EXPECT_FALSE(easeEqual(ease::inQuad, ease::outQuad));
  EXPECT_TRUE(easeEqual(ease::outBack(1.7f), ease::outBack(1.7f)));
  EXPECT_FALSE(easeEqual(ease::outBack(1.7f), ease::outBack(2.4f)));
  const float factor = 2.0f;
  const Easing captured = [factor](float progress) { return progress * factor; };
  EXPECT_FALSE(easeEqual(captured, captured));
  // Two empty slots are the same slot: a binding that shapes nothing
  // must not re-patch against another that shapes nothing.
  EXPECT_TRUE(easeEqual({}, {}));
}

TEST(BindingEquality, TwoBindingsThatShapeNothingAreEqual) {
  EXPECT_TRUE(Binding{} == Binding{});
}

TEST(BindingEquality, ABindingComparesEveryFieldItHolds) {
  // The stages are read LIVE, so a binding that pruned on a field it does
  // not compare would keep shaping through the old value for as long as
  // the node lives. Every field is named here, one claim each.
  EXPECT_TRUE(furnished() == furnished());

  const auto differs = [](const std::function<void(Binding&)>& change) {
    Binding changed = furnished();
    change(changed);
    return !(furnished() == changed);
  };
  EXPECT_TRUE(differs([](Binding& binding) { binding.from.low += 0.1f; }))
      << "from.low";
  EXPECT_TRUE(differs([](Binding& binding) { binding.from.high += 0.1f; }))
      << "from.high";
  EXPECT_TRUE(differs([](Binding& binding) { binding.clampFrom = false; }))
      << "clampFrom";
  EXPECT_TRUE(differs([](Binding& binding) { binding.alternate = false; }))
      << "alternate";
  EXPECT_TRUE(differs([](Binding& binding) {
    binding.envelope.shape = Envelope::Shape::Cosine;
  })) << "envelope.shape";
  EXPECT_TRUE(
      differs([](Binding& binding) { binding.envelope.riseStart += 0.01f; }))
      << "envelope.riseStart";
  EXPECT_TRUE(
      differs([](Binding& binding) { binding.envelope.holdStart += 0.01f; }))
      << "envelope.holdStart";
  EXPECT_TRUE(
      differs([](Binding& binding) { binding.envelope.holdEnd += 0.01f; }))
      << "envelope.holdEnd";
  EXPECT_TRUE(
      differs([](Binding& binding) { binding.envelope.fallEnd += 0.01f; }))
      << "envelope.fallEnd";
  EXPECT_TRUE(differs([](Binding& binding) { binding.envelope.duty += 0.01f; }))
      << "envelope.duty";
  EXPECT_TRUE(
      differs([](Binding& binding) { binding.envelope.curve = ease::inQuad; }))
      << "envelope.curve";
  EXPECT_TRUE(differs([](Binding& binding) { binding.ease = ease::outQuad; }))
      << "ease";
  EXPECT_TRUE(differs([](Binding& binding) { binding.quantize += 1; }))
      << "quantize";
  EXPECT_TRUE(differs([](Binding& binding) { binding.reverse = false; }))
      << "reverse";
  EXPECT_TRUE(differs([](Binding& binding) { binding.to.low -= 1.0f; }))
      << "to.low";
  EXPECT_TRUE(differs([](Binding& binding) { binding.to.high += 1.0f; }))
      << "to.high";
  EXPECT_TRUE(differs([](Binding& binding) { binding.wrap += 1.0f; }))
      << "wrap";
  EXPECT_TRUE(differs([](Binding& binding) { binding.wiggle.amount += 1.0f; }))
      << "wiggle.amount";
  EXPECT_TRUE(
      differs([](Binding& binding) { binding.wiggle.frequency += 1.0f; }))
      << "wiggle.frequency";
  EXPECT_TRUE(differs([](Binding& binding) { binding.wiggle.seed += 1u; }))
      << "wiggle.seed";
  EXPECT_TRUE(differs([](Binding& binding) { binding.wiggle.octaves += 1; }))
      << "wiggle.octaves";
  EXPECT_TRUE(differs([](Binding& binding) { binding.wiggle.falloff += 0.1f; }))
      << "wiggle.falloff";
  EXPECT_TRUE(differs([](Binding& binding) { binding.clamp.low -= 1.0f; }))
      << "clamp.low";
  EXPECT_TRUE(differs([](Binding& binding) { binding.clamp.high += 1.0f; }))
      << "clamp.high";
}

TEST(BindingEquality, BothCurvesCompareUnderTheCurveRule) {
  // A curve that cannot be read back — a capturing lambda — makes the
  // binding holding it unequal even to a copy of itself, in either slot,
  // so the binding re-patches every describe rather than pruning onto a
  // stale curve.
  const float factor = 2.0f;
  const Easing captured = [factor](float progress) { return progress * factor; };
  const Binding eased{.ease = captured};
  EXPECT_FALSE(eased == eased);
  const Binding shaped{.envelope = envelope::shaped(captured)};
  EXPECT_FALSE(shaped == shaped);

  // A shaped curve at the same settings is the same curve.
  EXPECT_TRUE((Binding{.ease = ease::outBack(1.7f)} ==
               Binding{.ease = ease::outBack(1.7f)}));
  EXPECT_FALSE((Binding{.ease = ease::outBack(1.7f)} ==
                Binding{.ease = ease::outBack(2.4f)}));
}
