#pragma once

/** @file
 * The SigilVideo decode surface. Encoded bytes remain owned by a Video,
 * compressed packets are decoded on demand, and a small presentation-frame
 * cache follows the playhead. Device frames remain native until a caller asks
 * for a raster image. A Graphite recorder lets the device executor wrap the
 * native planes directly for GPU composition.
 */

#include <include/core/SkImage.h>
#include <include/core/SkSamplingOptions.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "sigilvideo/Types.h"

class SkCanvas;
class SkRect;

namespace skgpu::graphite {
class Recorder;
}

namespace sigil::video {

struct DecodeOptions {
  HardwarePreference hardware = HardwarePreference::Preferred;
  size_t cachedFrames = 4;

  /** Native Metal device used by the destination Graphite recorder. Null
   *  selects the system device. Ignored outside the VideoToolbox executor. */
  void* metalDevice = nullptr;

  bool operator==(const DecodeOptions&) const = default;
};

struct VideoProbe {
  int width = 0;
  int height = 0;
  double durationSeconds = 0.0;
  double frameRate = 0.0;
  int64_t frameCount = 0;
  std::string codec;
  std::string container;
  bool hasAudio = false;
  bool hasAlpha = false;
};

struct VideoFrame {
  sk_sp<SkImage> image;
  NativeFrame native;
  double presentationSeconds = 0.0;
  double durationSeconds = 0.0;
  int64_t index = 0;
  bool hardwareDecoded = false;
  bool hasAlpha = false;

  explicit operator bool() const { return image != nullptr || native; }
};

/** A seekable, on-demand decoder over one encoded video. Not thread-safe. */
class Video {
 public:
  ~Video();
  Video(const Video&) = delete;
  Video& operator=(const Video&) = delete;

  const VideoProbe& probe() const;

  /** Whether the decoder holds a platform hardware configuration. True from
   *  the moment a hardware decoder opens, which is before the device has
   *  been asked for a decompression session: a device grants a limited
   *  number of them at once, and a refusal arrives on the first decode and
   *  clears this. Frames then arrive through the software path, or not at
   *  all under `HardwarePreference::Required`. */
  bool hardwareConfigured() const;

  /** Whether the most recently decoded frame arrived as a native device
   *  surface. False until a frame has been decoded, so a caller that needs
   *  the fact rather than the intent decodes one frame first. */
  bool hardwareDecoding() const;

  /** The frame covering @p seconds. Passing a Graphite recorder enables
   *  direct device-plane composition when the decoder produced one. */
  VideoFrame frameAt(double seconds,
                     skgpu::graphite::Recorder* recorder = nullptr);

  /** Decodes the frame covering @p seconds without binding it to a graphics
   *  recorder. Hardware frames retain only their native surface; software and
   *  alpha frames carry an immutable raster image. This is the worker-thread
   *  half of asynchronous presentation. */
  VideoFrame decodeAt(double seconds);

  /** Draws the current frame into @p destination. Looping normalizes time by
   *  the probed duration; a non-looping draw clamps to the final frame. */
  bool draw(SkCanvas& canvas, const SkRect& destination, double seconds,
            const SkSamplingOptions& sampling =
                SkSamplingOptions(SkFilterMode::kLinear),
            bool loop = true);

 private:
  struct Impl;
  explicit Video(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> m_impl;

  friend std::shared_ptr<Video> decodeVideo(const std::byte*, size_t,
                                            const DecodeOptions&,
                                            const std::filesystem::path&);
};

/** Opens encoded bytes and prepares their best video stream. Null when the
 *  container or codec cannot be opened under the requested hardware policy. */
std::shared_ptr<Video> decodeVideo(const std::byte* bytes, size_t size,
                                   const DecodeOptions& options = {},
                                   const std::filesystem::path& pathHint = {});

/** Container and stream metadata without retaining a decoder. */
std::optional<VideoProbe> probeVideo(
    const std::byte* bytes, size_t size,
    const std::filesystem::path& pathHint = {});

}  // namespace sigil::video
