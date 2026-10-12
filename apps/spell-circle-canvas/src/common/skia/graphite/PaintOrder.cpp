// The fence a device whose destination reads cost the depth attachment
// needs to paint a scene in the order it was described, and the cover a
// pass that resumes on that device needs to keep what came before it.
// PaintOrder.h says what goes wrong without either.

#include <gpu/GpuTypes.h>
#include <gpu/graphite/Context.h>
#include <gpu/graphite/Recorder.h>
#include <gpu/graphite/Recording.h>
#include <include/core/SkBlendMode.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPicture.h>
#include <include/core/SkRect.h>
#include <include/core/SkShader.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/PaintOrder.h>
// Semi-private, installed by the vcpkg port: the clip reset a cover takes
// to reach the whole device.
#include <src/core/SkCanvasPriv.h>

#include <optional>

#include "Waits.h"

namespace sigil::skia {

namespace {

/** Whether drawing with @p paint may make the backend read what is
 *  already on the canvas.
 *
 *  A blend the hardware can express is a fixed function of source and
 *  destination and reads nothing. What hardware cannot express is
 *  resolved by reading the destination back into the fragment program:
 *  every blender a paint carries of its own, every mode past the
 *  coefficient ones, and — because a draw's coverage multiplies the
 *  result rather than the source — every coefficient mode outside the
 *  set that survives being scaled by coverage. That set is the answer
 *  below; anything else is treated as a read.
 *
 *  Erring toward a read costs one render pass and never a pixel; missing
 *  one costs the picture, so the list is the conservative one. */
bool readsDestination(const SkPaint& paint) {
  const std::optional<SkBlendMode> mode = paint.asBlendMode();
  if (!mode) return paint.getBlender() != nullptr;
  switch (*mode) {
    case SkBlendMode::kClear:
    case SkBlendMode::kSrc:
    case SkBlendMode::kDst:
    case SkBlendMode::kSrcOver:
    case SkBlendMode::kDstOver:
    case SkBlendMode::kDstOut:
    case SkBlendMode::kSrcATop:
    case SkBlendMode::kXor:
      return false;
    default:
      return true;
  }
}

}  // namespace

PaintOrderCanvas::PaintOrderCanvas(GraphiteContext& context, SkCanvas* target)
    : SkPaintFilterCanvas(target),
      m_context(&context),
      m_target(target),
      m_fencing(needed(context)),
      m_coveredAt{kNeverCovered} {}

bool PaintOrderCanvas::needed(const GraphiteContext& context) {
  const skgpu::graphite::Context* on = context.context();
  return on && on->backend() == skgpu::BackendApi::kVulkan;
}

skgpu::graphite::Recorder* PaintOrderCanvas::recorder() const {
  return m_target->recorder();
}

uint32_t PaintOrderCanvas::recordingsClosed() const {
  return skia::recordingsClosed(m_context->recorder());
}

void PaintOrderCanvas::fence() const {
  skgpu::graphite::Recorder* recorder = m_context->recorder();
  skgpu::graphite::Context* context = m_context->context();
  if (!recorder || !context) return;
  // A snapped recording MUST be inserted: this recorder replays in order,
  // and one dropped recording kills it for the rest of the process.
  std::unique_ptr<skgpu::graphite::Recording> recording = recorder->snap();
  if (!recording) return;
  skgpu::graphite::InsertRecordingInfo insert;
  insert.fRecording = recording.get();
  PendingWaits waits(recorder);
  waits.attach(insert);
  auto lock = m_context->lockContext();
  context->insertRecording(insert);
  ++m_fences;
}

void PaintOrderCanvas::cover() const {
  if (!m_fencing || m_coveredAt.empty()) return;
  const uint32_t now = this->recordingsClosed();
  if (m_coveredAt.back() == now) return;
  m_coveredAt.back() = now;
  // A transparent source over the destination leaves every pixel as it
  // was, and a shader rather than a transparent paint colour keeps the
  // canvas from discarding the draw as one that paints nothing. The clip
  // is lifted for it alone: the pass has to reach every pixel of the
  // device whatever the scene has clipped to.
  SkPaint transparent;
  transparent.setShader(SkShaders::Color(SK_ColorTRANSPARENT));
  m_target->save();
  SkCanvasPriv::ResetClip(m_target);
  m_target->resetMatrix();
  m_target->drawRect(SkRect::Make(m_target->getDeviceClipBounds()),
                     transparent);
  m_target->restore();
}

bool PaintOrderCanvas::onFilter(SkPaint& paint) const {
  if (!m_fencing) return true;
  if (m_pending) {
    m_pending = false;
    this->fence();
  }
  this->cover();
  if (readsDestination(paint)) m_pending = true;
  return true;
}

void PaintOrderCanvas::onDrawPicture(const SkPicture* picture,
                                     const SkMatrix* matrix,
                                     const SkPaint* paint) {
  this->SkCanvas::onDrawPicture(picture, matrix, paint);
}

void PaintOrderCanvas::willSave() {
  m_saves.push_back({.layer = false, .readsDestination = false});
  SkPaintFilterCanvas::willSave();
}

SkCanvas::SaveLayerStrategy PaintOrderCanvas::getSaveLayerStrategy(
    const SaveLayerRec& rec) {
  // A backdrop is read from the device beneath, which ends that device's
  // pass where the layer opens; the device resumes when the layer is
  // composited back, and must be covered then.
  if (m_fencing && rec.fBackdrop && !m_coveredAt.empty())
    m_coveredAt.back() = kNeverCovered;
  m_saves.push_back({.layer = true,
                     .readsDestination = m_fencing && rec.fPaint &&
                                         readsDestination(*rec.fPaint)});
  // A new layer begins cleared, so its first pass loads nothing.
  m_coveredAt.push_back(this->recordingsClosed());
  return SkPaintFilterCanvas::getSaveLayerStrategy(rec);
}

void PaintOrderCanvas::willRestore() {
  if (!m_saves.empty()) {
    const Save closing = m_saves.back();
    m_saves.pop_back();
    m_restoreReadsDestination = closing.readsDestination;
    m_restoreCloses = closing.layer;
    if (closing.layer && m_coveredAt.size() > 1) m_coveredAt.pop_back();
  }
  SkPaintFilterCanvas::willRestore();
}

void PaintOrderCanvas::didRestore() {
  SkPaintFilterCanvas::didRestore();
  // The composite a closing layer draws lands in the device beneath it,
  // which may be resuming a pass; covering it now puts the cover in the
  // same pass as the composite.
  if (m_restoreCloses) {
    m_restoreCloses = false;
    this->cover();
  }
  // The composite a layer's restore draws is the reading draw, so the
  // pass ends after it rather than before it.
  if (m_restoreReadsDestination) {
    m_restoreReadsDestination = false;
    m_pending = true;
  }
}

}  // namespace sigil::skia
