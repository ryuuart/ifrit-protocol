// Graphics-API-independent half of SkiaOffscreenSurface. The constructor —
// wrapping a QRhiTexture's native texture in an SkSurface — is per-API:
// exactly one of SkiaOffscreenSurfaceMetal.mm or
// SkiaOffscreenSurfaceVulkan.cpp is compiled into a given build (see
// CMakeLists.txt), matching the SkiaGraphiteContext create() TU.

#include "SkiaGraphiteContext.h"
#include "SkiaOffscreenSurface.h"

#include <include/core/SkSurface.h>
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Recording.h>

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

  skgpu::graphite::InsertRecordingInfo recordingInfo;
  recordingInfo.fRecording = recording.get();
  context->insertRecording(recordingInfo);
  // Asynchronous submit: the Graphite context was built on Qt's own command
  // queue (see SkiaGraphiteContext::create — Metal command queue or Vulkan
  // graphics queue alike), and both APIs execute work on one queue in
  // submission order — Qt's subsequent render pass (preview blit, mipmap
  // generation, Syphon/Spout publish) is committed after this and therefore
  // observes the finished texture without any CPU wait. Blocking here
  // (SyncToCpu::kYes) used to charge the whole GPU frame — dominated by
  // glyph rendering — to the render thread's prePaint.
  context->submit(skgpu::graphite::SubmitInfo(skgpu::graphite::SyncToCpu::kNo));
  // Lets Graphite reclaim resources of previously finished submissions
  // without ever blocking.
  context->checkAsyncWorkCompletion();
}
