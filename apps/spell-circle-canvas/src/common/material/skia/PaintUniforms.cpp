/** @file
 * THE DOORS THAT TAKE A NAME: every set() and bind() overload and slot(), for
 * both the sksl recipe and the material instance. Each validates the name
 * against what the effect declares, copies the recipe on write because a
 * paint is a value, and refreshes the static snapshot a constant changed.
 */

#include <include/core/SkTypes.h>  // SkDebugf
#include <sigilmaterial/skia/ShaderLeaf.h>

#include <algorithm>
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

/** A native paint filling a recipe slot, compared by value and resolved
 *  with the same frame as the parent. */
class MaterialLeaf final : public ShaderLeaf {
 public:
  explicit MaterialLeaf(Paint source) : m_source(std::move(source)) {}
  sk_sp<SkShader> shader() const override {
    return skia::PaintAccess::asShader(m_source);
  }
  sk_sp<SkShader> shaderAt(const FrameData& frame) const override {
    return m_source.isSolid()
               ? PaintAccess::asShader(m_source)
               : PaintAccess::shaderFor(m_source, paintFrameOf(frame));
  }
  bool animated() const override { return m_source.isRunning(); }
  bool geometryDependent() const override {
    return m_source.geometryDependent();
  }

 protected:
  bool equals(const sigil::material::Leaf& other) const override {
    return m_source == static_cast<const MaterialLeaf&>(other).m_source;
  }

 private:
  Paint m_source;
};

}  // namespace

bool PaintAccess::storeUniform(Paint& paint, std::string name, float value) {
  if (!paint.m_live) return false;
  if (!validUniform(paint.m_live->effect, name, UniformType::kFloat)) {
    warnUnknownUniform("set", name);
    return false;
  }
  paint.detachLive();
  std::erase_if(paint.m_live->constantArrays,
                [&](const auto& entry) { return entry.first == name; });
  if (name == "uTime")
    paint.m_live->usesTime = false;
  else if (name == "uContentScale")
    paint.m_live->usesScale = false;
  putByName(paint.m_live->constants, std::move(name), value);
  return true;
}

bool PaintAccess::storeUniform(Paint& paint, std::string name,
                               std::array<float, 2> value) {
  if (!paint.m_live) return false;
  if (!validUniform(paint.m_live->effect, name, UniformType::kFloat2)) {
    warnUnknownUniform("set", name);
    return false;
  }
  paint.detachLive();
  std::erase_if(paint.m_live->constantArrays,
                [&](const auto& entry) { return entry.first == name; });
  putByName(paint.m_live->constants2, std::move(name), value);
  return true;
}

bool PaintAccess::storeUniform(Paint& paint, std::string name,
                               std::array<float, 4> value) {
  if (!paint.m_live) return false;
  if (!validUniform(paint.m_live->effect, name, UniformType::kFloat4)) {
    warnUnknownUniform("set", name);
    return false;
  }
  paint.detachLive();
  std::erase_if(paint.m_live->constantArrays,
                [&](const auto& entry) { return entry.first == name; });
  putByName(paint.m_live->constants4, std::move(name), value);
  return true;
}

bool PaintAccess::storeUniform(Paint& paint, std::string name,
                               std::vector<float> values) {
  if (!paint.m_live) return false;
  if (!validFloatPacket(paint.m_live->effect, name, values.size())) {
    warnUnknownUniform("set", name);
    return false;
  }
  paint.detachLive();
  const auto sameName = [&](const auto& entry) { return entry.first == name; };
  std::erase_if(paint.m_live->constants, sameName);
  std::erase_if(paint.m_live->constants2, sameName);
  std::erase_if(paint.m_live->constants4, sameName);
  if (name == "uWorld") paint.m_live->usesWorld = false;
  if (name == "uLocalToSample") paint.m_live->usesSampling = false;
  putByName(paint.m_live->constantArrays, std::move(name), std::move(values));
  return true;
}

bool PaintAccess::storeBinding(Paint& paint, std::string name,
                               motion::Animatable<float> output) {
  if (!paint.m_live) return false;
  if (!validUniform(paint.m_live->effect, name, UniformType::kFloat)) {
    warnUnknownUniform("bind", name);
    return false;
  }
  paint.detachLive();
  if (name == "uTime")
    paint.m_live->usesTime = false;
  else if (name == "uContentScale")
    paint.m_live->usesScale = false;
  std::erase_if(paint.m_live->blocks,
                [&](const auto& entry) { return entry.first == name; });
  putByName(paint.m_live->binds, std::move(name),
            Paint::Live::Binding(std::move(output)));
  return true;
}

