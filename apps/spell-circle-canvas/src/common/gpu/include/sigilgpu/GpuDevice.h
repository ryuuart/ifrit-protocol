#pragma once

/** @file
 * @ingroup gpu-device
 *
 * The device seam: a GPU device and its one command queue, the textures
 * and fences that live on it, and the native objects either side of the
 * seam is spelled in — Metal's and Vulkan's own handles, bridged to
 * opaque values so no graphics header travels with this one.
 */

/** @defgroup gpu-device The device seam
 *  The GPU device itself: one device and its one command queue, created
 *  here or adopted from a host that owns them, with the textures and
 *  fences living on it named by handles that go stale rather than
 *  dangle, and destruction that waits out the frames still in flight. It
 *  knows nothing about what draws — no Skia, no Diligent, no Qt — which
 *  is what lets two drawing APIs stand on one device. */

#include <sigilcore/report/Error.h>
#include <sigilgpu/Fence.h>
#include <sigilgpu/Handle.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>

/** THE GPU DEVICE SEAM: one device and its one command queue, created
 *  here or adopted from a host that owns them, with the textures and
 *  fences living on it named by handles that go stale rather than
 *  dangle, and destruction that waits out the frames still in flight.
 *  Every native object either side of the seam is the API's own handle
 *  bridged to an opaque value, so no graphics header travels with these
 *  ones.
 *
 *  It knows nothing about what draws — no Skia, no Diligent, no Qt — and
 *  that is what lets two drawing APIs stand on one device instead of
 *  each creating its own and copying between them. */
namespace sigil::gpu {

/**
 * The raw Vulkan handles a device is stood up on or adopted from. Every
 * field is the Vulkan object bridged to an opaque pointer or integer, so
 * this header pulls in no Vulkan header: `instance`, `physicalDevice`,
 * `device` and `queue` are the dispatchable handles (pointers) as-is;
 * `apiVersion` is packed the way Vulkan packs it (major, minor, patch);
 * `getInstanceProcAddr` is the loader's `vkGetInstanceProcAddr`, from
 * which every other entry point is resolved.
 */
struct VulkanHandles {
  /** A function pointer as Vulkan's own loader returns one. */
  using VoidFunction = void (*)();
  /** The signature of `vkGetInstanceProcAddr` with the instance opaque. */
  using GetInstanceProcAddr = VoidFunction (*)(void* instance,
                                               const char* name);

