/** @file
 * THE STAGES A LIGHT IS MADE OF — blur, dilate, deepen, whiten — and
 * emit(), which lays a light made from a layer back over that layer.
 * Each stage is an ordinary effect, chained with then(); emit() is the
 * one join a glow needs that then() cannot say.
 */

#include <include/core/SkColorFilter.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/skia/Effect.h>

#include <algorithm>
#include <memory>

#include "EffectInternal.h"

namespace sigil::material::skia {

Effect Effect::blur(float sigma) {
  const float s = std::max(0.0f, sigma);
  return filter(SkImageFilters::Blur(s, s, nullptr));
}

Effect Effect::dilate(float pixels) {
  if (!(pixels > 0)) return {};
  const float grow[20] = {1, 0, 0, 0, 0,
                          0, 1, 0, 0, 0,
                          0, 0, 1, 0, 0,
                          0, 0, 0, 4, 0};
  const float reach = pixels * 1.5f;
  return filter(SkImageFilters::ColorFilter(
      SkColorFilters::Matrix(grow),
      SkImageFilters::Blur(reach, reach, nullptr)));
}

Effect Effect::deepen(float amount) {
  const sk_sp<SkRuntimeEffect>& program =
      effectProgram(EffectProgram::HaloDeepening);
  if (!(amount > 0)) return {};
  return colorProgram(program, {{"uDeepening", amount}});
}

Effect Effect::whiten(float amount, float threshold, float knee) {
  const sk_sp<SkRuntimeEffect>& program =
      effectProgram(EffectProgram::CoreWhitening);
  if (!(amount > 0)) return {};
  const float gate = std::clamp(threshold, 0.0f, 1.0f);
  return colorProgram(program,
                      {{"uThreshold", gate},
                       {"uTop", std::min(gate + std::max(knee, 0.0f), 1.0f)},
                       {"uWhitening", std::min(amount, 1.0f)}});
}

Effect Effect::emit(const Effect& light, SkBlendMode mode) const {
  Effect e;
  if (isAnimated() || anyChildNeedsContext() || light.isAnimated() ||
      light.anyChildNeedsContext()) {
    e.m_chainA = std::make_shared<const Effect>(*this);
    e.m_chainB = std::make_shared<const Effect>(light);
    e.m_chainBlend = mode;
    return e;
  }
  e.m_filter =
      SkImageFilters::Blend(mode, liftedFilter(), light.liftedFilter());
  return e;
}

}  // namespace sigil::material::skia
