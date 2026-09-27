/** @file
 * The streaming decode: input that is not a video, WebM alpha, seeking,
 * the frame a playback time answers under each timing, the cache's
 * capacity, the decode pool in its synchronous mode and torn down with
 * asks in flight, and what a required hardware decode means.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkRect.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmedia/video/Video.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace {

namespace media = sigil::media;
using namespace std::chrono_literals;

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

std::shared_ptr<const media::Video> bearClip(media::VideoOptions options = {
                                                 .cachedFrames = 4}) {
  const std::vector<std::byte> bytes = assetBytes("bear-vp8a.webm");
  if (bytes.empty()) return nullptr;
  options.hardware = media::HardwarePreference::Disabled;
  return media::decode<media::Video>(bytes, options, "bear-vp8a.webm");
}

/** @p video's frame at @p elapsed under @p timing, painted over one small
 *  raster surface. Null when there was no frame. */
SkBitmap painted(const media::Video& video,
                 std::chrono::duration<double> elapsed, media::Timing timing) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(64, 48));
  bitmap.eraseColor(SK_ColorTRANSPARENT);
  const media::Frame frame = video.frameAt(elapsed, timing);
  if (!frame.image) {
    bitmap.reset();
    return bitmap;
  }
  SkCanvas canvas(bitmap);
  canvas.drawImageRect(frame.image,
                       SkRect::MakeIWH(bitmap.width(), bitmap.height()),
                       SkSamplingOptions(SkFilterMode::kLinear));
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
  std::span<const std::byte> bytes;
  const char* label;
};

class RefusedInput : public ::testing::TestWithParam<NotAVideo> {};

TEST_P(RefusedInput, DecodesToNothingAndProbesToNothing) {
  EXPECT_EQ(media::decode<media::Video>(GetParam().bytes), nullptr);
  EXPECT_FALSE(probeDocument(std::type_identity<media::Video>{},
                             GetParam().bytes, {}));
}

constexpr std::array<std::byte, 8> kGarbage = {
    std::byte{'n'}, std::byte{'o'}, std::byte{'t'}, std::byte{'v'},
    std::byte{'i'}, std::byte{'d'}, std::byte{'e'}, std::byte{'o'},
};

INSTANTIATE_TEST_SUITE_P(
    VideoDecode, RefusedInput,
    ::testing::Values(NotAVideo{{}, "NoBytesAtAll"},
                      NotAVideo{kGarbage, "BytesOfSomethingElse"}),
    [](const ::testing::TestParamInfo<NotAVideo>& info) {
      return std::string(info.param.label);
    });

TEST(VideoDecode, PreservesAndPremultipliesWebMAlpha) {
  const std::vector<std::byte> bytes = assetBytes("bear-vp8a.webm");
  ASSERT_FALSE(bytes.empty());

  const std::optional<media::Metadata> probe = probeDocument(
      std::type_identity<media::Video>{}, bytes, "bear-vp8a.webm");
  ASSERT_TRUE(probe);
  EXPECT_TRUE(probe->hasAlpha);

  const auto video = bearClip();
  ASSERT_NE(video, nullptr);
  EXPECT_TRUE(video->metadata().hasAlpha);
  const media::Frame frame = video->frameAt(0s);
  ASSERT_TRUE(frame);
  ASSERT_TRUE(frame.image);
  EXPECT_EQ(toSk(frame.image)->alphaType(), kPremul_SkAlphaType);

  SkBitmap pixels;
  pixels.allocPixels(
      SkImageInfo::Make(toSk(frame.image)->width(), toSk(frame.image)->height(),
                        kRGBA_8888_SkColorType, kPremul_SkAlphaType));
  ASSERT_TRUE(toSk(frame.image)->readPixels(nullptr, pixels.pixmap(), 0, 0));
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

  ASSERT_TRUE(video->frameAt(800ms));
}

TEST(VideoDecode, SeekingBackwardReturnsTheCoveringFrame) {
  const auto video = bearClip();
  ASSERT_NE(video, nullptr);
  const media::Frame later = video->frameAt(800ms);
  ASSERT_TRUE(later);
  EXPECT_GT(later.index, 0);
  EXPECT_LE(later.time.count(), 0.8);
  EXPECT_GT((later.time + later.duration).count(), 0.8);

  const media::Frame first = video->frameAt(0s);
  ASSERT_TRUE(first);
  EXPECT_EQ(first.index, 0);
  EXPECT_EQ(first.time.count(), 0.0);
}