  void* instance = nullptr;
  void* physicalDevice = nullptr;
  void* device = nullptr;
  void* queue = nullptr;
  uint32_t queueFamilyIndex = 0;
  uint32_t apiVersion = 0;
  GetInstanceProcAddr getInstanceProcAddr = nullptr;
};

/** The graphics API behind a device. */
enum class Backend { Metal, Vulkan };

/**
 * The native objects behind a device, as the API's own handles bridged to
 * opaque values: an id<MTLDevice> and id<MTLCommandQueue> for Metal, and
 * the VulkanHandles set for Vulkan. Only the fields of `backend` are
 * read. A device adopted from one of these never frees the objects; the
 * caller keeps them alive for the device's lifetime.
 *
 * A VULKAN DEVICE WHOSE DRIVER RUNS OVER METAL also answers, in the
 * `native()` it reports, the id<MTLDevice> the driver runs on as
 * `mtlDevice` — the device a Metal texture must stand on for this one to
 * name it, and the one a Metal queue painting for it is made on. Its
 * `mtlCommandQueue` stays null: that queue is the driver's.
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
  ShaderRead = 1u << 0u,
  ShaderWrite = 1u << 1u,
  RenderTarget = 1u << 2u,
};
/** The union of two usage selections. */
constexpr TextureUsage operator|(TextureUsage left, TextureUsage right) {
  return static_cast<TextureUsage>(static_cast<uint32_t>(left) |
                                   static_cast<uint32_t>(right));
}
/** Whether @p set includes the usage @p flag names. */
constexpr bool has(TextureUsage set, TextureUsage flag) {
  return (static_cast<uint32_t>(set) & static_cast<uint32_t>(flag)) != 0;
}

/** A texture to create: size, format, usage, how many mip levels it
 *  carries, and whether the CPU may read and write its bytes directly
 *  (shared storage). */
struct TextureDescription {
  int width = 0;
  int height = 0;
  TextureFormat format = TextureFormat::RGBA8Unorm;
  TextureUsage usage = TextureUsage::ShaderRead | TextureUsage::RenderTarget;
  /** How many levels the texture holds, level 0 at `width` x `height`
   *  and each one after it half the size of the last. One is a texture
   *  with no chain. A count past what the size can carry is clamped to
   *  what it can, since a level under one texel on a side does not
   *  exist. A CHAIN IS NOT ONLY A FILTERING AID: a prefiltered
   *  environment puts a different image on every level, and the level a
   *  shader reads is what its roughness picked. */
  int mipLevels = 1;
  bool cpuAccessible = false;
  /** A debug name the API shows in its tools; may be null. */
  const char* label = nullptr;
};

/** How many levels a chain over @p width x @p height can hold: one more
 *  each time EITHER side can still be halved, so the chain ends at one
 *  pixel by one and a long thin texture keeps its length in levels. */
int mipLevelsFor(int width, int height);

/**
 * A texture as the API's own object, bridged to an opaque value: an
 * id<MTLTexture> for Metal; a VkImage with its layout, format and — for
 * one the device allocated, or one imported with ownership — the
 * VkDeviceMemory behind it for Vulkan. Size travels with it because
 * Vulkan does not carry it on the handle. `vkLayout` is the layout the
 * image was last left in as far as the device knows; a device-created
 * image starts undefined.
 *
 * ON A VULKAN DEVICE WHOSE DRIVER RUNS OVER METAL a texture is both: the
 * VkImage, and in `mtlTexture` the id<MTLTexture> the driver keeps behind
 * it, which is the very memory the image names — what a queue on Metal
 * draws into or samples for a texture this device holds. It is the
 * driver's object, held by the image, and is never retained for the
 * caller. Elsewhere `mtlTexture` is null on a Vulkan texture.
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
  /** Levels in the chain, level 0 at `width` x `height`. */
  int mipLevels = 1;

  explicit operator bool() const {
    return mtlTexture != nullptr || vkImage != 0;
  }
};

/**
 * A fence as the API's own object, bridged to an opaque value: an
 * id<MTLSharedEvent> for Metal, a timeline VkSemaphore for Vulkan. On a
 * Vulkan device whose driver runs over Metal a fence is both, the
 * semaphore and in `mtlSharedEvent` the event the driver keeps behind it,
 * which a queue on Metal waits on or raises; elsewhere that field is null
 * on a Vulkan fence.
 */
struct NativeFence {
  Backend backend = Backend::Metal;
  void* mtlSharedEvent = nullptr;
  uint64_t vkSemaphore = 0;

