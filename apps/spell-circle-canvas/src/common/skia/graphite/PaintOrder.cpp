// The fence a device whose destination reads cost the depth attachment
// needs to paint a scene in the order it was described. PaintOrder.h says
// what goes wrong without it.

#include <gpu/GpuTypes.h>
#include <gpu/graphite/Context.h>
#include <gpu/graphite/Recorder.h>
#include <gpu/graphite/Recording.h>
#include <include/core/SkBlendMode.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPicture.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/PaintOrder.h>

#include <optional>

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
      m_fencing(needed(context)) {}

bool PaintOrderCanvas::needed(const GraphiteContext& context) {
  const skgpu::graphite::Context* on = context.context();
  return on && on->backend() == skgpu::BackendApi::kVulkan;
}

skgpu::graphite::Recorder* PaintOrderCanvas::recorder() const {
  return m_target->recorder();
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
  auto lock = m_context->lockContext();
  context->insertRecording(insert);
  ++m_fences;
}

bool PaintOrderCanvas::onFilter(SkPaint& paint) const {
  if (!m_fencing) return true;
  if (m_pending) {
    m_pending = false;
    this->fence();
  }
  if (readsDestination(paint)) m_pending = true;
  return true;
}

void PaintOrderCanvas::onDrawPicture(const SkPicture* picture,
                                     const SkMatrix* matrix,
                                     const SkPaint* paint) {
  this->SkCanvas::onDrawPicture(picture, matrix, paint);
}

void PaintOrderCanvas::willSave() {
  m_layerReadsDestination.push_back(false);
  SkPaintFilterCanvas::willSave();
}

SkCanvas::SaveLayerStrategy PaintOrderCanvas::getSaveLayerStrategy(
    const SaveLayerRec& rec) {
  m_layerReadsDestination.push_back(m_fencing && rec.fPaint &&
                                    readsDestination(*rec.fPaint));
  return SkPaintFilterCanvas::getSaveLayerStrategy(rec);
}

void PaintOrderCanvas::willRestore() {
  if (!m_layerReadsDestination.empty()) {
    m_restoreReadsDestination = m_layerReadsDestination.back();
    m_layerReadsDestination.pop_back();
  }
  SkPaintFilterCanvas::willRestore();
}

void PaintOrderCanvas::didRestore() {
  SkPaintFilterCanvas::didRestore();
  // The composite a layer's restore draws is the reading draw, so the
  // pass ends after it rather than before it.
  if (m_restoreReadsDestination) {
    m_restoreReadsDestination = false;
    m_pending = true;
  }
}

}  // namespace sigil::skia