bool PaintAccess::storeBinding(Paint& paint, std::string name,
                               motion::Animatable<Color> output) {
  if (!paint.m_live) return false;
  if (!validUniform(paint.m_live->effect, name, UniformType::kFloat4)) {
    warnUnknownUniform("bind", name);
    return false;
  }
  paint.detachLive();
  std::erase_if(paint.m_live->blocks,
                [&](const auto& entry) { return entry.first == name; });
  putByName(paint.m_live->binds, std::move(name),
            Paint::Live::Binding(std::move(output)));
  return true;
}

bool PaintAccess::storeSlot(Paint& paint, std::string name, Paint source) {
  if (!paint.m_live) return false;
  if (!detail::declaresShaderChild(paint.m_live->effect, name)) {
    SkDebugf(
        "Paint::slot: \"%s\" is not declared by the effect as "
        "`uniform shader` — ignored\n",
        name.c_str());
    return false;
  }
  paint.detachLive();
  putByName(paint.m_live->slots, std::move(name), std::move(source));
  return true;
}

}  // namespace sigil::material::skia

namespace sigil::material {

using skia::MaterialLeaf;
using skia::PaintAccess;
using skia::putByName;
using skia::UniformType;
using skia::validUniform;
using skia::warnUnknownUniform;

Paint& Paint::set(std::string name, float value) {
  if (m_backed) {
    detachBacked();
    m_backed->material.set(name, value);
    m_shader = PaintAccess::hold(PaintAccess::buildBacked(*this, nullptr));
    return *this;
  }
  if (!m_live) {
    SkDebugf(
        "Paint::set(\"%s\", const): ignored — this material has no "
        "named uniforms (only sksl() does)\n",
        name.c_str());
    return *this;
  }
  if (PaintAccess::storeUniform(*this, std::move(name), value))
    PaintAccess::refresh(*this);
  return *this;
}

Paint& Paint::slot(std::string name, Paint source) {
  if (m_backed) {
    detachBacked();
    if (source.m_backed)
      m_backed->material.slot(name, source.m_backed->material);
    else
      m_backed->material.slot(name, MaterialLeaf(std::move(source)));
    m_shader = PaintAccess::hold(PaintAccess::buildBacked(*this, nullptr));
    return *this;
  }
  if (!m_live) {
    SkDebugf(
        "Paint::slot(\"%s\"): ignored — this material declares no "
        "slots (only sksl() does)\n",
        name.c_str());
    return *this;
  }
  if (PaintAccess::storeSlot(*this, std::move(name), std::move(source)))
    PaintAccess::refresh(*this);
  return *this;
}

Paint& Paint::bind(std::string name, motion::Animatable<float> output) {
  if (m_backed) {
    detachBacked();
    m_backed->material.bind(name, std::move(output));
    m_shader = PaintAccess::hold(PaintAccess::buildBacked(*this, nullptr));
    return *this;
  }
  if (!m_live) {
    SkDebugf(
        "Paint::bind(\"%s\", value): ignored — this material has "
        "no named uniforms (only sksl() does)\n",
        name.c_str());
    return *this;
  }
  const bool live = output.isRunning();
  if (PaintAccess::storeBinding(*this, std::move(name), std::move(output)) &&
      !live)
    PaintAccess::refresh(*this);
  return *this;
}

Paint& Paint::bind(std::string name, motion::Animatable<Color> output) {
  if (m_backed) {
    detachBacked();
    m_backed->material.bind(name, std::move(output));
    m_shader = PaintAccess::hold(PaintAccess::buildBacked(*this, nullptr));
    return *this;
  }
  if (!m_live) {
    SkDebugf(
        "Paint::bind(\"%s\", color): ignored — this material has "
        "no named uniforms (only sksl() does)\n",
        name.c_str());
    return *this;
  }
  const bool live = output.isRunning();
  if (PaintAccess::storeBinding(*this, std::move(name), std::move(output)) &&
      !live)
    PaintAccess::refresh(*this);
  return *this;
}

Paint& Paint::set(std::string name, std::array<float, 2> value) {
  if (m_backed) {
    detachBacked();
    m_backed->material.set(name, glm::vec2(value[0], value[1]));
    m_shader = PaintAccess::hold(PaintAccess::buildBacked(*this, nullptr));
    return *this;
  }
  if (!m_live) {
    SkDebugf(
        "Paint::set(\"%s\", float2): ignored — this material has "
        "no named uniforms (only sksl() does)\n",
        name.c_str());
    return *this;
  }
  if (PaintAccess::storeUniform(*this, std::move(name), value))
    PaintAccess::refresh(*this);
  return *this;
}

Paint& Paint::set(std::string name, Color value) {
  if (m_backed) {
    detachBacked();
    m_backed->material.set(
        name, sigil::material::Color{value.r, value.g, value.b, value.a});
    m_shader = PaintAccess::hold(PaintAccess::buildBacked(*this, nullptr));
    return *this;
  }
  if (!m_live) {
    SkDebugf(
        "Paint::set(\"%s\", color): ignored — this material has "
        "no named uniforms (only sksl() does)\n",
        name.c_str());
    return *this;
  }
  if (PaintAccess::storeUniform(*this, std::move(name), value))
    PaintAccess::refresh(*this);
  return *this;
}

Paint& Paint::set(std::string name, std::array<float, 4> value) {
  if (m_backed) {
    detachBacked();
    m_backed->material.set(name,
                           glm::vec4(value[0], value[1], value[2], value[3]));
    m_shader = PaintAccess::hold(PaintAccess::buildBacked(*this, nullptr));
    return *this;
  }
  if (!m_live) {
    SkDebugf(
        "Paint::set(\"%s\", float4): ignored — this material has "
        "no named uniforms (only sksl() does)\n",
        name.c_str());
    return *this;
  }
  if (PaintAccess::storeUniform(*this, std::move(name), value))
    PaintAccess::refresh(*this);
  return *this;
}

Paint& Paint::set(std::string name, std::vector<float> values) {
  if (m_backed) {
    detachBacked();
    m_backed->material.set(name, std::span<const float>(values));
    m_shader = PaintAccess::hold(PaintAccess::buildBacked(*this, nullptr));
    return *this;
  }
  if (!m_live) {
    SkDebugf(
        "Paint::set(\"%s\", array): ignored — this material has "
        "no named uniforms (only sksl() does)\n",
        name.c_str());
    return *this;
  }
  if (PaintAccess::storeUniform(*this, std::move(name), std::move(values)))
    PaintAccess::refresh(*this);
  return *this;
}

Paint& Paint::bind(std::string name,
                   std::shared_ptr<const material::UniformBlock> block) {
  if (m_backed) {
    detachBacked();
    m_backed->material.bind(name, std::move(block));
    m_shader = PaintAccess::hold(PaintAccess::buildBacked(*this, nullptr));
    return *this;
  }
  if (!m_live) {
    SkDebugf(
        "Paint::bind(\"%s\", block): ignored — this material has "
        "no named uniforms (only sksl() does)\n",
        name.c_str());
    return *this;
  }
  if (!block) {
    const bool bound = std::ranges::any_of(
        m_live->blocks, [&](const auto& entry) { return entry.first == name; });
    if (!bound) return *this;
    detachLive();
    std::erase_if(m_live->blocks,
                  [&](const auto& entry) { return entry.first == name; });
    m_live->usesTime =
        validUniform(m_live->effect, "uTime", UniformType::kFloat) &&
        !m_live->ownsUniform("uTime");
    m_live->usesScale =
        validUniform(m_live->effect, "uContentScale", UniformType::kFloat) &&
        !m_live->ownsUniform("uContentScale");
    m_live->usesWorld = skia::validWorldUniform(m_live->effect) &&
                        !m_live->ownsUniform("uWorld");
    m_live->usesSampling = validUniform(m_live->effect, "uLocalToSample",
                                        UniformType::kFloat3x3) &&
                           !m_live->ownsUniform("uLocalToSample");
    m_shader = PaintAccess::hold(PaintAccess::build(*m_live, nullptr));
    return *this;
  }
  if (!skia::validFloatPacket(m_live->effect, name, block->size())) {
    warnUnknownUniform("bind", name);
    return *this;
  }
  detachLive();
  if (name == "uTime")
    m_live->usesTime = false;
  else if (name == "uContentScale")
    m_live->usesScale = false;
  else if (name == "uWorld")
    m_live->usesWorld = false;
  else if (name == "uLocalToSample")
    m_live->usesSampling = false;
  std::erase_if(m_live->binds,
                [&](const auto& entry) { return entry.first == name; });
  putByName(m_live->blocks, std::move(name), std::move(block));
  return *this;  // now LIVE; painting resolves per frame (resolve())
}

}  // namespace sigil::material
