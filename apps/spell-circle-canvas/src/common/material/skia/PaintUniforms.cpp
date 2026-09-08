/** @file
 * THE DOORS THAT TAKE A NAME: every uniform() overload and child(), for
 * both the sksl recipe and the material instance. Each validates the name
 * against what the effect declares, copies the recipe on write because a
 * paint is a value, and refreshes the static snapshot a constant changed.
 */

#include <include/core/SkTypes.h>  // SkDebugf
#include <sigilmaterial/texture/ShaderLeaf.h>

#include <array>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "PaintInternal.h"

namespace sigil::material::skia {

namespace {

/** A compose Paint filling a slot of a recipe-backed one: the static
 *  snapshot of a native source (a gradient, an image), compared by
 *  compose's own equality so two equal descriptions prune. A child that
 *  needs a paint context to resolve is sampled once here, as its own
 *  documentation says a recipe slot does. */
class MaterialLeaf final : public sigil::material::ShaderLeaf {
 public:
  explicit MaterialLeaf(Paint source) : m_source(std::move(source)) {}
  sk_sp<SkShader> shader() const override { return m_source.asShader(); }
  bool animated() const override { return m_source.isAnimated(); }

 protected:
  bool equals(const sigil::material::Leaf& other) const override {
    return m_source == static_cast<const MaterialLeaf&>(other).m_source;
  }

