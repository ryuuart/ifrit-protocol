#import <Metal/Metal.h>

#include "SkiaOffscreenSurface.h"
#include "SkiaGraphiteContext.h"
#include <rhi/qrhi.h>

#include <include/core/SkColorSpace.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/BackendTexture.h>
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Recording.h>
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
  id<MTLTexture> mtlTex = (__bridge id<MTLTexture>)reinterpret_cast<void *>(
      texture->nativeTexture().object);

  const skgpu::graphite::BackendTexture backendTexture =
      skgpu::graphite::BackendTextures::MakeMetal(
          SkISize::Make(pixelSize.width(), pixelSize.height()),
          (__bridge CFTypeRef)mtlTex);

  m_surface = SkSurfaces::WrapBackendTexture(
      context.recorder(), backendTexture, /*colorSpace=*/nullptr,
      /*props=*/nullptr);
}

SkiaOffscreenSurface::~SkiaOffscreenSurface() = default;

SkCanvas *SkiaOffscreenSurface::canvas() const {
  return m_surface ? m_surface->getCanvas() : nullptr;
}

void SkiaOffscreenSurface::submit() {
  auto *recorder = m_context.recorder();
  auto *context = m_context.context();
  if (!recorder || !context)
    return;

  std::unique_ptr<skgpu::graphite::Recording> recording = recorder->snap();
  if (!recording)
    return;

  skgpu::graphite::InsertRecordingInfo info;
  info.fRecording = recording.get();
  context->insertRecording(info);
  // Asynchronous submit: the Graphite context was built on Qt's own
  // MTLCommandQueue (see SkiaGraphiteContext::create), and Metal executes
  // command buffers on one queue in enqueue order — Qt's subsequent render
  // pass (preview blit, mipmap generation, Syphon publish) is committed
  // after this and therefore observes the finished texture without any CPU
  // wait. Blocking here (SyncToCpu::kYes) used to charge the whole GPU
  // frame — dominated by glyph rendering — to the render thread's prePaint.
  context->submit(skgpu::graphite::SubmitInfo(skgpu::graphite::SyncToCpu::kNo));
  // Lets Graphite reclaim resources of previously finished submissions
  // without ever blocking.
  context->checkAsyncWorkCompletion();
}
