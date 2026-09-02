// GraphiteContext::create(GpuDevice&): the one factory that reads a
// device rather than raw handles. It lives in the device feature so the
// graphite feature stays below it — a GpuDevice is what the caller
// holds, so the caller links this feature already.

#include <sigilskia/device/GpuDevice.h>
#include <sigilskia/graphite/GraphiteContext.h>

namespace sigil::skia {

std::unique_ptr<GraphiteContext> GraphiteContext::create(GpuDevice& device) {
  const NativeDevice& native = device.native();
  switch (native.backend) {
    case Backend::Metal:
#ifdef __APPLE__
      return createMetal(native.mtlDevice, native.mtlCommandQueue);
#else
      return nullptr;
#endif
    case Backend::Vulkan:
      return createVulkan(native.vulkan);
  }
  return nullptr;
}

}  // namespace sigil::skia
