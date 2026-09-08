/** @file
 * THE TWO BLURS: the directional one, Skia's separable Gaussian along an
 * axis, and the parametric one, a small pyramid of constant-sigma blurs
 * a map mixes between — a fixed number of passes whatever range is asked
 * for, where a spatially-varying kernel would pay its widest radius at
 * every pixel.
 */

#include <include/core/SkMatrix.h>
#include <include/core/SkRect.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkTypes.h>  // SkDebugf
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilshaders/MaterialSkia.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "EffectInternal.h"

namespace sigil::material::skia {

/** directionalBlur's filter, built from stock Skia filters rather than a
 *  hand-written kernel: the separable Gaussian stays Skia's, which is both
 *  faster than an SkSL rewrite and pixel-identical to a caller who was
 *  already spelling this out by hand.
 *
 *  An axis-aligned angle IS SkImageFilters::Blur with the sigmas swapped.
 *  Any other angle is a rotate → Blur → unrotate sandwich: three filter
 *  nodes, with bounds handled by the filter graph rather than by us. */
sk_sp<SkImageFilter> makeDirectionalBlur(float sigma, float angleDeg,
                                         float across) {
  float axis = std::fmod(angleDeg, 180.0f);  // a blur axis has no sign
  if (axis < 0) axis += 180.0f;
  if (axis == 0.0f) return SkImageFilters::Blur(sigma, across, nullptr);
  if (axis == 90.0f) return SkImageFilters::Blur(across, sigma, nullptr);
  const SkSamplingOptions sampling(SkFilterMode::kLinear);
  sk_sp<SkImageFilter> aligned = SkImageFilters::MatrixTransform(
      SkMatrix::RotateDeg(-angleDeg), sampling, nullptr);
  sk_sp<SkImageFilter> blurred =
      SkImageFilters::Blur(sigma, across, std::move(aligned));
  return SkImageFilters::MatrixTransform(SkMatrix::RotateDeg(angleDeg),
                                         sampling, std::move(blurred));
}

Effect Effect::directionalBlur(float sigma, float angleDeg, float across) {
  Effect e;
  e.m_dirBlur = DirectionalBlur{sigma, angleDeg, across};
  e.m_filter = makeDirectionalBlur(sigma, angleDeg, across);
  return e;
}

namespace {
/** THE PYRAMID, and the whole of blur()'s recipe: a
 *  small fixed number of CONSTANT-sigma blurs of the layer, blended by the
 *  parameter. Skia's Gaussian is separable and its cost is a function of
 *  sigma; a spatially-varying sigma is NOT separable, so an author-written
 *  kernel pays the worst radius at every pixel. Three fixed levels
 *  (0, maxSigma/2, maxSigma) plus one mix pass is a constant number of
 *  full-resolution passes no matter how wide the range gets.
 *
 *  Branch-free because both nested mixes are exact at the level sigmas:
 *  t=0 → level0, t=1 → level1, t=2 → level2, and linear in sigma between.
 *  The level COUNT is deliberately not in the API — see Effect::blur. */
sk_sp<SkRuntimeEffect> paramBlurMix() {
  static const sk_sp<SkRuntimeEffect> fx = [] {
    auto [effect, error] = SkRuntimeEffect::MakeForShader(
        SkString(shaderSource("ParamBlurMix.sksl")));
    if (!effect)
      SkDebugf("[material] skia::Effect::blur: mix shader failed: %s\n",
               error.c_str());
    return effect;
  }();
  return fx;
}
}  // namespace

std::shared_ptr<const Effect::BlurLevels> makeBlurLevels(float maxSigma) {
  if (!(maxSigma > 0)) return nullptr;
  auto levels = std::make_shared<Effect::BlurLevels>();
  levels->maxSigma = maxSigma;
  levels->half =
      SkImageFilters::Blur(maxSigma * 0.5f, maxSigma * 0.5f, nullptr);
  levels->full = SkImageFilters::Blur(maxSigma, maxSigma, nullptr);
  return levels;
}

/** blur()'s filter DAG over HELD levels. The intermediates are per-draw
 *  image-filter surfaces inside the effect's ONE saveLayer — the same
 *  place every effect intermediate already lives — so there is nothing
 *  new to invalidate. A null input means "the source", which is level 0.
 *
 *  The levels are inputs by identity: Skia's image-filter cache keys a
 *  result on the filter node, so a mix re-wrapped around the same two
 *  blur nodes finds both blurred layers already made. @p sigma is the
 *  sigma the map's white asks for; inside the declared range it rides
 *  the held pyramid as a scale on the mix parameter, and above it clamps
 *  to the range. */
sk_sp<SkImageFilter> makeParamBlur(const Effect::BlurLevels* levels,
                                   float sigma, sk_sp<SkShader> sigmaMap,
                                   SkSize box) {
  const sk_sp<SkRuntimeEffect> fx = paramBlurMix();
  if (!sigmaMap || !fx || !levels) {
    // No map or no SkSL: the honest fallback is the constant blur the
    // parameter would have modulated, which is also what sigma == 0
    // means (no blur anywhere).
    return sigma > 0 ? SkImageFilters::Blur(sigma, sigma, nullptr) : nullptr;
  }
  SkRuntimeShaderBuilder b(fx);
  b.child("param") = std::move(sigmaMap);
  b.uniform("scale") = std::min(sigma / levels->maxSigma, 1.0f);
  std::string_view names[3] = {"level0", "level1", "level2"};
  const sk_sp<SkImageFilter> inputs[3] = {
      nullptr,  // level 0 IS the layer, unblurred
      levels->half, levels->full};
  sk_sp<SkImageFilter> mix = SkImageFilters::RuntimeShader(b, names, inputs, 3);
  // THE REACH, DECLARED. A runtime shader may write any pixel, so Skia
  // treats a node built from one as covering everything and gives it a
  // layer the size of the whole clip: a small node's blur would evaluate
  // over the entire canvas, and its cost would be the canvas's rather than
  // its own. This blur's reach is known — the box the map is defined over,
  // grown by the Gaussian support of the range's largest level, which is
  // the furthest any of the three levels can carry a pixel. Stated only
  // where the box is known; without one the filter stays as Skia found it.
  if (box.isEmpty()) return mix;
  const float reach = levels->maxSigma * 3.0f;
  return SkImageFilters::Crop(
      SkRect::MakeWH(box.width(), box.height()).makeOutset(reach, reach),
      std::move(mix));
}

Effect Effect::blur(Paint sigmaMap, float maxSigma) {
  Effect e;
  e.m_paramBlur = ParamBlur{maxSigma};
  e.m_blurLevels = makeBlurLevels(maxSigma);
  e.m_children.emplace_back("sigma",
                            std::make_shared<const Paint>(std::move(sigmaMap)));
  // The static snapshot, built context-free exactly as Material::child
  // refreshes m_shader at store time; a context-needing map rebuilds this
  // per paint in resolvedImageFilter().
  e.m_filter = e.buildFilter(nullptr);
  return e;
}

}  // namespace sigil::material::skia
