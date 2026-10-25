/** @file
 * The pipelines a device keeps on disk between runs: a kept set is read
 * back only whole, and a device made over a set that does not hold comes
 * up on an empty cache and keeps a framed one when it goes.
 */

#include <gtest/gtest.h>
#include <sigilgeometry/device/Device.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#include "KeptPipelines.h"
#include "ScratchDir.h"

using namespace sigil;

/** THE KEPT PIPELINES ARE HANDED TO THE DRIVER ONLY AS A DRIVER WROTE
 *  THEM. The driver rebuilds every shader library a kept set holds while
 *  the device is made and trusts what it rebuilds, so a set torn by two
 *  processes closing at once, cut short, or written by anything else is
 *  refused whole, and two writers closing at once leave one of their
 *  sets whole rather than a mixture of both. No device is needed: these
 *  are bytes on a disk. */
TEST(Device, AKeptPipelineSetIsReadBackOnlyWhole) {
  const sigil::test::ScratchDir store("sigil_device_kept_pipelines");
  const std::filesystem::path file = store.path / "device pipelines.bin";
  const auto bytes = [](size_t size, unsigned seed) {
    std::vector<char> made(size);
    for (size_t at = 0; at < size; ++at)
      made[at] = (char)((at * 131u + seed * 7919u + 7u) & 0xffu);
    return made;
  };
  const auto readFile = [&] {
    std::ifstream in(file, std::ios::binary);
    return std::vector<char>(std::istreambuf_iterator<char>(in),
                             std::istreambuf_iterator<char>());
  };
  const auto writeFile = [&](const std::vector<char>& written) {
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out.write(written.data(), (std::streamsize)written.size());
  };

  const std::vector<char> set = bytes(1 << 16, 1);
  ASSERT_TRUE(geometry::device::writeKeptPipelines(file, set));
  EXPECT_EQ(geometry::device::readKeptPipelines(file), set);
  for (const auto& entry : std::filesystem::directory_iterator(store.path))
    EXPECT_EQ(entry.path(), file) << "a partial write was left behind";

  // Bytes no run framed are refused.
  writeFile(bytes(4096, 2));
  EXPECT_TRUE(geometry::device::readKeptPipelines(file).empty());

  // A whole set torn: its first half, then what another writer left at
  // the same offsets.
  ASSERT_TRUE(geometry::device::writeKeptPipelines(file, set));
  std::vector<char> torn = readFile();
  const std::vector<char> other = bytes(torn.size(), 3);
  std::copy(other.begin() + (long)torn.size() / 2, other.end(),
            torn.begin() + (long)torn.size() / 2);
  writeFile(torn);
  EXPECT_TRUE(geometry::device::readKeptPipelines(file).empty());

  // Cut short.
  ASSERT_TRUE(geometry::device::writeKeptPipelines(file, set));
  std::vector<char> cut = readFile();
  cut.resize(cut.size() - 100);
  writeFile(cut);
  EXPECT_TRUE(geometry::device::readKeptPipelines(file).empty());

  // Two writers at once, many times over: what stands is always one of
  // the two sets, whole.
  const std::vector<char> second = bytes(1 << 17, 4);
  for (int round = 0; round < 8; ++round) {
    std::thread one(
        [&] { (void)geometry::device::writeKeptPipelines(file, set); });
    std::thread two(
        [&] { (void)geometry::device::writeKeptPipelines(file, second); });
    one.join();
    two.join();
    const std::vector<char> standing =
        geometry::device::readKeptPipelines(file);
    EXPECT_TRUE(standing == set || standing == second)
        << "round " << round << " left " << standing.size() << " bytes";
  }
}

/** …and a device made over a store holding a set that does not hold comes
 *  up on an empty cache, and keeps a framed one when it goes. A process
 *  makes one device, so this one is made only where no other case in the
 *  process made one first. */
TEST(Device, ADeviceOverATornPipelineSetComesUpOnAnEmptyCache) {
  const sigil::test::ScratchDir store("sigil_device_pipeline_store");
  const std::filesystem::path file = store.path / "device pipelines.bin";
  {
    std::vector<char> foreign(4096);
    for (size_t at = 0; at < foreign.size(); ++at)
      foreign[at] = (char)((at * 131u + 7u) & 0xffu);
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out.write(foreign.data(), (std::streamsize)foreign.size());
  }
  geometry::device::DeviceConfig config;
  config.pipelineStore = store.path;
  {
    const auto device = geometry::device::Device::create(config);
    if (!device)
      GTEST_SKIP() << "no device of its own here: " << device.error().message;
    EXPECT_NE((*device)->pipelineStateCache(), nullptr);
  }
  EXPECT_FALSE(geometry::device::readKeptPipelines(file).empty())
      << "the device kept no framed set when it went";
}
