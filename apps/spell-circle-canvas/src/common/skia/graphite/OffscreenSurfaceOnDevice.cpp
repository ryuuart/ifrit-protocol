// The OffscreenSurface entry points that name a texture and a fence by
// handle: declared by the graphite feature, defined here because a
// GpuDevice is what they read and a host holding one links this feature
// already. Dependencies keep pointing one way — the graphite feature
// stays below the device.

#include <sigilskia/device/GpuDevice.h>
#include <sigilskia/graphite/OffscreenSurface.h>

namespace sigil::skia {

namespace {

/** The wrap the texture's own API needs, over the native objects behind
 *  a handle. An empty export — what a stale or null handle yields — has
 *  no texture in it, so the wrap fails and `canvas()` stays null. */
OffscreenSurface wrapNative(GraphiteContext& context,
                            const NativeTexture& native) {
  if (native.backend == Backend::Vulkan) {
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

OffscreenSurface::OffscreenSurface(GraphiteContext& context, GpuDevice& device,
                                   TextureHandle texture)
    : OffscreenSurface(wrapNative(context, device.exportNative(texture))) {}

FenceValue OffscreenSurface::submit(GpuDevice& device, FenceHandle fence) {
  submit();
  return device.signal(fence);
}

}  // namespace sigil::skia
