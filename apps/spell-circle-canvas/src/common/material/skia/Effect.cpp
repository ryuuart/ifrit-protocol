/** @file
 * The post-processing value with bodies: filters, SkSL programs with
 * children and uniforms, the two blurs, chaining, and what an effect
 * declares about its own motion and its need for the root frame.
 */

#include "sigilmaterial/skia/Effect.h"

#include <include/core/SkPaint.h>
#include <include/core/SkTypes.h>  // SkDebugf — the slot diagnostics
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/skia/SkiaCompiler.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <utility>

#include "ShaderSources.h"

namespace sigil::material::skia {

Effect Effect::filter(sk_sp<SkImageFilter> f) {
  Effect e;
  e.m_filter = std::move(f);
  return e;
}

Effect Effect::recipe(const Material& material) {
  install();
  static constexpr std::string_view kContent[] = {"content"};
  std::unique_ptr<SkRuntimeShaderBuilder> built =
      skia::builder(material, {}, {}, kContent);
  if (!built) return {};
  return filter(SkImageFilters::RuntimeShader(*built, "content", nullptr));
}

Effect Effect::glow(SkColor4f color, float sigma) {
  return filter(SkImageFilters::DropShadow(0, 0, sigma, sigma,
                                           color.toSkColor(), nullptr));
}

Effect Effect::phosphorBloom(float radius, float threshold, float intensity,
                             float chroma, float hueDrift, float tail) {
  static const sk_sp<SkRuntimeEffect> effect = [] {
    auto [program, error] = SkRuntimeEffect::MakeForShader(
        SkString(shaderSource("PhosphorBloom.sksl")));
    if (!program)
      SkDebugf("[material] skia::Effect::phosphorBloom: shader failed: %s\n",
               error.c_str());
    return program;
  }();

  constexpr float kDegree = 3.14159265f / 180.0f;
  return shader(effect, {{"uRadius", std::max(radius, 0.0f)},
                         {"uThreshold", std::clamp(threshold, 0.0f, 0.99f)},
                         {"uIntensity", std::max(intensity, 0.0f)},
                         {"uChroma", std::clamp(chroma, 0.0f, 1.0f)},
                         {"uHueDrift", hueDrift * kDegree},
                         {"uTail", std::max(tail, 0.0f)}});
}

namespace {
/** A uniform name this effect will not take, said once per name.
 *
 *  Once, because a description is rebuilt every frame in a live-coding host:
 *  a per-call warning would bury the console under one typo. The ledger is
 *  capped so a program that mints names cannot grow it without bound. */
void warnUndeclaredEffectUniform(const char* door, const std::string& name) {
  static std::vector<std::string> seen;
  for (const std::string& s : seen)
    if (s == name) return;
  if (seen.size() >= 16) return;
  seen.push_back(name);
  SkDebugf(
      "[material] skia::Effect::%s(\"%s\"): the effect declares no uniform by "
      "that name at this value's size — ignored (warned once; an array "
      "must supply the declared total float count exactly)\n",
      door, name.c_str());
}
}  // namespace

Effect Effect::shader(sk_sp<SkRuntimeEffect> effect,
                      std::vector<std::pair<std::string, float>> uniforms) {
  Effect e;
  if (!effect) return e;
  // Material's guardrail, and for the same reason: SkRuntimeShaderBuilder
  // answers a name the effect does not declare — or one whose declared size
  // is not four bytes, which is every float2, float4 and array — with a
  // debug abort and no write, and this Skia is built without SK_DEBUG, so
  // the value is dropped and the effect paints with a zeroed uniform. Drop
  // the entry here instead, loudly, and keep the recipe free of anything
  // buildFilter would have to re-check.
  std::erase_if(uniforms, [&](const std::pair<std::string, float>& entry) {
    if (detail::declaresUniform(effect, entry.first, sizeof(float)))
      return false;
    warnUndeclaredEffectUniform("shader", entry.first);
    return true;
  });
  SkRuntimeShaderBuilder builder(effect);
  for (const auto& [name, value] : uniforms) builder.uniform(name) = value;
  e.m_filter = SkImageFilters::RuntimeShader(builder, "content", nullptr);
  e.m_effect = std::move(effect);
  e.m_uniforms = std::move(uniforms);
  return e;
}

namespace {
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
}  // namespace

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

/** The two blurred levels of the pyramid at a declared range: the layer
 *  at half the range and at the range. Level 0 is the layer itself. */
struct Effect::BlurLevels {
  float maxSigma = 0;
  sk_sp<SkImageFilter> half, full;
};

namespace {
std::shared_ptr<const Effect::BlurLevels> makeBlurLevels(float maxSigma) {
  if (!(maxSigma > 0)) return nullptr;
  auto levels = std::make_shared<Effect::BlurLevels>();
  levels->maxSigma = maxSigma;
  levels->half = SkImageFilters::Blur(maxSigma * 0.5f, maxSigma * 0.5f, nullptr);
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
                                   float sigma, sk_sp<SkShader> sigmaMap) {
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
  return SkImageFilters::RuntimeShader(b, names, inputs, 3);
}
}  // namespace

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

Effect& Effect::child(std::string name, Paint source) {
  // Which names this effect kind can fill — Material::child's structure,
  // one branch per kind, warn-and-ignore everywhere else.
  if (m_paramBlur) {
    if (name != "sigma") {
      SkDebugf(
          "[material] skia::Effect::child(\"%s\") on a blur() — its one child "
          "is \"sigma\", the map; ignored\n",
          name.c_str());
      return *this;
    }
  } else if (m_effect) {
    if (name == "content") {
      SkDebugf(
          "[material] skia::Effect::child(\"content\"): ignored — \"content\" "
          "is the node's own rendered layer, filled by the library\n");
      return *this;
    }
    if (!detail::declaresShaderChild(m_effect, name)) {
      SkDebugf(
          "[material] skia::Effect::child: \"%s\" is not declared by the "
          "effect "
          "as `uniform shader` — ignored\n",
          name.c_str());
      return *this;
    }
  } else {
    SkDebugf(
        "[material] skia::Effect::child(\"%s\"): ignored — this effect has no "
        "shader children to fill (only shader() and blur() do)\n",
        name.c_str());
    return *this;
  }
  auto held = std::make_shared<const Paint>(std::move(source));
  // Last write wins on a name, like Material::child: re-filling a slot
  // replaces rather than stacking two entries the builder would both
  // assign.
  bool replaced = false;
  for (auto& slot : m_children)
    if (slot.first == name) {
      slot.second = std::move(held);
      replaced = true;
      break;
    }
  if (!replaced) m_children.emplace_back(std::move(name), std::move(held));
  // Refresh the static snapshot (Material::child does the same). Note this
  // must be an UNCONDITIONAL rebuild: resolvedImageFilter(nullptr) would
  // hand back the snapshot it is meant to replace, and a STATIC child on a
  // shader() effect — one the paint path has no reason to re-resolve —
  // would then never reach the filter at all.
  m_filter = buildFilter(nullptr);
  return *this;
}

bool Effect::anyChildNeedsContext() const {
  for (const auto& [name, child] : m_children)
    if (child && (child->isAnimated() || child->geometryDependent()))
      return true;
  return false;
}

sk_sp<SkShader> Effect::childShaderFor(std::string_view name,
                                       const PaintFrame* frame) const {
  for (const auto& [slot, child] : m_children)
    if (slot == name)
      return child ? detail::childShader(*child, frame) : nullptr;
  return nullptr;
}

Effect& Effect::uniform(std::string name, motion::Animatable<float> value) {
  // Every dropped binding says so — Material's guardrail: warn and ignore,
  // never a debug abort (one sketch typo must not kill the hot-reload
  // host). A silent drop here loses an animation with no diagnostic.
  if (m_dirBlur) {
    // The recipe's named parameters — anything else warns and is ignored.
    if (name != "sigma" && name != "angle" && name != "across") {
      SkDebugf(
          "[material] skia::Effect::uniform(\"%s\") on a directionalBlur() — "
          "not one of \"sigma\"/\"angle\"/\"across\"; ignored\n",
          name.c_str());
      return *this;
    }
    m_bound.emplace_back(std::move(name), std::move(value));
    return *this;
  }
  if (m_paramBlur) {
    if (name != "maxSigma") {
      SkDebugf(
          "[material] skia::Effect::uniform(\"%s\") on a blur() — its one "
          "parameter is \"maxSigma\" (the MAP is child(\"sigma\", "
          "Paint)); ignored\n",
          name.c_str());
      return *this;
    }
    m_bound.emplace_back(std::move(name), std::move(value));
    return *this;
  }
  if (m_effect) {
    // The shader() path is the one that takes arbitrary names, so it is the
    // one that must ask the effect. A rejected binding is not recorded at
    // all, which also means it declares no volatility: an ignored binding
    // that still marked the node live would cost a repaint every frame
    // forever, for a value nothing reads.
    if (!detail::declaresUniform(m_effect, name, sizeof(float))) {
      warnUndeclaredEffectUniform("uniform", name);
      return *this;
    }
    m_bound.emplace_back(std::move(name), std::move(value));
    return *this;
  }
  SkDebugf(
      "[material] skia::Effect::uniform(\"%s\"): ignored — this effect has no "
      "uniform to receive it (only shader(), directionalBlur() and "
      "blur() do; a filter() wraps an already-built SkImageFilter)\n",
      name.c_str());
  return *this;
}

namespace {
/** The gate every constant-uniform door on Effect shares: only a shader()
 *  effect has named declarations to fill, and a name it does not declare
 *  at the value's size warns once and is ignored — Material's rule. */
bool effectTakesConstant(const sk_sp<SkRuntimeEffect>& effect,
                         const std::string& name, size_t bytes,
                         bool otherKind) {
  if (otherKind || !effect) {
    SkDebugf(
        "[material] skia::Effect::uniform(\"%s\", const): ignored — only a "
        "shader() effect has named declarations to fill (directionalBlur "
        "and blur take their parameters at construction or as bound "
        "Outputs)\n",
        name.c_str());
    return false;
  }
  if (!detail::declaresUniform(effect, name, bytes)) {
    warnUndeclaredEffectUniform("uniform", name);
    return false;
  }
  return true;
}
}  // namespace

Effect& Effect::uniform(std::string name, float value) {
  if (!effectTakesConstant(m_effect, name, sizeof(float),
                           m_dirBlur || m_paramBlur))
    return *this;
  m_uniforms.emplace_back(std::move(name), value);
  m_filter = buildFilter(nullptr);  // refresh the snapshot, as child() does
  return *this;
}

Effect& Effect::uniform(std::string name, std::array<float, 2> value) {
  if (!effectTakesConstant(m_effect, name, 2 * sizeof(float),
                           m_dirBlur || m_paramBlur))
    return *this;
  m_uniforms2.emplace_back(std::move(name), value);
  m_filter = buildFilter(nullptr);
  return *this;
}

Effect& Effect::uniform(std::string name, std::array<float, 4> value) {
  if (!effectTakesConstant(m_effect, name, 4 * sizeof(float),
                           m_dirBlur || m_paramBlur))
    return *this;
  m_uniforms4.emplace_back(std::move(name), value);
  m_filter = buildFilter(nullptr);
  return *this;
}

Effect& Effect::uniform(std::string name, std::vector<float> values) {
  // An array validates by TOTAL float count — all the builder checks, and
  // the builder refuses a partial write, so the count must be exact.
  if (!effectTakesConstant(m_effect, name, values.size() * sizeof(float),
                           m_dirBlur || m_paramBlur))
    return *this;
  m_uniformArrays.emplace_back(std::move(name), std::move(values));
  m_filter = buildFilter(nullptr);
  return *this;
}

Effect& Effect::uniform(std::string name,
                        std::shared_ptr<const UniformBlock> block) {
  if (!block) {
    SkDebugf(
        "[material] skia::Effect::uniform(\"%s\", block): null UniformBlock — "
        "there is nothing to read at paint time; ignored\n",
        name.c_str());
    return *this;
  }
  if (!effectTakesConstant(m_effect, name, block->size() * sizeof(float),
                           m_dirBlur || m_paramBlur))
    return *this;
  // A rejected block is not recorded, so it declares no volatility —
  // the same rule a rejected Output binding follows.
  m_blocks.emplace_back(std::move(name), std::move(block));
  return *this;  // now LIVE: read at every paint, like a bound Output
}

Effect Effect::then(const Effect& next) const {
  Effect e;
  const bool thisReal = m_filter || isAnimated();
  const bool nextReal = next.m_filter || next.isAnimated();
  if (!thisReal) return next;
  if (!nextReal) return *this;
  if (isAnimated() || next.isAnimated()) {
    // A live side cannot precompose: hold both and re-compose per paint.
    e.m_chainA = std::make_shared<const Effect>(*this);
    e.m_chainB = std::make_shared<const Effect>(next);
    return e;
  }
  e.m_filter = SkImageFilters::Compose(next.m_filter, m_filter);
  return e;
}

sk_sp<SkImageFilter> Effect::resolvedImageFilter(const PaintFrame* ctx) const {
  if (m_chainA)
    return SkImageFilters::Compose(m_chainB->resolvedImageFilter(ctx),
                                   m_chainA->resolvedImageFilter(ctx));
  // A context-needing child (live or geometry tier) has to be re-resolved
  // per paint; a static one is already in the snapshot. Same question
  // Material::build's memo asks of its children, same answer.
  if (m_bound.empty() && m_blocks.empty() && !(ctx && anyChildNeedsContext()))
    return m_filter;
  return buildFilter(ctx);
}

/** THE FILTER, built from the recipe — unconditionally, which is what
 *  separates it from resolvedImageFilter(): the store-time snapshot and the
 *  per-paint resolve are the SAME construction differing only in whether
 *  there is a context, exactly as Material::build(live, ctx) is. */
sk_sp<SkImageFilter> Effect::buildFilter(const PaintFrame* ctx) const {
  if (m_paramBlur) {  // re-wrap the held pyramid with the parameter's scale
    float sigma = m_paramBlur->maxSigma;
    for (const auto& [name, out] : m_bound)
      if (name == "maxSigma") sigma = motion::resolveFloatAt(nullptr, out);
    // A declared 0 holds no pyramid; a bound value then builds one at
    // the value, at every paint — the cost declaring the range avoids.
    const std::shared_ptr<const BlurLevels> levels =
        m_blurLevels ? m_blurLevels : makeBlurLevels(sigma);
    return makeParamBlur(levels.get(), sigma, childShaderFor("sigma", ctx));
  }
  if (m_dirBlur) {  // rebuild the sandwich from the bound parameters
    DirectionalBlur d = *m_dirBlur;
    for (const auto& [name, out] : m_bound) {
      const float v = motion::resolveFloatAt(nullptr, out);
      if (name == "sigma")
        d.sigma = v;
      else if (name == "angle")
        d.angleDeg = v;
      else if (name == "across")
        d.across = v;
    }
    return makeDirectionalBlur(d.sigma, d.angleDeg, d.across);
  }
  if (!m_effect) return m_filter;
  SkRuntimeShaderBuilder builder(m_effect);
  for (const auto& [name, value] : m_uniforms) builder.uniform(name) = value;
  for (const auto& [name, value] : m_uniforms2) builder.uniform(name) = value;
  for (const auto& [name, value] : m_uniforms4) builder.uniform(name) = value;
  for (const auto& [name, values] : m_uniformArrays)
    builder.uniform(name).set(values.data(), (int)values.size());
  for (const auto& [name, out] : m_bound)
    builder.uniform(name) = motion::resolveFloatAt(nullptr, out);
  for (const auto& [name, block] : m_blocks)
    builder.uniform(name).set(block->values().data(), (int)block->size());
  // The child slots, against the painting node's box (Paint::child's
  // contract: a child sees the SAME frame, because there is one node).
  // "content" is the library's and is filled by the factory below.
  for (const auto& [name, child] : m_children)
    if (child) builder.child(name) = detail::childShader(*child, ctx);
  return SkImageFilters::RuntimeShader(builder, "content", nullptr);
}

bool Effect::isAnimated() const {
  // A bound block is a bound Output whose value is a table: read at every
  // paint, so the node must stay volatile for as long as it is attached.
  if (!m_blocks.empty()) return true;
  // A scalar binding counts only while it is LIVE: one holding a plain
  // number is a uniform value, and a node does not repaint forever for a
  // constant.
  for (const auto& [name, out] : m_bound)
    if (motion::isLive(nullptr, out)) return true;
  // Tier inheritance: a live child makes the whole effect live, so the node
  // is declared volatile and no cache can sample the parameter once and
  // freeze it. Material answers this question for its own subtree, so the
  // recursion stops at the child.
  for (const auto& [name, child] : m_children)
    if (child && child->isAnimated()) return true;
  return m_chainA && (m_chainA->isAnimated() || m_chainB->isAnimated());
}

bool Effect::usesWorldSpace() const {
  // Same tier-inheritance shape as isAnimated(): Material's own recursion
  // answers for blend layers and nested children.
  for (const auto& [name, child] : m_children)
    if (child && child->usesWorldSpace()) return true;
  return m_chainA && (m_chainA->usesWorldSpace() || m_chainB->usesWorldSpace());
}

/** Children compare by VALUE (Material::operator==, recursive), by the same
 *  rule material children follow: anything read live that did not
 *  participate in reconciler equality would leave a pruned node sampling
 *  the parameter its recording was made with. */
static bool childrenEqual(
    const std::vector<std::pair<std::string, std::shared_ptr<const Paint>>>& a,
    const std::vector<std::pair<std::string, std::shared_ptr<const Paint>>>&
        b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (a[i].first != b[i].first) return false;
    if (!a[i].second || !b[i].second) {
      if (a[i].second != b[i].second) return false;
      continue;
    }
    if (!(*a[i].second == *b[i].second)) return false;
  }
  return true;
}

bool Effect::operator==(const Effect& o) const {
  if (isAnimated() || o.isAnimated())
    return false;                    // live never prunes — the material rule
  if (m_paramBlur || o.m_paramBlur)  // blur(): by RECIPE + the sigma MAP
    return m_paramBlur == o.m_paramBlur &&
           childrenEqual(m_children, o.m_children);
  if (m_dirBlur || o.m_dirBlur)       // directionalBlur(): by RECIPE, so a
    return m_dirBlur == o.m_dirBlur;  // re-described equal one prunes
  if (m_effect || o.m_effect)
    return m_effect == o.m_effect && m_uniforms == o.m_uniforms &&
           m_uniforms2 == o.m_uniforms2 && m_uniforms4 == o.m_uniforms4 &&
           m_uniformArrays == o.m_uniformArrays &&
           childrenEqual(m_children, o.m_children);
  return m_filter == o.m_filter;  // filter(): pointer identity, as ever
}

}  // namespace sigil::material::skia
