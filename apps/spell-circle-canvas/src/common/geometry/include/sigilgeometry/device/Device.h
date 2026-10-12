#pragma once

/** @file
 * @ingroup geometry-device
 *
 * The one GPU device 2D and 3D share, and the lock that keeps their
 * submissions in one stream.
 */

#include <sigilcore/hardware/Fence.h>
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
struct TextureDescription;
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
   *  id<MTLTexture>, and the id<MTLSharedEvent> a fence made on `gpu()`
   *  is, both bridged to void* and neither retained. What a queue a host
   *  made on Metal itself — a window's — needs to sample a texture drawn
   *  here and to wait on the GPU for the drawing to finish.
   *  @silent Null where the driver is not one over Metal or cannot say,
   *  which a caller answers by taking the long way round. */
  void* metalTexture(Diligent::ITexture* texture) const;
  void* metalEvent(core::hardware::FenceHandle fence) const;
  /** …and the same for a texture named on `gpu()`, which is how a
   *  texture 2D paints into is named: what a Graphite context on Metal
   *  paints through for a texture this device made. Null for a stale
   *  handle, besides everything the two above are null for. */
  void* metalTexture(core::hardware::TextureHandle texture) const;

  /** A TEXTURE METAL MADE, NAMED ON `gpu()` WITH NO COPY: @p mtlTexture —
   *  an id<MTLTexture> bridged to void*, made on the Metal device the
   *  driver runs over — becomes a Vulkan image over the very same
   *  memory, so a pass on this device samples what a queue on Metal drew
   *  into it. @p description states the texture as Metal made it: its
   *  size, format and levels, and its usage, which bounds what this side
   *  may do with it — a texture made without render-target usage cannot
   *  be drawn into here either. The handle owns the image and not the
   *  texture; destroying it lets go of the image, and the texture lives
   *  as long as its maker keeps it.
   *  @silent A null handle where there is no `gpu()`, where the driver is
   *  not one over Metal or cannot import, and where the texture is
   *  missing. */
  core::hardware::TextureHandle importMetalTexture(
      void* mtlTexture,
      const core::hardware::TextureDescription& description) const;

  /** A WAIT ON THIS DEVICE'S QUEUE FOR WORK A QUEUE ON METAL DID: nothing
   *  submitted to the queue after this runs until @p mtlSharedEvent — an
   *  id<MTLSharedEvent> bridged to void*, raised by that other queue —
   *  reaches @p value. It is `metalEvent()` the other way round: there
   *  this device signals and a Metal queue waits, here a Metal queue
   *  signals and this device waits, so what that queue painted is
   *  sampled here only once it is painted and no thread waits for it.
   *  Each event is brought over once and kept for as long as the device
   *  lives, so a timeline raised every frame costs one crossing.
   *  @trap Made while a `QueueLock` is held, like every wait on `gpu()`.
   *  The value must be raised by the other queue: nothing queued here
   *  after the wait runs until it is.
   *  @silent False, with nothing queued, where there is no `gpu()` or the
   *  driver is not one over Metal or cannot bring the event over. */
  bool waitForMetalEvent(void* mtlSharedEvent,
                         core::hardware::FenceValue value);

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
