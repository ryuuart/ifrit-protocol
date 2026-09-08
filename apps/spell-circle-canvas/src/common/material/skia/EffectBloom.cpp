/** @file
 * THE BLOOM: the bright pass that says which pixels glow, and the
 * phosphor halo gathered around them — twenty-four taps spent over a
 * layer reduced to the coarseness the reach allows, resampled up, and
 * laid back over the sharp source.
 */

#include <include/core/SkMatrix.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkTypes.h>  // SkDebugf
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilshaders/MaterialSkia.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

#include "EffectInternal.h"

namespace sigil::material::skia {

sk_sp<SkRuntimeEffect> bloomProgram(const char* door, const char* file) {
  auto [program, error] =
      SkRuntimeEffect::MakeForShader(SkString(shaderSource(file)));
  if (!program)
    SkDebugf("[material] skia::Effect::%s: %s failed: %s\n", door, file,
             error.c_str());
  return program;
}

namespace {

/** HOW COARSE THE HALO MAY BE GATHERED, and the whole of why the effect
 *  costs what it does. The gather is twenty-four taps; the picture it
 *  makes is a low-frequency one, so the taps are spent over a REDUCED
 *  layer and the answer resampled back up — a quarter of the pixels at
 *  two, a sixteenth at four.
 *
 *  The limit is the INNERMOST ring: it sits at 0.28 of the reach, and a
 *  ring smaller than a pixel in the layer it is gathered on is not a ring
 *  any more, it is the centre tap eight times. So the divisor is at most
 *  0.28 * radius, taken down to a power of two — which is the reduction a
 *  bilinear resample answers exactly — and capped at four, beyond which
 *  the bright pass would alias on its own sources. A small reach comes
 *  back as one and is gathered whole. */
int haloDivisor(float radius) {
  const float most = 0.28f * radius;
  if (most >= 4.0f) return 4;
  if (most >= 2.0f) return 2;
  return 1;
}

}  // namespace

/** The bloom's filter DAG: reduce, gather, enlarge, composite.
 *
 *  @p haloBuilder arrives with every uniform of the halo program already
 *  set; its radius is rewritten here, because inside a reduced layer a
 *  reach is measured in that layer's pixels. The gather declares its own
 *  sampling radius, so Skia bounds the node to the reduced source grown
 *  by the reach instead of giving a runtime shader the whole clip. The
 *  composite reads the source at full resolution, so the sharp picture
 *  never passes through a resample — only the light added to it does. */
sk_sp<SkImageFilter> makePhosphorBloom(SkRuntimeShaderBuilder& haloBuilder,
                                       const sk_sp<SkRuntimeEffect>& composite,
                                       float radius) {
  const int divisor = haloDivisor(radius);
  const float scale = 1.0f / static_cast<float>(divisor);
  const float reach = radius * scale;
  haloBuilder.uniform("uRadius") = reach;
  const SkSamplingOptions resample(SkFilterMode::kLinear, SkMipmapMode::kNone);

  sk_sp<SkImageFilter> source;  // null IS the layer, at its own resolution
  if (divisor > 1)
    source = SkImageFilters::MatrixTransform(SkMatrix::Scale(scale, scale),
                                             resample, nullptr);
  sk_sp<SkImageFilter> halo = SkImageFilters::RuntimeShader(
      haloBuilder, reach, "content", std::move(source));
  if (divisor > 1)
    halo = SkImageFilters::MatrixTransform(
        SkMatrix::Scale(static_cast<float>(divisor),
                        static_cast<float>(divisor)),
        resample, std::move(halo));
  if (!composite) return halo;

  SkRuntimeShaderBuilder over(composite);
  std::string_view names[2] = {"content", "halo"};
  const sk_sp<SkImageFilter> inputs[2] = {nullptr, std::move(halo)};
  return SkImageFilters::RuntimeShader(over, names, inputs, 2);
}

Effect Effect::brightPass(float threshold, float knee) {
  static const sk_sp<SkRuntimeEffect> program =
      bloomProgram("brightPass", "BrightPass.sksl");
  const float gate = std::clamp(threshold, 0.0f, 1.0f);
  return shader(program,
                {{"uThreshold", gate},
                 {"uTop", std::min(gate + std::max(knee, 0.0f), 1.0f)}});
}

Effect Effect::phosphorBloom(float radius, float threshold, float intensity,
                             float chroma, float hueDrift, float tail) {
  static const sk_sp<SkRuntimeEffect> halo =
      bloomProgram("phosphorBloom", "PhosphorHalo.sksl");

  constexpr float kDegree = 3.14159265f / 180.0f;
  Effect e = shader(halo, {{"uRadius", std::max(radius, 0.0f)},
                           {"uThreshold", std::clamp(threshold, 0.0f, 0.99f)},
                           {"uIntensity", std::max(intensity, 0.0f)},
                           {"uChroma", std::clamp(chroma, 0.0f, 1.0f)},
                           {"uHueDrift", hueDrift * kDegree},
                           {"uTail", std::max(tail, 0.0f)}});
  if (!e.m_effect) return e;
  e.m_gatheredHalo = true;
  e.m_filter = e.buildFilter(nullptr);
  return e;
}

}  // namespace sigil::material::skia
