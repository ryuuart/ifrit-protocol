// Metal wrap of OffscreenSurface: an id<MTLTexture> — an offscreen canvas
// texture or a CAMetalLayer drawable alike — as an SkSurface on a Metal
// GraphiteContext.

#import <Metal/Metal.h>

#include <include/core/SkColorSpace.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/BackendTexture.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Surface.h>
#include <include/gpu/graphite/mtl/MtlGraphiteTypes_cpp.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/OffscreenSurface.h>

namespace sigil::skia {

OffscreenSurface::OffscreenSurface(GraphiteContext &context, void *mtlTexture, int width,
                                   int height)
    : m_context(&context) {
  if (!mtlTexture) return;

  const skgpu::graphite::BackendTexture backendTexture =
      skgpu::graphite::BackendTextures::MakeMetal(SkISize::Make(width, height),
                                                  static_cast<CFTypeRef>(mtlTexture));

  m_surface = SkSurfaces::WrapBackendTexture(context.recorder(), backendTexture,
                                             /*colorSpace=*/nullptr,
                                             /*props=*/nullptr);
}

}  // namespace sigil::skia
