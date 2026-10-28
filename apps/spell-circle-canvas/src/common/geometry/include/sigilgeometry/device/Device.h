#pragma once

/** @file
 * @ingroup geometry-device
 *
 * The one GPU device 2D and 3D share, and the lock that keeps their
 * submissions in one stream.
 */

#include <sigilcore/report/Error.h>
#include <sigilgpu/Handle.h>

#include <expected>
#include <filesystem>
#include <memory>
#include <string>

namespace Diligent {
struct IRenderDevice;
struct IDeviceContext;
struct ITexture;
struct IPipelineStateCache;
}  // namespace Diligent

namespace sigil::gpu {
class GpuDevice;
}  // namespace sigil::gpu

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
 * WHERE 2D PAINTS DEPENDS ON THE DRIVER. Elsewhere `graphite()` records
 * onto the Vulkan queue the 3D side draws on. On Apple, where the driver
 * runs Vulkan over Metal, `graphite()` is Graphite's METAL backend on a
 * queue of its own on the Metal device the driver runs over, painting
 * the Metal texture the driver keeps behind each image — the same memory
 * — so a page is painted by Metal and sampled by Vulkan with no copy,
 * and the device makes no Vulkan Graphite at all. Two queues then need
 * two orders between them, which `waitForPainting()` and
 * `holdPaintingUntilSampled()` keep on the GPU.
 *
 * `gpu()` and `graphite()` are null when the adoption failed — a driver
 * without timeline semaphores, for instance, since that is what a
 * SigilSkia fence is, or on Apple a driver that cannot name its Metal
 * objects. `renderDevice()` and `context()` are unaffected by that: 3D is
 * whole without the shared 2D path.
 */
class Device {
 public:
  /** The error that says why when no Vulkan device could be created —
   *  no Vulkan runtime on the machine, most often. A failure to ADOPT
   *  the device is not a failure to create it: the device comes back
   *  with `gpu()` and `graphite()` null. */
  static std::expected<std::unique_ptr<Device>, core::Error> create(
      const DeviceConfig& config);
  ~Device();
  Device(const Device&) = delete;
  Device& operator=(const Device&) = delete;

  /** The Diligent device and its immediate context. Never null on a
   *  device that was created. This object owns them, and every borrower
   *  is done with them before it goes. */
  Diligent::IRenderDevice* renderDevice() const;
  Diligent::IDeviceContext* context() const;

  /** The adopted device and Skia's Graphite context painting into the
   *  textures it names, for painting 2D into a texture a 3D pass reads —
   *  on Apple a Metal context, the device's own or the one a host handed
   *  `paintWith`, elsewhere one on the Vulkan queue. Both null
   *  together. */
  gpu::GpuDevice* gpu() const;
  skia::GraphiteContext* graphite() const;

  /** THE 3D QUEUE WAITS FOR THE 2D PAINTING: nothing submitted to the
   *  Vulkan queue after this runs until everything `graphite()` submitted
   *  before it has finished on the GPU. One call covers every texture
   *  painted so far, so a frame makes it once, before its first pass
   *  samples one; a wait queued while that pass is still being recorded
   *  holds it too, since nothing recorded is submitted until later.
   *  Nothing where 2D shares the Vulkan queue, whose submission order is
   *  the order. Takes the queue lock; call it without one held. */
  void waitForPainting();
  /** …AND THE NEXT 2D PAINTING WAITS FOR THE 3D QUEUE: everything
   *  recorded on the device so far is submitted, and the next submission
   *  `graphite()` makes through its own recorder waits on the GPU until
   *  all of it has finished. A frame makes it after its last pass that
   *  sampled a painted texture, because the next painting repaints a part
   *  of that very texture in place, over what a pass may still be
   *  reading. Nothing where 2D shares the Vulkan queue. Takes the queue
   *  lock; call it without one held. */
  void holdPaintingUntilSampled();

  /** A HOST'S OWN METAL QUEUE PAINTS THE 2D from here on: @p painter, a
   *  Metal device on the Metal device the Vulkan driver runs over — a
   *  window's, adopted — and @p graphite, Graphite on it, are what
   *  `graphite()` answers in place of the device's own, so a window and
   *  the pages it shows stand on one context and each draws the other's
   *  images as they stand. Both orders between the queues cover the
   *  host's queue too, and the device's own while a texture adopted
   *  before the host came still paints there. Null for both gives the
   *  device's own back. False, with nothing changed, where 2D does not
   *  paint on Metal here, where only one of the two is given, or where
   *  they stand on another Metal device. Both must outlive their use
   *  here: a host withdraws them before either goes, and the device
   *  waits for its queue to drain when one is withdrawn. Call it from the
   *  thread that paints, without the queue lock held. */
  bool paintWith(gpu::GpuDevice* painter, skia::GraphiteContext* graphite);

  /** The driver's cache every pipeline built on this device is made
   *  through, kept where `DeviceConfig::pipelineStore` says; null when the
   *  device keeps none. */
  Diligent::IPipelineStateCache* pipelineStateCache() const;

  /** WAITS FOR EVERY PIPELINE STILL BUILDING on the device's workers.
   *  A pipeline asked for ahead of the frame that draws with it builds on
   *  a worker while the thread that asked goes on; one still building
   *  when the process ends runs the driver's compiler while the process
   *  tears down the compiler's own statics around it, and faults. The
   *  device waits for them when it goes; a process that keeps its device
   *  for its whole life calls this before it ends. */
  void finishPipelines() const;

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
  void* metalEvent(gpu::FenceHandle fence) const;

  /** Holds the one Vulkan command queue for as long as it lives.
   *
   *  Diligent's passes go into that queue, and where 2D shares it
   *  Graphite's submissions do too, so later work observes finished
   *  textures with no CPU synchronization — but only while nothing
   *  interleaves the two streams. Every Graphite submit on a `graphite()`
   *  that shares the queue, and every fence signal or wait taken on
   *  `gpu()`, is therefore made while one of these is held; a Graphite
   *  submit on Apple goes to a Metal queue and needs none. Diligent takes
   *  the same lock from inside its own submissions, which is why the lock
   *  does not nest. */
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
  /** Stands `graphite()` up where this platform paints 2D, over the
   *  adopted `gpu()`; leaves it null with the reason in @p error. */
  void paintOnTheDevice(std::string* error);
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

}  // namespace sigil::geometry::device
