#import <Metal/Metal.h>

#include "ComposeBenchGpu.h"

namespace ifrit::compose::bench {

void *gpuDevice() {
  static id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  return (__bridge void *)device;
}

void *gpuQueue() {
  static id<MTLCommandQueue> queue =
      [(__bridge id<MTLDevice>)gpuDevice() newCommandQueue];
  return (__bridge void *)queue;
}

} // namespace ifrit::compose::bench