  explicit operator bool() const {
    return mtlSharedEvent != nullptr || vkSemaphore != 0;
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
  /** A device this library owns, on the platform's own API: the system
   *  default Metal device and a fresh queue on it; off Apple, or with no
   *  Metal device, the error that says so.
   *
   *  THERE IS NO OWNED VULKAN DEVICE. Whoever owns the Vulkan API in a
   *  process creates it — a renderer that cannot attach to a device
   *  someone else made has no choice about that — and this adopts what
   *  it made. Two instances in one process would mean two loaders, two
   *  queues and a copy between them, which is what adopting exists to
   *  avoid. */
  static std::expected<std::unique_ptr<GpuDevice>, core::Error> createOwned();
  /** A device over objects the host owns and keeps alive; the device
   *  never frees them. A Vulkan set needs instance, physical device,
   *  device and queue, a device created with timeline semaphores
   *  enabled, and the host's own `getInstanceProcAddr` — dispatching
   *  through a second copy of the same loader is what makes two APIs
   *  stop being one device. The error says which required handle is
   *  missing. */
  static std::expected<std::unique_ptr<GpuDevice>, core::Error> adopt(
      const NativeDevice& native);

  /** Releases every texture and fence the device still names, once the
   *  queue has gone idle — the device going away is the last frame, and
   *  a wait or a draw still queued on one of them is let finish first.
   *  @trap On an adopted queue, under the same lock as any other submit. */
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
  TextureHandle createTexture(const TextureDescription& desc);
  /** A handle over a texture the host made. With @p takeOwnership the
   *  device releases it on destroy like one of its own; without, the
   *  host keeps it alive and destroy only forgets it.
   *
   *  A METAL TEXTURE ON A VULKAN DEVICE whose driver runs over Metal is
   *  named by a VkImage made over the texture's own memory, so a pass
   *  here samples what a queue on Metal drew into it with no copy. The
   *  texture states itself — size, levels, format, the usage it was made
   *  with — and the image may do no more than that usage allows: an
   *  image is drawn into only where the texture was made a render target,
   *  and written by shaders only where it was made writable. The handle
   *  OWNS THE IMAGE and never the texture, whatever @p takeOwnership
   *  says: destroy releases the image, and the texture lives as long as
   *  its maker keeps it, which is past the frames in flight after the
   *  destroy. `exportNative` answers the image and that very texture.
   *  @silent A null handle when the texture is missing, from another API
   *  this device cannot take (a Vulkan image on Metal; a Metal texture on
   *  a driver not over Metal, on another Metal device, or in a format
   *  `TextureFormat` does not name). */
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
  /** A HANDLE OVER A FENCE ANOTHER QUEUE RAISES, the reverse of
   *  `exportNative(fence)`: there this device's queue raises a fence and
   *  another waits on it, here another queue raises one and this device
   *  waits — `waitGpu` holds this queue until it reaches a value, and
   *  `waitCpu` and `completedValue` read it. On a Vulkan device whose
   *  driver runs over Metal an id<MTLSharedEvent> is brought over as a
   *  timeline semaphore over that very event; a fence of the device's
   *  own API is named as it stands. The numbering is the other queue's,
   *  so `signal` on an imported fence queues nothing.
   *
   *  IMPORT AN EVENT ONCE AND KEEP THE HANDLE for as long as its waits
   *  are wanted: each import is a semaphore of its own. The handle owns
   *  what the import made and never what it was handed — destroying it
   *  releases the semaphore and leaves the event its maker's — and, like
   *  every fence, nothing may still wait on it when it goes. A device
   *  going away lets its queue go idle before it releases one.
   *  @silent A null handle when the fence is missing or from another API
   *  this device cannot take (a Vulkan semaphore on Metal; a Metal event
   *  on a driver not over Metal). */
  FenceHandle importNative(const NativeFence& fence);
  /** Releases the fence now; nothing may still wait on it. */
  void destroyFence(FenceHandle fence);
  /** Queues a signal after everything submitted so far and returns the
   *  value it raises the fence to; kFenceInitialValue for a stale handle
   *  and for an imported one, which only the queue that made it
   *  raises. */
  FenceValue signal(FenceHandle fence);
  /** Queues a wait: nothing submitted to the queue after this runs until
   *  the fence reaches @p value. The value must already be signalled or
   *  be signalled from another queue — a signal queued on this same
   *  queue after the wait sits behind it and never runs, and the GPU
   *  eventually fails the whole batch. `exportNative` hands the fence
   *  to another queue for exactly that. */
  void waitGpu(FenceHandle fence, FenceValue value);
  /** The native fence behind a live handle — an id<MTLSharedEvent> on
   *  Metal, a timeline VkSemaphore on Vulkan and, where the driver runs
   *  over Metal, the event behind it as well — for another queue or
   *  process to signal or wait on; empty for a stale handle. An imported
   *  fence answers the very object it was imported from. */
  NativeFence exportNative(FenceHandle fence) const;
  /** Blocks until the fence reaches @p value or @p timeout passes. */
  FenceWait waitCpu(
      FenceHandle fence, FenceValue value,
      std::chrono::milliseconds timeout = kFenceDefaultTimeout) const;
  /** The value the fence has reached; kFenceInitialValue for a stale
   *  handle. */
  FenceValue completedValue(FenceHandle fence) const;
  /** True while the handle names a fence. */
  bool isValid(FenceHandle handle) const;

  class DeviceBackend;

 private:
  explicit GpuDevice(std::unique_ptr<DeviceBackend> backend);
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

}  // namespace sigil::gpu
