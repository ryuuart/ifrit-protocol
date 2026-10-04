/** @file
 * THE MATERIAL AS ONE SKIA PAINT: the gradient bases held as parts only
 * this executor draws, and the lowering that folds a material's base and
 * layer stack into the paint a canvas is handed — each layer blended over
 * the accumulation, mixed back by its opacity, and through its mask where
 * it has one. Alpha cutoff removes samples of the completed colour stack;
 * lighting channels and effects are read by their own renderers.
 */

#include <include/core/SkString.h>
#include <include/core/SkTypes.h>  // SkDebugf
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilshaders/MaterialSkia.h>

#include <string>
#include <utility>
#include <vector>

#include "PaintInternal.h"

namespace sigil::material::skia {

namespace {

/** A paint as a material's base. */
class PaintPart final : public material::detail::Part {
 public:
  explicit PaintPart(Paint paint) : paint(std::move(paint)) {}
  bool equals(const material::detail::Part& other) const override {
    const auto* same = dynamic_cast<const PaintPart*>(&other);
    return same && same->paint == paint;
  }
  bool isRunning() const override { return paint.isRunning(); }
  bool geometryDependent() const override { return paint.geometryDependent(); }
  Paint paint;
};

Material asMaterial(Paint paint) {
  return Material(std::shared_ptr<const material::detail::Part>(
      std::make_shared<const PaintPart>(std::move(paint))));
}

Paint maskedLayer(Paint under, Paint over, Paint mask, const Mask& how,
                  float opacity, bool snapshot) {
  static const sk_sp<SkRuntimeEffect> effect = [] {
    auto [built, error] = SkRuntimeEffect::MakeForShader(
        SkString(std::string(shaderSource("MaskedLayer.sksl")).c_str()));
    if (!built)
      SkDebugf("sigilmaterial masked layer shader: %s\n", error.c_str());
    return built;
  }();
  if (!effect) return under;
  Paint masked = PaintAccess::unresolvedSksl(effect);
  PaintAccess::storeUniform(masked, "uChannel", (float)how.channel);
  PaintAccess::storeUniform(masked, "uLow", how.low);
  PaintAccess::storeUniform(masked, "uHigh", how.high);
  PaintAccess::storeUniform(masked, "uInvert", how.invert ? 1.0f : 0.0f);
  PaintAccess::storeUniform(masked, "uOpacity", opacity);
  PaintAccess::storeSlot(masked, "uUnder", std::move(under));
  PaintAccess::storeSlot(masked, "uOver", std::move(over));
  PaintAccess::storeSlot(masked, "uMask", std::move(mask));
  if (snapshot) PaintAccess::refresh(masked);
  return masked;
}

Paint lowerPaint(const Material& material, bool snapshot) {
  if (!material.isComposed())
    return snapshot ? Paint::recipe(material)
                    : PaintAccess::unresolvedRecipe(material);
  Paint accumulated;
  if (const Color* color = material.color()) {
    accumulated = Paint::solid(*color);
  } else if (const auto* part =
                 dynamic_cast<const PaintPart*>(material.source())) {
    accumulated = part->paint;
  } else if (material.hasProgram()) {
    Material base = material.base();
    base.worldSpace(false);
    accumulated = snapshot ? Paint::recipe(std::move(base))
                           : PaintAccess::unresolvedRecipe(std::move(base));
  }
  const auto blend = [&](std::vector<std::pair<Paint, BlendMode>> layers) {
    return snapshot ? Paint::blend(std::move(layers))
                    : PaintAccess::unresolvedBlend(std::move(layers));
  };
  for (const Layer& layer : material.layers()) {
    Paint top = lowerPaint(layer.source, snapshot);
    const float opacity = layer.options.opacity;
    if (layer.options.mask) {
      Paint blended = blend({{accumulated, BlendMode::Normal},
                             {std::move(top), layer.options.blend}});
      accumulated =
          maskedLayer(std::move(accumulated), std::move(blended),
                      lowerPaint(layer.options.mask->source, snapshot),
                      *layer.options.mask, opacity, snapshot);
      continue;
    }
    if (opacity < 1.0f) top.amount(opacity);
    accumulated = blend({{std::move(accumulated), BlendMode::Normal},
                         {std::move(top), layer.options.blend}});
  }
  if (const SurfaceOptions* surface = material.surface())
    accumulated = PaintAccess::alphaCutout(std::move(accumulated),
                                           surface->alphaCutoff, snapshot);
  if (material.worldSpace()) accumulated.worldSpace();
  return accumulated;
}

}  // namespace

Paint PaintAccess::alphaCutout(Paint source, float cutoff, bool snapshot) {
  if (!(cutoff > 0.0f) || source.isNone()) return source;
  if (source.isSolid()) {
    if (!(source.solidColor().a < cutoff)) return source;
    Paint cutout = Paint::solid({0, 0, 0, 0});
    cutout.m_amount = source.m_amount;
    cutout.m_bleed = source.m_bleed;
    cutout.m_worldSpace = source.m_worldSpace;
    return cutout;
  }
  static const sk_sp<SkRuntimeEffect> effect = [] {
    auto [built, error] = SkRuntimeEffect::MakeForShader(
        SkString(std::string(shaderSource("AlphaCutout.sksl")).c_str()));
    if (!built)
      SkDebugf("sigilmaterial alpha cutoff shader: %s\n", error.c_str());
    return built;
  }();
  if (!effect) return source;
  Paint cutout = PaintAccess::unresolvedSksl(effect);
  cutout.m_amount = source.m_amount;
  cutout.m_bleed = source.m_bleed;
  cutout.m_worldSpace = source.m_worldSpace;
  cutout.m_live->childAnchorsToRoot = true;
  PaintAccess::storeUniform(cutout, "uAlphaCutoff", cutoff);
  PaintAccess::storeSlot(cutout, "uColor", std::move(source));
  if (snapshot) PaintAccess::refresh(cutout);
  return cutout;
}

Material base(Paint paint) { return asMaterial(std::move(paint)); }

bool detail::paintUsesWorldSpace(const Material& material) {
  if (material.worldSpace() ||
      (material.hasProgram() &&
       material.recipe().reads(FrameInput::WorldTransform)))
    return true;
  if (const auto* part = dynamic_cast<const PaintPart*>(material.source());
      part && part->paint.usesWorldSpace())
    return true;
  for (const Layer& layer : material.layers())
    if (paintUsesWorldSpace(layer.source) ||
        (layer.options.mask && paintUsesWorldSpace(layer.options.mask->source)))
      return true;
  for (const auto& [name, child] : material.slots())
    if (child.material && material.recipe().samples(Target::SkSL, name) &&
        paintUsesWorldSpace(*child.material))
      return true;
  return false;
}

Paint paint(const Material& material) { return lowerPaint(material, true); }

Paint PaintAccess::prepareMaterial(const Material& material) {
  return lowerPaint(material, false);
}

}  // namespace sigil::material::skia

namespace sigil::material {

Material linearGradient(glm::vec2 start, glm::vec2 end, ColorStops stops,
                        GradientOptions options) {
  return skia::asMaterial(
      Paint::linearGradient(start, end, std::move(stops), options));
}

Material radialGradient(glm::vec2 center, float radius, ColorStops stops,
                        GradientOptions options) {
  return skia::asMaterial(
      Paint::radialGradient(center, radius, std::move(stops), options));
}

Material conicGradient(glm::vec2 center, ColorStops stops,
                       GradientOptions options) {
  return skia::asMaterial(
      Paint::conicGradient(center, std::move(stops), options));
}

}  // namespace sigil::material
