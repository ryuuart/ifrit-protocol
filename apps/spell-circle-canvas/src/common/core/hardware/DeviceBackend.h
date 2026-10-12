#pragma once
// The per-API half of GpuDevice, private to the feature: what a backend
// must do with native objects for the device to name them by handle.
// GpuDevice.cpp owns every handle table and the deferred-destroy queue
// and calls down here for the native work; each API implements this
// once (GpuDeviceMetal.mm for Metal).

#include <sigilcore/hardware/Fence.h>
#include <sigilcore/hardware/GpuDevice.h>

#include <chrono>
#include <cstdint>
#include <string>

namespace sigil::core::hardware {

/** The level count a description actually gets: what it asked for, held
 *  to at least one and to no more than the size can carry. Every backend
 *  needs the same number, so it is derived once here rather than twice
 *  from two spellings of the same rule. */
inline int clampedMipLevels(const TextureDescription& desc) {
  const int possible = mipLevelsFor(desc.width, desc.height);
  return desc.mipLevels < 1
             ? 1
             : (desc.mipLevels > possible ? possible : desc.mipLevels);
}

class GpuDevice::DeviceBackend {
 public:
  virtual ~DeviceBackend() = default;

  virtual const NativeDevice& native() const = 0;

  /** A native texture the backend owns (+1 reference), or empty. */
  virtual NativeTexture createTexture(const TextureDescription& desc) = 0;
  /** What a handle over a host's @p texture names: the texture itself
   *  when it is this API's, with whatever the backend adds to it, or an
   *  object the backend made over a texture of another API, which
   *  @p made then reports so the handle owns it. Empty when the backend
   *  cannot take the texture. */
  virtual NativeTexture importTexture(const NativeTexture& texture,
                                      bool* made) = 0;
  /** Takes a +1 reference on a host texture so it survives the host
   *  dropping it, or does nothing when the device is not to own it. */
  virtual void retainTexture(const NativeTexture& texture) = 0;
  /** Drops one reference. */
  virtual void releaseTexture(const NativeTexture& texture) = 0;

  /** An opaque native fence at kFenceInitialValue. */
  virtual void* createFence() = 0;
  /** The opaque native fence a host's @p fence is waited on through:
   *  the fence itself when it is this API's, or one the backend made over
   *  a fence of another API, which @p made then reports so the handle
   *  owns it. Null when the backend cannot take the fence. */
  virtual void* importFence(const NativeFence& fence, bool* made) = 0;
  /** @p fence as the API's own objects, for `exportNative`. */
  virtual NativeFence exportFence(void* fence) = 0;
  virtual void destroyFence(void* fence) = 0;
  /** Queues a signal to @p value behind everything submitted so far. */
  virtual void signalFence(void* fence, FenceValue value) = 0;
  /** Queues a wait for @p value ahead of everything submitted after. */
  virtual void waitFenceGpu(void* fence, FenceValue value) = 0;
  /** Blocks for @p value or the timeout. */
  virtual bool waitFenceCpu(void* fence, FenceValue value,
                            std::chrono::milliseconds timeout) = 0;
  virtual FenceValue completedValue(void* fence) = 0;

  /** Blocks until everything submitted to the queue has finished. */
  virtual void waitIdle() = 0;
};

#ifdef __APPLE__
/** WHAT A METAL TEXTURE WAS MADE AS, read off the texture itself: the
 *  device it stands on, its size and levels, its format where
 *  `TextureFormat` names it, and the usage it was made with. */
struct MetalTextureFacts {
  void* mtlDevice = nullptr;
  int width = 0;
  int height = 0;
  int mipLevels = 1;
  TextureFormat format = TextureFormat::RGBA8Unorm;
  TextureUsage usage = TextureUsage::ShaderRead;
};

/** The facts of @p mtlTexture; false where it is not a plain
 *  two-dimensional, single-sample texture in a format `TextureFormat`
 *  names. Implemented beside the Metal backend, which is where an
 *  Objective-C object is read. */
bool readMetalTexture(void* mtlTexture, MetalTextureFacts* facts);
#endif

/** The Metal implementation; null when there is no Metal device. */
std::unique_ptr<GpuDevice::DeviceBackend> createMetalBackend(
    const NativeDevice& native, bool owned);

/** The Vulkan implementation over handles the host owns; null, with the
 *  reason in @p error, when a required handle or entry point is
 *  missing. There is no owned Vulkan device: whoever owns the Vulkan API
 *  in a process creates it and this adopts it. */
std::unique_ptr<GpuDevice::DeviceBackend> createVulkanBackend(
    const NativeDevice& native, std::string* error);

}  // namespace sigil::core::hardware
