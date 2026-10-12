#pragma once

/** @file
 * @ingroup geometry-device
 *
 * The one GPU device 2D and 3D share, and the lock that keeps their
 * submissions in one stream.
 */

#include <sigilcore/hardware/Handle.h>

#include <filesystem>
#include <memory>
#include <string>

namespace Diligent {
struct IRenderDevice;
struct IDeviceContext;
struct ITexture;
struct IPipelineStateCache;
}  // namespace Diligent

namespace sigil::core::hardware {
class GpuDevice;
}  // namespace sigil::core::hardware

namespace sigil::skia {
class GraphiteContext;
}  // namespace sigil::skia

/** THE ONE GPU DEVICE 2D AND 3D SHARE. Diligent creates the Vulkan
 *  device and cannot attach to one that already exists, so the single
 *  point where a device is made has to sit at or below every consumer of
 *  one: a point operator's device executor, a mesh painter's device draw
 *  and a frame runtime's passes all stand on the device created here and
 *  the hardware device adopted from it, and none of them could create it
 *  for the others. The lock that keeps both APIs' submissions in one
 *  stream is here for the same reason. */
namespace sigil::geometry::device {

/** What a device is asked for when it is created. */
struct DeviceConfig {
  /** Turn the backend's validation layers on. They cost every call and
   *  report on standard error, so they are off unless asked for. */
  bool validation = false;
  /** WHERE THE PIPELINES BUILT ON THE DEVICE ARE KEPT between runs: the
   *  driver's own cache of them, read when the device is made and written
   *  when it goes, so a pipeline a run built is not built again from
   *  nothing by the next. Empty keeps nothing. The driver discards what
   *  another device or driver wrote. */
  std::filesystem::path pipelineStore;
};

/**
 * One Vulkan device for 2D and 3D: Diligent creates it, SigilSkia adopts
 * it.
 *
 * Diligent creates the device because this build of it cannot attach to
 * a device that already exists and has no Metal backend. SigilSkia then
 * adopts the Vulkan device, queue and loader entry points Diligent made,
 * so a texture named on `gpu()` is an image both APIs can reach: 2D
 * drawing through `graphite()` lands in it and a 3D pass samples it,
 * with no copy in either direction and one handle table naming both.
 *
 * `gpu()` and `graphite()` are null when the adoption failed — a driver
 * without timeline semaphores, for instance, since that is what a
 * SigilSkia fence is. `renderDevice()` and `context()` are unaffected
 * by that: 3D is whole without the shared 2D path.
 */
class Device {
 public:
  /** Null with the reason in @p error when no Vulkan device could be
   *  created — no Vulkan runtime on the machine, most often. A failure
   *  to ADOPT the device is not a failure to create it: the device
   *  comes back with `gpu()` and `graphite()` null. */
  static std::unique_ptr<Device> create(const DeviceConfig& config,
                                        std::string* error);
  ~Device();
  Device(const Device&) = delete;
  Device& operator=(const Device&) = delete;

  /** The Diligent device and its immediate context. Never null on a
   *  device that was created. This object owns them, and every borrower
   *  is done with them before it goes. */
  Diligent::IRenderDevice* renderDevice() const;
  Diligent::IDeviceContext* context() const;

  /** The adopted device and Skia's Graphite context standing on it, for
   *  painting 2D into a texture a 3D pass reads. Both null together. */
  core::hardware::GpuDevice* gpu() const;
  skia::GraphiteContext* graphite() const;

  /** The driver's cache every pipeline built on this device is made
   *  through, kept where `DeviceConfig::pipelineStore` says; null when the
   *  device keeps none. */
  Diligent::IPipelineStateCache* pipelineStateCache() const;

  /** THE METAL OBJECTS BEHIND THIS DEVICE'S OWN, where the driver runs
   *  Vulkan over Metal and exports what it made: @p texture's
   *  id<MTLTexture>, for a texture the engine made, and the
   *  id<MTLSharedEvent> a fence made on `gpu()` is, both bridged to void*
   *  and neither retained. What a queue a host made on Metal itself — a
   *  window's — needs to sample a texture drawn here and to wait on the
   *  GPU for the drawing to finish. A texture or fence named on `gpu()`
   *  answers its Metal object through `gpu()->exportNative`, and a Metal
   *  texture or event comes in through `gpu()->importNative`.
   *  @silent Null where the driver is not one over Metal or cannot say,
   *  which a caller answers by taking the long way round. */
  void* metalTexture(Diligent::ITexture* texture) const;
  void* metalEvent(core::hardware::FenceHandle fence) const;

  /** Holds the one command queue for as long as it lives.
   *
   *  Graphite's submissions and Diligent's passes go into that one
   *  queue, so later work observes finished textures with no CPU
   *  synchronization — but only while nothing interleaves the two
   *  streams. Every Graphite submit on `graphite()`, and every fence
   *  signal or wait taken on `gpu()`, is therefore made while one of
   *  these is held. Diligent takes the same lock from inside its own
   *  submissions, which is why the lock does not nest. */
  class QueueLock {
   public:
    explicit QueueLock(Device& device);
    ~QueueLock();
    QueueLock(const QueueLock&) = delete;
    QueueLock& operator=(const QueueLock&) = delete;

   private:
    Device* m_device;
  };

 private:
  Device();
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

}  // namespace sigil::geometry::device
