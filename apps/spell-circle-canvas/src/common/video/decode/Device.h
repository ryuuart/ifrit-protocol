#pragma once

#include <include/core/SkRefCnt.h>

#include <memory>

#include "sigilvideo/Types.h"

struct AVFrame;
class SkImage;

namespace skgpu::graphite {
class Recorder;
}

namespace sigil::video::device {

/** State shared by every native frame wrapped for one decoder. */
class Context {
 public:
  virtual ~Context() = default;
};

std::shared_ptr<Context> makeContext(void* metalDevice);
NativeFrame retainNativeFrame(const AVFrame* frame);
sk_sp<SkImage> wrapNativeFrame(const NativeFrame& frame,
                               skgpu::graphite::Recorder* recorder,
                               Context& context);

}  // namespace sigil::video::device
