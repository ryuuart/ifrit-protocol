/** @file
 * The factory's refusals: what a caller is handed when there is nothing
 * to publish from or nothing to publish under. These cases need no
 * graphics device and never interpret the placeholder device address.
 */

#include <gtest/gtest.h>
#include <sigilio/publish/Publisher.h>

namespace {

using namespace sigil::io::publish;

TEST(PublishFactory, WithNoDeviceThereIsNothingToPublishFrom) {
  EXPECT_EQ(createPublisher("a name", Backend::Metal, nullptr), nullptr);
}

TEST(PublishFactory, AnUnnamedPublicationIsRefused) {
  // The address stands in for a device and is never dereferenced: a
  // publication nobody could subscribe to is refused before the device
  // is looked at, which is what this asserts.
  int marker = 0;
  EXPECT_EQ(createPublisher("", Backend::Metal, &marker), nullptr);
}

TEST(PublishFactory, UnsupportedBackendsAreRefusedBeforeReadingTheDevice) {
  int marker = 0;
#if defined(__APPLE__)
  EXPECT_EQ(createPublisher("unsupported", Backend::Direct3D11, &marker),
            nullptr);
#else
  EXPECT_EQ(createPublisher("unsupported", Backend::Metal, &marker), nullptr);
#endif
}

}  // namespace
