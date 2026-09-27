/** @file
 * THE MATERIAL AS ONE SKIA PAINT: the gradient bases held as parts only
 * this executor draws, and the lowering that folds a material's base and
 * layer stack into the paint a canvas is handed — each layer blended over
 * the accumulation, mixed back by its opacity, and through its mask where
 * it has one. The surface and the effects are read by the renderers that
 * have a use for them and are not part of the paint.
 */

#include <include/effects/SkRuntimeEffect.h>
#include <include/core/SkString.h>
#include <include/core/SkTypes.h>  // SkDebugf
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilshaders/MaterialSkia.h>

#include <string>
#include <utility>

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
  bool geometryDependent() const override {
    return paint.geometryDependent();
  }
  Paint paint;
};

Material asMaterial(Paint paint) {
  return Material(std::shared_ptr<const material::detail::Part>(
      std::make_shared<const PaintPart>(std::move(paint))));
}

Paint maskedLayer(Paint under, Paint over, Paint mask, const Mask& how,
                  float opacity) {
  static const sk_sp<SkRuntimeEffect> effect = [] {
    auto [built, error] = SkRuntimeEffect::MakeForShader(
        SkString(std::string(shaderSource("MaskedLayer.sksl")).c_str()));
    if (!built)
      SkDebugf("sigilmaterial masked layer shader: %s\n", error.c_str());
    return built;
  }();
  if (!effect) return under;
  Paint masked = PaintAccess::sksl(
      effect, {{"uChannel", (float)how.channel},
               {"uLow", how.low},
               {"uHigh", how.high},
               {"uInvert", how.invert ? 1.0f : 0.0f},
               {"uOpacity", opacity}});
  masked.slot("uUnder", std::move(under));
  masked.slot("uOver", std::move(over));
  masked.slot("uMask", std::move(mask));
  return masked;
}

}  // namespace

Material base(Paint paint) { return asMaterial(std::move(paint)); }

Paint paint(const Material& material) {
  if (!material.isComposed()) return Paint::recipe(material);
  Paint accumulated;
  if (const Color* color = material.color()) {
    accumulated = Paint::solid(*color);
  } else if (const auto* part =
                 dynamic_cast<const PaintPart*>(material.source())) {
    accumulated = part->paint;
  } else if (material.hasProgram()) {
    accumulated = Paint::recipe(material.base());
  }
  for (const Layer& layer : material.layers()) {
    Paint top = paint(layer.source);
    const float opacity = layer.options.opacity;
    if (layer.options.mask) {
      Paint blended =
          Paint::blend({{accumulated, BlendMode::Normal},
                        {std::move(top), layer.options.blend}});
      accumulated =
          maskedLayer(std::move(accumulated), std::move(blended),
                      paint(layer.options.mask->source), *layer.options.mask,
                      opacity);
      continue;
    }
    if (opacity < 1.0f) top.amount(opacity);
    accumulated = Paint::blend({{std::move(accumulated), BlendMode::Normal},
                                {std::move(top), layer.options.blend}});
  }
  return accumulated;
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
