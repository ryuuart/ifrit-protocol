/** @file
 * The two comparators an identity prune reads a shaped binding through:
 * a curve compared by its shape and its settings, and the whole record
 * compared field by field.
 */

#include <gtest/gtest.h>
#include <sigilmotion/bind/Bind.h>

#include <functional>

#include "support/StandsAlone.h"

using namespace sigil::motion;
namespace ch = choreograph;

namespace {

/** A record with every slot carrying something other than its default,
 *  so a comparator that skipped a field would have to skip a field that
 *  is actually set. */
BoundFloat furnished(const ch::Output<float>* source) {
  return bind(source)
      .source(0.25f, 0.75f)
      .trapezoid(0.1f, 0.3f, 0.7f, 0.9f)
      .map(&ch::easeInQuad)
      .quantize(5)
      .scale(3.0f)
      .offset(-1.5f)
      .wrap(2.0f)
      .wiggle(4.0f, 6.0f, 11u, 3, 0.4f)
      .clamp(-2.0f, 2.0f)
      .value();
}

}  // namespace

TEST(Bind, ACurveComparesByItsShapeAndItsSettings) {
  // A plain function is its pointer, a shaped curve is its shape and its
  // numbers, and a capturing lambda is unequal to everything — including
  // to itself, because a std::function holding one cannot be read back.
  EXPECT_TRUE(easeEqual(&ch::easeInQuad, &ch::easeInQuad));
  EXPECT_FALSE(easeEqual(&ch::easeInQuad, &ch::easeOutQuad));
  EXPECT_TRUE(easeEqual(ease::outBack(1.7f), ease::outBack(1.7f)));
  EXPECT_FALSE(easeEqual(ease::outBack(1.7f), ease::outBack(2.4f)));
  const float k = 2.0f;
  const ch::EaseFn captured = [k](float t) { return t * k; };
  EXPECT_FALSE(easeEqual(captured, captured));
  // Two empty slots are the same slot: a binding that shapes nothing
  // must not re-patch against another that shapes nothing.
  EXPECT_TRUE(easeEqual({}, {}));
}

TEST(Bind, ABoundMapComparesEveryFieldItHolds) {
  // The map is read LIVE, so a record that pruned on a field it does not
  // compare would keep shaping through the old value for as long as the
  // node lives. Every field is named here, one claim each.
  ch::Output<float> phase = 0.0f, other = 0.0f;
  EXPECT_TRUE(boundMapEqual(furnished(&phase), furnished(&phase)));

  const auto differs = [&](const std::function<void(BoundFloat&)>& change) {
    BoundFloat changed = furnished(&phase);
    change(changed);
    return !boundMapEqual(furnished(&phase), changed);
  };
  EXPECT_TRUE(differs([&](BoundFloat& b) { b.source = &other; })) << "source";
  EXPECT_TRUE(differs([](BoundFloat& b) { b.inScale += 1.0f; })) << "inScale";
  EXPECT_TRUE(differs([](BoundFloat& b) { b.inOffset += 1.0f; })) << "inOffset";
  EXPECT_TRUE(differs([](BoundFloat& b) { b.curve = &ch::easeOutQuad; }))
      << "curve";
  EXPECT_TRUE(differs([](BoundFloat& b) { b.clampInput = !b.clampInput; }))
      << "clampInput";
  EXPECT_TRUE(differs([](BoundFloat& b) { b.envelope = Envelope::kCosine; }))
      << "envelope";
  EXPECT_TRUE(differs([](BoundFloat& b) { b.riseStart += 0.01f; }))
      << "riseStart";
  EXPECT_TRUE(differs([](BoundFloat& b) { b.holdStart += 0.01f; }))
      << "holdStart";
  EXPECT_TRUE(differs([](BoundFloat& b) { b.holdEnd += 0.01f; })) << "holdEnd";
  EXPECT_TRUE(differs([](BoundFloat& b) { b.fallEnd += 0.01f; })) << "fallEnd";
  EXPECT_TRUE(differs([](BoundFloat& b) { b.duty += 0.01f; })) << "duty";
  EXPECT_TRUE(differs([](BoundFloat& b) { b.waveFn = &ch::easeInQuad; }))
      << "waveFn";
  EXPECT_TRUE(differs([](BoundFloat& b) { b.steps += 1; })) << "steps";
  EXPECT_TRUE(differs([](BoundFloat& b) { b.scale += 1.0f; })) << "scale";
  EXPECT_TRUE(differs([](BoundFloat& b) { b.offset += 1.0f; })) << "offset";
  EXPECT_TRUE(differs([](BoundFloat& b) { b.clamped = !b.clamped; }))
      << "clamped";
  EXPECT_TRUE(differs([](BoundFloat& b) { b.lo -= 1.0f; })) << "lo";
  EXPECT_TRUE(differs([](BoundFloat& b) { b.hi += 1.0f; })) << "hi";
  EXPECT_TRUE(differs([](BoundFloat& b) { b.wiggleAmount += 1.0f; }))
      << "wiggleAmount";
  EXPECT_TRUE(differs([](BoundFloat& b) { b.wiggleFrequency += 1.0f; }))
      << "wiggleFrequency";
  EXPECT_TRUE(differs([](BoundFloat& b) { b.wiggleSeed += 1u; }))
      << "wiggleSeed";
  EXPECT_TRUE(differs([](BoundFloat& b) { b.wiggleOctaves += 1; }))
      << "wiggleOctaves";
  EXPECT_TRUE(differs([](BoundFloat& b) { b.wiggleFalloff += 0.1f; }))
      << "wiggleFalloff";
  EXPECT_TRUE(differs([](BoundFloat& b) { b.wrapPeriod += 1.0f; }))
      << "wrapPeriod";
}