 private:
  Paint m_source;
};

}  // namespace

Paint& Paint::uniform(std::string name, float value) {
  if (m_backed) {
    detachBacked();
    m_backed->material.set(name, value);
    m_shader = buildBacked(nullptr);
    return *this;
  }
  if (!m_live) {
    SkDebugf(
        "skia::Paint::uniform(\"%s\", const): ignored — this material has no "
        "named uniforms (only sksl() does)\n",
        name.c_str());
    return *this;
  }
  if (!validUniform(m_live->effect, name, sizeof(float))) {
    warnUnknownUniform("uniform", name);
    return *this;
  }
  detachLive();
  // Constants own their slot (injection ownership): a baked-in uTime /
  // uContentScale never ticks again, so it must not keep the material Live
  // — the header promises "the material stays static".
  if (name == "uTime")
    m_live->usesTime = false;
  else if (name == "uContentScale")
    m_live->usesScale = false;
  putByName(m_live->constants, std::move(name), value);
  m_shader = build(*m_live, nullptr);  // refresh the static snapshot
  return *this;
}

Paint& Paint::child(std::string name, Paint source) {
  if (m_backed) {
    detachBacked();
    if (source.m_backed)
      m_backed->material.child(name, source.m_backed->material);
    else
      m_backed->material.child(name, MaterialLeaf(std::move(source)));
    m_shader = buildBacked(nullptr);
    return *this;
  }
  if (!m_live) {
    SkDebugf(
        "skia::Paint::child(\"%s\"): ignored — this material has no shader "
        "children (only sksl() does)\n",
        name.c_str());
    return *this;
  }
  if (!detail::declaresShaderChild(m_live->effect, name)) {
    SkDebugf(
        "skia::Paint::child: \"%s\" is not declared by the effect as "
        "`uniform shader` — ignored\n",
        name.c_str());
    return *this;
  }
  detachLive();
  // Last write wins on a name, so re-filling a slot replaces rather than
  // stacking (two entries would both be assigned and the second silently
  // shadow the first in the builder).
  for (auto& slot : m_live->children)
    if (slot.first == name) {
      slot.second = std::move(source);
      m_shader = build(*m_live, nullptr);
      return *this;
    }
  m_live->children.emplace_back(std::move(name), std::move(source));
  m_shader = build(*m_live, nullptr);  // refresh the static snapshot
  return *this;
}

Paint& Paint::uniform(std::string name, motion::Animatable<float> output) {
  if (m_backed) {
    detachBacked();
    m_backed->material.bind(name, std::move(output));
    return *this;  // now LIVE
  }
  if (!m_live) {
    SkDebugf(
        "skia::Paint::uniform(\"%s\", &output): ignored — this material has "
        "no named uniforms (only sksl() does)\n",
        name.c_str());
    return *this;
  }
  if (!validUniform(m_live->effect, name, sizeof(float))) {
    warnUnknownUniform("uniform", name);
    return *this;
  }
  detachLive();
  putByName(m_live->binds, std::move(name), std::move(output));
  return *this;  // now LIVE; painting resolves per frame (resolve())
}

Paint& Paint::uniform(std::string name, std::array<float, 2> value) {
  if (m_backed) {
    detachBacked();
    m_backed->material.set(name, glm::vec2(value[0], value[1]));
    m_shader = buildBacked(nullptr);
    return *this;
  }
  if (!m_live) {
    SkDebugf(
        "skia::Paint::uniform(\"%s\", float2): ignored — this material has "
        "no named uniforms (only sksl() does)\n",
        name.c_str());
    return *this;
  }
  if (!validUniform(m_live->effect, name, 2 * sizeof(float))) {
    warnUnknownUniform("uniform", name);
    return *this;
  }
  detachLive();
  putByName(m_live->constants2, std::move(name), value);
  m_shader = build(*m_live, nullptr);  // refresh the static snapshot
  return *this;
}

Paint& Paint::uniform(std::string name, SkColor4f value) {
  if (m_backed) {
    detachBacked();
    m_backed->material.set(
        name, sigil::material::Color{value.fR, value.fG, value.fB, value.fA});
    m_shader = buildBacked(nullptr);
    return *this;
  }
  if (!m_live) {
    SkDebugf(
        "skia::Paint::uniform(\"%s\", color): ignored — this material has "
        "no named uniforms (only sksl() does)\n",
        name.c_str());
    return *this;
  }
  if (!validUniform(m_live->effect, name, 4 * sizeof(float))) {
    warnUnknownUniform("uniform", name);
    return *this;
  }
  detachLive();
  putByName(m_live->constants4, std::move(name),
            std::array<float, 4>{value.fR, value.fG, value.fB, value.fA});
  m_shader = build(*m_live, nullptr);  // refresh the static snapshot
  return *this;
}

Paint& Paint::uniform(std::string name, std::array<float, 4> value) {
  if (m_backed) {
    detachBacked();
    m_backed->material.set(name,
                           glm::vec4(value[0], value[1], value[2], value[3]));
    m_shader = buildBacked(nullptr);
    return *this;
  }
  if (!m_live) {
    SkDebugf(
        "skia::Paint::uniform(\"%s\", float4): ignored — this material has "
        "no named uniforms (only sksl() does)\n",
        name.c_str());
    return *this;
  }
  if (!validUniform(m_live->effect, name, 4 * sizeof(float))) {
    warnUnknownUniform("uniform", name);
    return *this;
  }
  detachLive();
  putByName(m_live->constants4, std::move(name), value);
  m_shader = build(*m_live, nullptr);  // refresh the static snapshot
  return *this;
}

Paint& Paint::uniform(std::string name, std::vector<float> values) {
  if (m_backed) {
    detachBacked();
    m_backed->material.set(name, std::span<const float>(values));
    m_shader = buildBacked(nullptr);
    return *this;
  }
  if (!m_live) {
    SkDebugf(
        "skia::Paint::uniform(\"%s\", array): ignored — this material has "
        "no named uniforms (only sksl() does)\n",
        name.c_str());
    return *this;
  }
  // Validated by TOTAL byte size, which is all the builder distinguishes —
  // and the builder refuses a partial array write, so the count must be
  // the declaration's exactly.
  if (!validUniform(m_live->effect, name, values.size() * sizeof(float))) {
    warnUnknownUniform("uniform", name);
    return *this;
  }
  detachLive();
  putByName(m_live->constantArrays, std::move(name), std::move(values));
  m_shader = build(*m_live, nullptr);  // refresh the static snapshot
  return *this;
}

Paint& Paint::uniform(std::string name,
                      std::shared_ptr<const material::UniformBlock> block) {
  if (m_backed) {
    detachBacked();
    m_backed->material.bind(name, std::move(block));
    return *this;  // now LIVE
  }
  if (!m_live) {
    SkDebugf(
        "skia::Paint::uniform(\"%s\", block): ignored — this material has "
        "no named uniforms (only sksl() does)\n",
        name.c_str());
    return *this;
  }
  if (!block) {
    SkDebugf(
        "skia::Paint::uniform(\"%s\", block): null UniformBlock — there is "
        "nothing to read at paint time; ignored\n",
        name.c_str());
    return *this;
  }
  if (!validUniform(m_live->effect, name, block->size() * sizeof(float))) {
    warnUnknownUniform("uniform", name);
    return *this;
  }
  detachLive();
  putByName(m_live->blocks, std::move(name), std::move(block));
  return *this;  // now LIVE; painting resolves per frame (resolve())
}

}  // namespace sigil::material::skia
