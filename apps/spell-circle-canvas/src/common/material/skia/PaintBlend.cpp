/** @file
 * THE BLEND: layers folded into ONE shader, each composited over the
 * accumulation with its mode and mixed back toward it by its strength.
 * The fold has an eager caller at construction and a deferred one per
 * draw, and is written once so the two cannot paint different pictures.
 */

#include <include/core/SkBlender.h>
#include <include/core/SkShader.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/advanced/Program.h>
#include <sigilshaders/MaterialSkia.h>

#include <cstddef>
#include <format>
#include <functional>
#include <span>
#include <utility>
#include <vector>

#include "PaintInternal.h"

namespace sigil::material::skia {

namespace {
/** @p layer blended over @p under by @p mode and mixed back toward
 *  @p under by @p amount, as ONE program that reads @p under once. A mix
 *  of `under` with a blend shader over that same `under` would hold it
 *  twice, and a backend expands every child a shader holds into its own
 *  copy: a stack of partly opaque layers would then sample its first
 *  input twice as often with each layer, until one draw binds more
 *  textures than a fragment program can sample, and the draw is dropped.
 *  Compiled once for the whole process: the body is fixed text, so a
 *  second compile of it could only produce the same program. */
sk_sp<SkShader> mixLayer(sk_sp<SkShader> under, sk_sp<SkShader> layer,
                         SkBlendMode mode, float amount) {
  static const sk_sp<SkRuntimeEffect> fx = [] {
    auto [effect, err] =
        SkRuntimeEffect::MakeForShader(SkString(shaderSource("Mix.sksl")));
    if (!effect)
      material::report(
          std::format("the blend mix body did not compile: {}", err.c_str()));
    return effect;
  }();
  if (!fx) return SkShaders::Blend(mode, std::move(under), std::move(layer));
  SkRuntimeShaderBuilder builder(fx);
  builder.child("uUnder") = std::move(under);
  builder.child("uLayer") = std::move(layer);
  builder.child("uMode") = SkBlender::Mode(mode);
  builder.uniform("uAmount") = amount;
  return builder.makeShader();
}
}  // namespace

sk_sp<SkShader> PaintAccess::foldLayers(
    std::span<const std::pair<Paint, BlendMode>> layers,
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
    const float amount = layer.m_amount;
    acc = amount >= 1.0f ? SkShaders::Blend(toSkBlendMode(mode), std::move(acc),
                                            std::move(src))
                         : mixLayer(std::move(acc), std::move(src),
                                    toSkBlendMode(mode), amount);
  }
  return acc;
}

}  // namespace sigil::material::skia

namespace sigil::material {

using skia::PaintAccess;

Paint Paint::blend(std::vector<std::pair<Paint, BlendMode>> layers) {
  Paint paint = PaintAccess::unresolvedBlend(std::move(layers));
  PaintAccess::retainSnapshot(paint);
  return paint;
}

}  // namespace sigil::material

namespace sigil::material::skia {

Paint PaintAccess::unresolvedBlend(
    std::vector<std::pair<Paint, BlendMode>> layers) {
  if (layers.empty()) return {};
  // Keep the layer materials as the comparable recipe (recursive equality) —
  // a blend containing a live layer compares by that layer's identity, so it
  // stays conservatively un-pruned, as it must (the snapshot sampled Outputs).
  auto recipe = std::make_shared<Paint::Recipe>();
  recipe->kind = Paint::Recipe::Kind::Blend;
  recipe->layers = std::move(layers);
  recipe->solidLayers.reserve(recipe->layers.size());
  for (const auto& [layer, mode] : recipe->layers)
    recipe->solidLayers.push_back(layer.isSolid() ? asShader(layer) : nullptr);
  Paint paint;
  paint.m_recipe = std::move(recipe);
  return paint;
}

/** Resolve every child before testing the framed memo, so live values and
 *  geometry changes remain visible. The retained child references prevent
 *  shader address reuse from making a changed source match an old fold. */
sk_sp<SkShader> PaintAccess::foldBlend(const Paint& self,
                                       const PaintFrame* paintFrame) {
  const auto& recipe = *self.m_recipe;
  if (!paintFrame)
    return foldLayers(recipe.layers, [](const Paint& layer) {
      return detail::childShader(layer, nullptr);
    });

  Paint::Recipe::BlendInputs inputs;
  const PaintFrame childFrame =
      self.m_worldSpace ? rootSamplingFrame(*paintFrame) : *paintFrame;
  inputs.shaders.reserve(recipe.layers.size());
  for (size_t i = 0; i < recipe.layers.size(); ++i)
    inputs.shaders.push_back(
        recipe.solidLayers[i]
            ? recipe.solidLayers[i]
            : detail::childShader(recipe.layers[i].first, &childFrame));
  inputs.worldSpace = self.m_worldSpace;
  if (inputs.worldSpace) inputs.toRoot = paintFrame->toRoot;
  if (auto held = recipe.blendMemo.hit(inputs)) return held;

  size_t next = 0;
  auto built = foldLayers(recipe.layers,
                          [&](const Paint&) { return inputs.shaders[next++]; });
  if (inputs.worldSpace) built = anchorToRoot(std::move(built), *paintFrame);
  recipe.blendMemo.store(std::move(inputs), built);
  return built;
}

}  // namespace sigil::material::skia
