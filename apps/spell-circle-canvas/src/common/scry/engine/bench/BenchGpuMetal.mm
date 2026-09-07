/** @file
 * The bench helpers on Metal: the process's own device and a solid
 * texture on it for the updateTexture() arm.
 */

#import <Metal/Metal.h>

#include <sigilcore/hardware/GpuDevice.h>

#include <memory>
#include <vector>

#include "BenchGpu.h"

namespace sigil::scry::bench {

sigil::core::hardware::GpuDevice *gpuDevice() {
  static std::unique_ptr<sigil::core::hardware::GpuDevice> device =
      sigil::core::hardware::GpuDevice::createOwned();
  return device.get();
}

sigil::core::hardware::TextureHandle makeSolidTexture(int width, int height) {
  sigil::core::hardware::GpuDevice *device = gpuDevice();
  if (!device) return {};
  sigil::core::hardware::TextureDesc desc;
  desc.width = width;
  desc.height = height;
  desc.format = sigil::core::hardware::TextureFormat::BGRA8Unorm;
  desc.cpuAccessible = true;
  const sigil::core::hardware::TextureHandle handle = device->createTexture(desc);
  id<MTLTexture> texture = (__bridge id<MTLTexture>)device->exportNative(handle).mtlTexture;
  std::vector<uint32_t> pixels((size_t)width * height, 0xff2266aa);
  [texture replaceRegion:MTLRegionMake2D(0, 0, width, height)
             mipmapLevel:0
               withBytes:pixels.data()
             bytesPerRow:(NSUInteger)width * 4];
  return handle;
}

}  // namespace sigil::scry::bench
