/** @file
 * A frame made drawable: its raster image, or its device surface bound
 * through the binding the source that produced it attached.
 */

#include "sigilmedia/advanced/Skia.h"

#include "sigilmedia/core/Frame.h"

namespace sigil::media {

sk_sp<SkImage> deviceImage(const Frame& frame,
                           skgpu::graphite::Recorder* recorder) {
  if (frame.image) return toSk(frame.image);
  if (frame.device.binding) return frame.device.binding->image(recorder);
  return nullptr;
}

}  // namespace sigil::media
