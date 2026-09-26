#pragma once

/** @file
 * @ingroup media-core
 * A FRAME THAT STANDS ON A DEVICE: the platform surface a hardware decode
 * produced, or a texture another renderer painted, carried unexamined to
 * whoever can bind it where it stands — and `deviceImage()`, the one call
 * that turns such a frame into an image a canvas draws. Hosts, executors
 * and the drawing libraries reach for this; a sketch draws a `Frame`
 * through a leaf or a pen, which call it.
 */

#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>

#include <cstdint>
#include <memory>

namespace skgpu::graphite {
class Recorder;
}  // namespace skgpu::graphite

namespace sigil::media {

struct Frame;

/** HOW A FRAME STANDING ON A DEVICE BECOMES AN IMAGE: made by the source
 *  that produced the frame, which alone knows what its surface is.
 *  `image(recorder)` wraps the surface for @p recorder where it stands,
 *  and with no recorder reads it back into host memory. An
 *  implementation keeps what it made, so a frame drawn in several places
 *  is bound once per recorder. */
class DeviceBinding {
 public:
  virtual ~DeviceBinding() = default;
  /** The frame as an image @p recorder draws, or read back to the CPU
   *  when @p recorder is null; null when neither can be made. */
  virtual sk_sp<SkImage> image(skgpu::graphite::Recorder* recorder) = 0;
};

/** WHERE A FRAME'S PIXELS STAND WHEN THEY STAND ON A DEVICE. Nothing in
 *  this library reads the surface itself: it is carried to a renderer on
 *  the same device, and `binding` is what makes it an image anywhere
 *  else. */
struct DeviceFrame {
  /** Which surface the frame is. */
  enum class Kind {
    None,         ///< No device surface; the frame is a raster image only.
    PixelBuffer,  ///< A platform video buffer — on Apple a `CVPixelBuffer` in `storage`.
    Texture,      ///< A graphics API texture named by `device`, `pointer` and `handle`.
  };
  /** The colour matrix a planar surface's luma and chroma were coded in. */
  enum class YuvMatrix { Bt601, Bt709, Bt2020 };

  Kind kind = Kind::None;
  /** Holds the surface alive for as long as any copy of this value
   *  stands; for a `PixelBuffer`, `storage.get()` is the buffer. */
  std::shared_ptr<void> storage;
  /** A `Texture`'s device, and the texture itself as the graphics API's
   *  own object bridged to opaque values, with its format and layout. */
  const void* device = nullptr;
  const void* pointer = nullptr;
  uint64_t handle = 0;
  uint32_t format = 0;
  uint32_t layout = 0;
  int width = 0;
  int height = 0;
  YuvMatrix yuvMatrix = YuvMatrix::Bt601;
  /** Whether a planar surface's samples span the full range rather than
   *  the studio range the matrix otherwise implies. */
  bool fullRange = false;
  /** What turns this surface into an image; null for a surface that
   *  only a renderer on its own device can use. */
  std::shared_ptr<DeviceBinding> binding;

  /** Whether a device surface is held. */
  explicit operator bool() const { return kind != Kind::None; }
};

/** Which way a decoder took the platform's video device. */
struct HardwareUse {
  /** The decoder holds a hardware configuration: true from the moment it
   *  opens, before the device has granted a decompression session. */
  bool configured = false;
  /** The most recently decoded frame arrived as a device surface — the
   *  fact, where `configured` is the intent. */
  bool decoding = false;
};

/** THE FRAME AS AN IMAGE @p recorder DRAWS: its raster image when it has
 *  one, its device surface bound for @p recorder when it has that, and
 *  that surface read back to host memory when @p recorder is null. Null
 *  for an empty frame. What a leaf, a pen and a texture call; a sketch
 *  draws a frame through one of them. */
sk_sp<SkImage> deviceImage(const Frame& frame,
                           skgpu::graphite::Recorder* recorder);

}  // namespace sigil::media
