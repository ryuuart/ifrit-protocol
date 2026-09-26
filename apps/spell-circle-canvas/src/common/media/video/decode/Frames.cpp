/** @file
 * The decode loop and the presentation-frame cache: pulling the next frame
 * out of the codec, answering a time from what is held, and turning a held
 * frame into the image a caller asked for.
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iterator>
#include <optional>
#include <utility>

#include "DecodeInternal.h"

namespace sigil::media {
namespace {

struct FrameDeleter {
  void operator()(AVFrame* frame) const { av_frame_free(&frame); }
};

SharedFrame cloneFrame(const AVFrame* frame) {
  return SharedFrame(av_frame_clone(frame), FrameDeleter{});
}

}  // namespace

Video::Decoder::CachedFrame* Video::Decoder::decodeNext() {
  while (true) {
    int result = avcodec_receive_frame(codecContext, receiveFrame);
    if (result >= 0) {
      const int64_t timestamp = receiveFrame->best_effort_timestamp;
      timestampless |= timestamp == AV_NOPTS_VALUE;
      double presentation =
          timestamp == AV_NOPTS_VALUE
              ? (metadata.frameRate > 0 ? decodedSequence / metadata.frameRate
                                        : cursorSeconds)
              : timestamp * rational(stream->time_base) - streamStartSeconds;
      presentation = std::max(0.0, presentation);
      double duration =
          receiveFrame->duration > 0
              ? receiveFrame->duration * rational(stream->time_base)
              : (metadata.frameRate > 0 ? 1.0 / metadata.frameRate
                                        : 1.0 / 30.0);
      CachedFrame cached;
      cached.decoded = cloneFrame(receiveFrame);
      metadata.hasAlpha |=
          pixelFormatHasAlpha(static_cast<AVPixelFormat>(receiveFrame->format));
      cached.presentationSeconds = presentation;
      cached.durationSeconds = duration;
      cached.index = metadata.frameRate > 0
                         ? static_cast<int64_t>(
                               std::llround(presentation * metadata.frameRate))
                         : decodedSequence;
      cached.hardwareDecoded =
          receiveFrame->format == hardwarePixelFormat && hardwareActive;
      decodedNative = cached.hardwareDecoded;
      if (options.hardware == HardwarePreference::Required &&
          !cached.hardwareDecoded) {
        error = "the decoder did not produce a native hardware frame";
        av_frame_unref(receiveFrame);
        return nullptr;
      }
      if (cached.hardwareDecoded)
        cached.surface = device::retainNativeFrame(receiveFrame);
      av_frame_unref(receiveFrame);
      cursorSeconds = presentation;
      ++decodedSequence;
      cache.push_back(std::move(cached));
      while (cache.size() > options.cachedFrames) cache.pop_front();
      return &cache.back();
    }
    if (result == AVERROR_EOF) return nullptr;
    if (result != AVERROR(EAGAIN)) {
      error = ffmpegError(result);
      return nullptr;
    }

    bool submitted = false;
    while (!submitted) {
      result = av_read_frame(formatContext, packet);
      if (result < 0) {
        if (!sentDrain) {
          result = avcodec_send_packet(codecContext, nullptr);
          sentDrain = true;
          if (result < 0 && result != AVERROR_EOF) error = ffmpegError(result);
        }
        submitted = true;
        break;
      }
      if (packet->stream_index == videoStream) {
        result = avcodec_send_packet(codecContext, packet);
        av_packet_unref(packet);
        if (result < 0 && result != AVERROR(EAGAIN)) {
          error = ffmpegError(result);
          return nullptr;
        }
        submitted = true;
      } else {
        av_packet_unref(packet);
      }
    }
  }
}

Video::Decoder::CachedFrame* Video::Decoder::cachedAt(double seconds) {
  // The newest held frame at or before the ask, and the oldest one
  // after it.
  auto before = cache.end();
  auto after = cache.end();
  for (auto found = cache.begin(); found != cache.end(); ++found) {
    if (found->presentationSeconds <= seconds) {
      if (before == cache.end() ||
          found->presentationSeconds > before->presentationSeconds)
        before = found;
    } else if (after == cache.end() ||
               found->presentationSeconds < after->presentationSeconds) {
      after = found;
    }
  }
  if (before == cache.end()) return nullptr;
  // The frame answers a time it covers, and a time in the gap that its
  // duration leaves before the frame that follows it — the same answer
  // the decode loop gives, so a repeated ask does not depend on what
  // the cache happens to hold. Two frames that are not neighbours in
  // decode order say nothing about the gap between them: one that was
  // evicted may cover it.
  const bool covers =
      seconds < before->presentationSeconds + before->durationSeconds;
  if (!covers && (after == cache.end() || after->index != before->index + 1))
    return nullptr;
  if (std::next(before) != cache.end()) {
    CachedFrame promoted = std::move(*before);
    cache.erase(before);
    cache.push_back(std::move(promoted));
  }
  return &cache.back();
}

Frame Video::Decoder::materialize(CachedFrame& cached) {
  Frame frame;
  frame.time = std::chrono::duration<double>(cached.presentationSeconds);
  frame.duration = std::chrono::duration<double>(cached.durationSeconds);
  frame.index = cached.index;
  if (cached.hardwareDecoded && cached.surface) {
    // A hardware frame stays where it was decoded: the binding wraps it
    // for whichever recorder draws it, once per recorder, and reads it
    // back only for a caller with none.
    if (!cached.binding) {
      if (!deviceContext) deviceContext = device::makeContext();
      cached.binding = std::make_shared<FrameBinding>(
          cached.decoded, cached.surface, deviceContext, metadata.hasAlpha);
    }
    frame.device = cached.surface;
    frame.device.binding = cached.binding;
    return frame;
  }
  if (!cached.raster)
    cached.raster = rasterizeFrame(cached.decoded.get(), cached.hardwareDecoded,
                                   metadata.hasAlpha, sws, swsSource);
  frame.image = cached.raster;
  return frame;
}

Frame Video::Decoder::frameAt(double seconds) {
  seconds = std::max(0.0, seconds);
  if (CachedFrame* found = cachedAt(seconds))
    return materialize(*found);

  const bool unopened = !std::isfinite(cursorSeconds);
  // A stream placed by decode order is read from wherever it stands,
  // because the only place a seek can put it is the beginning: it
  // repositions for an ask behind the playhead and for nothing else.
  const bool reposition = timestampless
                              ? (!unopened && seconds + 0.001 < cursorSeconds)
                              : (unopened || seconds + 0.001 < cursorSeconds ||
                                 seconds > cursorSeconds + 2.0);
  if (reposition && !seekTo(unopened && metadata.hasAlpha ? 0.0 : seconds))
    return {};

  // The newest decoded frame at or before the asked time. When frame
  // durations leave a gap before the next frame, it is the answer; it
  // is materialized in the cache, never on this copy, so its raster or
  // device wrap is there for the next ask.
  std::optional<CachedFrame> preceding;
  const auto materializePreceding = [&]() {
    CachedFrame* cached = cachedAt(preceding->presentationSeconds);
    if (!cached) {
      cache.push_back(std::move(*preceding));
      while (cache.size() > options.cachedFrames) cache.pop_front();
      cached = &cache.back();
    }
    return materialize(*cached);
  };
  for (int decoded = 0; decoded < 10000; ++decoded) {
    CachedFrame* current = decodeNext();
    if (!current) break;
    if (current->presentationSeconds <= seconds) preceding = *current;
    if (seconds < current->presentationSeconds + current->durationSeconds) {
      if (current->presentationSeconds <= seconds || !preceding)
        return materialize(*current);
      return materializePreceding();
    }
    if (current->presentationSeconds > seconds)
      return preceding ? materializePreceding()
                       : materialize(*current);
  }

  if (preceding) return materializePreceding();
  CachedFrame* last = nullptr;
  for (CachedFrame& frame : cache)
    if (!last || frame.presentationSeconds > last->presentationSeconds)
      last = &frame;
  return last ? materialize(*last) : Frame{};
}

}  // namespace sigil::media
