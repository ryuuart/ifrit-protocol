/** @file
 * The headless device on Metal.
 */

#import <Metal/Metal.h>

#include <sigilskia/graphite/GraphiteContext.h>

#include "sigilsketch/plate/Graphite.h"

namespace sigil::sketch {

std::unique_ptr<skia::GraphiteContext> headlessGraphite() {
  // Held for the process: every surface, texture and pipeline the
  // context makes is the device's, and outliving the device is not a
  // thing any of them may do.
  static id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  static id<MTLCommandQueue> queue = [device newCommandQueue];
  if (!device || !queue) return nullptr;
  return skia::GraphiteContext::createMetal((__bridge void *)device, (__bridge void *)queue);
}

}  // namespace sigil::sketch
