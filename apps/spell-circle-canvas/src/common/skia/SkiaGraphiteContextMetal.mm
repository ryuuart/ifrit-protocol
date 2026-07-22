#import <Metal/Metal.h>

#include "SkiaGraphiteContext.h"

#include <gpu/graphite/Context.h>
#include <gpu/graphite/ContextOptions.h>
#include <gpu/graphite/Recorder.h>
#include <gpu/graphite/mtl/MtlBackendContext.h>

std::unique_ptr<SkiaGraphiteContext>
SkiaGraphiteContext::createMetal(void *mtlDevice, void *mtlCommandQueue) {
  // Qt-free Metal bring-up point, shared by the Qt adapter (which extracts
  // these handles from a QRhi in SkiaQtInteropMetal.mm) and the native
  // macOS app (which owns its device/queue outright).
  if (!mtlDevice || !mtlCommandQueue)
    return nullptr;

  skgpu::graphite::MtlBackendContext backendContext;
  // sk_cfp adopts without retaining, but the caller still owns
  // `mtlDevice`/`mtlCommandQueue` (no ownership is transferred to us here) —
  // this file compiles without ARC (see the __bridge_retained-has-no-effect
  // warning otherwise), so retain Skia its own +1 ref explicitly via
  // CFRetain before adopting, or the context would end up holding an
  // under-retained reference.
  backendContext.fDevice.reset(CFRetain(static_cast<CFTypeRef>(mtlDevice)));
  backendContext.fQueue.reset(
      CFRetain(static_cast<CFTypeRef>(mtlCommandQueue)));

  std::unique_ptr<skgpu::graphite::Context> context =
      skgpu::graphite::ContextFactory::MakeMetal(backendContext, {});
  if (!context)
    return nullptr;

  std::unique_ptr<skgpu::graphite::Recorder> recorder =
      context->makeRecorder(SkiaGraphiteContext::makeRecorderOptions());
  if (!recorder)
    return nullptr;

  return std::unique_ptr<SkiaGraphiteContext>(
      new SkiaGraphiteContext(std::move(context), std::move(recorder)));
}
