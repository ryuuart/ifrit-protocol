// A bare H.264 elementary stream is where a decoder meets frames that
// carry no presentation timestamp at all: the container that would have
// stamped them is not there. The bytes are made here from an MP4 this
// encoder writes, which is why the case stands beside the encode cases.

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>
#include <include/core/SkData.h>
#include <sigilvideo/decode/Decode.h>
#include <sigilvideo/encode/Encode.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <vector>

namespace {

constexpr int kWidth = 64;
constexpr int kHeight = 64;
constexpr int kFrames = 12;
constexpr int kFramesPerSecond = 10;

uint64_t bigEndian(const uint8_t* at, size_t width) {
  uint64_t value = 0;
  for (size_t i = 0; i < width; ++i) value = (value << 8) | at[i];
  return value;
}

/** The payload of the first top-level box of @p type, empty when the file
 *  holds none. */
std::vector<uint8_t> boxPayload(const std::vector<uint8_t>& file,
                                const char* type) {
  size_t at = 0;
  while (at + 8 <= file.size()) {
    uint64_t size = bigEndian(file.data() + at, 4);
    size_t header = 8;
    if (size == 1) {
      if (at + 16 > file.size()) break;
      size = bigEndian(file.data() + at + 8, 8);
      header = 16;
    } else if (size == 0) {
      size = file.size() - at;
    }
    if (size < header || at + size > file.size()) break;
    if (std::memcmp(file.data() + at + 4, type, 4) == 0)
      return {file.begin() + static_cast<ptrdiff_t>(at + header),
              file.begin() + static_cast<ptrdiff_t>(at + size)};
    at += static_cast<size_t>(size);
  }
  return {};
}

void appendStartCode(std::vector<std::byte>& stream) {
  static constexpr uint8_t kStartCode[] = {0, 0, 0, 1};
  for (uint8_t code : kStartCode) stream.push_back(std::byte{code});
}

void appendNal(std::vector<std::byte>& stream, const uint8_t* nal,
               size_t size) {
  appendStartCode(stream);
  for (size_t i = 0; i < size; ++i) stream.push_back(std::byte{nal[i]});
}

/** The Annex-B elementary stream of an MP4's H.264 samples: the parameter
 *  sets out of `avcC` ahead of every length-prefixed NAL in `mdat`, each
 *  behind a start code. Empty when the file is not the AVC MP4 this
 *  encoder writes. */
std::vector<std::byte> elementaryStream(const SkData& mp4) {
  const std::vector<uint8_t> file(
      static_cast<const uint8_t*>(mp4.data()),
      static_cast<const uint8_t*>(mp4.data()) + mp4.size());

  // The sample description carries `avcC` several boxes deep; the fourcc
  // occurs once, so the search does not have to descend.
  static constexpr uint8_t kAvcC[] = {'a', 'v', 'c', 'C'};
  size_t configuration = file.size();
  for (size_t at = 0; at + sizeof(kAvcC) <= file.size(); ++at)
    if (std::memcmp(file.data() + at, kAvcC, sizeof(kAvcC)) == 0) {
      configuration = at + sizeof(kAvcC);
      break;
    }
  if (configuration + 6 > file.size()) return {};

  std::vector<std::byte> stream;
  size_t at = configuration;
  const size_t lengthSize = (file[at + 4] & 0x03u) + 1u;
  const unsigned parameterSets = file[at + 5] & 0x1fu;
  at += 6;
  const auto appendParameterSets = [&](unsigned count) {
    for (unsigned i = 0; i < count; ++i) {
      if (at + 2 > file.size()) return false;
      const size_t size = static_cast<size_t>(bigEndian(file.data() + at, 2));
      at += 2;
      if (at + size > file.size()) return false;
      appendNal(stream, file.data() + at, size);
      at += size;
    }
    return true;
  };
  if (!appendParameterSets(parameterSets)) return {};
  if (at >= file.size()) return {};
  const unsigned pictureSets = file[at++];
  if (!appendParameterSets(pictureSets)) return {};

  // Every sample in the media data is a run of length-prefixed NALs, and
  // the run is unbroken across samples, so one walk covers the file.
  const std::vector<uint8_t> media = boxPayload(file, "mdat");
  size_t sample = 0;
  while (sample + lengthSize <= media.size()) {
    const size_t size =
        static_cast<size_t>(bigEndian(media.data() + sample, lengthSize));
    sample += lengthSize;
    if (size == 0 || sample + size > media.size()) return {};
    appendNal(stream, media.data() + sample, size);
    sample += size;
  }
  return stream;
}

/** The grey the frame at @p index was made with. The ramp is what tells
 *  one frame from another when nothing in the stream names them. */
int rampLevel(int index) { return 16 + index * 18; }

/** The green channel at the middle of @p image, negative when it cannot
 *  be read. A grey frame answers its own level. */
int centreLevel(const sk_sp<SkImage>& image) {
  if (!image) return -1;
  SkBitmap pixel;
  pixel.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
  if (!image->readPixels(nullptr, pixel.pixmap(), image->width() / 2,
                         image->height() / 2))
    return -1;
  return static_cast<int>(SkColorGetG(pixel.getColor(0, 0)));
}

/** An MP4 whose frames step through the ramp. */
sk_sp<SkData> rampMp4() {
  auto encoder = sigil::video::Encoder::make(
      sigil::video::Format::Mp4,
      {.width = kWidth,
       .height = kHeight,
       .framesPerSecond = kFramesPerSecond,
       .bitRate = 2'000'000,
       .hardware = sigil::video::HardwarePreference::Disabled});
  if (!encoder) return nullptr;
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(kWidth, kHeight));
  for (int frame = 0; frame < kFrames; ++frame) {
    const auto level = static_cast<uint8_t>(rampLevel(frame));
    bitmap.eraseColor(SkColorSetRGB(level, level, level));
    if (!encoder->append(bitmap.pixmap())) return nullptr;
  }
  return encoder->finish();
}

}  // namespace

