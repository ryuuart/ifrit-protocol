#import <Metal/Metal.h>

#include "BenchGpu.h"

#include <vector>

namespace sigil::scry::bench {

void *gpuDevice() {
  static id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  return (__bridge void *)device;
}

void *gpuQueue() {
  static id<MTLCommandQueue> queue =
      [(__bridge id<MTLDevice>)gpuDevice() newCommandQueue];
  return (__bridge void *)queue;
}

void *makeSolidTexture(int width, int height) {
  MTLTextureDescriptor *desc = [MTLTextureDescriptor
      texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                   width:width
                                  height:height
                               mipmapped:NO];
  desc.storageMode = MTLStorageModeShared;
  id<MTLTexture> texture =
      [(__bridge id<MTLDevice>)gpuDevice() newTextureWithDescriptor:desc];
  std::vector<uint32_t> pixels((size_t)width * height, 0xff2266aa);
  [texture replaceRegion:MTLRegionMake2D(0, 0, width, height)
             mipmapLevel:0
               withBytes:pixels.data()
             bytesPerRow:(NSUInteger)width * 4];
  return (__bridge_retained void *)texture;
}

} // namespace sigil::scry::bench
