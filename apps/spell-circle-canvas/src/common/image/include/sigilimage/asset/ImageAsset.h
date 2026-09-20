#pragma once

/** @file
 * @ingroup image-asset
 * The decoded image document: ImageProbe, the metadata read without a
 * pixel decode; Frame, one premultiplied SkImage with its duration; and
 * ImageAsset, the frames with their playback, decoded from SkData
 * through Skia's own codecs.
 */

/** @defgroup image-asset Assets
 *  The decoded image document a caller draws: its frames, their timing,
 *  and the metadata that can be read without decoding any of them.
 *  @{ */
/** @} */

#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>

#include <optional>
#include <string>
#include <vector>

class SkData;

/** What a picture MEANS, for a caller that already has its bytes: the
 *  decoded document and its frames, the metadata a probe reads, the
 *  encoders that write pixels back out, and the coverage and distance
 *  answers a silhouette question needs. Where those bytes come from —
 *  a path, a URI, a mount, a cache — belongs to whoever fetched them;
 *  nothing here opens a file. */
namespace sigil::image {

/** Cheap metadata from encoded bytes, no pixel decode. */
struct ImageProbe {
  int width = 0;
  int height = 0;
  int channels = 4;
  int frames = 1;                   ///< >1 for animations
  bool floatingPoint = false;       ///< HDR/float source (EXR, float TIFF…)
  std::string format;               ///< "png", "openexr", "psd", …
  std::vector<std::string> layers;  ///< EXR subimages/layer prefixes
  std::vector<std::string> channelNames;  ///< EXR channel names
};

/** One decoded frame: a premultiplied, immutable, raster-backed SkImage
 *  plus how long it stays on screen (0 for still images). */
struct Frame {
  sk_sp<SkImage> image;
  float durationMs = 0.0f;
};

/**
 * A decoded image document — still or animated — ready to draw on any
 * SkCanvas, with every frame fully composited at decode time, so
 * drawing frame N never depends on frame N-1. PNG, JPEG, WebP, GIF and
 * AVIF, animation included where the format carries it.
 *
 * Decoding is CPU-side and EAGER: an asset's frames stay resident for
 * its lifetime, which fits decode once at import and draw per frame,
 * and does not fit streaming video-sized content.
 */
class ImageAsset {
 public:
  /** Decodes an encoded image from memory; nullopt when the bytes are not
   *  one of the supported formats or are corrupt. */
  static std::optional<ImageAsset> decode(sk_sp<SkData> encoded);

  /** Sniffs encoded bytes: dimensions, frame count, format name;
   *  nullopt when the bytes are not a supported format. */
  static std::optional<ImageProbe> probe(sk_sp<SkData> encoded);

  /** Wraps an already-rendered still image as a one-frame asset — the
   *  bridge for a texture generated on an intermediate canvas or
   *  surface. A null image yields an empty asset. */
  static ImageAsset wrap(sk_sp<SkImage> image);

  /** Width of every frame, in pixels; 0 for an empty asset. */
  int width() const { return m_width; }
  /** Height of every frame, in pixels; 0 for an empty asset. */
  int height() const { return m_height; }

  /** Whether the asset carries more than one frame. */
  bool animated() const { return m_frames.size() > 1; }
  /** Every frame in playback order, already composited. */
  const std::vector<Frame>& frames() const { return m_frames; }

  /** Sum of all frame durations; 0 for still images. */
  float totalDurationMs() const { return m_totalDurationMs; }

  /** The repetition count of an animation that never stops. */
  static constexpr int kInfinite = -1;
  /** Number of times the animation plays, or kInfinite. Still images
   *  report kInfinite. */
  int repetitionCount() const { return m_repetitionCount; }

  /** The frame to show at @p milliseconds since playback start, looping
   *  according to repetitionCount(). A finished finite animation holds
   *  its last frame, and a still always answers its one frame. */
  const Frame& frameAt(double milliseconds) const;

 private:
  ImageAsset() = default;

  std::vector<Frame> m_frames;
  int m_width = 0;
  int m_height = 0;
  float m_totalDurationMs = 0.0f;
  int m_repetitionCount = kInfinite;
};

}  // namespace sigil::image
