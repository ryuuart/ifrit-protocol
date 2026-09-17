/** @file
 * The factory's refusals: what a caller is handed when there is nothing
 * to receive on or nothing to receive under. The default-device query
 * also has one stable answer across repeated calls.
 */

#include <gtest/gtest.h>
#include <sigilio/publish/Subscription.h>

namespace {

using namespace sigil::io::publish;

TEST(PublishSubscription, WithNoDeviceThereIsNothingToReceiveOn) {
  EXPECT_EQ(subscribe("a name", "", nullptr), nullptr);
}

TEST(PublishSubscription, AnUnnamedPublicationIsRefused) {
  // The address stands in for a device and is never dereferenced: a
  // publication nobody could have announced is refused before the device
  // is looked at, which is what this asserts.
  int marker = 0;
  EXPECT_EQ(subscribe("", "an application", &marker), nullptr);
}

TEST(PublishSubscription, TheMachinesDeviceIsTheSameOneEveryTime) {
  // A caller with no device of its own subscribes on this, and two
  // subscriptions that received on two devices could not hand their
  // frames to one drawing. It is null where this build has no Metal, so
  // what is asserted is that the answer does not change, not that there
  // is one.
  EXPECT_EQ(defaultMetalDevice(), defaultMetalDevice());
}

}  // namespace
