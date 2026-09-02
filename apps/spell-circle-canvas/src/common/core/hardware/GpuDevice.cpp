// The API-independent half of GpuDevice: the handle tables, the frame
// counter and the deferred-destroy queue. Every native operation goes
// through the backend interface in DeviceBackend.h.

#include <sigilcore/hardware/GpuDevice.h>

#include <cstdio>
#include <deque>
#include <mutex>

#include "DeviceBackend.h"

namespace sigil::core::hardware {

int mipLevelsFor(int width, int height) {
  int levels = 1;
  int w = width, h = height;
  while (w > 1 || h > 1) {
    w = w > 1 ? w / 2 : 1;
    h = h > 1 ? h / 2 : 1;
    ++levels;
  }
  return width > 0 && height > 0 ? levels : 1;
}

namespace {

struct TextureSlot {
  NativeTexture native;
  bool owned = false;
};

struct FenceSlot {
  void* native = nullptr;
  FenceValue lastSignalled = kFenceInitialValue;
};

struct DeferredRelease {
  NativeTexture native;
  bool owned = false;
  uint64_t frame = 0;
};

}  // namespace

struct GpuDevice::Impl {
  std::unique_ptr<Backend_> backend;
  HandleTable<TextureSlot, TextureHandle> textures;
  HandleTable<FenceSlot, FenceHandle> fences;
  std::deque<DeferredRelease> deferred;
  uint64_t frame = 0;
  // Every table and the queue are behind this; native calls that wait
  // (a CPU fence wait) run outside it.
  mutable std::mutex mutex;
};

namespace {

void report(std::string* error, const char* message) {
  if (error) *error = message;
}

}  // namespace

std::unique_ptr<GpuDevice> GpuDevice::createOwned(std::string* error) {
  std::unique_ptr<Backend_> impl;
#ifdef __APPLE__
  NativeDevice native;
  native.backend = Backend::Metal;
  impl = createMetalBackend(native, /*owned=*/true);
  if (!impl) report(error, "no Metal device");
#else
  report(error,
         "this platform has no device of its own to make; adopt the one the "
         "renderer created");
#endif
  if (!impl) return nullptr;
  return std::unique_ptr<GpuDevice>(new GpuDevice(std::move(impl)));
}

std::unique_ptr<GpuDevice> GpuDevice::adopt(const NativeDevice& native,
                                            std::string* error) {
  std::unique_ptr<Backend_> impl;
  switch (native.backend) {
    case Backend::Metal: {
#ifdef __APPLE__
      if (!native.mtlDevice || !native.mtlCommandQueue) {
        report(error, "a Metal device and command queue are both required");
        return nullptr;
      }
      impl = createMetalBackend(native, /*owned=*/false);
      if (!impl) report(error, "no Metal device");
#else
      report(error, "Metal exists only on Apple platforms");
#endif
      break;
    }
    case Backend::Vulkan:
      impl = createVulkanBackend(native, error);
      break;
  }
  if (!impl) return nullptr;
  return std::unique_ptr<GpuDevice>(new GpuDevice(std::move(impl)));
}

GpuDevice::GpuDevice(std::unique_ptr<Backend_> backend)
    : m_impl(std::make_unique<Impl>()) {
  m_impl->backend = std::move(backend);
}

// The lists walked below were sized while the device was alive, so the
// only throw left in them is the allocator's, and a device that cannot
// release its own textures has nothing to unwind to.
// NOLINTNEXTLINE(bugprone-exception-escape)
GpuDevice::~GpuDevice() {
  // The device going away is the last frame: nothing can still be in
  // flight once the backend's queue is dropped with it, so every texture
  // still named and every deferred one is released now, and every fence
  // with them.
  Impl& impl = *m_impl;
  for (const TextureSlot& slot : impl.textures.drain())
    if (slot.owned) impl.backend->releaseTexture(slot.native);
  for (const DeferredRelease& release : impl.deferred)
    if (release.owned) impl.backend->releaseTexture(release.native);
  impl.deferred.clear();
  for (const FenceSlot& slot : impl.fences.drain())
    impl.backend->destroyFence(slot.native);
}

Backend GpuDevice::backend() const { return m_impl->backend->native().backend; }

const NativeDevice& GpuDevice::native() const {
  return m_impl->backend->native();
}

void GpuDevice::beginFrame() {
  Impl& impl = *m_impl;
  const std::lock_guard<std::mutex> lock(m_impl->mutex);
  ++impl.frame;
  while (!impl.deferred.empty() &&
         impl.frame >= impl.deferred.front().frame + kFramesInFlight) {
    const DeferredRelease& release = impl.deferred.front();
    if (release.owned) impl.backend->releaseTexture(release.native);
    impl.deferred.pop_front();
  }
}

uint64_t GpuDevice::frameIndex() const { return m_impl->frame; }

TextureHandle GpuDevice::createTexture(const TextureDesc& desc) {
  if (desc.width <= 0 || desc.height <= 0) return {};
  const std::lock_guard<std::mutex> lock(m_impl->mutex);
  NativeTexture native = m_impl->backend->createTexture(desc);
  if (!native) return {};
  return m_impl->textures.allocate(TextureSlot{native, /*owned=*/true});
}

TextureHandle GpuDevice::importNative(const NativeTexture& texture,
                                      bool takeOwnership) {
  if (!texture || texture.backend != backend()) return {};
  const std::lock_guard<std::mutex> lock(m_impl->mutex);
  if (takeOwnership) m_impl->backend->retainTexture(texture);
  return m_impl->textures.allocate(TextureSlot{texture, takeOwnership});
}

NativeTexture GpuDevice::exportNative(TextureHandle handle) const {
  const std::lock_guard<std::mutex> lock(m_impl->mutex);
  const TextureSlot* slot = m_impl->textures.find(handle);
  return slot ? slot->native : NativeTexture{};
}

bool GpuDevice::isValid(TextureHandle handle) const {
  const std::lock_guard<std::mutex> lock(m_impl->mutex);
  return m_impl->textures.contains(handle);
}

void GpuDevice::destroy(TextureHandle handle) {
  Impl& impl = *m_impl;
  const std::lock_guard<std::mutex> lock(m_impl->mutex);
  if (!impl.textures.contains(handle)) return;
  TextureSlot slot = impl.textures.release(handle);
  impl.deferred.push_back(DeferredRelease{slot.native, slot.owned, impl.frame});
}

size_t GpuDevice::pendingDestroys() const {
  const std::lock_guard<std::mutex> lock(m_impl->mutex);
  return m_impl->deferred.size();
}

size_t GpuDevice::textureCount() const {
  const std::lock_guard<std::mutex> lock(m_impl->mutex);
  return m_impl->textures.size();
}

FenceHandle GpuDevice::createFence() {
  const std::lock_guard<std::mutex> lock(m_impl->mutex);
  void* native = m_impl->backend->createFence();
  if (!native) return {};
  return m_impl->fences.allocate(FenceSlot{native, kFenceInitialValue});
}

void GpuDevice::destroyFence(FenceHandle fence) {
  Impl& impl = *m_impl;
  const std::lock_guard<std::mutex> lock(m_impl->mutex);
  if (!impl.fences.contains(fence)) return;
  FenceSlot slot = impl.fences.release(fence);
  impl.backend->destroyFence(slot.native);
}

FenceValue GpuDevice::signal(FenceHandle fence) {
  const std::lock_guard<std::mutex> lock(m_impl->mutex);
  FenceSlot* slot = m_impl->fences.find(fence);
  if (!slot) return kFenceInitialValue;
  const FenceValue value = ++slot->lastSignalled;
  m_impl->backend->signalFence(slot->native, value);
  return value;
}

void GpuDevice::waitGpu(FenceHandle fence, FenceValue value) {
  const std::lock_guard<std::mutex> lock(m_impl->mutex);
  FenceSlot* slot = m_impl->fences.find(fence);
  if (!slot) return;
  m_impl->backend->waitFenceGpu(slot->native, value);
}

FenceWait GpuDevice::waitCpu(FenceHandle fence, FenceValue value,
                             std::chrono::milliseconds timeout) const {
  void* native = nullptr;
  {
    const std::lock_guard<std::mutex> lock(m_impl->mutex);
    const FenceSlot* slot = m_impl->fences.find(fence);
    if (!slot) return FenceWait::Invalid;
    native = slot->native;
  }
  // Outside the lock: a wait must not hold every other thread's naming
  // hostage, and the native fence outlives the wait because destroyFence
  // is documented to come only once nothing waits.
  return m_impl->backend->waitFenceCpu(native, value, timeout)
             ? FenceWait::Reached
             : FenceWait::TimedOut;
}

void* GpuDevice::exportNative(FenceHandle fence) const {
  const std::lock_guard<std::mutex> lock(m_impl->mutex);
  const FenceSlot* slot = m_impl->fences.find(fence);
  return slot ? slot->native : nullptr;
}

FenceValue GpuDevice::completedValue(FenceHandle fence) const {
  const std::lock_guard<std::mutex> lock(m_impl->mutex);
  const FenceSlot* slot = m_impl->fences.find(fence);
  if (!slot) return kFenceInitialValue;
  return m_impl->backend->completedValue(slot->native);
}

bool GpuDevice::isValid(FenceHandle handle) const {
  const std::lock_guard<std::mutex> lock(m_impl->mutex);
  return m_impl->fences.contains(handle);
}

}  // namespace sigil::core::hardware
