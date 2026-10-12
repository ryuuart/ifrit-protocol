// Metal bring-up of GraphiteContext: Graphite on an id<MTLDevice> and
// id<MTLCommandQueue> the caller owns, so Graphite's submissions and the
// caller's own command buffers execute in order on that one queue.

#import <Metal/Metal.h>

#include <gpu/graphite/Context.h>
#include <gpu/graphite/ContextOptions.h>
#include <gpu/graphite/Recorder.h>
#include <gpu/graphite/mtl/MtlBackendContext.h>
#include <sigilskia/graphite/GraphiteContext.h>

namespace sigil::skia {

std::unique_ptr<GraphiteContext> GraphiteContext::createMetal(void *mtlDevice,
                                                              void *mtlCommandQueue) {
  if (!mtlDevice || !mtlCommandQueue) return nullptr;

  skgpu::graphite::MtlBackendContext backendContext;
  // sk_cfp adopts without retaining, but the caller still owns
  // `mtlDevice`/`mtlCommandQueue` (no ownership is transferred here) —
  // this file compiles without ARC, so give Skia its own +1 ref explicitly
  // via CFRetain before adopting, or the context would end up holding an
  // under-retained reference.
  backendContext.fDevice.reset(CFRetain(static_cast<CFTypeRef>(mtlDevice)));
  backendContext.fQueue.reset(CFRetain(static_cast<CFTypeRef>(mtlCommandQueue)));

  std::unique_ptr<skgpu::graphite::Context> context =
      skgpu::graphite::ContextFactory::MakeMetal(backendContext, makeContextOptions());
  if (!context) return nullptr;

  // The device every recorder draws on is named to `deviceOf`, so a
  // texture made on another API over the same device can be told apart
  // from one that is not on it before a draw samples it.
  std::unique_ptr<GraphiteContext> made(new GraphiteContext(std::move(context), mtlDevice));
  if (!made->recorder()) return nullptr;
  return made;
}

}  // namespace sigil::skia
