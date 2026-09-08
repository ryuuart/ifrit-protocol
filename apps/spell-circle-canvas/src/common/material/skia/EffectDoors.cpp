/** @file
 * THE DOORS THAT TAKE A NAME: child(), every uniform() overload, and the
 * questions asked of the slots they fill. Each validates the name against
 * what the effect kind can receive, warns and ignores what it cannot, and
 * refreshes the snapshot a constant changed.
 */

#include <include/core/SkTypes.h>  // SkDebugf
#include <include/effects/SkRuntimeEffect.h>

#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "EffectInternal.h"

namespace sigil::material::skia {

namespace {

/** ONE ENTRY PER NAME, in every uniform lane, as `child()` does for its
 *  slots: setting a uniform twice replaces the first value rather than
 *  stacking two the builder would both assign — an unbounded lane, and a
 *  recipe that compares unequal to the same effect described once. */
template <class Value>
void putByName(std::vector<std::pair<std::string, Value>>& lane,
               std::string name, Value value) {
  for (auto& entry : lane)
    if (entry.first == name) {
      entry.second = std::move(value);
      return;
    }
  lane.emplace_back(std::move(name), std::move(value));
}

}  // namespace

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
    putByName(m_bound, std::move(name), std::move(value));
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
    putByName(m_bound, std::move(name), std::move(value));
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
    putByName(m_bound, std::move(name), std::move(value));
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
  putByName(m_uniforms, std::move(name), value);
  m_filter = buildFilter(nullptr);  // refresh the snapshot, as child() does
  return *this;
}

Effect& Effect::uniform(std::string name, std::array<float, 2> value) {
  if (!effectTakesConstant(m_effect, name, 2 * sizeof(float),
                           m_dirBlur || m_paramBlur))
    return *this;
  putByName(m_uniforms2, std::move(name), value);
  m_filter = buildFilter(nullptr);
  return *this;
}

Effect& Effect::uniform(std::string name, std::array<float, 4> value) {
  if (!effectTakesConstant(m_effect, name, 4 * sizeof(float),
                           m_dirBlur || m_paramBlur))
    return *this;
  putByName(m_uniforms4, std::move(name), value);
  m_filter = buildFilter(nullptr);
  return *this;
}

Effect& Effect::uniform(std::string name, std::vector<float> values) {
  // An array validates by TOTAL float count — all the builder checks, and
  // the builder refuses a partial write, so the count must be exact.
  if (!effectTakesConstant(m_effect, name, values.size() * sizeof(float),
                           m_dirBlur || m_paramBlur))
    return *this;
  putByName(m_uniformArrays, std::move(name), std::move(values));
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
  putByName(m_blocks, std::move(name), std::move(block));
  return *this;  // now LIVE: read at every paint, like a bound Output
}

}  // namespace sigil::material::skia
