// Qt -> Metal adapter for the Qt-free Graphite plumbing in this directory:
// extracts the native device/queue/texture handles QRhi is built on and
// forwards to SkiaGraphiteContext::createMetal() /
// SkiaOffscreenSurface(void*, int, int). The Vulkan siblings
// (SkiaGraphiteContextVulkan.cpp / SkiaOffscreenSurfaceVulkan.cpp) still
// implement the Qt entry points directly; a Windows bring-up would follow
// this split if a Qt-free Vulkan consumer ever appears.

#include "SkiaGraphiteContext.h"
#include "SkiaOffscreenSurface.h"

#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>
#include <QSize>

std::unique_ptr<SkiaGraphiteContext> SkiaGraphiteContext::create(QRhi *rhi) {
  // This factory is the single Graphite bring-up point per graphics API: it
  // inspects the QRhi backend and returns null for anything it cannot
  // serve, so callers never assume Metal.
  if (!rhi || rhi->backend() != QRhi::Metal) return nullptr;

  const auto *nativeHandles = static_cast<const QRhiMetalNativeHandles *>(rhi->nativeHandles());
  // qrhi_platform.h declares MTLDevice/MTLCommandQueue as opaque types; Qt
  // keeps ownership of both (createMetal retains its own references).
  return createMetal(nativeHandles->dev, nativeHandles->cmdQueue);
}

namespace {

// Qt packs the id<MTLTexture> pointer into a quint64 on Metal; the handle
// comes back as an integer and is only ever handed on as an opaque pointer.
void *nativeTextureHandle(QRhiTexture *texture) {
  if (!texture) return nullptr;
  // NOLINTNEXTLINE(performance-no-int-to-ptr): Qt hands the handle over as an integer
  return reinterpret_cast<void *>(texture->nativeTexture().object);
}

}  // namespace

SkiaOffscreenSurface::SkiaOffscreenSurface(SkiaGraphiteContext &context, QRhiTexture *texture,
                                           QSize pixelSize)
    : SkiaOffscreenSurface(context, nativeTextureHandle(texture), pixelSize.width(),
                           pixelSize.height()) {}
