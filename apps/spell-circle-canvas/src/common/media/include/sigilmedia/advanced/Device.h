#pragma once

/** @file
 * @ingroup media-core
 * A FRAME THAT STANDS ON A DEVICE: the platform surface a hardware decode
 * produced, or a texture another renderer painted, carried unexamined to
 * whoever can bind it where it stands. The binding that turns such a
 * frame into an image a canvas draws, and `deviceImage()`, the one call
 * that does it, speak the renderer and stand in `advanced/Skia.h`. Hosts,
 * executors and the drawing libraries reach for this; a sketch draws a
 * `Frame` through a leaf or a pen, which call it.
 */

#include <cstdint>
#include <memory>

namespace sigil::media {

struct Frame;

/** How a frame standing on a device becomes an image: made by the source
 *  that produced the frame, and declared with the renderer it answers in,
 *  in `advanced/Skia.h`. */
class DeviceBinding;

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

}  // namespace sigil::media
