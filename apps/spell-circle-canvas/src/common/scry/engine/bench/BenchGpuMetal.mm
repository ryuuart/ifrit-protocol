/** @file
 * The bench helpers on Metal: the process's own device and a solid
 * texture on it for the updateTexture() arm.
 */

#import <Metal/Metal.h>

#include <sigilgpu/GpuDevice.h>

#include <memory>
#include <vector>

#include "BenchGpu.h"

namespace sigil::scry::bench {

sigil::gpu::GpuDevice *gpuDevice() {
  static std::unique_ptr<sigil::gpu::GpuDevice> device =
      sigil::gpu::GpuDevice::createOwned().value_or(nullptr);
  return device.get();
}

sigil::gpu::TextureHandle makeSolidTexture(int width, int height) {
  sigil::gpu::GpuDevice *device = gpuDevice();
  if (!device) return {};
  sigil::gpu::TextureDescription desc;
  desc.width = width;
  desc.height = height;
  desc.format = sigil::gpu::TextureFormat::BGRA8Unorm;
  desc.cpuAccessible = true;
  const sigil::gpu::TextureHandle handle = device->createTexture(desc);
  id<MTLTexture> texture = (__bridge id<MTLTexture>)device->exportNative(handle).mtlTexture;
  std::vector<uint32_t> pixels((size_t)width * height, 0xff2266aa);
  [texture replaceRegion:MTLRegionMake2D(0, 0, width, height)
             mipmapLevel:0
               withBytes:pixels.data()
             bytesPerRow:(NSUInteger)width * 4];
  return handle;
}

}  // namespace sigil::scry::bench
