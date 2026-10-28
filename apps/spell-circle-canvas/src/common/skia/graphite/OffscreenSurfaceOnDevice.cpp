// The OffscreenSurface entry points that name a texture and a fence by
// handle. Dependencies point one way: Graphite stands on the hardware
// device, and the hardware device knows nothing of Skia.

#include <include/gpu/GpuTypes.h>
#include <include/gpu/graphite/Recorder.h>
#include <sigilgpu/GpuDevice.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/OffscreenSurface.h>
#include <sigilskia/graphite/TextureImage.h>
// The fenced canvas the surface hands out: a constructor here must be able
// to destroy one.
#include <sigilskia/graphite/PaintOrder.h>

#include <utility>

namespace sigil::skia {

namespace {

/** The wrap the CONTEXT's API needs, over the native objects behind a
 *  handle: a context recording on Metal paints the Metal texture, which a
 *  Vulkan device whose driver runs over Metal names beside its own image,
 *  and any other context paints the texture in its own API. An empty
 *  export — what a stale or null handle yields — has no texture in it, so
 *  the wrap fails and `canvas()` stays null. */
OffscreenSurface wrapNative(GraphiteContext& context,
                            const gpu::NativeTexture& native,
                            sk_sp<SkColorSpace> colorSpace) {
#ifdef __APPLE__
  const skgpu::graphite::Recorder* recorder = context.recorder();
  if (recorder && recorder->backend() == skgpu::BackendApi::kMetal) {
    // A texture on another Metal device is one this context cannot reach.
    void* texture = drawsOnDeviceOf(*recorder, native.mtlTexture)
                        ? native.mtlTexture
                        : nullptr;
    return OffscreenSurface(context, texture, native.width, native.height,
                            std::move(colorSpace));
  }
#endif
  if (native.backend == gpu::Backend::Vulkan) {
    VulkanImage image;
    image.image = native.vkImage;
    image.layout = native.vkLayout;
    image.format = native.vkFormat;
    image.width = native.width;
    image.height = native.height;
    image.mipLevels = native.mipLevels;
    return OffscreenSurface(context, image, std::move(colorSpace));
  }
  // A Metal texture on a context that does not record on Metal cannot be
  // painted, and an empty Vulkan image is the one wrap that is always
  // available to fail with.
  return OffscreenSurface(context, VulkanImage{}, std::move(colorSpace));
}

}  // namespace

OffscreenSurface::OffscreenSurface(GraphiteContext& context,
                                   gpu::GpuDevice& device,
                                   gpu::TextureHandle texture,
                                   sk_sp<SkColorSpace> colorSpace)
    : OffscreenSurface(wrapNative(context, device.exportNative(texture),
                                  std::move(colorSpace))) {}

gpu::FenceValue OffscreenSurface::submit(gpu::GpuDevice& device,
                                         gpu::FenceHandle fence) {
  // A moved-from surface submits nothing, so there is nothing for a
  // signal to stand behind and the fence keeps the value it has.
  if (!m_context) return gpu::kFenceInitialValue;
  submit();
  return device.signal(fence);
}

}  // namespace sigil::skia
