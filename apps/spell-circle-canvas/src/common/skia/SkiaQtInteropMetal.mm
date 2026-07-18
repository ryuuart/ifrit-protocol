// Qt -> Metal adapter for the Qt-free Graphite plumbing in this directory:
// extracts the native device/queue/texture handles QRhi is built on and
// forwards to SkiaGraphiteContext::createMetal() /
// SkiaOffscreenSurface(void*, int, int). The Vulkan siblings
// (SkiaGraphiteContextVulkan.cpp / SkiaOffscreenSurfaceVulkan.cpp) still
// implement the Qt entry points directly; a Windows bring-up would follow
// this split if a Qt-free Vulkan consumer ever appears.

#include "SkiaGraphiteContext.h"
#include "SkiaOffscreenSurface.h"

#include <QSize>
#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>

std::unique_ptr<SkiaGraphiteContext> SkiaGraphiteContext::create(QRhi *rhi) {
  // This factory is the single Graphite bring-up point per graphics API: it
  // inspects the QRhi backend and returns null for anything it cannot
  // serve, so callers never assume Metal.
  if (!rhi || rhi->backend() != QRhi::Metal)
    return nullptr;

  const auto *nativeHandles =
      static_cast<const QRhiMetalNativeHandles *>(rhi->nativeHandles());
  // qrhi_platform.h declares MTLDevice/MTLCommandQueue as opaque types; Qt
  // keeps ownership of both (createMetal retains its own references).
  return createMetal(nativeHandles->dev, nativeHandles->cmdQueue);
}

SkiaOffscreenSurface::SkiaOffscreenSurface(SkiaGraphiteContext &context,
                                           QRhiTexture *texture,
                                           QSize pixelSize)
    // Qt packs the id<MTLTexture> pointer into a quint64 on Metal (same
    // pattern as SyphonBridge::publishFrame).
    : SkiaOffscreenSurface(
          context,
          texture ? reinterpret_cast<void *>(texture->nativeTexture().object)
                  : nullptr,
          pixelSize.width(), pixelSize.height()) {}
