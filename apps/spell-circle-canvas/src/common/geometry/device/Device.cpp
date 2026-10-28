#include <Common/interface/ThreadPool.h>
#include <Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <Graphics/GraphicsEngine/interface/PipelineStateCache.h>
#include <Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h>
#include <sigilcore/report/Report.h>
#include <sigilgeometry/device/Device.h>
#include <sigilgpu/GpuDevice.h>
#include <sigilskia/graphite/GraphiteContext.h>
#ifdef __APPLE__
#include <include/gpu/graphite/Recorder.h>
#include <sigilskia/graphite/TextureImage.h>
#endif

#include <Common/interface/RefCntAutoPtr.hpp>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <system_error>
#include <vector>

#include "AdoptDevice.h"
#include "KeptPipelines.h"

namespace sigil::geometry::device {
namespace dg = Diligent;

struct Device::Impl {
  dg::RefCntAutoPtr<dg::IRenderDevice> device;
  dg::RefCntAutoPtr<dg::IDeviceContext> context;
  /** The device above, adopted, and Graphite standing on it. Declared
   *  after the Diligent objects so they are torn down first: they borrow
   *  the Vulkan device and queue those own. Both are null when the
   *  adoption failed, which leaves the 3D side whole. */
  std::unique_ptr<gpu::GpuDevice> gpu;
  /** WHERE 2D PAINTS ON APPLE: a Metal device and queue on the Metal
   *  device the Vulkan driver runs over, which `graphite` stands on in
   *  place of the Vulkan queue. Null elsewhere, where Graphite shares the
   *  Vulkan queue. */
  std::unique_ptr<gpu::GpuDevice> painter;
  std::unique_ptr<skia::GraphiteContext> graphite;
  /** The two orders between the queues, where they are two. `painted` is
   *  raised on the painter's queue behind everything 2D submitted, and
   *  `paintedHere` is that fence brought over to the Vulkan device, which
   *  waits on it; `sampled` is raised on the Vulkan queue behind every
   *  pass, and `sampledEvent` is the Metal event behind it, which the
   *  painter's next submission waits on. */
  gpu::FenceHandle painted;
  gpu::FenceHandle paintedHere;
  gpu::FenceHandle sampled;
  void* sampledEvent = nullptr;
  /** A HOST'S OWN METAL QUEUE AND GRAPHITE, painting in place of the two
   *  above while installed — a window's — with the fence raised on its
   *  queue and that fence brought over here. The host owns the device and
   *  the context; the fences are this device's. */
  gpu::GpuDevice* hostPainter = nullptr;
  skia::GraphiteContext* hostGraphite = nullptr;
  gpu::FenceHandle hostPainted;
  gpu::FenceHandle hostPaintedHere;

  /** Lets go of the host's painter: its fence and the one brought over,
   *  once the Vulkan queue has passed every wait on it. */
  void withdrawHost() {
    if (!hostPainter) return;
    context->Flush();
    context->WaitForIdle();
    if (hostPaintedHere) gpu->destroyFence(hostPaintedHere);
    if (hostPainted) hostPainter->destroyFence(hostPainted);
    hostPainter = nullptr;
    hostGraphite = nullptr;
    hostPainted = {};
    hostPaintedHere = {};
  }
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

/** What the cache held when the device went, kept in @p file. */
void keepPipelines(dg::IPipelineStateCache* cache,
                   const std::filesystem::path& file) {
  if (!cache || file.empty()) return;
  dg::RefCntAutoPtr<dg::IDataBlob> blob;
  cache->GetData(&blob);
  if (!blob || blob->GetSize() == 0) return;
  (void)writeKeptPipelines(
      file, std::span(static_cast<const char*>(blob->GetConstDataPtr()),
                      blob->GetSize()));
}

}  // namespace

Device::Device() : m_impl(std::make_unique<Impl>()) {}

Device::~Device() {
  if (!m_impl) return;
  // Every pipeline still building is waited for before anything it builds
  // against goes.
  finishPipelines();
  keepPipelines(m_impl->pipelines, m_impl->pipelineFile);
  m_impl->pipelines.Release();
  // Graphite's teardown and the adopted device's both touch the shared
  // queue — the adopted device lets it go idle before it releases what it
  // still names — so they go under the queue lock and ahead of the
  // Diligent objects that own it.
  if (m_impl->gpu) {
    // A host still painting here is let go first; its device and context
    // are its own.
    m_impl->withdrawHost();
    QueueLock lock(*this);
    m_impl->graphite.reset();
    // The painter lets its queue go idle before it goes, and a wait still
    // queued there passes once the Vulkan queue reaches what it waits
    // for, which nothing here holds back.
    m_impl->painter.reset();
    m_impl->gpu.reset();
  }
  // Anything recorded but not submitted is flushed before the context
  // goes, so teardown never leaves commands behind.
  if (m_impl->context) m_impl->context->Flush();
}

std::expected<std::unique_ptr<Device>, core::Error> Device::create(
    const DeviceConfig& config) {
  using namespace dg;
  std::string reason;
  std::string* error = &reason;
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
    return std::unexpected(core::Error{std::move(reason)});
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
    return std::unexpected(core::Error{std::move(reason)});
  }

