#include "ifritimage/ImageAsset.h"

#include <include/codec/SkAvifDecoder.h>
#include <include/codec/SkCodec.h>
#include <include/codec/SkGifDecoder.h>
#include <include/codec/SkJpegDecoder.h>
#include <include/codec/SkPngDecoder.h>
#include <include/codec/SkWebpDecoder.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkData.h>
#include <include/core/SkImageInfo.h>

#include <algorithm>
#include <cmath>

namespace ifrit::image {
namespace {

/** The formats this library imports. SkCodec sniffs the actual bytes, so
 *  callers never declare a format — file extensions are irrelevant. */
SkSpan<const SkCodecs::Decoder> supportedDecoders() {
  static const SkCodecs::Decoder kDecoders[] = {
      SkPngDecoder::Decoder(),  SkJpegDecoder::Decoder(),
      SkWebpDecoder::Decoder(), SkGifDecoder::Decoder(),
      SkAvifDecoder::Decoder(),
  };
  return {kDecoders, std::size(kDecoders)};
}

/** Frames whose stated duration is ~0 are a webism: legacy encoders wrote
 *  0/10 ms expecting the player to substitute a sane tick, and every
 *  browser normalizes to 100 ms. Match that so such GIFs animate instead
 *  of flickering through instantly. */
float normalizedDuration(int rawMs) {
  return rawMs <= 10 ? 100.0f : static_cast<float>(rawMs);
}

} // namespace

std::optional<ImageAsset> ImageAsset::decode(sk_sp<SkData> encoded) {
  if (!encoded)
    return std::nullopt;

  std::unique_ptr<SkCodec> codec =
      SkCodec::MakeFromData(std::move(encoded), supportedDecoders());
  if (!codec)
    return std::nullopt;

  // Every frame decodes into the same premultiplied N32 layout the scene
  // canvases draw with, regardless of the source's bit depth or subsampling.
  const SkImageInfo frameInfo = codec->getInfo()
                                    .makeColorType(kN32_SkColorType)
                                    .makeAlphaType(kPremul_SkAlphaType);

  const int frameCount = std::max(1, codec->getFrameCount());

  ImageAsset asset;
  asset.m_width = frameInfo.width();
  asset.m_height = frameInfo.height();
  asset.m_frames.reserve(frameCount);

  // Animated formats delta-encode: a frame may only cover part of the
  // canvas and rely on a fully composited earlier frame beneath it.
  // Feeding that prior frame's pixels back through Options.fPriorFrame
  // makes SkCodec apply the disposal/blend rules, so each stored frame
  // comes out fully composited and self-contained.
  std::vector<SkBitmap> decoded(static_cast<size_t>(frameCount));
  for (int index = 0; index < frameCount; ++index) {
    SkCodec::FrameInfo info{};
    if (frameCount > 1)
      codec->getFrameInfo(index, &info);

    SkBitmap &bitmap = decoded[static_cast<size_t>(index)];
    if (!bitmap.tryAllocPixels(frameInfo))
      return std::nullopt;

    SkCodec::Options options;
    options.fFrameIndex = index;
    const int required = info.fRequiredFrame;
    if (index > 0 && required != SkCodec::kNoFrame && required < index) {
      // Seed with the composited frame this one builds on. Copying keeps
      // `decoded[required]` intact for later frames that need it too.
      decoded[static_cast<size_t>(required)].readPixels(bitmap.pixmap(), 0, 0);
      options.fPriorFrame = required;
    }

    const SkCodec::Result result = codec->getPixels(
        frameInfo, bitmap.getPixels(), bitmap.rowBytes(), &options);
    // Truncated files can still yield usable leading frames.
    if (result != SkCodec::kSuccess && result != SkCodec::kIncompleteInput)
      return std::nullopt;

    bitmap.setImmutable();
    Frame frame;
    frame.image = bitmap.asImage();
    frame.durationMs = frameCount > 1 ? normalizedDuration(info.fDuration) : 0;
    if (!frame.image)
      return std::nullopt;
    asset.m_totalDurationMs += frame.durationMs;
    asset.m_frames.push_back(std::move(frame));
  }

  if (frameCount > 1) {
    const int reps = codec->getRepetitionCount();
    asset.m_repetitionCount =
        reps == SkCodec::kRepetitionCountInfinite ? kInfinite : reps + 1;
  }
  return asset;
}

std::optional<ImageAsset> ImageAsset::load(const std::string &path) {
  return decode(SkData::MakeFromFileName(path.c_str()));
}

const Frame &ImageAsset::frameAt(double milliseconds) const {
  if (m_frames.size() == 1 || m_totalDurationMs <= 0.0f)
    return m_frames.front();

  double t = std::max(0.0, milliseconds);
  const double total = m_totalDurationMs;
  if (m_repetitionCount == kInfinite) {
    t = std::fmod(t, total);
  } else if (t >= total * m_repetitionCount) {
    return m_frames.back(); // finished finite animation holds the end
  } else {
    t = std::fmod(t, total);
  }

  for (const Frame &frame : m_frames) {
    t -= frame.durationMs;
    if (t < 0.0)
      return frame;
  }
  return m_frames.back();
}

} // namespace ifrit::image
