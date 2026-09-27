#include "Device.h"
#include <sigilmedia/advanced/Skia.h>

namespace sigil::media::device {

std::shared_ptr<Context> makeContext() { return nullptr; }

DeviceFrame retainNativeFrame(const AVFrame*) { return {}; }

sk_sp<SkImage> wrapNativeFrame(const DeviceFrame&, skgpu::graphite::Recorder*,
                               Context&) {
  return nullptr;
}

}  // namespace sigil::media::device
