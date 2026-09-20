#pragma once

/** @file
 * Types shared by the independent video decoder and encoder.
 */

#include <cstddef>
#include <memory>

/** Video meaning for a Skia application: encoded container bytes open as a
 *  seekable clip whose frames decode around a playhead, and pixels travel
 *  the other way through an incremental encoder that finishes as container
 *  bytes. Reach for it to draw a clip into a canvas, to compose one on the
 *  GPU, or to write frames out as a file's worth of bytes. Nothing here
 *  opens a file or resolves a name: a caller hands in the encoded bytes and
 *  stores the encoded bytes it gets back. */
namespace sigil::video {

/** Whether a codec may use a platform video device. */
enum class HardwarePreference {
  Disabled,   ///< Decode and encode on the CPU whatever the platform offers.
  Preferred,  ///< Take the device when it opens, fall back to the CPU.
  Required,   ///< Fail rather than fall back to the CPU.
};

/** A platform frame kept alive for a host compositor or display overlay. */
struct NativeFrame {
  /** Which platform surface `storage` holds. */
  enum class Kind {
    None,                     ///< No native surface; the frame is raster only.
    VideoToolboxPixelBuffer,  ///< An Apple `CVPixelBuffer`.
  };

  Kind kind = Kind::None;
  /** The surface itself, type-erased and kept alive for as long as any
   *  holder of this value needs it. */
  std::shared_ptr<void> storage;
  int width = 0;
  int height = 0;

  /** The colour matrix the stream's luma and chroma planes were coded in. */
  enum class YuvMatrix {
    Rec601,
    Rec709,
    Bt2020,
  };
  YuvMatrix yuvMatrix = YuvMatrix::Rec601;
  /** Whether the coded samples span the full range rather than the studio
   *  range the matrix otherwise implies. */
  bool fullRange = false;

  /** Whether a native surface is held. */
  explicit operator bool() const { return storage != nullptr; }
  /** The surface as an untyped pointer, for the platform call that knows
   *  what `kind` means. Null when no surface is held. */
  void* handle() const { return storage.get(); }
};

}  // namespace sigil::video
