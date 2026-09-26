#pragma once

/** @file
 * The device executor's seam, private to the decode feature: a platform
 * video buffer retained off a decoded frame, and wrapped as an image a
 * Graphite recorder composites where it stands. A stub answers nothing
 * off Apple platforms.
 */

#include <include/core/SkRefCnt.h>

#include <memory>

#include "sigilmedia/advanced/Device.h"

struct AVFrame;
class SkImage;

namespace skgpu::graphite {
class Recorder;
}

namespace sigil::media::device {

/** State shared by every native frame wrapped on one device. */
class Context {
 public:
  virtual ~Context() = default;
};

/** The context for this machine's video device; null where there is
 *  none. One per device, shared by every decoder on it. */
std::shared_ptr<Context> makeContext();
/** The platform buffer behind a hardware-decoded @p frame, retained. */
DeviceFrame retainNativeFrame(const AVFrame* frame);
/** @p frame's planes as one image @p recorder composites; null when the
 *  surface or the recorder cannot take it. */
sk_sp<SkImage> wrapNativeFrame(const DeviceFrame& frame,
                               skgpu::graphite::Recorder* recorder,
                               Context& context);

}  // namespace sigil::media::device
