#import <Metal/Metal.h>

#include "SkiaGraphiteContext.h"
#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>

#include <gpu/graphite/Context.h>
#include <gpu/graphite/ContextOptions.h>
#include <gpu/graphite/Recorder.h>
#include <gpu/graphite/mtl/MtlBackendContext.h>

SkiaGraphiteContext::SkiaGraphiteContext(
    std::unique_ptr<skgpu::graphite::Context> context,
    std::unique_ptr<skgpu::graphite::Recorder> recorder)
    : m_context(std::move(context)), m_recorder(std::move(recorder)) {}

SkiaGraphiteContext::~SkiaGraphiteContext() = default;

std::unique_ptr<SkiaGraphiteContext> SkiaGraphiteContext::create(QRhi *rhi) {
  // This factory is the single Graphite bring-up point: it inspects the
  // QRhi backend and returns null for anything it cannot serve, so callers
  // never assume Metal. A Vulkan/Dawn port adds a branch here (Vulkan:
  // QRhiVulkanNativeHandles → skgpu::graphite::VulkanBackendContext →
  // ContextFactory::MakeVulkan) plus the matching texture-wrap branch in
  // SkiaOffscreenSurface — nothing outside src/platform/skia changes.
  if (!rhi || rhi->backend() != QRhi::Metal)
    return nullptr;

  const auto *nativeHandles =
      static_cast<const QRhiMetalNativeHandles *>(rhi->nativeHandles());
  // qrhi_platform.h forward-declares MTLDevice/MTLCommandQueue as opaque C
  // structs; __bridge recasts without touching ARC ownership (matches
  // SyphonBridge::start()).
  id<MTLDevice> device = (__bridge id<MTLDevice>)nativeHandles->dev;
  id<MTLCommandQueue> queue =
      (__bridge id<MTLCommandQueue>)nativeHandles->cmdQueue;

  skgpu::graphite::MtlBackendContext backendContext;
  // sk_cfp adopts without retaining, but Qt still owns `device`/`queue` (no
  // ownership is transferred to us here) — this file compiles without ARC
  // (see the __bridge_retained-has-no-effect warning otherwise), so retain
  // Skia its own +1 ref explicitly via CFRetain before adopting, or the
  // context would end up holding an under-retained reference.
  backendContext.fDevice.reset(CFRetain((__bridge CFTypeRef)device));
  backendContext.fQueue.reset(CFRetain((__bridge CFTypeRef)queue));

  std::unique_ptr<skgpu::graphite::Context> context =
      skgpu::graphite::ContextFactory::MakeMetal(backendContext, {});
  if (!context)
    return nullptr;

  std::unique_ptr<skgpu::graphite::Recorder> recorder = context->makeRecorder();
  if (!recorder)
    return nullptr;

  return std::unique_ptr<SkiaGraphiteContext>(
      new SkiaGraphiteContext(std::move(context), std::move(recorder)));
}
