/** @file
 * THE FILTER VALUE over its Skia node: each stock pass, parameter door and
 * chain forwards to the executor's `Effect`, copying the node on write
 * because a filter is a value; and the executor's entrances that hand a
 * filter to a Skia canvas.
 */

#include <include/core/SkColor.h>
#include <include/effects/SkImageFilters.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Filter.h>
#include <sigilmaterial/skia/Paint.h>

#include <cmath>
#include <memory>
#include <utility>

#include "EffectInternal.h"

namespace sigil::material {

/** The executor's filter a `Filter` holds (opaque in the header). */
struct Filter::Node {
  skia::Effect effect;
  /** The steps that read coverage, which no image filter carries: a
   *  renderer that knows the layer's shape draws them. */
  std::vector<CoverageEffect> coverage;
};

}  // namespace sigil::material

namespace sigil::material::skia {

/** The executor's view of a filter: the node inside it, and a filter
 *  around a node. A friend of the filter, so its header names no Skia
 *  type. */
struct FilterAccess {
  static const Effect* effect(const Filter& filter) {
    return filter.m_node ? &filter.m_node->effect : nullptr;
  }
  /** @p effect as a filter; an effect that paints nothing is the filter
   *  of none, so the two spellings of "nothing" compare equal. */
  static Filter wrap(Effect effect, std::vector<CoverageEffect> coverage = {}) {
    if (effect == Effect{} && coverage.empty()) return {};
    return Filter(std::make_shared<const Filter::Node>(
        Filter::Node{std::move(effect), std::move(coverage)}));
  }
  static Effect& edit(Filter& filter) { return filter.edit().effect; }
};

namespace {

/** A colour function of the one colour-adjust body: @p mode picks which. */
Filter colorAdjust(float mode, float amount) {
  const sk_sp<SkRuntimeEffect>& program =
      effectProgram(EffectProgram::ColorAdjust);
  if (!program) return {};
  return FilterAccess::wrap(
      Effect::colorProgram(program, {{"uMode", mode}, {"uAmount", amount}}));
}

}  // namespace

Filter filter(sk_sp<SkImageFilter> imageFilter) {
  return FilterAccess::wrap(Effect::filter(std::move(imageFilter)));
}

Filter filter(sk_sp<SkColorFilter> colorFilter) {
  return FilterAccess::wrap(Effect::filter(std::move(colorFilter)));
}

Filter program(sk_sp<SkRuntimeEffect> effect,
               std::vector<std::pair<std::string, float>> parameters) {
  return FilterAccess::wrap(
      Effect::shader(std::move(effect), std::move(parameters)));
}

Filter lowered(const Material& program, SkColorType surface) {
  if (!program.hasProgram()) return {};
  return FilterAccess::wrap(Effect::recipe(program, surface));
}

sk_sp<SkImageFilter> imageFilter(const Filter& filter) {
  const Effect* effect = FilterAccess::effect(filter);
  return effect ? effect->imageFilter() : nullptr;
}

sk_sp<SkColorFilter> colorFilter(const Filter& filter) {
  const Effect* effect = FilterAccess::effect(filter);
  return effect ? effect->colorFilter() : nullptr;
}

sk_sp<SkImageFilter> resolvedImageFilter(const Filter& filter,
                                         const FrameData* frame) {
  const Effect* effect = FilterAccess::effect(filter);
  return effect ? effect->resolvedImageFilter(frame) : nullptr;
}

std::span<const sk_sp<SkRuntimeEffect>> everyFilterProgram() {
  return everyEffectProgram();
}

}  // namespace sigil::material::skia

