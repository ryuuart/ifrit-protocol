#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <sigilvideo/decode/Decode.h>
#include <sigilvideo/decode/Playback.h>

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <vector>

namespace {

std::vector<std::byte> readAsset(const char* name) {
  std::ifstream input(std::filesystem::path(SIGILVIDEO_TEST_ASSET_DIR) / name,
                      std::ios::binary | std::ios::ate);
  if (!input) return {};
  const std::streamsize size = input.tellg();
  if (size <= 0) return {};
  input.seekg(0);
  std::vector<std::byte> bytes(static_cast<size_t>(size));
  if (!input.read(reinterpret_cast<char*>(bytes.data()), size)) return {};
  return bytes;
}

std::shared_ptr<sigil::video::Video> bearClip(size_t cachedFrames = 4) {
  const std::vector<std::byte> bytes = readAsset("bear-vp8a.webm");
  if (bytes.empty()) return nullptr;
  sigil::video::DecodeOptions options;
  options.hardware = sigil::video::HardwarePreference::Disabled;
  options.cachedFrames = cachedFrames;
  return sigil::video::decodeVideo(bytes.data(), bytes.size(), options,
                                   "bear-vp8a.webm");
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

TEST(VideoDecode, SeekingBackwardReturnsTheCoveringFrame) {
  std::shared_ptr<sigil::video::Video> video = bearClip();
  ASSERT_NE(video, nullptr);
  const sigil::video::VideoFrame later = video->frameAt(0.8);
  ASSERT_TRUE(later);
  EXPECT_GT(later.index, 0);
  EXPECT_LE(later.presentationSeconds, 0.8);
  EXPECT_GT(later.presentationSeconds + later.durationSeconds, 0.8);

  const sigil::video::VideoFrame first = video->frameAt(0.0);
  ASSERT_TRUE(first);
  EXPECT_EQ(first.index, 0);
  EXPECT_EQ(first.presentationSeconds, 0.0);
}

TEST(VideoDecode, TheCacheHoldsCachedFramesAndNoMore) {
  // Room for both: the first frame's raster is the same object on the
  // second ask.
  std::shared_ptr<sigil::video::Video> roomy = bearClip(4);
  ASSERT_NE(roomy, nullptr);
  const sigil::video::VideoFrame first = roomy->frameAt(0.0);
  ASSERT_TRUE(first);
  const sigil::video::VideoFrame later = roomy->frameAt(0.1);
  ASSERT_TRUE(later);
  EXPECT_NE(later.index, first.index);
  EXPECT_EQ(roomy->frameAt(0.0).image.get(), first.image.get());

  // Room for one: the later ask evicts the first, which decodes again
  // into a new image.
  std::shared_ptr<sigil::video::Video> tight = bearClip(1);
  ASSERT_NE(tight, nullptr);
  const sigil::video::VideoFrame only = tight->frameAt(0.0);
  ASSERT_TRUE(only);
  ASSERT_TRUE(tight->frameAt(0.1));
  const sigil::video::VideoFrame again = tight->frameAt(0.0);
  ASSERT_TRUE(again);
  EXPECT_EQ(again.index, only.index);
  EXPECT_NE(again.image.get(), only.image.get());
}

TEST(VideoDecode, CapacityZeroBehavesAsOne) {
  std::shared_ptr<sigil::video::Video> video = bearClip(0);
  ASSERT_NE(video, nullptr);
  const sigil::video::VideoFrame first = video->frameAt(0.0);
  ASSERT_TRUE(first);
  EXPECT_EQ(video->frameAt(0.0).image.get(), first.image.get());
}

TEST(VideoDecode, PlaybackServesTheRequestedFrame) {
  std::shared_ptr<sigil::video::Video> video = bearClip();
  ASSERT_NE(video, nullptr);

  // No worker: each request decodes before it returns, so the answer is
  // readable from the next frame() with nothing to wait for.
  sigil::video::Playback playback({.workerThreads = 0});
  const sigil::video::Playback::Handle handle = playback.add(video);
  EXPECT_EQ(playback.add(video), handle);  // a clip registers once
  EXPECT_EQ(playback.size(), 1u);
  EXPECT_FALSE(playback.ready(handle));
  playback.request(handle, 0.0);
  const sigil::video::VideoFrame first = playback.frame(handle, nullptr);
  ASSERT_TRUE(first);
  EXPECT_TRUE(playback.ready(handle));
  ASSERT_NE(first.image, nullptr);
  EXPECT_EQ(first.index, 0);

  playback.request(handle, 0.8);
  const sigil::video::VideoFrame later = playback.frame(handle, nullptr);
  ASSERT_TRUE(later);
  EXPECT_NE(later.index, first.index);

  // A request inside the frame on show is coalesced away.
  playback.request(handle, later.presentationSeconds);
  EXPECT_EQ(playback.frame(handle, nullptr).image.get(), later.image.get());
}

TEST(VideoDecode, PlaybackWorkersOutliveRequestsInFlight) {
  std::shared_ptr<sigil::video::Video> video = bearClip();
  ASSERT_NE(video, nullptr);
  // Requests are queued and the pool is torn down with them in flight or
  // pending; the destructor joins its workers. Nothing here waits on a
  // clock: the assertion is that this returns.
  sigil::video::Playback playback({.workerThreads = 2});
  const sigil::video::Playback::Handle handle = playback.add(video);
  playback.request(handle, 0.0);
  playback.request(handle, 0.5);
  EXPECT_EQ(playback.size(), 1u);
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
