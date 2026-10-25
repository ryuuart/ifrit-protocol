/** @file
 * THE MATERIAL AS ONE SKIA PAINT: the gradient bases held as parts only
 * this executor draws, and the lowering that folds a material's base and
 * layer stack into the paint a canvas is handed — each layer blended over
 * the accumulation, mixed back by its opacity, and through its mask where
 * it has one. Alpha cutoff removes samples of the completed colour stack;
 * lighting channels and effects are read by their own renderers.
 */

#include <include/core/SkBlender.h>
#include <include/core/SkString.h>
#include <include/core/SkTypes.h>  // SkDebugf
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/advanced/Program.h>  // reportOnce
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
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

/** @p layer blended over @p under by @p mode, mixed back toward @p under
 *  by @p mask as @p how reads it and by @p opacity. ONE program reads
 *  @p under once and blends inside: holding it again inside a blend
 *  shader would make each masked layer sample everything beneath it
 *  twice, and a stack of them would bind more textures than a fragment
 *  program can sample. */
Paint maskedLayer(Paint under, Paint layer, SkBlendMode mode, Paint mask,
                  const Mask& how, float opacity, bool snapshot) {
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
  PaintAccess::storeSlot(masked, "uLayer", std::move(layer));
  PaintAccess::storeBlender(masked, "uMode", SkBlender::Mode(mode));
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
      // A layer of nothing leaves what is beneath it; over nothing, the
      // layer stands alone, unblended.
      if (top.isNone()) continue;
      const SkBlendMode mode = accumulated.isNone()
                                   ? SkBlendMode::kSrc
                                   : toSkBlendMode(layer.options.blend);
      accumulated =
          maskedLayer(std::move(accumulated), std::move(top), mode,
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

int PaintAccess::samplerCount(const Paint& paint) {
  if (paint.m_isSolid) return 0;
  if (paint.m_backed) return skia::samplerCount(paint.m_backed->material);
  int count = 0;
  if (paint.m_recipe) {
    switch (paint.m_recipe->kind) {
      case Paint::Recipe::Kind::Image:
      case Paint::Recipe::Kind::Buffer:
        count += 1;
        break;
      case Paint::Recipe::Kind::Blend:
        for (const auto& [layer, mode] : paint.m_recipe->layers)
          count += samplerCount(layer);
        break;
      default:
        break;
    }
  }
  if (paint.m_live) {
    for (const auto& [name, child] : paint.m_live->slots)
      count += samplerCount(child);
  } else if (!paint.m_recipe) {
    // A raw shader says what it is only when it is one image.
    if (const sk_sp<SkShader>& shader = snapshot(paint);
        shader && shader->isAImage())
      count += 1;
  }
  return count;
}

int samplerCount(const Paint& paint) {
  return PaintAccess::samplerCount(paint);
}

int detail::baseSamplerCount(const Material& material) {
  const auto* part = dynamic_cast<const PaintPart*>(material.source());
  return part ? samplerCount(part->paint) : 0;
}

namespace {

/** Whether @p material's colour stack, lowered as one shader, stays within
 *  the samplers one fragment program may declare — said once, with both
 *  counts, when it does not. A recipe standing alone is not asked here:
 *  its builder refuses it by the same rule. */
bool stackWithinSamplerLimit(const Material& material) {
  if (!material.isComposed()) return true;
  const int samplers = samplerCount(material);
  if (samplers <= kSamplerLimit) return true;
  const std::string base = material.hasProgram()
                               ? "recipe \"" + material.recipe().name() + "\""
                               : std::string("a paint");
  reportOnce("samplers:stack:" + base + ":" + std::to_string(samplers),
             "a colour stack of " + std::to_string(material.layers().size()) +
                 " layers over " + base + " lowers to " +
                 std::to_string(samplers) +
                 " image samplers and a fragment program may declare " +
                 std::to_string(kSamplerLimit) + ", so it is not painted");
  return false;
}

}  // namespace

// REFUSED HERE, LOUDLY, RATHER THAN ON THE DEVICE, QUIETLY. A colour stack
// lowers to one shader, which a GPU backend inlines into one fragment
// program; past the sampler limit the driver rejects that program, the pass
// is dropped and nothing names the material that asked.
Paint paint(const Material& material) {
  if (!stackWithinSamplerLimit(material)) return {};
  return lowerPaint(material, true);
}

Paint PaintAccess::prepareMaterial(const Material& material) {
  if (!stackWithinSamplerLimit(material)) return {};
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

Material linearGradient(ColorStops stops, LinearDirection direction,
                        GradientOptions options) {
  return skia::asMaterial(
      skia::detail::cssLinearGradient(std::move(stops), direction, options));
}

Material radialGradient(ColorStops stops, RadialEndingShape shape,
                        GradientOptions options) {
  return skia::asMaterial(
      skia::detail::cssRadialGradient(std::move(stops), shape, options));
}

Material conicGradient(ColorStops stops, ConicStart start,
                       GradientOptions options) {
  return skia::asMaterial(
      skia::detail::cssConicGradient(std::move(stops), start, options));
}

}  // namespace sigil::material
