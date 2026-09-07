// GraphiteContext::create(GpuDevice&): the one factory that reads a
// hardware device rather than raw handles. Graphite is what stands on a
// device, so the bring-up over one lives here and the hardware feature
// below stays free of Skia.

#include <sigilcore/hardware/GpuDevice.h>
#include <sigilskia/graphite/GraphiteContext.h>

namespace sigil::skia {

std::unique_ptr<GraphiteContext> GraphiteContext::create(
    core::hardware::GpuDevice& device) {
  using core::hardware::Backend;
  const core::hardware::NativeDevice& native = device.native();
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