  std::unique_ptr<Device> device(new Device());
  device->m_impl->device.Attach(rawDevice);
  device->m_impl->context.Attach(rawContext);

  // THE DRIVER'S PIPELINE CACHE, seeded with what the last run kept.
  if (!config.pipelineStore.empty()) {
    const std::filesystem::path file = pipelineFileIn(config.pipelineStore);
    const std::vector<char> kept = readKeptPipelines(file);
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
  // made, adopted by SigilSkia, with Graphite painting into the textures
  // it names — on Apple through Metal, elsewhere on that same queue. A
  // failure here is reported and left behind — the 3D side needs none of
  // it.
  std::string adoptError;
  device->m_impl->gpu = adoptVulkanDevice(device->m_impl->device,
                                          device->m_impl->context, &adoptError);
  if (device->m_impl->gpu) device->paintOnTheDevice(&adoptError);
  if (!device->m_impl->graphite) {
    device->m_impl->painter.reset();
    device->m_impl->gpu.reset();
    // The reason is a property of the machine, not of this device, so
    // one line covers every device a process brings up — and two threads
    // bringing devices up at once still write it once, which is what the
    // exchange is for.
    static std::atomic<bool> warned{false};
    if (!warned.exchange(true)) {
      core::report(
          "SigilGeometry",
          "2D on the 3D device is unavailable: " +
              (adoptError.empty()
                   ? std::string("Graphite declined the adopted device")
                   : adoptError));
    }
  }
  return device;
}

#ifdef __APPLE__

void Device::paintOnTheDevice(std::string* error) {
  Impl& impl = *m_impl;
  // 2D PAINTS ON METAL HERE. Graphite's Vulkan backend reaches Metal
  // through the driver's translation, which restarts a render pass at
  // every draw that reads its destination; on Metal itself that read is
  // the hardware's own. The painter stands on the very Metal device the
  // driver runs over, so a Metal texture the driver keeps behind one of
  // its images is one the painter can draw into.
  void* under = impl.gpu->native().mtlDevice;
  if (!under) {
    *error = "the Vulkan driver names no Metal device under its own";
    return;
  }
  std::string reason;
  auto made = gpu::GpuDevice::createOwned();
  if (!made) reason = std::move(made.error().message);
  std::unique_ptr<gpu::GpuDevice> painter = std::move(made).value_or(nullptr);
  if (!painter || painter->native().mtlDevice != under) {
    *error = painter ? "the system's Metal device is not the one the Vulkan "
                       "driver runs over"
                     : reason;
    return;
  }
  std::unique_ptr<skia::GraphiteContext> graphite =
      skia::GraphiteContext::create(*painter);
  if (!graphite) {
    *error = "Graphite declined the Metal device";
    return;
  }
  // THE TWO FENCES, made once: the painter's, brought over to the Vulkan
  // device once and kept, since every import is a semaphore of its own;
  // and the Vulkan device's, whose Metal event the painter waits on.
  const gpu::FenceHandle painted = painter->createFence();
  const gpu::FenceHandle paintedHere =
      painted ? impl.gpu->importNative(painter->exportNative(painted))
              : gpu::FenceHandle{};
  const gpu::FenceHandle sampled = impl.gpu->createFence();
  void* sampledEvent =
      sampled ? impl.gpu->exportNative(sampled).mtlSharedEvent : nullptr;
  if (!paintedHere || !sampledEvent) {
    *error = "the Vulkan driver cannot bring a Metal event over";
    if (paintedHere) impl.gpu->destroyFence(paintedHere);
    if (sampled) impl.gpu->destroyFence(sampled);
    return;
  }
  impl.painter = std::move(painter);
  impl.graphite = std::move(graphite);
  impl.painted = painted;
  impl.paintedHere = paintedHere;
  impl.sampled = sampled;
  impl.sampledEvent = sampledEvent;
}

void Device::waitForPainting() {
  Impl& impl = *m_impl;
  if (!impl.painter || !impl.paintedHere) return;
  // Raised on each painting queue, behind every submission Graphite made
  // there so far: one value covers every texture painted before it. The
  // device's own queue is waited for as well while a host paints, since a
  // texture adopted before the host came may still be painted there.
  const gpu::FenceValue own = impl.painter->signal(impl.painted);
  const gpu::FenceValue host = impl.hostPainter
                                   ? impl.hostPainter->signal(impl.hostPainted)
                                   : gpu::kFenceInitialValue;
  const QueueLock lock(*this);
  impl.gpu->waitGpu(impl.paintedHere, own);
  if (impl.hostPainter) impl.gpu->waitGpu(impl.hostPaintedHere, host);
}

void Device::holdPaintingUntilSampled() {
  Impl& impl = *m_impl;
  if (!impl.painter || !impl.sampledEvent) return;
  // What was recorded is submitted first, so the signal stands behind it.
  // The context takes the queue's lock itself.
  impl.context->Flush();
  gpu::FenceValue value;
  {
    const QueueLock lock(*this);
    value = impl.gpu->signal(impl.sampled);
  }
  skia::waitBeforeSampling(*impl.graphite->recorder(), impl.sampledEvent,
                           value);
  if (impl.hostGraphite && impl.hostGraphite->recorder())
    skia::waitBeforeSampling(*impl.hostGraphite->recorder(), impl.sampledEvent,
                             value);
}

bool Device::paintWith(gpu::GpuDevice* painter,
                       skia::GraphiteContext* graphite) {
  Impl& impl = *m_impl;
  if (!impl.painter || (painter == nullptr) != (graphite == nullptr))
    return false;
  if (painter) {
    // THE HOST'S QUEUE STANDS ON THE METAL DEVICE THE DRIVER RUNS OVER, or
    // it could not paint what the Vulkan device made.
    void* under = impl.gpu->native().mtlDevice;
    const skgpu::graphite::Recorder* recorder = graphite->recorder();
    if (painter->backend() != gpu::Backend::Metal ||
        painter->native().mtlDevice != under || !recorder ||
        recorder->backend() != skgpu::BackendApi::kMetal ||
        skia::deviceOf(*recorder) != under)
      return false;
  }
  // Everything painted so far, through either queue, is waited for by the
  // Vulkan queue before the painter it was painted through goes.
  waitForPainting();
  impl.withdrawHost();
  if (!painter) return true;
  const gpu::FenceHandle painted = painter->createFence();
  const gpu::FenceHandle paintedHere =
      painted ? impl.gpu->importNative(painter->exportNative(painted))
              : gpu::FenceHandle{};
  if (!paintedHere) {
    if (painted) painter->destroyFence(painted);
    return false;
  }
  impl.hostPainter = painter;
  impl.hostGraphite = graphite;
  impl.hostPainted = painted;
  impl.hostPaintedHere = paintedHere;
  // The host's first painting waits for everything the Vulkan queue was
  // handed before it came, which may still be sampling a texture.
  holdPaintingUntilSampled();
  return true;
}

#else

void Device::paintOnTheDevice(std::string* error) {
  // One queue: Graphite records onto the Vulkan queue the 3D side draws
  // on, so submission order is all the order the two need.
  m_impl->graphite = skia::GraphiteContext::create(*m_impl->gpu);
  if (!m_impl->graphite) *error = "Graphite declined the adopted device";
}

// One queue orders both APIs' work by submission, so neither direction
// needs a wait.
void Device::waitForPainting() {}
void Device::holdPaintingUntilSampled() {}

// 2D paints on the Vulkan queue here, which no host's Metal queue
// replaces.
bool Device::paintWith(gpu::GpuDevice*, skia::GraphiteContext*) {
  return false;
}

#endif

Diligent::IRenderDevice* Device::renderDevice() const { return m_impl->device; }
Diligent::IDeviceContext* Device::context() const { return m_impl->context; }
gpu::GpuDevice* Device::gpu() const { return m_impl->gpu.get(); }
skia::GraphiteContext* Device::graphite() const {
  return m_impl->hostGraphite ? m_impl->hostGraphite : m_impl->graphite.get();
}
void Device::finishPipelines() const {
  if (!m_impl || !m_impl->device) return;
  if (Diligent::IThreadPool* workers =
          m_impl->device->GetShaderCompilationThreadPool())
    workers->WaitForAllTasks();
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
