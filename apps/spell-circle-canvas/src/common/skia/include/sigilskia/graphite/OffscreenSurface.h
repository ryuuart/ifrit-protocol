#pragma once

/** @file
 * @ingroup skia-graphite
 * An SkSurface over a texture someone else owns, for a frame's worth of
 * drawing, with the fences that say when the device may have it back.
 */

#include <include/core/SkRefCnt.h>
// The names a device gives its resources. Handles and fence values are
// plain values with no device code behind them; the entry points here
// that read one are defined by the device feature, so a caller of those
// links it.
#include <sigilcore/hardware/Fence.h>
#include <sigilcore/hardware/Handle.h>

#include <cstdint>
#include <memory>

class SkCanvas;
class SkSurface;

namespace sigil::skia {

class GraphiteContext;
class PaintOrderCanvas;

/** A VULKAN IMAGE TO DRAW INTO, as opaque values so this header pulls in
 *  no Vulkan header: `image` is the VkImage, a 64-bit non-dispatchable
 *  handle, `layout` and `format` are the VkImageLayout and VkFormat
 *  enumerators it currently has, and the size is in pixels. Its memory
 *  stays the caller's.
 *  @trap It must have been created for colour attachment, input
 *  attachment, sampling and transfer both ways. */
struct VulkanImage {
  uint64_t image = 0;
  uint32_t layout = 0;
  uint32_t format = 0;
  int width = 0;
  int height = 0;
};

/** AN EXISTING NATIVE TEXTURE AS AN SkSurface, with no copy, so SkCanvas
 *  draws land directly in it: an offscreen canvas texture, a CAMetalLayer
 *  drawable, a swapchain image. Thin and cheap — construct one fresh per
 *  use, and drive it with the context it was made on. One wrapping
 *  constructor per graphics API, both Qt-free; a host whose textures a
 *  GpuDevice names hands the handle instead and never spells an API. */
class OffscreenSurface {
 public:
#ifdef __APPLE__
  /** Metal wrap: @p mtlTexture is an id<MTLTexture> bridged to void*,
   *  created on the same device @p context was built from. */
  OffscreenSurface(GraphiteContext& context, void* mtlTexture, int width,
                   int height);
#endif

  /** Vulkan wrap of an image created on the same device @p context was
   *  built from. Leaves `canvas()` null when the build's Skia carries no
   *  Vulkan backend. */
  OffscreenSurface(GraphiteContext& context, const VulkanImage& image);

  /** The texture @p texture names on @p device, whichever API that
   *  device is: the wrap a host holding a GpuDevice reaches for, in
   *  place of the native handle its API spells. `canvas()` is null when
   *  the handle is stale or the wrap failed. Defined by the hardware
   *  feature's device value.
   *  @trap A Vulkan image is wrapped in the layout the device last knew,
   *  undefined for one nothing has drawn into, so its contents before
   *  the first draw are not preserved. */
  OffscreenSurface(GraphiteContext& context, core::hardware::GpuDevice& device,
                   core::hardware::TextureHandle texture);

  /** Moves the wrap. A moved-from surface holds neither the surface nor
   *  the context: `canvas()` and `surface()` are null and every `submit()`
   *  on it does nothing, so the recorder's work is only ever submitted by
   *  the surface that was moved into. */
  OffscreenSurface(OffscreenSurface&& other) noexcept;
  OffscreenSurface& operator=(OffscreenSurface&&) = delete;
  OffscreenSurface(const OffscreenSurface&) = delete;
  OffscreenSurface& operator=(const OffscreenSurface&) = delete;
  ~OffscreenSurface();

  /** Null if wrapping the backend texture failed. Draws described
   *  through it keep the order they were described in, whatever the
   *  backend does with them — see <sigilskia/graphite/PaintOrder.h>. */
  SkCanvas* canvas() const;
  /** The wrapped surface itself, for a readback or a snapshot; null if
   *  wrapping failed. */
  SkSurface* surface() const;

  /** Snaps the Recorder's accumulated draw commands into a Recording,
   *  inserts it into the Context, and submits it to the GPU asynchronously.
   *  Safe because Graphite shares the host's command queue: the host's
   *  later GPU work is ordered after this submission on the same queue. */
  void submit();

  /** Submits as `submit()` does, then queues a signal of @p fence on
   *  @p device behind it and returns the value the fence will reach —
   *  kFenceInitialValue for a stale handle, and for a moved-from surface,
   *  which submits nothing to signal. Defined by the device feature.
   *  @trap The wait belongs on another queue or on the CPU: Graphite
   *  shares this one, so a wait queued ahead of the signal never passes. */
  core::hardware::FenceValue submit(core::hardware::GpuDevice& device,
                                    core::hardware::FenceHandle fence);

 private:
  GraphiteContext* m_context;
  sk_sp<SkSurface> m_surface;
  /** Made on the first `canvas()` and kept, because it carries the
   *  fence's own state across the draws of one frame. */
  mutable std::unique_ptr<PaintOrderCanvas> m_ordered;
};

}  // namespace sigil::skia
