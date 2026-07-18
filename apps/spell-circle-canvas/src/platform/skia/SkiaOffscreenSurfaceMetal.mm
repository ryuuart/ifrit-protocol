#import <Metal/Metal.h>

#include "SkiaGraphiteContext.h"
#include "SkiaOffscreenSurface.h"
#include <rhi/qrhi.h>

#include <include/core/SkColorSpace.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/BackendTexture.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Surface.h>
#include <include/gpu/graphite/mtl/MtlGraphiteTypes_cpp.h>

SkiaOffscreenSurface::SkiaOffscreenSurface(SkiaGraphiteContext &context,
                                           QRhiTexture *texture,
                                           QSize pixelSize)
    : m_context(context) {
  if (!texture)
    return;

  // Qt packs the id<MTLTexture> pointer into a quint64 on Metal (same
  // pattern as SyphonBridge::publishFrame).
  id<MTLTexture> metalTexture =
      (__bridge id<MTLTexture>)reinterpret_cast<void *>(
          texture->nativeTexture().object);

  const skgpu::graphite::BackendTexture backendTexture =
      skgpu::graphite::BackendTextures::MakeMetal(
          SkISize::Make(pixelSize.width(), pixelSize.height()),
          (__bridge CFTypeRef)metalTexture);

  m_surface = SkSurfaces::WrapBackendTexture(context.recorder(), backendTexture,
                                             /*colorSpace=*/nullptr,
                                             /*props=*/nullptr);
}
