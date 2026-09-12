/** @file
 * THE BLEND: layers folded into ONE shader, each composited over the
 * accumulation with its mode and mixed back toward it by its strength.
 * The fold has an eager caller at construction and a deferred one per
 * draw, and is written once so the two cannot paint different pictures.
 */

#include <include/core/SkShader.h>
#include <include/core/SkTypes.h>  // SkDebugf
#include <include/effects/SkRuntimeEffect.h>
#include <sigilshaders/MaterialSkia.h>

#include <functional>
#include <span>
#include <utility>
#include <vector>

#include "PaintInternal.h"

namespace sigil::material::skia {

namespace {
/** mix(a, b, t) as one nested shader — what a blend layer's amount()
 *  lerps with (SkShaders has Blend but no Lerp). Compiled once for the
 *  whole process: the body is fixed text, so a second compile of it could
 *  only produce the same program. */
sk_sp<SkShader> mixShaders(sk_sp<SkShader> a, sk_sp<SkShader> b, float t) {
  static const sk_sp<SkRuntimeEffect> fx = [] {
    auto [effect, err] =
        SkRuntimeEffect::MakeForShader(SkString(shaderSource("Mix.sksl")));
    if (!effect)
      SkDebugf("[material] the blend mix body did not compile: %s\n",
               err.c_str());
    return effect;
  }();
  if (!fx) return b;
  SkRuntimeShaderBuilder builder(fx);
  builder.child("a") = std::move(a);
  builder.child("b") = std::move(b);
  builder.uniform("uAmt") = t;
  return builder.makeShader();
}
}  // namespace

sk_sp<SkShader> Paint::foldLayers(
    std::span<const std::pair<Paint, SkBlendMode>> layers,
    const std::function<sk_sp<SkShader>(const Paint&)>& shaderOf) {
  sk_sp<SkShader> acc;
  bool first = true;
  for (const auto& [layer, mode] : layers) {
    sk_sp<SkShader> src = shaderOf(layer);
    // The first layer IS the accumulation: it has nothing beneath it to
    // composite with, so neither its mode nor its amount is read — there
    // is nothing to mix back toward.
    if (first || !acc) {
      acc = std::move(src);
      first = false;
      continue;
    }
    if (!src) continue;
    // amount(): composite the layer in full, then mix the result back
    // toward the accumulation — Photoshop layer opacity, not src-alpha
    // thinning (the two differ on every non-porter-duff mode).
    const float amt = layer.m_amount;
    sk_sp<SkShader> blended = SkShaders::Blend(mode, acc, std::move(src));
    acc = amt >= 1.0f ? std::move(blended)
                      : mixShaders(std::move(acc), std::move(blended), amt);
  }
  return acc;
}

Paint Paint::blend(std::vector<std::pair<Paint, SkBlendMode>> layers) {
  if (layers.empty()) return {};
  Paint m = shader(
      foldLayers(layers, [](const Paint& layer) { return layer.asShader(); }));
  // Keep the layer materials as the comparable recipe (recursive equality) —
  // a blend containing a live layer compares by that layer's identity, so it
  // stays conservatively un-pruned, as it must (the snapshot sampled Outputs).
  auto rec = std::make_shared<Recipe>();
  rec->kind = Recipe::Kind::Blend;
  rec->layers = std::move(layers);
  m.m_recipe = std::move(rec);
  return m;
}

/** THE BLEND FOLD, in one place because it has two callers that must agree:
 *  `paintFrame` non-null is resolve()'s per-frame form, null is asShader()'s
 *  context-free one. Either way the LAYERS are re-read here rather than the
 *  flattened snapshot blend() built, which is the whole point — a live layer
 *  contributes its current value per call. */
sk_sp<SkShader> Paint::foldBlend(const PaintFrame* paintFrame) const {
  return foldLayers(m_recipe->layers, [paintFrame](const Paint& layer) {
    return detail::childShader(layer, paintFrame);
  });
}

}  // namespace sigil::material::skia
