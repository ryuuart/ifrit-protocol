/** @file
 * The Skia-codec backend: PNG, JPEG, WebP, GIF and AVIF sniffed from the
 * bytes, stills and animations decoded into premultiplied frames, their
 * metadata read without a pixel decode, and the first frame's pixels as
 * channel planes.
 */

#include <include/codec/SkAvifDecoder.h>
#include <include/codec/SkCodec.h>
#include <include/codec/SkEncodedImageFormat.h>
#include <include/codec/SkGifDecoder.h>
#include <include/codec/SkJpegDecoder.h>
#include <include/codec/SkPngDecoder.h>
#include <include/codec/SkWebpDecoder.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>
#include <include/core/SkData.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPixmap.h>

#include <algorithm>
#include <chrono>
#include <vector>

#include "Backends.h"

namespace sigil::media::backend {

namespace {

/** The formats the Skia route reads. SkCodec sniffs the bytes, so a
 *  caller never declares a format. */
SkSpan<const SkCodecs::Decoder> supportedDecoders() {
  static const SkCodecs::Decoder kDecoders[] = {
      SkPngDecoder::Decoder(),  SkJpegDecoder::Decoder(),
      SkWebpDecoder::Decoder(), SkGifDecoder::Decoder(),
      SkAvifDecoder::Decoder(),
  };
  return {kDecoders, std::size(kDecoders)};
}

std::unique_ptr<SkCodec> codecFor(const std::byte* bytes, size_t size) {
  return SkCodec::MakeFromData(SkData::MakeWithoutCopy(bytes, size),
                               supportedDecoders());
}

/** Frames whose stated duration is ~0 are a webism: legacy encoders wrote
 *  0 or 10 ms expecting the player to substitute a sane tick, and every
 *  browser plays such a frame for 100 ms. Matching them makes such GIFs
 *  animate instead of flickering through at once. */
std::chrono::duration<double> normalizedDuration(int milliseconds) {
  return std::chrono::duration<double>(
      milliseconds <= 10 ? 0.1 : milliseconds / 1000.0);
}

}  // namespace

std::optional<Image> decodeWithSkia(const std::byte* bytes, size_t size) {
  std::unique_ptr<SkCodec> codec = codecFor(bytes, size);
  if (!codec) return std::nullopt;

  // Every frame decodes into the premultiplied N32 layout the canvases
  // draw with, whatever the source's bit depth or subsampling.
  const SkImageInfo frameInfo = codec->getInfo()
                                    .makeColorType(kN32_SkColorType)
                                    .makeAlphaType(kPremul_SkAlphaType);
  const int frameCount = std::max(1, codec->getFrameCount());

  // Animated formats delta-encode: a frame may cover only part of the
  // canvas and rely on a fully composited earlier frame beneath it.
  // Feeding that frame's pixels back through Options.fPriorFrame makes
  // SkCodec apply the disposal and blend rules, so each stored frame
  // comes out composited and self-contained.
  std::vector<SkBitmap> decoded(static_cast<size_t>(frameCount));
  std::vector<Frame> frames;
  frames.reserve(static_cast<size_t>(frameCount));
  for (int index = 0; index < frameCount; ++index) {
    SkCodec::FrameInfo info{};
    if (frameCount > 1) codec->getFrameInfo(index, &info);

    SkBitmap& bitmap = decoded[static_cast<size_t>(index)];
    if (!bitmap.tryAllocPixels(frameInfo)) return std::nullopt;

    SkCodec::Options options;
    options.fFrameIndex = index;
    const int required = info.fRequiredFrame;
    if (index > 0 && required != SkCodec::kNoFrame && required < index) {
      // Seeded with the composited frame this one builds on; the copy
      // keeps that frame intact for the later ones that need it too.
      decoded[static_cast<size_t>(required)].readPixels(bitmap.pixmap(), 0, 0);
      options.fPriorFrame = required;
    }

    const SkCodec::Result result = codec->getPixels(
        frameInfo, bitmap.getPixels(), bitmap.rowBytes(), &options);
    // A truncated file can still yield usable leading frames.
    if (result != SkCodec::kSuccess && result != SkCodec::kIncompleteInput)
      return std::nullopt;

    bitmap.setImmutable();
    Frame frame;
    frame.image = bitmap.asImage();
    if (!frame.image) return std::nullopt;
    if (frameCount > 1) frame.duration = normalizedDuration(info.fDuration);
    frames.push_back(std::move(frame));
  }

  int repetitions = -1;
  if (frameCount > 1) {
    const int stated = codec->getRepetitionCount();
    repetitions = stated == SkCodec::kRepetitionCountInfinite ? -1 : stated + 1;
  }
  return Image(std::move(frames), repetitions);
}

std::optional<Metadata> probeWithSkia(const std::byte* bytes, size_t size) {
  std::unique_ptr<SkCodec> codec = codecFor(bytes, size);
  if (!codec) return std::nullopt;
  Metadata metadata;
  metadata.width = codec->dimensions().width();
  metadata.height = codec->dimensions().height();
  metadata.frames = std::max(1, codec->getFrameCount());
  if (metadata.frames > 1) {
    std::chrono::duration<double> total{};
    for (const SkCodec::FrameInfo& info : codec->getFrameInfo())
      total += normalizedDuration(info.fDuration);
    metadata.duration = total;
    const int stated = codec->getRepetitionCount();
    metadata.repetitions =
        stated == SkCodec::kRepetitionCountInfinite ? -1 : stated + 1;
  }
  metadata.hasAlpha = codec->getInfo().alphaType() != kOpaque_SkAlphaType;
  switch (codec->getEncodedFormat()) {
    case SkEncodedImageFormat::kPNG:
      metadata.format = "png";
      break;
    case SkEncodedImageFormat::kJPEG:
      metadata.format = "jpeg";
      break;
    case SkEncodedImageFormat::kWEBP:
      metadata.format = "webp";
      break;
    case SkEncodedImageFormat::kGIF:
      metadata.format = "gif";
      break;
    case SkEncodedImageFormat::kAVIF:
      metadata.format = "avif";
      break;
    default:
      metadata.format = "image";
      break;
  }
  return metadata;
}

std::optional<Channels> decodeChannelsWithSkia(const std::byte* bytes,
                                               size_t size) {
  // LDR web formats: decoded through the Skia route and the premultiplied
  // N32 pixels normalized to 0..1 floats named R/G/B/A.
  const std::optional<Image> image = decodeWithSkia(bytes, size);
  if (!image || image->frames().empty()) return std::nullopt;
  const sk_sp<SkImage>& picture = image->frames().front().image;
  SkPixmap pixmap;
  if (!picture->peekPixels(&pixmap)) return std::nullopt;
  Channels channels;
  channels.width = picture->width();
  channels.height = picture->height();
  channels.names = {"R", "G", "B", "A"};
  channels.data.resize((size_t)channels.width * channels.height * 4);
  for (int y = 0; y < channels.height; ++y)
    for (int x = 0; x < channels.width; ++x) {
      const SkColor color = pixmap.getColor(x, y);
      float* destination =
          channels.data.data() + ((size_t)y * channels.width + x) * 4;
      destination[0] = SkColorGetR(color) / 255.0f;
      destination[1] = SkColorGetG(color) / 255.0f;
      destination[2] = SkColorGetB(color) / 255.0f;
      destination[3] = SkColorGetA(color) / 255.0f;
    }
  return channels;
}

}  // namespace sigil::media::backend
