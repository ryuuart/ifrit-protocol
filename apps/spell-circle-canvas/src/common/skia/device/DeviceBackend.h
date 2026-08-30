#pragma once
// The per-API half of GpuDevice, private to the feature: what a backend
// must do with native objects for the device to name them by handle.
// GpuDevice.cpp owns every handle table and the deferred-destroy queue
// and calls down here for the native work; each API implements this
// once (GpuDeviceMetal.mm today).

#include <sigilskia/device/Fence.h>
#include <sigilskia/device/GpuDevice.h>

#include <chrono>
#include <cstdint>

namespace sigil::skia {

class GpuDevice::Backend_ {
 public:
  virtual ~Backend_() = default;

  virtual const NativeDevice& native() const = 0;

  /** A native texture the backend owns (+1 reference), or empty. */
  virtual NativeTexture createTexture(const TextureDesc& desc) = 0;
  /** Takes a +1 reference on a host texture so it survives the host
   *  dropping it, or does nothing when the device is not to own it. */
  virtual void retainTexture(const NativeTexture& texture) = 0;
  /** Drops one reference. */
  virtual void releaseTexture(const NativeTexture& texture) = 0;

  /** An opaque native fence at kFenceInitialValue. */
  virtual void* createFence() = 0;
  virtual void destroyFence(void* fence) = 0;
  /** Queues a signal to @p value behind everything submitted so far. */
  virtual void signalFence(void* fence, FenceValue value) = 0;
  /** Queues a wait for @p value ahead of everything submitted after. */
  virtual void waitFenceGpu(void* fence, FenceValue value) = 0;
  /** Blocks for @p value or the timeout. */
  virtual bool waitFenceCpu(void* fence, FenceValue value,
                            std::chrono::milliseconds timeout) = 0;
  virtual FenceValue completedValue(void* fence) = 0;
};

/** The Metal implementation; null when there is no Metal device. */
std::unique_ptr<GpuDevice::Backend_> createMetalBackend(
    const NativeDevice& native, bool owned);

}  // namespace sigil::skia