TEST(VideoDecode, TimestamplessStreamIsPlacedByDecodeOrder) {
  const sk_sp<SkData> mp4 = rampMp4();
  ASSERT_NE(mp4, nullptr);
  const std::vector<std::byte> stream = elementaryStream(*mp4);
  ASSERT_FALSE(stream.empty());

  sigil::video::DecodeOptions options;
  options.hardware = sigil::video::HardwarePreference::Disabled;
  // Room for one frame only, so an ask behind the playhead has to find
  // its way back rather than read the answer out of the cache.
  options.cachedFrames = 1;
  std::shared_ptr<sigil::video::Video> video = sigil::video::decodeVideo(
      stream.data(), stream.size(), options, "ramp.h264");
  ASSERT_NE(video, nullptr);
  // A bare stream declares no duration, and the demuxer's own frame rate
  // is what every frame's place is measured against.
  const double frameRate = video->probe().frameRate;
  ASSERT_GT(frameRate, 0.0);
  const double step = 1.0 / frameRate;

  const sigil::video::VideoFrame first = video->frameAt(0.0);
  ASSERT_TRUE(first);
  EXPECT_EQ(first.index, 0);
  EXPECT_DOUBLE_EQ(first.presentationSeconds, 0.0);
  EXPECT_NEAR(centreLevel(first.image), rampLevel(0), 10);

  const sigil::video::VideoFrame third = video->frameAt(2.5 * step);
  ASSERT_TRUE(third);
  EXPECT_EQ(third.index, 2);
  EXPECT_NEAR(third.presentationSeconds, 2.0 * step, step * 0.25);
  EXPECT_NEAR(centreLevel(third.image), rampLevel(2), 10);

  const sigil::video::VideoFrame sixth = video->frameAt(5.5 * step);
  ASSERT_TRUE(sixth);
  EXPECT_EQ(sixth.index, 5);
  EXPECT_NEAR(centreLevel(sixth.image), rampLevel(5), 10);

  // Behind the playhead: decode order restarts from the beginning, so
  // the same time answers the same frame it did the first time.
  const sigil::video::VideoFrame again = video->frameAt(0.0);
  ASSERT_TRUE(again);
  EXPECT_EQ(again.index, 0);
  EXPECT_DOUBLE_EQ(again.presentationSeconds, 0.0);
  EXPECT_NEAR(centreLevel(again.image), rampLevel(0), 10);

  const sigil::video::VideoFrame thirdAgain = video->frameAt(2.5 * step);
  ASSERT_TRUE(thirdAgain);
  EXPECT_EQ(thirdAgain.index, 2);
  EXPECT_NEAR(centreLevel(thirdAgain.image), rampLevel(2), 10);
}