namespace sigil::material {

using skia::Effect;
using skia::FilterAccess;

Filter::Node& Filter::edit() {
  if (!m_node) {
    m_node = std::make_shared<const Node>();
  } else if (m_node.use_count() > 1) {
    m_node = std::make_shared<const Node>(*m_node);
  }
  // The node is held const so copies share it; this filter is its only
  // holder once the lines above have run.
  return const_cast<Node&>(*m_node);
}

Filter Filter::blur(float sigma) { return FilterAccess::wrap(Effect::blur(sigma)); }

Filter Filter::blur(Paint sigmaMap, float maximumSigma) {
  return FilterAccess::wrap(Effect::blur(std::move(sigmaMap), maximumSigma));
}

Filter Filter::blur(const Material& sigmaMap, float maximumSigma) {
  return blur(skia::paint(sigmaMap), maximumSigma);
}

Filter Filter::directionalBlur(float sigma, float angleDegrees, float across) {
  return FilterAccess::wrap(Effect::directionalBlur(sigma, angleDegrees, across));
}

Filter Filter::dropShadow(Color color, ShadowOptions options) {
  return FilterAccess::wrap(Effect::dropShadow(color, options.offset.x,
                                               options.offset.y, options.blur));
}

Filter Filter::glow(Color color, float sigma) {
  return FilterAccess::wrap(Effect::glow(color, sigma));
}

Filter Filter::shadow(Color color, ShadowOptions options) {
  CoverageEffect step;
  step.kind = CoverageEffect::Kind::Shadow;
  step.color = color;
  step.shadow = options;
  return FilterAccess::wrap({}, {step});
}

Filter Filter::stroke(Color color, StrokeOptions options) {
  CoverageEffect step;
  step.kind = CoverageEffect::Kind::Stroke;
  step.color = color;
  step.stroke = options;
  return FilterAccess::wrap({}, {step});
}

Filter Filter::bevel(BevelOptions options) {
  CoverageEffect step;
  step.kind = CoverageEffect::Kind::Bevel;
  step.bevel = options;
  return FilterAccess::wrap({}, {step});
}

std::span<const CoverageEffect> Filter::coverage() const {
  if (!m_node) return {};
  return m_node->coverage;
}

Filter Filter::withoutCoverage() const {
  if (!m_node) return {};
  return FilterAccess::wrap(m_node->effect);
}

Filter Filter::bloom(BloomOptions options) {
  return FilterAccess::wrap(skia::bloom(options));
}

Filter Filter::phosphorBloom(float radius, float threshold, float intensity,
                             float chroma, float hueDrift, float tail) {
  return FilterAccess::wrap(Effect::phosphorBloom(radius, threshold, intensity,
                                                  chroma, hueDrift, tail));
}

Filter Filter::brightness(float amount) { return skia::colorAdjust(0, amount); }
Filter Filter::contrast(float amount) { return skia::colorAdjust(1, amount); }
Filter Filter::saturate(float amount) { return skia::colorAdjust(2, amount); }
Filter Filter::hueRotate(float degrees) {
  return skia::colorAdjust(3, degrees * 3.14159265358979f / 180.0f);
}

Filter Filter::brightPass(float threshold, float knee) {
  return FilterAccess::wrap(Effect::brightPass(threshold, knee));
}

Filter Filter::deepen(float amount) {
  return FilterAccess::wrap(Effect::deepen(amount));
}

Filter Filter::whiten(float amount, float threshold, float knee) {
  return FilterAccess::wrap(Effect::whiten(amount, threshold, knee));
}

Filter Filter::dilate(float pixels) {
  return FilterAccess::wrap(Effect::dilate(pixels));
}

// A pass is a program over the layer; a material with no program base has
// no body to run, so it is the filter of nothing.
Filter Filter::of(const Material& program) {
  if (!program.hasProgram()) return {};
  return FilterAccess::wrap(Effect::recipe(program));
}

Filter Filter::of(const Material& program, float sampleRadius) {
  if (!program.hasProgram()) return {};
  return FilterAccess::wrap(Effect::recipe(program, sampleRadius));
}

Filter& Filter::slot(std::string name, Paint source) {
  edit().effect.slot(std::move(name), std::move(source));
  return *this;
}

Filter& Filter::set(std::string name, float value) {
  edit().effect.set(std::move(name), value);
  return *this;
}

Filter& Filter::set(std::string name, std::array<float, 2> value) {
  edit().effect.set(std::move(name), value);
  return *this;
}

Filter& Filter::set(std::string name, std::array<float, 4> value) {
  edit().effect.set(std::move(name), value);
  return *this;
}

Filter& Filter::set(std::string name, std::vector<float> values) {
  edit().effect.set(std::move(name), std::move(values));
  return *this;
}

Filter& Filter::bind(std::string name, motion::Animatable<float> value) {
  edit().effect.bind(std::move(name), std::move(value));
  return *this;
}

Filter& Filter::bind(std::string name,
                     std::shared_ptr<const UniformBlock> block) {
  edit().effect.bind(std::move(name), std::move(block));
  return *this;
}

Filter Filter::then(const Filter& next) const {
  if (!m_node) return next;
  if (!next.m_node) return *this;
  std::vector<CoverageEffect> coverage = m_node->coverage;
  coverage.insert(coverage.end(), next.m_node->coverage.begin(),
                  next.m_node->coverage.end());
  return FilterAccess::wrap(m_node->effect.then(next.m_node->effect),
                            std::move(coverage));
}

Filter Filter::emit(const Filter& light, BlendMode mode) const {
  if (!light.m_node) return *this;
  const Effect none;
  const Effect& self = m_node ? m_node->effect : none;
  return FilterAccess::wrap(self.emit(light.m_node->effect, mode),
                            m_node ? m_node->coverage
                                   : std::vector<CoverageEffect>{});
}

bool Filter::isRunning() const {
  return m_node && m_node->effect.isRunning();
}

bool Filter::usesWorldSpace() const {
  return m_node && m_node->effect.usesWorldSpace();
}

bool Filter::operator==(const Filter& other) const {
  // The same node is not enough: a live filter never compares equal, even
  // to itself, so a node carrying one repaints every frame.
  if (!m_node || !other.m_node) return m_node == other.m_node;
  return m_node->effect == other.m_node->effect &&
         m_node->coverage == other.m_node->coverage;
}

namespace {

/** A filter as a material's effects stage. */
class EffectsPart final : public detail::Part {
 public:
  explicit EffectsPart(Filter filter) : filter(std::move(filter)) {}
  bool equals(const detail::Part& other) const override {
    const auto* same = dynamic_cast<const EffectsPart*>(&other);
    return same && same->filter == filter;
  }
  bool isRunning() const override { return filter.isRunning(); }
  bool geometryDependent() const override { return filter.usesWorldSpace(); }
  Filter filter;
};

}  // namespace

EffectsStage::EffectsStage(const Filter& chain)
    : m_part(chain.isNone() ? nullptr
                            : std::make_shared<const EffectsPart>(chain)) {}

Material& Material::effects(const Filter& chain) {
  placeEffects(chain.isNone() ? nullptr
                              : std::make_shared<const EffectsPart>(chain));
  return *this;
}

const Filter* Material::effects() const {
  const auto* part = dynamic_cast<const EffectsPart*>(effectsPart());
  return part ? &part->filter : nullptr;
}

}  // namespace sigil::material
