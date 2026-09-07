#pragma once

/** @file Types shared by the independent video decoder and encoder. */

#include <cstddef>
#include <memory>

namespace sigil::video {

/** Whether a codec may use a platform video device. */
enum class HardwarePreference {
  Disabled,
  Preferred,
  Required,
};

/** A platform frame kept alive for a host compositor or display overlay. */
struct NativeFrame {
  enum class Kind {
    None,
    VideoToolboxPixelBuffer,
  };

  Kind kind = Kind::None;
  std::shared_ptr<void> storage;
  int width = 0;
  int height = 0;

  enum class YuvMatrix {
    Rec601,
    Rec709,
    Bt2020,
  };
  YuvMatrix yuvMatrix = YuvMatrix::Rec601;
  bool fullRange = false;

  explicit operator bool() const { return storage != nullptr; }
  void* handle() const { return storage.get(); }
};

}  // namespace sigil::video
