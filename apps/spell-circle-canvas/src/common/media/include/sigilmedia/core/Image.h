#pragma once

/** @file
 * @ingroup media-core
 * `Image`, the decoded image document — a still, or an animation whose
 * every frame is composited at decode time — and `ImageOptions`, what an
 * image is decoded with.
 */

#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSize.h>

#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "sigilmedia/core/Frame.h"

namespace sigil::media {

/** WHAT AN IMAGE IS DECODED WITH: `hub.load<media::Image>(uri, {…})`
 *  and `media::decode<media::Image>(bytes, {…})`. Left alone, an image
 *  decodes its default layer at its own size. */
struct ImageOptions {
  /** The EXR channel group to composite — "diffuse" selects
   *  diffuse.R/G/B and its alpha. Empty is the default layer, and a
   *  format without layers ignores it. */
  std::string layer;
  /** The raster size, in pixels, of a vector source. 0 on one axis
   *  derives it from the other by aspect; both 0 rasterizes at the
   *  intrinsic size, falling back to 512 for a percent-sized source that
   *  has none. A raster format ignores it. */
  int width = 0;
  int height = 0;  ///< The other axis of the same raster size.

  bool operator==(const ImageOptions&) const = default;
};

/**
 * A DECODED IMAGE DOCUMENT, still or animated, ready to draw on any
 * canvas: every frame fully composited at decode time, so drawing frame N
 * never depends on frame N-1. A still is a one-frame document, so one
 * `frameAt()` reads either.
 *
 * Decoding is CPU-side and EAGER: a document's frames stay resident for
 * its lifetime, which fits decode once and draw every frame, and does not
 * fit video-sized content — that is `media::Video`.
 */
class Image {
 public:
  /** What `hub.load<media::Image>` and `media::decode` take. */
  using Options = ImageOptions;

  /** An empty document: no frames, no size. */
  Image() = default;
  /** A document of @p frames in playback order, repeating
   *  @p repetitions times (negative for forever). Each frame's `time`
   *  and `index` are placed here from the durations before it, so a
   *  decoder states durations only. */
  explicit Image(std::vector<Frame> frames, int repetitions = -1);

  /** A PICTURE ALREADY RENDERED, as a one-frame document: a snapshot of a
   *  surface, a texture baked on an intermediate canvas. A null picture
   *  is an empty document. */
  static std::shared_ptr<const Image> of(sk_sp<SkImage> picture);

  /** The size of every frame in pixels; empty for an empty document. */
  SkISize size() const { return m_size; }
  /** The sum of every frame's duration; zero for a still. */
  std::chrono::duration<double> duration() const { return m_duration; }
  /** How many times an animation plays, negative for forever. A still
   *  answers forever. */
  int repetitions() const { return m_repetitions; }
  /** Whether the document moves: more than one frame. */
  bool isRunning() const { return m_frames.size() > 1; }
  /** Every frame in playback order, already composited. */
  const std::vector<Frame>& frames() const { return m_frames; }

  /** THE FRAME TO SHOW @p elapsed INTO PLAYBACK, under @p timing: an
   *  animation loops as its file says unless @p timing says otherwise,
   *  a finished finite one holds its last frame, and a still answers its
   *  one frame at any time. An empty frame for an empty document. */
  Frame frameAt(std::chrono::duration<double> elapsed,
                const Timing& timing = {}) const;

 private:
  std::vector<Frame> m_frames;
  SkISize m_size = SkISize::MakeEmpty();
  std::chrono::duration<double> m_duration{};
  int m_repetitions = -1;
};

/** WHAT AN IMAGE IS LOADED WITH, under the name a resource library asks
 *  by: found by argument-dependent lookup on the tag, so a hub takes
 *  `load<media::Image>(uri, {.width = 124})` with these options spelled
 *  as this library spells them. */
inline ImageOptions loadOptions(std::type_identity<Image>) { return {}; }

/** The name a resource library registers and asks for an image under, so
 *  every image of a program — a host and a sketch it loaded while running
 *  — reaches the one decoder: `hub.load<media::Image>(uri)`. */
inline std::string_view meaningName(std::type_identity<Image>) {
  return "media.Image";
}

}  // namespace sigil::media
