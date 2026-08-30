#pragma once
#include <include/core/SkRefCnt.h>

#include <cstdint>

class SkCanvas;
class SkSurface;

namespace sigil::skia {

class GraphiteContext;

/**
 * A Vulkan image to draw into, as opaque values so this header pulls in
 * no Vulkan header: `image` is the VkImage (a 64-bit non-dispatchable
 * handle), `layout` and `format` are the VkImageLayout and VkFormat
 * enumerators the image currently has, and the size is in pixels. The
 * image must have been created for colour attachment, input attachment,
 * sampling and transfer in both directions — the set a GpuDevice
 * creates with — and its memory stays the caller's.
 */
struct VulkanImage {
  uint64_t image = 0;
  uint32_t layout = 0;
  uint32_t format = 0;
  int width = 0;
  int height = 0;
};

/**
 * Wraps an existing native texture (an offscreen canvas texture, a
 * CAMetalLayer drawable, a swapchain image) in an SkSurface without
 * copying it, so SkCanvas draw calls land directly in that texture.
 * Construct fresh per use — it is a thin, cheap wrapper around a texture
 * someone else owns — and drive it with the context it was made on.
 *
 * The wrap is per graphics API, one constructor each, both Qt-free. A
 * QRhiTexture is wrapped through <sigilskia/qt/QtInterop.h>.
 */
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

  OffscreenSurface(OffscreenSurface&& other) noexcept;
  OffscreenSurface& operator=(OffscreenSurface&&) = delete;
  OffscreenSurface(const OffscreenSurface&) = delete;
  OffscreenSurface& operator=(const OffscreenSurface&) = delete;
  ~OffscreenSurface();

  /** Null if wrapping the backend texture failed. */
  SkCanvas* canvas() const;
  /** The wrapped surface itself, for a readback or a snapshot; null if
   *  wrapping failed. */
  SkSurface* surface() const;

  /** Snaps the Recorder's accumulated draw commands into a Recording,
   *  inserts it into the Context, and submits it to the GPU asynchronously.
   *  Safe because Graphite shares the host's command queue: the host's
   *  later GPU work is ordered after this submission on the same queue. */
  void submit();

 private:
  GraphiteContext* m_context;
  sk_sp<SkSurface> m_surface;
};

}  // namespace sigil::skia
