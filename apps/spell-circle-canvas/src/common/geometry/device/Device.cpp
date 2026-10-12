#include <Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <Graphics/GraphicsEngine/interface/PipelineStateCache.h>
#include <Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilgeometry/device/Device.h>
#include <sigilskia/graphite/GraphiteContext.h>

#include <Common/interface/RefCntAutoPtr.hpp>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <system_error>
#include <vector>

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
  /** The driver's pipeline cache, and the file it is kept in. */
  dg::RefCntAutoPtr<dg::IPipelineStateCache> pipelines;
  std::filesystem::path pipelineFile;
};

namespace {

/** THE FILE THE PIPELINE CACHE IS KEPT IN, inside the store a host
 *  named. */
std::filesystem::path pipelineFileIn(const std::filesystem::path& store) {
  return store / "device pipelines.bin";
}

/** What the cache held when the device went, written beside the file and
 *  moved over it, so a run that stops halfway through leaves the last
 *  whole one. */
void keepPipelines(dg::IPipelineStateCache* cache,
                   const std::filesystem::path& file) {
  if (!cache || file.empty()) return;
  dg::RefCntAutoPtr<dg::IDataBlob> blob;
  cache->GetData(&blob);
  if (!blob || blob->GetSize() == 0) return;
  std::error_code error;
  std::filesystem::create_directories(file.parent_path(), error);
  std::filesystem::path partial = file;
  partial += ".partial";
  {
    std::ofstream out(partial, std::ios::binary | std::ios::trunc);
    if (!out) return;
    out.write(static_cast<const char*>(blob->GetConstDataPtr()),
              (std::streamsize)blob->GetSize());
    if (!out) return;
  }
  std::filesystem::rename(partial, file, error);
}

}  // namespace

Device::Device() : m_impl(std::make_unique<Impl>()) {}

Device::~Device() {
  if (!m_impl) return;
  keepPipelines(m_impl->pipelines, m_impl->pipelineFile);
  m_impl->pipelines.Release();
  // Graphite's teardown and the adopted device's both touch the shared
  // queue — the adopted device lets it go idle before it releases what it
  // still names — so they go under the queue lock and ahead of the
  // Diligent objects that own it.
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
  // THE FLOAT MODEL, CHOSEN PER SHADER BEFORE THE DRIVER READS IT. This
  // driver can relax floating point: a square root becomes an
  // approximation and a divide becomes a reciprocal and a multiply, both
  // of which round differently from the same expression on a host. A
  // kernel compiled once for both would then be two answers. So the
  // driver is told to relax ON DEMAND — every shader that does not
  // declare the precise float model is compiled relaxed, which is what a
  // picture wants, and every kernel held to bit identity declares it in
  // its own module and is compiled exact. It is set before the instance
  // exists, which is the only moment it is read, and left alone when
  // something else already set it, so a caller may still pin either end.
  //
  // IT IS A PROCESS-WIDE WRITE, and `setenv` is not safe against a
  // concurrent read of the environment, so this call must run before any
  // other thread reads one. The first device a process brings up is
  // where it lands, which is why it stands at the head of the bring-up
  // rather than beside the instance it affects.
  setenv("MVK_CONFIG_FAST_MATH_ENABLED", "2", /*overwrite=*/0);
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
  // Timestamps: what a frame's stages are timed by when a host asks.
  // OPTIONAL, so a driver without them brings the device up and a timed
  // frame says it could not be.
  engineCI.Features.TimestampQueries = DEVICE_FEATURE_STATE_OPTIONAL;
  // Workers that build a pipeline while the thread that asked goes on
  // drawing. OPTIONAL, so a driver without them builds where it is asked.
  engineCI.Features.AsyncShaderCompilation = DEVICE_FEATURE_STATE_OPTIONAL;
  IRenderDevice* rawDevice = nullptr;
  IDeviceContext* rawContext = nullptr;
#ifdef __APPLE__
  // THE METAL OBJECTS BEHIND THE DEVICE'S OWN, which a window drawing on
  // Metal through a queue of its own samples a picture by. A driver that
  // does not offer them refuses the device, so it is asked for again
  // without them: they cost the window's zero-copy path, not the device.
  const char* const metalObjects[] = {"VK_EXT_metal_objects"};
  engineCI.DeviceExtensionCount = 1;
  engineCI.ppDeviceExtensionNames = metalObjects;
  factory->CreateDeviceAndContextsVk(engineCI, &rawDevice, &rawContext);
  if (!rawDevice || !rawContext) {
    if (rawContext) rawContext->Release();
    if (rawDevice) rawDevice->Release();
    rawDevice = nullptr;
    rawContext = nullptr;
    engineCI.DeviceExtensionCount = 0;
    engineCI.ppDeviceExtensionNames = nullptr;
    factory->CreateDeviceAndContextsVk(engineCI, &rawDevice, &rawContext);
  }
#else
  factory->CreateDeviceAndContextsVk(engineCI, &rawDevice, &rawContext);
#endif
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

  // THE DRIVER'S PIPELINE CACHE, seeded with what the last run kept.
  if (!config.pipelineStore.empty()) {
    const std::filesystem::path file = pipelineFileIn(config.pipelineStore);
    std::vector<char> kept;
    if (std::ifstream in{file, std::ios::binary})
      kept.assign(std::istreambuf_iterator<char>(in),
                  std::istreambuf_iterator<char>());
    PipelineStateCacheCreateInfo cacheInfo;
    cacheInfo.Desc.Name = "device pipelines";
    cacheInfo.Desc.Mode = PSO_CACHE_MODE_LOAD_STORE;
    cacheInfo.pCacheData = kept.empty() ? nullptr : kept.data();
    cacheInfo.CacheDataSize = (Uint32)kept.size();
    rawDevice->CreatePipelineStateCache(cacheInfo, &device->m_impl->pipelines);
    // What another driver wrote is refused; the cache then starts empty.
    if (!device->m_impl->pipelines && !kept.empty()) {
      cacheInfo.pCacheData = nullptr;
      cacheInfo.CacheDataSize = 0;
      rawDevice->CreatePipelineStateCache(cacheInfo,
                                          &device->m_impl->pipelines);
    }
    if (device->m_impl->pipelines) device->m_impl->pipelineFile = file;
  }

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
    // one line covers every device a process brings up — and two threads
    // bringing devices up at once still write it once, which is what the
    // exchange is for. It goes to stderr rather than through a report
    // sink, because every such sink in this tree stands in a library
    // this feature must not link: it sits under every consumer of a
    // device and reaching a material from here would put a material on
    // their link lines too.
    static std::atomic<bool> warned{false};
    if (!warned.exchange(true)) {
      fprintf(stderr, "[geometry] 2D on the 3D device is unavailable: %s\n",
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
Diligent::IPipelineStateCache* Device::pipelineStateCache() const {
  return m_impl->pipelines;
}

Device::QueueLock::QueueLock(Device& device) : m_device(&device) {
  m_device->m_impl->context->LockCommandQueue();
}
Device::QueueLock::~QueueLock() {
  m_device->m_impl->context->UnlockCommandQueue();
}

}  // namespace sigil::geometry::device
