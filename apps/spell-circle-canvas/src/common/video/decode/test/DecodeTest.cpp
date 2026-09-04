#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <sigilvideo/decode/Decode.h>
#include <sigilvideo/decode/Playback.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

namespace {

std::vector<std::byte> readAsset(const char* name) {
  std::ifstream input(std::filesystem::path(IFRIT_VIDEO_TEST_ASSET_DIR) / name,
                      std::ios::binary | std::ios::ate);
  if (!input) return {};
  const std::streamsize size = input.tellg();
  if (size <= 0) return {};
  input.seekg(0);
  std::vector<std::byte> bytes(static_cast<size_t>(size));
  if (!input.read(reinterpret_cast<char*>(bytes.data()), size)) return {};
  return bytes;
}

}  // namespace

TEST(VideoDecode, RejectsMalformedBytes) {
  constexpr std::array<std::byte, 8> bytes = {
      std::byte{'n'}, std::byte{'o'}, std::byte{'t'}, std::byte{'v'},
      std::byte{'i'}, std::byte{'d'}, std::byte{'e'}, std::byte{'o'},
  };
  EXPECT_EQ(sigil::video::decodeVideo(bytes.data(), bytes.size()), nullptr);
  EXPECT_FALSE(sigil::video::probeVideo(bytes.data(), bytes.size()));
}

TEST(VideoDecode, EmptyInputIsNotAVideo) {
  EXPECT_EQ(sigil::video::decodeVideo(nullptr, 0), nullptr);
  EXPECT_FALSE(sigil::video::probeVideo(nullptr, 0));
}

TEST(VideoDecode, PreservesAndPremultipliesWebMAlpha) {
  const std::vector<std::byte> bytes = readAsset("bear-vp8a.webm");
  ASSERT_FALSE(bytes.empty());

  const std::optional<sigil::video::VideoProbe> probe =
      sigil::video::probeVideo(bytes.data(), bytes.size(), "bear-vp8a.webm");
  ASSERT_TRUE(probe);
  EXPECT_TRUE(probe->hasAlpha);

  sigil::video::DecodeOptions options;
  options.hardware = sigil::video::HardwarePreference::Disabled;
  std::shared_ptr<sigil::video::Video> video = sigil::video::decodeVideo(
      bytes.data(), bytes.size(), options, "bear-vp8a.webm");
  ASSERT_NE(video, nullptr);
  const sigil::video::VideoFrame frame = video->frameAt(0.0);
  ASSERT_TRUE(frame);
  EXPECT_TRUE(frame.hasAlpha);
  EXPECT_EQ(frame.image->alphaType(), kPremul_SkAlphaType);

  SkBitmap pixels;
  pixels.allocPixels(
      SkImageInfo::Make(frame.image->width(), frame.image->height(),
                        kRGBA_8888_SkColorType, kPremul_SkAlphaType));
  ASSERT_TRUE(frame.image->readPixels(nullptr, pixels.pixmap(), 0, 0));
  bool foundTranslucent = false;
  for (int y = 0; y < pixels.height(); ++y) {
    for (int x = 0; x < pixels.width(); ++x) {
      const auto* rgba = static_cast<const uint8_t*>(pixels.getAddr(x, y));
      const unsigned alpha = rgba[3];
      foundTranslucent |= alpha > 0 && alpha < 255;
      EXPECT_LE(rgba[0], alpha);
      EXPECT_LE(rgba[1], alpha);
      EXPECT_LE(rgba[2], alpha);
    }
  }
  EXPECT_TRUE(foundTranslucent);

  const sigil::video::VideoFrame later = video->frameAt(0.8);
  ASSERT_TRUE(later);
  EXPECT_TRUE(later.hasAlpha);

  options.hardware = sigil::video::HardwarePreference::Required;
  EXPECT_EQ(sigil::video::decodeVideo(bytes.data(), bytes.size(), options,
                                      "bear-vp8a.webm"),
            nullptr);
}

TEST(VideoDecode, PlaybackKeepsDecodeOffTheRenderThread) {
  const std::vector<std::byte> bytes = readAsset("bear-vp8a.webm");
  ASSERT_FALSE(bytes.empty());
  sigil::video::DecodeOptions options;
  options.hardware = sigil::video::HardwarePreference::Disabled;
  std::shared_ptr<sigil::video::Video> video = sigil::video::decodeVideo(
      bytes.data(), bytes.size(), options, "bear-vp8a.webm");
  ASSERT_NE(video, nullptr);

  sigil::video::Playback playback({.workerThreads = 2});
  const sigil::video::Playback::Handle handle = playback.add(video);
  EXPECT_FALSE(playback.ready(handle));
  playback.request(handle, 0.0);
  sigil::video::VideoFrame first;
  const auto firstDeadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (!first && std::chrono::steady_clock::now() < firstDeadline) {
    first = playback.frame(handle, nullptr);
    std::this_thread::yield();
  }
  ASSERT_TRUE(first);
  EXPECT_TRUE(playback.ready(handle));
  ASSERT_NE(first.image, nullptr);

  playback.request(handle, 0.8);
  sigil::video::VideoFrame later = first;
  const auto laterDeadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (later.index == first.index &&
         std::chrono::steady_clock::now() < laterDeadline) {
    later = playback.frame(handle, nullptr);
    std::this_thread::yield();
  }
  EXPECT_NE(later.index, first.index);
}

#if !defined(__APPLE__)
TEST(VideoDecode, RequiredDeviceFailsWhereNoExecutorExists) {
  constexpr std::array<std::byte, 4> bytes = {};
  const sigil::video::DecodeOptions options{
      .hardware = sigil::video::HardwarePreference::Required};
  EXPECT_EQ(sigil::video::decodeVideo(bytes.data(), bytes.size(), options),
            nullptr);
}
#endif
