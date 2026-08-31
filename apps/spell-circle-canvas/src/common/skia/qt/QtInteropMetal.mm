// Qt -> Metal adapter over the Qt-free Graphite bring-up: extracts the
// native device, queue and texture handles a QRhi is built on and forwards
// to GraphiteContext::createMetal() and the Metal OffscreenSurface
// constructor. Qt keeps ownership of every handle.

#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>
#include <sigilskia/qt/QtInterop.h>

#include <QSize>

namespace sigil::skia {

std::unique_ptr<GraphiteContext> createGraphiteContext(QRhi *rhi) {
  // One graphics API per build: null for any backend this adapter cannot
  // serve, so callers never assume Metal.
  if (!rhi || rhi->backend() != QRhi::Metal) return nullptr;

  const auto *nativeHandles = static_cast<const QRhiMetalNativeHandles *>(rhi->nativeHandles());
  // qrhi_platform.h declares MTLDevice/MTLCommandQueue as opaque types; Qt
  // keeps ownership of both (createMetal retains its own references).
  return GraphiteContext::createMetal(nativeHandles->dev, nativeHandles->cmdQueue);
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

OffscreenSurface wrapTexture(GraphiteContext &context, QRhiTexture *texture, QSize pixelSize) {
  return OffscreenSurface(context, nativeTextureHandle(texture), pixelSize.width(),
                          pixelSize.height());
}

}  // namespace sigil::skia
