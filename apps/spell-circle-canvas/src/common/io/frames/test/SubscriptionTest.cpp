/** @file
 * The factory's refusals: what a caller is handed when there is nothing
 * to receive on or nothing to receive under. The default-device query
 * also has one stable answer across repeated calls.
 */

#include <gtest/gtest.h>
#include <sigilio/frames/Publisher.h>
#include <sigilio/frames/Subscription.h>
#include <sigilio/hub/Hub.h>

namespace {

using namespace sigil::io;

TEST(PublishSubscription, AnUnnamedPublicationIsRefused) {
  // The address stands in for a device and is never dereferenced: a
  // publication nobody could have announced is refused before the device
  // is looked at, which is what this asserts.
  int marker = 0;
  Hub hub;
  EXPECT_FALSE(hub.subscribe(
      "syphon://", {.application = "an application", .device = {.handle = &marker}}));
}

TEST(PublishSubscription, ASchemeNoProtocolCarriesIsRefused) {
  int marker = 0;
  Hub hub;
  EXPECT_FALSE(hub.subscribe("ndi://a name", {.device = {.handle = &marker}}));
}

TEST(PublishSubscription, AHandleOntoNothingHasNoFrame) {
  EXPECT_FALSE(frames::Subscription().latest());
  EXPECT_EQ(frames::Subscription().state().readiness, ReadyState::Closed);
}

TEST(PublishSubscription, TheMachinesDeviceIsTheSameOneEveryTime) {
  // A caller with no device of its own subscribes on this, and two
  // subscriptions that received on two devices could not hand their
  // frames to one drawing. It is null where this build has no Metal, so
  // what is asserted is that the answer does not change, not that there
  // is one.
  EXPECT_EQ(frames::defaultMetalDevice(), frames::defaultMetalDevice());
}

}  // namespace
