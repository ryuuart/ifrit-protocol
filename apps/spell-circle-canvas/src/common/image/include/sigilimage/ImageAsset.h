#pragma once

#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>

#include <optional>
#include <string>
#include <vector>

class SkData;

namespace sigil::image {

/** One decoded frame: a premultiplied, immutable, raster-backed SkImage
 *  plus how long it stays on screen (0 for still images). */
struct Frame {
  sk_sp<SkImage> image;
  float durationMs = 0.0f;
};

/**
 * A decoded image document — still or animated — ready to draw on any
 * SkCanvas (the offscreen scene canvases included; upload to the GPU
 * happens implicitly on first draw through a Graphite recorder).
 *
 * Formats: PNG, JPEG, WebP, GIF, and AVIF, including animation for the
 * formats that carry it (GIF, animated WebP, animated AVIF/AVIS). Every
 * frame is fully composited at decode time — disposal and blend semantics
 * of the source format are already applied, so drawing frame N never
 * depends on frame N-1.
 *
 * Decoding is CPU-side and eager: an asset's frames stay resident for its
 * lifetime, which fits canvas-drawing workloads (decode once at import,
 * draw per frame). Not for streaming video-sized content.
 */
class ImageAsset {
public:
  /** Decodes an encoded image from memory; nullopt when the bytes are not
   *  one of the supported formats or are corrupt. */
  static std::optional<ImageAsset> decode(sk_sp<SkData> encoded);

  /** Reads and decodes a file; nullopt on I/O or decode failure. */
  static std::optional<ImageAsset> load(const std::string &path);

  /** Wraps an already-rendered still image as a one-frame asset — the
   *  bridge for textures generated on an intermediate canvas/surface
   *  (procedural nine-slice frames, baked patterns). Null images yield
   *  an empty asset. */
  static ImageAsset wrap(sk_sp<SkImage> image);

  int width() const { return m_width; }
  int height() const { return m_height; }

  bool animated() const { return m_frames.size() > 1; }
  const std::vector<Frame> &frames() const { return m_frames; }

  /** Sum of all frame durations; 0 for still images. */
  float totalDurationMs() const { return m_totalDurationMs; }

  /** Number of times the animation plays, or kInfinite (the common case
   *  for GIFs/stickers). Still images report kInfinite. */
  static constexpr int kInfinite = -1;
  int repetitionCount() const { return m_repetitionCount; }

  /**
   * The frame to show at `milliseconds` since playback start, looping
   * according to repetitionCount() — a finished finite animation holds its
   * last frame. Still images always return the one frame.
   */
  const Frame &frameAt(double milliseconds) const;

private:
  ImageAsset() = default;

  std::vector<Frame> m_frames;
  int m_width = 0;
  int m_height = 0;
  float m_totalDurationMs = 0.0f;
  int m_repetitionCount = kInfinite;
};

} // namespace sigil::image
