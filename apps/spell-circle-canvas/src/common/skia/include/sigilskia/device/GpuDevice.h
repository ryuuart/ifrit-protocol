#pragma once
#include <sigilskia/device/Fence.h>
#include <sigilskia/device/Handle.h>
#include <sigilskia/graphite/GraphiteContext.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace sigil::skia {

/** The graphics API behind a device. */
enum class Backend { Metal, Vulkan };

/**
 * The native objects behind a device, as the API's own handles bridged to
 * opaque values: an id<MTLDevice> and id<MTLCommandQueue> for Metal, and
 * the VulkanHandles set for Vulkan. Only the fields of `backend` are
 * read. A device adopted from one of these never frees the objects; the
 * caller keeps them alive for the device's lifetime.
 */
struct NativeDevice {
  Backend backend = Backend::Metal;
  void* mtlDevice = nullptr;
  void* mtlCommandQueue = nullptr;
  VulkanHandles vulkan;
};

/** The pixel format of a texture the device creates. */
enum class TextureFormat { RGBA8Unorm, BGRA8Unorm, RGBA16Float };

/** How a texture may be used, combined by |. */
enum class TextureUsage : uint32_t {
  ShaderRead = 1u << 0,
  ShaderWrite = 1u << 1,
  RenderTarget = 1u << 2,
};
constexpr TextureUsage operator|(TextureUsage a, TextureUsage b) {
  return static_cast<TextureUsage>(static_cast<uint32_t>(a) |
                                   static_cast<uint32_t>(b));
}
constexpr bool has(TextureUsage set, TextureUsage flag) {
  return (static_cast<uint32_t>(set) & static_cast<uint32_t>(flag)) != 0;
}

/** A texture to create: size, format, usage, and whether the CPU may
 *  read and write its bytes directly (shared storage). */
struct TextureDesc {
  int width = 0;
  int height = 0;
  TextureFormat format = TextureFormat::RGBA8Unorm;
  TextureUsage usage = TextureUsage::ShaderRead | TextureUsage::RenderTarget;
  bool cpuAccessible = false;
  /** A debug name the API shows in its tools; may be null. */
  const char* label = nullptr;
};

/**
 * A texture as the API's own object, bridged to an opaque value: an
 * id<MTLTexture> for Metal; a VkImage with its layout, format and — for
 * one the device allocated, or one imported with ownership — the
 * VkDeviceMemory behind it for Vulkan. Size travels with it because
 * Vulkan does not carry it on the handle. `vkLayout` is the layout the
 * image was last left in as far as the device knows; a device-created
 * image starts undefined.
 */
struct NativeTexture {
  Backend backend = Backend::Metal;
  void* mtlTexture = nullptr;
  uint64_t vkImage = 0;
  uint64_t vkMemory = 0;
  uint32_t vkLayout = 0;
  uint32_t vkFormat = 0;
  int width = 0;
  int height = 0;

  explicit operator bool() const {
    return mtlTexture != nullptr || vkImage != 0;
  }
};

/**
 * A GPU device and its one command queue, either created by this library
 * or adopted from a host that owns them, with the resources that live on
 * it named by handles. Textures are created, imported from the host and
 * exported back to it; fences are timelines signalled by the queue and
 * waited on by the queue or the CPU; and a texture's destruction is
 * deferred three frames, so a texture the GPU may still be reading —
 * up to three frames can be in flight — is released only once every
 * frame that could reference it has been retired by `beginFrame()`.
 *
 * Handles carry a generation: a handle kept past `destroy()` names
 * nothing and every call on it fails, rather than reaching the resource
 * that has since taken its slot. Every call is safe from any thread —
 * a host and a worker can name and forget textures on one device — and
 * the queue behind it orders their work by submission; `beginFrame()`
 * is the one call that belongs to a single thread, the one that counts
 * frames.
 */
