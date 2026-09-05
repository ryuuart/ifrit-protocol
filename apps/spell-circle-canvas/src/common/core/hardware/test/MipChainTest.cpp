// How deep a mip chain a size can carry. The rule has no backend in it —
// it is halving until nothing is left to halve — so it is asked without
// one, and the backends are held to it where a device exists.

#include <sigilcore/hardware/GpuDevice.h>

#include <gtest/gtest.h>

using namespace sigil::core::hardware;

TEST(MipChain, IsAsDeepAsTheSizeAllows) {
  EXPECT_EQ(mipLevelsFor(1, 1), 1);
  EXPECT_EQ(mipLevelsFor(8, 8), 4);
  EXPECT_EQ(mipLevelsFor(256, 128), 9);
  // A chain runs until BOTH sides are one, so a wide panorama keeps
  // levels after its height has bottomed out.
  EXPECT_EQ(mipLevelsFor(1024, 2), 11);
}
