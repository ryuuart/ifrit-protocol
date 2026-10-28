// GraphiteContext::create(GpuDevice&): the one factory that reads a
// hardware device rather than raw handles. Graphite is what stands on a
// device, so the bring-up over one lives here and SigilGpu below stays
// free of Skia.

#include <sigilgpu/GpuDevice.h>
#include <sigilskia/graphite/GraphiteContext.h>

namespace sigil::skia {

std::unique_ptr<GraphiteContext> GraphiteContext::create(
    gpu::GpuDevice& device) {
  using gpu::Backend;
  const gpu::NativeDevice& native = device.native();
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
