#include "Device.h"

namespace sigil::video::device {

std::shared_ptr<Context> makeContext(void*) { return nullptr; }

NativeFrame retainNativeFrame(const AVFrame*) { return {}; }

sk_sp<SkImage> wrapNativeFrame(const NativeFrame&, skgpu::graphite::Recorder*,
                               Context&) {
  return nullptr;
}

}  // namespace sigil::video::device
