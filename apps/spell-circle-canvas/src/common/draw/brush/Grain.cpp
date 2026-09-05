/** @file
 * The grain: a tiled texture turned into the coverage a mark keeps.
 */

#include "Executors.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkColorFilter.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkShader.h>
#include <sigildraw/Pen.h>

#include <algorithm>
#include <utility>

namespace sigil::draw::brush {

namespace {

/** Luminance to coverage: full where the texture is white, down to
 *  one minus the depth where it is black. The rows are the colour
 *  matrix's, and only the alpha row matters — what the grain shader is
 *  used for reads its alpha alone. */
sk_sp<SkColorFilter> coverage(float depth) {
  const float taken = std::clamp(depth, 0.0f, 1.0f);
  const float matrix[20] = {
      0, 0, 0, 0, 0,
      0, 0, 0, 0, 0,
      0, 0, 0, 0, 0,
      0.2126f * taken, 0.7152f * taken, 0.0722f * taken, 0, 1.0f - taken,
  };
  return SkColorFilters::Matrix(matrix);
}

}  // namespace

sk_sp<SkShader> grainShader(const Grain& grain, const SkMatrix& local) {
  if (!grain.image) return nullptr;
  const float scale = std::max(0.001f, grain.scale);
  SkMatrix placement = local;
  placement.preScale(scale, scale);
  sk_sp<SkShader> texture = grain.image->makeShader(
      SkTileMode::kRepeat, SkTileMode::kRepeat,
      SkSamplingOptions(SkFilterMode::kLinear), &placement);
  if (!texture) return nullptr;
  return texture->makeWithColorFilter(coverage(grain.depth));
}

sk_sp<SkShader> throughGrain(sk_sp<SkShader> mark, const Grain& grain,
                             const SkMatrix& local) {
  sk_sp<SkShader> texture = grainShader(grain, local);
  if (!texture) return mark;
  return SkShaders::Blend(SkBlendMode::kDstIn, std::move(mark),
                          std::move(texture));
}

void layerGrain(Pen& pen, const Grain& grain) {
  SkCanvas* canvas = pen.canvas();
  sk_sp<SkShader> texture = grainShader(grain, SkMatrix::I());
  if (!canvas || !texture) return;
  SkPaint paint;
  paint.setShader(std::move(texture));
  paint.setBlendMode(SkBlendMode::kDstIn);
  canvas->drawPaint(paint);
}

}  // namespace sigil::draw::brush
