#import <Metal/Metal.h>

#include <sigilskia/device/GpuDevice.h>

#include <memory>
#include <vector>

#include "BenchGpu.h"

namespace sigil::scry::bench {

sigil::skia::GpuDevice *gpuDevice() {
  static std::unique_ptr<sigil::skia::GpuDevice> device =
      sigil::skia::GpuDevice::createOwned(sigil::skia::Backend::Metal);
  return device.get();
}

sigil::skia::TextureHandle makeSolidTexture(int width, int height) {
  sigil::skia::GpuDevice *device = gpuDevice();
  if (!device) return {};
  sigil::skia::TextureDesc desc;
  desc.width = width;
  desc.height = height;
  desc.format = sigil::skia::TextureFormat::BGRA8Unorm;
  desc.cpuAccessible = true;
  const sigil::skia::TextureHandle handle = device->createTexture(desc);
  id<MTLTexture> texture = (__bridge id<MTLTexture>)device->exportNative(handle).mtlTexture;
  std::vector<uint32_t> pixels((size_t)width * height, 0xff2266aa);
  [texture replaceRegion:MTLRegionMake2D(0, 0, width, height)
             mipmapLevel:0
               withBytes:pixels.data()
             bytesPerRow:(NSUInteger)width * 4];
  return handle;
}

}  // namespace sigil::scry::bench