TEST(VideoDecode, TimingHoldsOrWrapsOutsideTheDuration) {
  const auto video = bearClip();
  ASSERT_NE(video, nullptr);
  const std::chrono::duration<double> duration = video->duration();
  ASSERT_GT(duration.count(), 0.0);
  const media::Timing once{.loop = media::Loop::Once};
  const media::Timing forever{.loop = media::Loop::Forever};

  const SkBitmap first = painted(*video, 0s, once);
  const SkBitmap last = painted(
      *video, std::chrono::duration<double>(std::nextafter(duration.count(), 0.0)),
      once);
  ASSERT_FALSE(first.isNull());
  ASSERT_FALSE(last.isNull());
  ASSERT_FALSE(samePixels(first, last)) << "the clip has to move to be read";

  // Played once, the clip holds its ends: past the duration it stays on
  // the last frame the duration covers, before zero on the first.
  EXPECT_TRUE(samePixels(painted(*video, duration * 20.0, once), last));
  EXPECT_TRUE(samePixels(painted(*video, -duration, once), first));

  // Looped, it is the same clip over again, forwards and backwards — and
  // a video left to itself loops forwards.
  const SkBitmap quarter = painted(*video, duration * 0.25, once);
  ASSERT_FALSE(quarter.isNull());
  EXPECT_TRUE(samePixels(
      painted(*video, duration * 20.0 + duration * 0.25, forever), quarter));
  EXPECT_TRUE(samePixels(painted(*video, duration * -1.75, forever), quarter));
  EXPECT_TRUE(samePixels(painted(*video, duration * 3.25, {}), quarter));
}

TEST(VideoDecode, TheCacheHoldsCachedFramesAndNoMore) {
  // Room for both: the first frame's raster is the same object on the
  // second ask.
  const auto roomy = bearClip({.cachedFrames = 4});
  ASSERT_NE(roomy, nullptr);
  const media::Frame first = roomy->frameAt(0s);
  ASSERT_TRUE(first);
  const media::Frame later = roomy->frameAt(100ms);
  ASSERT_TRUE(later);
  EXPECT_NE(later.index, first.index);
  EXPECT_EQ(roomy->frameAt(0s).image.identity(), first.image.identity());

  // Room for one: the later ask evicts the first, which decodes again
  // into a new image.
  const auto tight = bearClip({.cachedFrames = 1});
  ASSERT_NE(tight, nullptr);
  const media::Frame only = tight->frameAt(0s);
  ASSERT_TRUE(only);
  ASSERT_TRUE(tight->frameAt(100ms));
  const media::Frame again = tight->frameAt(0s);
  ASSERT_TRUE(again);
  EXPECT_EQ(again.index, only.index);
  EXPECT_NE(again.image.identity(), only.image.identity());
}

TEST(VideoDecode, CapacityZeroBehavesAsOne) {
  const auto video = bearClip({.cachedFrames = 0});
  ASSERT_NE(video, nullptr);
  const media::Frame first = video->frameAt(0s);
  ASSERT_TRUE(first);
  EXPECT_EQ(video->frameAt(0s).image.identity(), first.image.identity());
}

TEST(VideoDecode, APoolServesTheAskedFrame) {
  // No worker: each ask decodes before it returns, so the answer is the
  // frame asked for with nothing to wait on.
  const auto pool =
      std::make_shared<media::Playback>(media::Playback::Options{.workers = 0});
  const auto video = bearClip({.playback = pool});
  ASSERT_NE(video, nullptr);
  EXPECT_FALSE(video->hasFrame());
  const media::Frame first = video->frameAt(0s);
  ASSERT_TRUE(first);
  EXPECT_TRUE(video->hasFrame());
  ASSERT_NE(first.image, nullptr);
  EXPECT_EQ(first.index, 0);

  const media::Frame later = video->frameAt(800ms);
  ASSERT_TRUE(later);
  EXPECT_NE(later.index, first.index);

  // An ask inside the frame on show is coalesced away.
  EXPECT_EQ(video->frameAt(later.time).image.identity(), later.image.identity());
}

TEST(VideoDecode, APoolOutlivesAsksInFlight) {
  // Asks are queued and the pool and its video are let go with them in
  // flight or pending; the workers are joined. Nothing here waits on a
  // clock: the assertion is that this returns.
  auto pool =
      std::make_shared<media::Playback>(media::Playback::Options{.workers = 2});
  auto video = bearClip({.playback = pool});
  ASSERT_NE(video, nullptr);
  video->frameAt(0s);
  video->frameAt(500ms);
  pool.reset();
  video.reset();
}

TEST(VideoDecode, RequiredMeansDeviceFramesOrNoFramesAtAll) {
  // The promise has two arms and every build takes one of them: where no
  // hardware decoder opened for this clip, Required yields nothing rather
  // than falling back to software; where one did, the frame that arrives
  // came off the device.
  const std::vector<std::byte> bytes = assetBytes("bear-vp8a.webm");
  ASSERT_FALSE(bytes.empty());
  const auto video = media::decode<media::Video>(
      bytes, {.hardware = media::HardwarePreference::Required},
      "bear-vp8a.webm");
  if (!video || !video->hardware().configured) {
    if (video) EXPECT_FALSE(video->frameAt(0s));
    return;
  }
  ASSERT_TRUE(video->frameAt(0s));
  EXPECT_TRUE(video->hardware().decoding);
}
