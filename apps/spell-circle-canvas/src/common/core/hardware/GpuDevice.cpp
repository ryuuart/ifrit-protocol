// The API-independent half of GpuDevice: the handle tables, the frame
// counter and the deferred-destroy queue. Every native operation goes
// through the backend interface in DeviceBackend.h.

#include <sigilcore/hardware/GpuDevice.h>

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
  /** The fence as the API's own objects, read once when it was named. */
  NativeFence exported;
  /** Whether the device made the native fence and so releases it. */
  bool owned = true;
  /** Whether another queue raises it, which leaves `signal` nothing to
   *  number. */
  bool imported = false;
};

struct DeferredRelease {
  NativeTexture native;
  bool owned = false;
  uint64_t frame = 0;
};

}  // namespace

struct GpuDevice::Impl {
  std::unique_ptr<DeviceBackend> backend;
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

std::expected<std::unique_ptr<GpuDevice>, Error> GpuDevice::createOwned() {
  std::string error;
  std::unique_ptr<DeviceBackend> impl;
#ifdef __APPLE__
  NativeDevice native;
  native.backend = Backend::Metal;
  impl = createMetalBackend(native, /*owned=*/true);
  if (!impl) report(&error, "no Metal device");
#else
  report(&error,
         "this platform has no device of its own to make; adopt the one the "
         "renderer created");
#endif
  if (!impl) return std::unexpected(Error{std::move(error)});
  return std::unique_ptr<GpuDevice>(new GpuDevice(std::move(impl)));
}

std::expected<std::unique_ptr<GpuDevice>, Error> GpuDevice::adopt(
    const NativeDevice& native) {
  std::string reason;
  std::string* error = &reason;
  std::unique_ptr<DeviceBackend> impl;
  switch (native.backend) {
    case Backend::Metal: {
#ifdef __APPLE__
      if (!native.mtlDevice || !native.mtlCommandQueue) {
        report(error, "a Metal device and command queue are both required");
        return std::unexpected(Error{std::move(reason)});
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
  if (!impl) return std::unexpected(Error{std::move(reason)});
  return std::unique_ptr<GpuDevice>(new GpuDevice(std::move(impl)));
}

GpuDevice::GpuDevice(std::unique_ptr<DeviceBackend> backend)
    : m_impl(std::make_unique<Impl>()) {
  m_impl->backend = std::move(backend);
}

// The lists walked below were sized while the device was alive, so the
// only throw left in them is the allocator's, and a device that cannot
// release its own textures has nothing to unwind to.
// NOLINTNEXTLINE(bugprone-exception-escape)
GpuDevice::~GpuDevice() {
  // The device going away is the last frame: every texture still named
  // and every deferred one is released now, and every fence with them —
  // once the queue has finished what it holds, since a draw into one of
  // them or a wait on one may still be queued.
  Impl& impl = *m_impl;
  if (impl.textures.size() != 0 || !impl.deferred.empty() ||
      impl.fences.size() != 0)
    impl.backend->waitIdle();
  for (const TextureSlot& slot : impl.textures.drain())
    if (slot.owned) impl.backend->releaseTexture(slot.native);
  for (const DeferredRelease& release : impl.deferred)
    if (release.owned) impl.backend->releaseTexture(release.native);
  impl.deferred.clear();
  for (const FenceSlot& slot : impl.fences.drain())
    if (slot.owned) impl.backend->destroyFence(slot.native);
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

uint64_t GpuDevice::frameIndex() const {
  // Read under the lock `beginFrame()` writes it under: the class
  // promises every call is safe from any thread.
  const std::lock_guard<std::mutex> lock(m_impl->mutex);
  return m_impl->frame;
}

TextureHandle GpuDevice::createTexture(const TextureDescription& desc) {
  if (desc.width <= 0 || desc.height <= 0) return {};
  const std::lock_guard<std::mutex> lock(m_impl->mutex);
  NativeTexture native = m_impl->backend->createTexture(desc);
  if (!native) return {};
  return m_impl->textures.allocate(TextureSlot{native, /*owned=*/true});
}

TextureHandle GpuDevice::importNative(const NativeTexture& texture,
                                      bool takeOwnership) {
  if (!texture) return {};
  const std::lock_guard<std::mutex> lock(m_impl->mutex);
  bool made = false;
  const NativeTexture named = m_impl->backend->importTexture(texture, &made);
  if (!named) return {};
  // The handle owns what the import made and nothing it was handed
  // unless asked to.
  if (made) return m_impl->textures.allocate(TextureSlot{named, true});
  if (takeOwnership) m_impl->backend->retainTexture(named);
  return m_impl->textures.allocate(TextureSlot{named, takeOwnership});
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
  FenceSlot slot;
  slot.native = native;
  slot.exported = m_impl->backend->exportFence(native);
  return m_impl->fences.allocate(slot);
}

FenceHandle GpuDevice::importNative(const NativeFence& fence) {
  if (!fence) return {};
  const std::lock_guard<std::mutex> lock(m_impl->mutex);
  bool made = false;
  void* native = m_impl->backend->importFence(fence, &made);
  if (!native) return {};
  FenceSlot slot;
  slot.native = native;
  slot.exported = m_impl->backend->exportFence(native);
  // The very object it was imported from is what it exports as.
  if (fence.mtlSharedEvent) slot.exported.mtlSharedEvent = fence.mtlSharedEvent;
  slot.owned = made;
  slot.imported = true;
  return m_impl->fences.allocate(slot);
}

void GpuDevice::destroyFence(FenceHandle fence) {
  Impl& impl = *m_impl;
  const std::lock_guard<std::mutex> lock(m_impl->mutex);
  if (!impl.fences.contains(fence)) return;
  FenceSlot slot = impl.fences.release(fence);
  if (slot.owned) impl.backend->destroyFence(slot.native);
}

FenceValue GpuDevice::signal(FenceHandle fence) {
  const std::lock_guard<std::mutex> lock(m_impl->mutex);
  FenceSlot* slot = m_impl->fences.find(fence);
  if (!slot || slot->imported) return kFenceInitialValue;
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

NativeFence GpuDevice::exportNative(FenceHandle fence) const {
  const std::lock_guard<std::mutex> lock(m_impl->mutex);
  const FenceSlot* slot = m_impl->fences.find(fence);
  return slot ? slot->exported : NativeFence{};
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
