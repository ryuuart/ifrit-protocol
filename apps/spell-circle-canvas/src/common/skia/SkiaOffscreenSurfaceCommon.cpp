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

#include <cstdio>

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
  const skgpu::graphite::InsertStatus status =
      context->insertRecording(recordingInfo);
  if (!status) {
    // Once per process, loudly: a failed insert never rendered this frame,
    // and under fRequireOrderedRecordings a skipped recording breaks the
    // recorder's chain permanently — every later frame silently no-renders
    // (src/sigilweave/docs/graphite_ordering_audit.md).
    static bool warned = false;
    if (!warned) {
      warned = true;
      std::fprintf(
          stderr,
          "[SpellCircleSkia] insertRecording failed (status %d%s%s); this "
          "frame did not render\n",
          static_cast<int>(
              static_cast<skgpu::graphite::InsertStatus::V>(status)),
          status.message().empty() ? "" : ": ",
          status.message().c_str());
    }
    // Still pump completion: this is the only place the process retires
    // finished GPU work, and a permanently failing chain must not also
    // leak every outstanding submission. Non-blocking.
    context->checkAsyncWorkCompletion();
    return;
  }
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
