#pragma once

/** @file
 * SigilCompose integration for a streaming SigilVideo clip. The adapter is a
 * live custom leaf: the compose kernel retains no codec vocabulary and the
 * video owns its frame cache and device surfaces.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRect.h>
#include <sigilvideo/decode/Decode.h>
#include <sigilvideo/decode/Playback.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include "sigilcompose/Compose.h"

namespace sigil::compose {

enum class VideoFit {
  Stretch,
  Contain,
  Cover,
};

struct VideoOptions {
  double startSeconds = 0.0;
  double playbackRate = 1.0;
  bool loop = true;
  VideoFit fit = VideoFit::Stretch;
  SkSamplingOptions sampling = SkSamplingOptions(SkFilterMode::kLinear);
  float opacity = 1.0f;
  SkBlendMode blend = SkBlendMode::kSrcOver;
};

namespace detail {

inline double videoTime(const sigil::video::Video& clip, double seconds,
                        bool loop) {
  const double duration = clip.probe().durationSeconds;
  if (duration <= 0.0) return std::max(0.0, seconds);
  if (!loop) return std::clamp(seconds, 0.0, std::nextafter(duration, 0.0));
  seconds = std::fmod(seconds, duration);
  return seconds < 0.0 ? seconds + duration : seconds;
}

inline void paintVideoFrame(SkCanvas& canvas,
                            const sigil::video::VideoFrame& frame,
                            const SkSize size, const VideoOptions& options) {
  if (!frame.image || size.isEmpty()) return;
  const SkRect image =
      SkRect::MakeWH(frame.image->width(), frame.image->height());
  SkRect source = image;
  SkRect destination = SkRect::MakeSize(size);
  const float sourceAspect = image.width() / image.height();
  const float destinationAspect = destination.width() / destination.height();
  if (options.fit == VideoFit::Cover) {
    if (sourceAspect > destinationAspect) {
      const float width = image.height() * destinationAspect;
      source = SkRect::MakeXYWH((image.width() - width) * 0.5f, 0.0f, width,
                                image.height());
    } else {
      const float height = image.width() / destinationAspect;
      source = SkRect::MakeXYWH(0.0f, (image.height() - height) * 0.5f,
                                image.width(), height);
    }
  } else if (options.fit == VideoFit::Contain) {
    if (sourceAspect > destinationAspect) {
      const float height = destination.width() / sourceAspect;
      destination =
          SkRect::MakeXYWH(0.0f, (destination.height() - height) * 0.5f,
                           destination.width(), height);
    } else {
      const float width = destination.height() * sourceAspect;
      destination = SkRect::MakeXYWH((destination.width() - width) * 0.5f, 0.0f,
                                     width, destination.height());
    }
  }

  SkPaint paint;
  paint.setAlphaf(std::clamp(options.opacity, 0.0f, 1.0f));
  paint.setBlendMode(options.blend);
  canvas.drawImageRect(frame.image, source, destination, options.sampling,
                       &paint, SkCanvas::kStrict_SrcRectConstraint);
}

}  // namespace detail

/** A video frame sampled from the composer's motion clock. Its intrinsic
 *  layout size is the encoded frame size and remains overridable by the usual
 *  width, height, grow, and aspect-ratio setters. */
inline Element video(std::shared_ptr<sigil::video::Video> clip,
                     VideoOptions options = {}) {
  const int width = clip ? clip->probe().width : 0;
  const int height = clip ? clip->probe().height : 0;
  Element leaf = custom([clip = std::move(clip), options](
                            SkCanvas& canvas, const PaintContext& context) {
    if (!clip) return;
    const double requested =
        options.startSeconds + context.elapsedSeconds * options.playbackRate;
    const double seconds = detail::videoTime(*clip, requested, options.loop);
    detail::paintVideoFrame(canvas, clip->frameAt(seconds, canvas.recorder()),
                            context.size, options);
  });
  if (width > 0) leaf.width(width);
  if (height > 0) leaf.height(height);
  leaf.cache(Cache::None);
  return leaf;
}

/** The asynchronous form over an already registered playback handle. Reuse
 *  the handle when several leaves present the same source clock; one decoded
 *  frame then feeds every leaf without a second decoder or native mapping. */
inline Element video(std::shared_ptr<sigil::video::Video> clip,
                     std::shared_ptr<sigil::video::Playback> playback,
                     sigil::video::Playback::Handle handle,
                     VideoOptions options = {}) {
  const int width = clip ? clip->probe().width : 0;
  const int height = clip ? clip->probe().height : 0;
  Element leaf = custom([clip = std::move(clip), playback = std::move(playback),
                         handle, options](SkCanvas& canvas,
                                          const PaintContext& context) {
    if (!clip || !playback) return;
    const double requested =
        options.startSeconds + context.elapsedSeconds * options.playbackRate;
    const double seconds = detail::videoTime(*clip, requested, options.loop);
    playback->request(handle, seconds);
    const sigil::video::VideoFrame frame =
        playback->frame(handle, canvas.recorder());
    detail::paintVideoFrame(canvas, frame, context.size, options);
  });
  if (width > 0) leaf.width(width);
  if (height > 0) leaf.height(height);
  leaf.cache(Cache::None);
  return leaf;
}

/** The asynchronous many-video form. The clip registers one clock with
 *  @p playback — the handle it already holds when a scene is described
 *  again, or when another leaf shows the same clip — so decode work is
 *  coalesced on the bounded worker pool and the leaf paints the last
 *  complete frame without blocking composition. Share one Playback across
 *  every video leaf in a scene. */
inline Element video(std::shared_ptr<sigil::video::Video> clip,
                     std::shared_ptr<sigil::video::Playback> playback,
                     VideoOptions options = {}) {
  const sigil::video::Playback::Handle handle =
      playback ? playback->add(clip) : 0;
  return video(std::move(clip), std::move(playback), handle,
               std::move(options));
}

}  // namespace sigil::compose
