/** @file
 * The factory's refusals: what a caller is handed when there is nothing
 * to publish from or nothing to publish under. These cases need no
 * graphics device and never interpret the placeholder device address.
 */

#include <gtest/gtest.h>
#include <sigilio/frames/Publisher.h>

namespace {

using namespace sigil::io::frames;

TEST(PublishFactory, WithNoDeviceThereIsNothingToPublishFrom) {
  EXPECT_FALSE(createPublisher("a name", Backend::Metal, nullptr));
}

TEST(PublishFactory, AnUnnamedPublicationIsRefused) {
  // The address stands in for a device and is never dereferenced: a
  // publication nobody could subscribe to is refused before the device
  // is looked at, which is what this asserts.
  int marker = 0;
  EXPECT_FALSE(createPublisher("", Backend::Metal, &marker));
}

TEST(PublishFactory, UnsupportedBackendsAreRefusedBeforeReadingTheDevice) {
  int marker = 0;
#if defined(__APPLE__)
  EXPECT_FALSE(createPublisher("unsupported", Backend::Direct3D11, &marker));
#else
  EXPECT_FALSE(createPublisher("unsupported", Backend::Metal, &marker));
#endif
}

}  // namespace
