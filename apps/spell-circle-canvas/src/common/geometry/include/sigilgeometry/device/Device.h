#pragma once

/** @file
 * The one GPU device 2D and 3D share, and the lock that keeps their
 * submissions in one stream.
 */

#include <memory>
#include <string>

namespace Diligent {
struct IRenderDevice;
struct IDeviceContext;
}  // namespace Diligent

namespace sigil::core::hardware {
class GpuDevice;
}  // namespace sigil::core::hardware

namespace sigil::skia {
class GraphiteContext;
}  // namespace sigil::skia

namespace sigil::geometry::device {

/** What a device is asked for when it is created. */
struct DeviceConfig {
  /** Turn the backend's validation layers on. They cost every call and
   *  report on standard error, so they are off unless asked for. */
  bool validation = false;
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
