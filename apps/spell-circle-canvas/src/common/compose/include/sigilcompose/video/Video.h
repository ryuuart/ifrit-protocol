#pragma once

/** @file
 * @ingroup compose-video
 *
 * SigilCompose integration for a streaming SigilMediaVideo clip. The adapter is a
 * live custom leaf: the compose kernel retains no codec vocabulary and the
 * video owns its frame cache and device surfaces.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRect.h>
#include <sigilmaterial/skia/Paint.h>  // material::Fit — how a frame meets its box
#include <sigilmedia/video/Video.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <utility>

#include "sigilcompose/Compose.h"

namespace sigil::compose {

/** WHAT A `video()` LEAF IS TOLD ABOUT ITS CLIP — where in the clip to
 *  start, how fast to run, whether to wrap round, how to fit the frame,
 *  and how to paint it. Every field has the value a leaf takes when it
 *  is left alone, so `video(clip)` plays the whole clip from its
 *  beginning at its own rate, looping, stretched to the node's box.
 *
 *  `startSeconds` and `playbackRate` are read against the scene's own
 *  elapsed time, so the leaf never holds a clock of its own; `loop`
 *  wraps that time round the clip's duration rather than clamping at
 *  its end. `sampling` is how the frame is filtered into the box, and
 *  `opacity` and `blend` are the leaf's own, applied as it paints
 *  rather than through the node's paint verbs — a custom leaf paints
 *  itself. */
struct VideoOptions {
  double startSeconds = 0.0;  ///< where in the clip time zero sits
  double playbackRate = 1.0;  ///< clip seconds per scene second
  bool loop = true;           ///< wrap past the end rather than stop on it
  /// How the frame meets the box when the two do not share an aspect
  /// ratio, in SigilMaterial's words for that question: `Stretch`
  /// distorts the frame to fill, `Contain` letterboxes the whole of it,
  /// `Cover` crops what overflows, and `Native` draws the frame at its
  /// own pixels from the box's corner.
  material::Fit fit = material::Fit::Stretch;
  /// How a frame is filtered into the box; linear unless stated.
  SkSamplingOptions sampling = SkSamplingOptions(SkFilterMode::kLinear);
  float opacity = 1.0f;                       ///< 0 clear to 1 solid
  SkBlendMode blend = SkBlendMode::kSrcOver;  ///< how the frame combines
};

namespace detail {

inline void paintVideoFrame(SkCanvas& canvas, const sigil::media::Frame& frame,
                            const SkSize size, const VideoOptions& options) {
  // A frame standing on a device is bound for the recorder this canvas
  // records on, and read back where the canvas has none.
  const sk_sp<SkImage> picture =
      sigil::media::deviceImage(frame, canvas.recorder());
  // Both sides are guarded: a frame whose image has no extent divides by
  // its own height below exactly as an empty destination divides by its
  // own.
  if (!picture || picture->width() <= 0 || picture->height() <= 0 ||
      size.isEmpty())
    return;
  const SkRect image = SkRect::MakeWH(picture->width(), picture->height());
  SkRect source = image;
  SkRect destination = SkRect::MakeSize(size);
  const float sourceAspect = image.width() / image.height();
  const float destinationAspect = destination.width() / destination.height();
  if (options.fit == material::Fit::Native) {
    destination = image;
  } else if (options.fit == material::Fit::Cover) {
    if (sourceAspect > destinationAspect) {
      const float width = image.height() * destinationAspect;
      source = SkRect::MakeXYWH((image.width() - width) * 0.5f, 0.0f, width,
                                image.height());
    } else {
      const float height = image.width() / destinationAspect;
      source = SkRect::MakeXYWH(0.0f, (image.height() - height) * 0.5f,
                                image.width(), height);
    }
  } else if (options.fit == material::Fit::Contain) {
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
  canvas.drawImageRect(picture, source, destination, options.sampling,
                       &paint, SkCanvas::kStrict_SrcRectConstraint);
}

}  // namespace detail

/** A video frame sampled from the composer's motion clock. Its intrinsic
 *  layout size is the encoded frame size and remains overridable by the usual
 *  width, height, flexGrow, and aspect-ratio setters. A clip opened with a
 *  decode pool is painted from the newest frame the pool finished, so the
 *  leaf never waits; several leaves showing one clip share its decode. */
inline Element video(std::shared_ptr<const sigil::media::Video> clip,
                     VideoOptions options = {}) {
  const int width = clip ? clip->size().width() : 0;
  const int height = clip ? clip->size().height() : 0;
  Element leaf = custom([clip = std::move(clip), options](
                            SkCanvas& canvas, const PaintContext& context) {
    if (!clip) return;
    const sigil::media::Timing timing{
        .start = std::chrono::duration<double>(options.startSeconds),
        .rate = options.playbackRate,
        .loop = options.loop ? sigil::media::Loop::Forever
                             : sigil::media::Loop::Once};
    detail::paintVideoFrame(
        canvas,
        clip->frameAt(std::chrono::duration<double>(context.elapsedSeconds),
                      timing),
        context.size, options);
  });
  if (width > 0) leaf.width(width);
  if (height > 0) leaf.height(height);
  leaf.cache(Cache::None);
  return leaf;
}

}  // namespace sigil::compose