class GpuDevice {
 public:
  /** A device this library owns. Metal: the system default device and a
   *  fresh queue on it. Vulkan: the loader found through the platform's
   *  library search (or SIGILSKIA_VULKAN_LIBRARY), an instance, the first
   *  physical device with a graphics queue, a device on it with timeline
   *  semaphores enabled, and that queue. Null when the backend is not
   *  available here — no Metal off Apple, no Vulkan loader or driver —
   *  with the reason in @p error when one is given. */
  static std::unique_ptr<GpuDevice> createOwned(Backend backend,
                                                std::string* error = nullptr);
  /** A device over objects the host owns and keeps alive; the device
   *  never frees them. A Vulkan set needs instance, physical device,
   *  device and queue, and a device created with timeline semaphores
   *  enabled; `getInstanceProcAddr` may be null, in which case the
   *  loader is found the way createOwned finds it. Null when a required
   *  handle is missing, with the reason in @p error when one is given. */
  static std::unique_ptr<GpuDevice> adopt(const NativeDevice& native,
                                          std::string* error = nullptr);

  ~GpuDevice();
  GpuDevice(const GpuDevice&) = delete;
  GpuDevice& operator=(const GpuDevice&) = delete;

  Backend backend() const;
  /** The native objects, whether owned or adopted. */
  const NativeDevice& native() const;

  /** Frames counted from zero; `beginFrame()` advances it and retires
   *  every deferred destruction three or more frames old. */
  void beginFrame();
  uint64_t frameIndex() const;
  /** The number of frames a destroyed texture stays alive after the
   *  frame it was destroyed in. */
  static constexpr uint64_t kFramesInFlight = 3;

  /** A new texture; a null handle when the API refuses the description. */
  TextureHandle createTexture(const TextureDesc& desc);
  /** A handle over a texture the host made. With @p takeOwnership the
   *  device releases it on destroy like one of its own; without, the
   *  host keeps it alive and destroy only forgets it. A null handle
   *  when the texture is missing or from another API. */
  TextureHandle importNative(const NativeTexture& texture,
                             bool takeOwnership = false);
  /** The native object behind a live handle, for a host that draws with
   *  the API directly; empty for a stale or null handle. */
  NativeTexture exportNative(TextureHandle handle) const;
  /** True while the handle names a texture. */
  bool isValid(TextureHandle handle) const;
  /** Forgets the handle now — it is stale from here on — and releases
   *  the texture after kFramesInFlight more frames. A stale handle does
   *  nothing. */
  void destroy(TextureHandle handle);
  /** Textures destroyed but not yet released. */
  size_t pendingDestroys() const;
  /** Live textures. */
  size_t textureCount() const;

  /** A fence at kFenceInitialValue. */
  FenceHandle createFence();
  /** Releases the fence now; nothing may still wait on it. */
  void destroyFence(FenceHandle fence);
  /** Queues a signal after everything submitted so far and returns the
   *  value it raises the fence to; kFenceInitialValue for a stale
   *  handle. */
  FenceValue signal(FenceHandle fence);
  /** Queues a wait: nothing submitted to the queue after this runs until
   *  the fence reaches @p value. The value must already be signalled or
   *  be signalled from another queue — a signal queued on this same
   *  queue after the wait sits behind it and never runs, and the GPU
   *  eventually fails the whole batch. `exportNative` hands the fence
   *  to another queue for exactly that. */
  void waitGpu(FenceHandle fence, FenceValue value);
  /** The native fence behind a live handle — an id<MTLSharedEvent>
   *  bridged to void* on Metal, a timeline VkSemaphore as void* on
   *  Vulkan — for another queue or process to signal or wait on; null
   *  for a stale handle. */
  void* exportNative(FenceHandle fence) const;
  /** Blocks until the fence reaches @p value or @p timeout passes. */
  FenceWait waitCpu(
      FenceHandle fence, FenceValue value,
      std::chrono::milliseconds timeout = kFenceDefaultTimeout) const;
  /** The value the fence has reached; kFenceInitialValue for a stale
   *  handle. */
  FenceValue completedValue(FenceHandle fence) const;
  /** True while the handle names a fence. */
  bool isValid(FenceHandle handle) const;

  class Backend_;

 private:
  explicit GpuDevice(std::unique_ptr<Backend_> backend);
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

}  // namespace sigil::skia
