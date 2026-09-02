// The OffscreenSurface entry points that name a texture and a fence by
// handle. Dependencies point one way: Graphite stands on the hardware
// device, and the hardware device knows nothing of Skia.

#include <sigilcore/hardware/GpuDevice.h>
#include <sigilskia/graphite/OffscreenSurface.h>

namespace sigil::skia {

namespace {

/** The wrap the texture's own API needs, over the native objects behind
 *  a handle. An empty export — what a stale or null handle yields — has
 *  no texture in it, so the wrap fails and `canvas()` stays null. */
OffscreenSurface wrapNative(GraphiteContext& context,
                            const core::hardware::NativeTexture& native) {
  if (native.backend == core::hardware::Backend::Vulkan) {
    VulkanImage image;
    image.image = native.vkImage;
    image.layout = native.vkLayout;
    image.format = native.vkFormat;
    image.width = native.width;
    image.height = native.height;
    return OffscreenSurface(context, image);
  }
#ifdef __APPLE__
  return OffscreenSurface(context, native.mtlTexture, native.width,
                          native.height);
#else
  // A Metal texture cannot exist here, and an empty Vulkan image is the
  // one wrap that is always available to fail with.
  return OffscreenSurface(context, VulkanImage{});
#endif
}

}  // namespace

OffscreenSurface::OffscreenSurface(GraphiteContext& context,
                                   core::hardware::GpuDevice& device,
                                   core::hardware::TextureHandle texture)
    : OffscreenSurface(wrapNative(context, device.exportNative(texture))) {}

core::hardware::FenceValue OffscreenSurface::submit(
    core::hardware::GpuDevice& device, core::hardware::FenceHandle fence) {
  submit();
  return device.signal(fence);
}

}  // namespace sigil::skia
