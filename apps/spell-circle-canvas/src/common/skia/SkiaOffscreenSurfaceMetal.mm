#import <Metal/Metal.h>

#include "SkiaGraphiteContext.h"
#include "SkiaOffscreenSurface.h"

#include <include/core/SkColorSpace.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/BackendTexture.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Surface.h>
#include <include/gpu/graphite/mtl/MtlGraphiteTypes_cpp.h>

SkiaOffscreenSurface::SkiaOffscreenSurface(SkiaGraphiteContext &context,
                                           void *mtlTexture, int width,
                                           int height)
    : m_context(context) {
  // Qt-free Metal wrap, shared by the Qt adapter (which unpacks the
  // id<MTLTexture> from a QRhiTexture in SkiaQtInteropMetal.mm) and the
  // native macOS app (offscreen canvas textures and CAMetalLayer
  // drawables alike).
  if (!mtlTexture)
    return;

  const skgpu::graphite::BackendTexture backendTexture =
      skgpu::graphite::BackendTextures::MakeMetal(
          SkISize::Make(width, height), static_cast<CFTypeRef>(mtlTexture));

  m_surface = SkSurfaces::WrapBackendTexture(context.recorder(), backendTexture,
                                             /*colorSpace=*/nullptr,
                                             /*props=*/nullptr);
}
