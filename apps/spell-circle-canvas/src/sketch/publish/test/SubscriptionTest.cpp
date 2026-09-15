/** @file
 * The factory's refusals: what a caller is handed when there is nothing
 * to receive on or nothing to receive under. Opening onto a real
 * publication needs a device and another application publishing, neither
 * of which a test binary has.
 */

#include <gtest/gtest.h>
#include <sigilsketch/publish/Subscription.h>

namespace {

using namespace sigil::sketch;

TEST(SketchSubscription, WithNoDeviceThereIsNothingToReceiveOn) {
  EXPECT_EQ(subscribe("a name", "", nullptr), nullptr);
}

TEST(SketchSubscription, AnUnnamedPublicationIsRefused) {
  // The address stands in for a device and is never dereferenced: a
  // publication nobody could have announced is refused before the device
  // is looked at, which is what this asserts.
  int marker = 0;
  EXPECT_EQ(subscribe("", "an application", &marker), nullptr);
}

TEST(SketchSubscription, TheMachinesDeviceIsTheSameOneEveryTime) {
  // A caller with no device of its own subscribes on this, and two
  // subscriptions that received on two devices could not hand their
  // frames to one drawing. It is null where this build has no Metal, so
  // what is asserted is that the answer does not change, not that there
  // is one.
  EXPECT_EQ(defaultMetalDevice(), defaultMetalDevice());
}

}  // namespace
