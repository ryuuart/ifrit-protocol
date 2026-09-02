#include <sigilgeometry/device/Device.h>

#include <Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilskia/graphite/GraphiteContext.h>

#include <Common/interface/RefCntAutoPtr.hpp>
#include <cstdio>
#include <cstdlib>

#include "AdoptDevice.h"

namespace sigil::geometry::device {
namespace dg = Diligent;

struct Device::Impl {
  dg::RefCntAutoPtr<dg::IRenderDevice> device;
  dg::RefCntAutoPtr<dg::IDeviceContext> context;
  /** The device above, adopted, and Graphite standing on it. Declared
   *  after the Diligent objects so they are torn down first: they borrow
   *  the Vulkan device and queue those own. Both are null when the
   *  adoption failed, which leaves the 3D side whole. */
  std::unique_ptr<core::hardware::GpuDevice> gpu;
  std::unique_ptr<skia::GraphiteContext> graphite;
};

Device::Device() : m_impl(std::make_unique<Impl>()) {}

Device::~Device() {
  if (!m_impl) return;
  // Graphite's teardown and the adopted device's both touch the shared
  // queue, so they go under the queue lock and ahead of the Diligent
  // objects that own it.
  if (m_impl->gpu) {
    QueueLock lock(*this);
    m_impl->graphite.reset();
    m_impl->gpu.reset();
  }
  // Anything recorded but not submitted is flushed before the context
  // goes, so teardown never leaves commands behind.
  if (m_impl->context) m_impl->context->Flush();
}

std::unique_ptr<Device> Device::create(const DeviceConfig& config,
                                       std::string* error) {
  using namespace dg;
  // THE FLOAT MODEL, PINNED BEFORE THE DRIVER READS IT. This driver
  // relaxes floating point by default: a square root becomes an
  // approximation and a divide becomes a reciprocal and a multiply, both
  // of which round differently from the same expression on a host. A
  // kernel compiled once for both is then two answers, so the relaxation
  // is turned off here — before the instance exists, which is the only
  // moment it is read — and left alone when something else already set
  // it, so a caller may still ask for the faster arithmetic.
  setenv("MVK_CONFIG_FAST_MATH_ENABLED", "0", /*overwrite=*/0);
  IEngineFactoryVk* factory = GetEngineFactoryVk();
  if (!factory) {
    if (error) *error = "Diligent Vulkan factory unavailable";
    return nullptr;
  }
  EngineVkCreateInfo engineCI;
  if (config.validation) engineCI.SetValidationLevel(VALIDATION_LEVEL_1);
  // Timeline semaphores: what a SigilSkia fence is, and the one thing
  // the adopted device needs beyond what the 3D side asks for. OPTIONAL
  // rather than ENABLED, so a driver without them still brings the
  // device up — it costs the shared 2D path, not the 3D one.
  engineCI.Features.NativeFence = DEVICE_FEATURE_STATE_OPTIONAL;
  IRenderDevice* rawDevice = nullptr;
  IDeviceContext* rawContext = nullptr;
  factory->CreateDeviceAndContextsVk(engineCI, &rawDevice, &rawContext);
  if (!rawDevice || !rawContext) {
    if (error)
      *error =
          "Vulkan device creation failed (is MoltenVK installed? "
          "brew install molten-vk vulkan-loader)";
    return nullptr;
  }

  std::unique_ptr<Device> device(new Device());
  device->m_impl->device.Attach(rawDevice);
  device->m_impl->context.Attach(rawContext);

  // One device for 2D and 3D: the Vulkan device and queue Diligent just
  // made, adopted by SigilSkia, with Graphite recording onto that same
  // queue. A failure here is reported and left behind — the 3D side
  // needs none of it.
  std::string adoptError;
  device->m_impl->gpu = adoptVulkanDevice(device->m_impl->device,
                                          device->m_impl->context, &adoptError);
  if (device->m_impl->gpu)
    device->m_impl->graphite =
        skia::GraphiteContext::create(*device->m_impl->gpu);
  if (!device->m_impl->graphite) {
    device->m_impl->gpu.reset();
    // The reason is a property of the machine, not of this device, so
    // one line covers every device a process brings up.
    static bool warned = false;
    if (!warned) {
      warned = true;
      fprintf(stderr, "[world] 2D on the 3D device is unavailable: %s\n",
              adoptError.empty() ? "Graphite declined the adopted device"
                                 : adoptError.c_str());
    }
  }
  return device;
}

Diligent::IRenderDevice* Device::renderDevice() const { return m_impl->device; }
Diligent::IDeviceContext* Device::context() const { return m_impl->context; }
core::hardware::GpuDevice* Device::gpu() const { return m_impl->gpu.get(); }
skia::GraphiteContext* Device::graphite() const {
  return m_impl->graphite.get();
}

Device::QueueLock::QueueLock(Device& device) : m_device(&device) {
  m_device->m_impl->context->LockCommandQueue();
}
Device::QueueLock::~QueueLock() {
  m_device->m_impl->context->UnlockCommandQueue();
}

}  // namespace sigil::geometry::device
