#import <Metal/Metal.h>

#include <sigilskia/graphite/GraphiteContext.h>

#include "GalleryGpu.h"

namespace compose_gallery {

std::unique_ptr<sigil::skia::GraphiteContext> makeHeadlessGraphite() {
  static id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  static id<MTLCommandQueue> queue = [device newCommandQueue];
  if (!device || !queue) return nullptr;
  return sigil::skia::GraphiteContext::createMetal((__bridge void *)device, (__bridge void *)queue);
}

}  // namespace compose_gallery
