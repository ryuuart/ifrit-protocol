#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkRect.h>
#include <sigilvideo/decode/Decode.h>
#include <sigilvideo/decode/Playback.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <vector>

namespace {

/** The bytes of the committed clip @p name: this library opens no paths
 *  of its own, so the test does the reading. */
std::vector<std::byte> assetBytes(const char* name) {
  std::ifstream input(std::filesystem::path(SIGIL_TEST_ASSET_DIR) / name,
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
  const std::vector<std::byte> bytes = assetBytes("bear-vp8a.webm");
  if (bytes.empty()) return nullptr;
  sigil::video::DecodeOptions options;
  options.hardware = sigil::video::HardwarePreference::Disabled;
  options.cachedFrames = cachedFrames;
  return sigil::video::decodeVideo(bytes.data(), bytes.size(), options,
                                   "bear-vp8a.webm");
}

/** @p video painted over one small raster surface at @p seconds. Null when
 *  the draw refused. */
SkBitmap painted(sigil::video::Video& video, double seconds, bool loop) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(64, 48));
  bitmap.eraseColor(SK_ColorTRANSPARENT);
  SkCanvas canvas(bitmap);
  if (!video.draw(canvas, SkRect::MakeIWH(bitmap.width(), bitmap.height()),
                  seconds, SkSamplingOptions(SkFilterMode::kLinear), loop))
    bitmap.reset();
  return bitmap;
}

bool samePixels(const SkBitmap& one, const SkBitmap& other) {
  if (one.isNull() || one.dimensions() != other.dimensions()) return false;
  for (int y = 0; y < one.height(); ++y)
    if (std::memcmp(one.getAddr32(0, y), other.getAddr32(0, y),
                    static_cast<size_t>(one.width()) * 4) != 0)
      return false;
  return true;
}

}  // namespace

// What is handed in when the caller does not have a video: nothing at
// all, and something that is not one.
struct NotAVideo {
  const std::byte* bytes;
  size_t size;
  const char* label;
};

class RefusedInput : public ::testing::TestWithParam<NotAVideo> {};

TEST_P(RefusedInput, DecodesToNothingAndProbesToNothing) {
  EXPECT_EQ(sigil::video::decodeVideo(GetParam().bytes, GetParam().size),
            nullptr);
  EXPECT_FALSE(sigil::video::probeVideo(GetParam().bytes, GetParam().size));
}

constexpr std::array<std::byte, 8> kGarbage = {
    std::byte{'n'}, std::byte{'o'}, std::byte{'t'}, std::byte{'v'},
    std::byte{'i'}, std::byte{'d'}, std::byte{'e'}, std::byte{'o'},
};

INSTANTIATE_TEST_SUITE_P(
    VideoDecode, RefusedInput,
    ::testing::Values(NotAVideo{nullptr, 0, "NoBytesAtAll"},
                      NotAVideo{kGarbage.data(), kGarbage.size(),
                                "BytesOfSomethingElse"}),
    [](const ::testing::TestParamInfo<NotAVideo>& info) {
      return std::string(info.param.label);
    });

TEST(VideoDecode, PreservesAndPremultipliesWebMAlpha) {
  const std::vector<std::byte> bytes = assetBytes("bear-vp8a.webm");
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

TEST(VideoDecode, DrawOutsideTheDurationClampsOrWraps) {
  std::shared_ptr<sigil::video::Video> video = bearClip();
  ASSERT_NE(video, nullptr);
  const double duration = video->probe().durationSeconds;
  ASSERT_GT(duration, 0.0);

  const SkBitmap first = painted(*video, 0.0, false);
  const SkBitmap last = painted(*video, std::nextafter(duration, 0.0), false);
  ASSERT_FALSE(first.isNull());
  ASSERT_FALSE(last.isNull());
  ASSERT_FALSE(samePixels(first, last)) << "the clip has to move to be read";

  // A non-looping draw holds the ends: past the duration it stays on the
  // last frame the duration covers, before zero on the first.
  EXPECT_TRUE(samePixels(painted(*video, duration * 20.0, false), last));
  EXPECT_TRUE(samePixels(painted(*video, -duration, false), first));

  // A looping draw is the same clip over again, forwards and backwards.
  const SkBitmap quarter = painted(*video, duration * 0.25, false);
  ASSERT_FALSE(quarter.isNull());
  EXPECT_TRUE(samePixels(
      painted(*video, duration * 20.0 + duration * 0.25, true), quarter));
  EXPECT_TRUE(samePixels(painted(*video, duration * -1.75, true), quarter));
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

TEST(VideoDecode, RequiredMeansDeviceFramesOrNoFramesAtAll) {
  // The promise has two arms and every build takes one of them: where no
  // hardware decoder opened for this clip, Required yields nothing rather
  // than falling back to software; where one did, the frame that arrives
  // came off the device.
  const std::vector<std::byte> bytes = assetBytes("bear-vp8a.webm");
  ASSERT_FALSE(bytes.empty());
  sigil::video::DecodeOptions options;
  options.hardware = sigil::video::HardwarePreference::Required;
  std::shared_ptr<sigil::video::Video> video = sigil::video::decodeVideo(
      bytes.data(), bytes.size(), options, "bear-vp8a.webm");
  if (!video || !video->hardwareConfigured()) {
    if (video) EXPECT_FALSE(video->frameAt(0.0));
    return;
  }
  ASSERT_TRUE(video->frameAt(0.0));
  EXPECT_TRUE(video->hardwareDecoding());
}
