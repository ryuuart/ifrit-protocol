/** @file
 * A post pass on the CPU: the images it reads, softened, graded or laid
 * one over another, and — where a selection is realised as coverage —
 * painted back over the picture only where that coverage stands.
 */

#include <include/core/SkBlendMode.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColorFilter.h>
#include <include/core/SkPaint.h>
#include <include/core/SkSamplingOptions.h>
#include <include/effects/SkColorMatrix.h>
#include <include/effects/SkImageFilters.h>

#include <variant>
#include <vector>

#include "Cpu.h"

namespace sigil::world::cpu {

namespace {

constexpr SkSamplingOptions kSampling;

/** Gain and lift per channel, then the tint — a grade written as the
 *  one matrix Skia applies in a single pass. */
sk_sp<SkColorFilter> gradeOf(const Levels& levels) {
  SkColorMatrix matrix;
  matrix.setScale(levels.gain * levels.tint.fR, levels.gain * levels.tint.fG,
                  levels.gain * levels.tint.fB, 1.0f);
  matrix.postTranslate(levels.lift * levels.tint.fR,
                       levels.lift * levels.tint.fG,
                       levels.lift * levels.tint.fB, 0.0f);
  return SkColorFilters::Matrix(matrix);
}

/** The operation, painted onto @p canvas from @p layers. The first layer is
 *  what a single-image operation reads; a composite lays every layer after it
 *  on top under its own mode and opacity. */
void paintOperation(SkCanvas& canvas, const PostOperation& operation,
                    const std::vector<sk_sp<SkImage>>& layers) {
  SkPaint paint;
  if (const Blur* blur = std::get_if<Blur>(&operation))
    paint.setImageFilter(
        SkImageFilters::Blur(blur->sigma, blur->sigma, nullptr));
  else if (const Levels* levels = std::get_if<Levels>(&operation))
    paint.setColorFilter(gradeOf(*levels));
  canvas.drawImage(layers.front(), 0, 0, kSampling, &paint);

  const Composite* composite = std::get_if<Composite>(&operation);
  if (!composite) return;
  for (size_t i = 1; i < layers.size(); ++i) {
    SkPaint over;
    over.setBlendMode(composite->mode);
    over.setAlphaf(composite->opacity);
    canvas.drawImage(layers[i], 0, 0, kSampling, &over);
  }
}

}  // namespace

void applyPost(const PassWork& work, Targets& targets) {
  const Pass& pass = *work.pass;
  const std::string* name = target(pass);
  if (!name) return;

  // The layers are taken BEFORE the target is cleared, because a pass
  // may write what it reads: clearing first would hand the operation its own
  // blank target instead of the picture.
  std::vector<sk_sp<SkImage>> layers;
  for (const std::string& read : pass.reads()) {
    if (read == work.coverageIn) continue;
    if (sk_sp<SkImage> image = targets.image(read))
      layers.push_back(std::move(image));
  }
  for (const std::string& before : pass.previous()) {
    if (sk_sp<SkImage> image = targets.previous(before))
      layers.push_back(std::move(image));
  }
  sk_sp<SkImage> coverage;
  if (!work.coverageIn.empty()) coverage = targets.image(work.coverageIn);

  SkCanvas* canvas = targets.canvas(*name);
  if (!canvas) return;
  canvas->clear(SkColor4f{0.0f, 0.0f, 0.0f, 0.0f});
  if (layers.empty()) return;

  if (!coverage) {
    paintOperation(*canvas, pass.operation(), layers);
    return;
  }
  // Masked: the picture stands everywhere, and the operation reaches it only
  // where the coverage does.
  canvas->drawImage(layers.front(), 0, 0, kSampling, nullptr);
  canvas->saveLayer(nullptr, nullptr);
  paintOperation(*canvas, pass.operation(), layers);
  SkPaint keep;
  keep.setBlendMode(SkBlendMode::kDstIn);
  canvas->drawImage(coverage, 0, 0, kSampling, &keep);
  canvas->restore();
}

}  // namespace sigil::world::cpu
