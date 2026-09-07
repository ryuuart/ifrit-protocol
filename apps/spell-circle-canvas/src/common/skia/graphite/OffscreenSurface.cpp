// Graphics-API-independent half of OffscreenSurface: everything after the
// wrap. The wrapping constructors are per API — OffscreenSurfaceMetal.mm
// and OffscreenSurfaceVulkan.cpp — one for each factory GraphiteContext
// has.

#include <include/core/SkSurface.h>
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Recording.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/OffscreenSurface.h>
#include <sigilskia/graphite/PaintOrder.h>

#include <cstdio>
#include <memory>
#include <mutex>
#include <utility>

namespace sigil::skia {

// A MOVED-FROM SURFACE DOES NOTHING. A defaulted move would leave the
// source holding the context it was built on, so its `submit()` would go
// on snapping and inserting the recorder's work — work that belongs to
// whoever the surface moved into. Wraps are returned by value, so the
// move is an ordinary path and not a corner.
OffscreenSurface::OffscreenSurface(OffscreenSurface&& other) noexcept
    : m_context(std::exchange(other.m_context, nullptr)),
      m_surface(std::move(other.m_surface)),
      m_ordered(std::move(other.m_ordered)) {}

OffscreenSurface::~OffscreenSurface() = default;

SkCanvas* OffscreenSurface::canvas() const {
  if (!m_surface) return nullptr;
  if (!m_context) return m_surface->getCanvas();
  if (!m_ordered)
    m_ordered =
        std::make_unique<PaintOrderCanvas>(*m_context, m_surface->getCanvas());
  return m_ordered.get();
}

SkSurface* OffscreenSurface::surface() const { return m_surface.get(); }

void OffscreenSurface::submit() {
  if (!m_context) return;
  auto* recorder = m_context->recorder();
  auto* context = m_context->context();
  if (!recorder || !context) return;

  std::unique_ptr<skgpu::graphite::Recording> recording = recorder->snap();
  if (!recording) return;

  // The context may be shared with another thread's recorder; every call
  // on it below is under the context's own lock.
  const std::unique_lock<std::mutex> lock = m_context->lockContext();

  skgpu::graphite::InsertRecordingInfo recordingInfo;
  recordingInfo.fRecording = recording.get();
  const skgpu::graphite::InsertStatus status =
      context->insertRecording(recordingInfo);
  if (!status) {
    // Once per process, loudly: a failed insert never rendered this frame,
    // and under fRequireOrderedRecordings a skipped recording breaks the
    // recorder's chain permanently — every later frame silently no-renders.
    static bool warned = false;
    if (!warned) {
      warned = true;
      std::fprintf(stderr,
                   "[SigilSkia] insertRecording failed (status %d%s%s); this "
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
  // Asynchronous submit: the Graphite context was built on the host's own
  // command queue (Metal command queue or Vulkan graphics queue alike), and
  // both APIs execute work on one queue in submission order — the host's
  // subsequent render pass (preview blit, mipmap generation, Syphon/Spout
  // publish) is committed after this and therefore observes the finished
  // texture without any CPU wait. Blocking here (SyncToCpu::kYes) would
  // charge the whole GPU frame — dominated by glyph rendering — to the
  // thread that submits.
  context->submit(skgpu::graphite::SubmitInfo(skgpu::graphite::SyncToCpu::kNo));
  // Lets Graphite reclaim resources of previously finished submissions
  // without ever blocking.
  context->checkAsyncWorkCompletion();
}

}  // namespace sigil::skia
