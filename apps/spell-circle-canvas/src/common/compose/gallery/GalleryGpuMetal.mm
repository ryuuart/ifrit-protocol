#import <Metal/Metal.h>

#include "GalleryGpu.h"
#include "SkiaGraphiteContext.h"

namespace compose_gallery {

std::unique_ptr<SkiaGraphiteContext> makeHeadlessGraphite() {
  static id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  static id<MTLCommandQueue> queue = [device newCommandQueue];
  if (!device || !queue)
    return nullptr;
  return SkiaGraphiteContext::createMetal((__bridge void *)device,
                                          (__bridge void *)queue);
}

} // namespace compose_gallery
