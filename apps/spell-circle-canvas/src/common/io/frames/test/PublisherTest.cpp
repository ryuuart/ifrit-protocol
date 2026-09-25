/** @file
 * The factory's refusals: what a caller is handed when there is nothing
 * to publish from or nothing to publish under. These cases need no
 * graphics device and never interpret the placeholder device address.
 */

#include <gtest/gtest.h>
#include <sigilio/frames/Publisher.h>
#include <sigilio/hub/Hub.h>

namespace {

using namespace sigil::io;

TEST(PublishFactory, AnUnnamedPublicationIsRefused) {
  // The address stands in for a device and is never dereferenced: a
  // publication nobody could subscribe to is refused before the device
  // is looked at, which is what this asserts.
  int marker = 0;
  Hub hub;
  EXPECT_FALSE(hub.publish("syphon://", {.device = {.handle = &marker}}));
}

TEST(PublishFactory, ASchemeNoProtocolCarriesIsRefused) {
  int marker = 0;
  Hub hub;
  EXPECT_FALSE(hub.publish("ndi://a name", {.device = {.handle = &marker}}));
  EXPECT_FALSE(hub.publish("a name", {.device = {.handle = &marker}}));
}

TEST(PublishFactory, ADeviceOfAnotherGraphicsApiIsRefused) {
  int marker = 0;
  Hub hub;
  EXPECT_FALSE(hub.publish(
      "syphon://a name",
      {.device = {.api = frames::GraphicsApi::Direct3D11, .handle = &marker}}));
}

TEST(PublishFactory, UnsupportedProtocolsAreRefusedBeforeReadingTheDevice) {
  int marker = 0;
  Hub hub;
#if defined(__APPLE__)
  EXPECT_FALSE(hub.publish("spout://unsupported", {.device = {.handle = &marker}}));
#else
  EXPECT_FALSE(hub.publish("syphon://unsupported", {.device = {.handle = &marker}}));
#endif
}

TEST(PublishFactory, AHandleOntoNothingSendsNothing) {
  int texture = 0;
  EXPECT_FALSE(frames::Publisher().send({.texture = &texture, .width = 2, .height = 2}));
}

}  // namespace
