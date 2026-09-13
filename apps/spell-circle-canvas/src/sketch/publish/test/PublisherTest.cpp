/** @file
 * The factory's refusals: what a caller is handed when there is nothing
 * to publish from or nothing to publish under. Standing a real server up
 * needs a device, which is the window's lane and not this binary's.
 */

#include <gtest/gtest.h>
#include <sigilsketch/publish/Publisher.h>

namespace {

using namespace sigil::sketch;

TEST(SketchPublisher, WithNoDeviceThereIsNothingToPublishFrom) {
  EXPECT_EQ(createPublisher("a name", nullptr), nullptr);
}

TEST(SketchPublisher, AnUnnamedPublicationIsRefused) {
  // The address stands in for a device and is never dereferenced: a
  // publication nobody could subscribe to is refused before the device
  // is looked at, which is what this asserts.
  int marker = 0;
  EXPECT_EQ(createPublisher("", &marker), nullptr);
}

}  // namespace
