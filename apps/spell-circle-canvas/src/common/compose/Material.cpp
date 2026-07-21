// Material — construction of the polymorphic paint value. Static materials
// (solid/gradient/image/blend/const-uniform sksl) resolve eagerly to a color
// or shader and collapse to a Fill (toFill()), riding the kernel's existing
// fill path. A LIVE material (sksl with a ch::Output-bound uniform) keeps its
// runtime-effect recipe and rebuilds its shader every frame from the current
// uniform values (resolve()); Element::fill routes it to the live path and the
// painter re-resolves it while its node stays volatile. See Material.h.

#include "sigilcompose/Material.h"

#include "ComposeInternal.h" // ElementNode, for Element::fill(Material)

#include <include/core/SkImage.h>
#include <include/core/SkShader.h>
#include <include/core/SkTypes.h> // SkDebugf
#include <include/effects/SkGradient.h>
#include <include/effects/SkRuntimeEffect.h>

#include <array>

namespace sigil::compose {

/** The sksl recipe behind a Material (opaque in the header). */
struct Material::Live {
  sk_sp<SkRuntimeEffect> effect;
  std::vector<std::pair<std::string, float>> constants;
  std::vector<std::pair<std::string, const choreograph::Output<float> *>> binds;
};

namespace {

struct RampArrays {
  std::vector<SkColor4f> colors;
  std::vector<float> positions; // empty → evenly spaced
};

RampArrays split(const std::vector<Stop> &stops) {
  RampArrays r;
  r.colors.reserve(stops.size());
  r.positions.reserve(stops.size());
  for (const Stop &s : stops) {
    r.colors.push_back(s.color);
    r.positions.push_back(s.pos);
  }
  return r;
}

// A named uniform is usable iff the effect declares it at the expected size —
// assigning an undeclared or mis-sized uniform SkDEBUGFAILs (aborts debug
// builds), which would let one typo kill the ComposeSketch hot-reload host.
// Unknown names warn-and-ignore instead (validated at sksl()/uniform() time,
// so build() never touches an invalid entry).
bool validUniform(const sk_sp<SkRuntimeEffect> &effect, std::string_view name,
                  size_t bytes) {
  if (!effect)
    return false;
  const SkRuntimeEffect::Uniform *u = effect->findUniform(name);
  return u && u->sizeInBytes() == bytes;
}

void warnUnknownUniform(const char *what, const std::string &name) {
  SkDebugf("Material::%s: uniform \"%s\" is not declared by the effect at "
           "float size — ignored\n",
           what, name.c_str());
}

// SkGradient's color/position spans are non-owning — keep `r` alive across the
// SkShaders::*Gradient call (the shader copies the stops during construction).
SkGradient makeGradient(const RampArrays &r, SkTileMode tile) {
  return SkGradient({{r.colors.data(), r.colors.size()},
                     {r.positions.data(), r.positions.size()}, tile},
                    {});
}

} // namespace

// Build a shader from an sksl recipe: constants, then bound Outputs at their
// current value, then the auto-injected uTime/uResolution/uContentScale — the
// last three only when the effect actually declares them (assigning a uniform
// the effect lacks aborts in debug builds). `ctx` is null for the static build.
sk_sp<SkShader> Material::build(const Live &live, const PaintContext *ctx) {
  if (!live.effect)
    return nullptr;
  SkRuntimeShaderBuilder b(live.effect);
  for (const auto &[name, value] : live.constants)
    b.uniform(name.c_str()) = value; // user-named: a typo aborts in debug
  for (const auto &[name, out] : live.binds)
    if (out)
      b.uniform(name.c_str()) = out->value();
  if (ctx) {
    // Auto-injects are size-checked too: a user declaring `uniform float
    // uResolution` must not receive a float2 write (SkDEBUGFAIL).
    if (validUniform(live.effect, "uTime", sizeof(float)))
      b.uniform("uTime") = (float)ctx->elapsedSeconds;
    if (validUniform(live.effect, "uContentScale", sizeof(float)))
      b.uniform("uContentScale") = ctx->contentScale;
    if (validUniform(live.effect, "uResolution", 2 * sizeof(float)))
      b.uniform("uResolution") =
          std::array<float, 2>{ctx->size.width(), ctx->size.height()};
  }
  return b.makeShader();
}

Material Material::solid(SkColor4f color) {
  Material m;
  m.m_isSolid = true;
  m.m_solid = color;
  return m;
}

Material Material::shader(sk_sp<SkShader> shader) {
  Material m;
  m.m_shader = std::move(shader);
  return m;
}

Material Material::linear(SkPoint a, SkPoint b, std::vector<Stop> stops,
                          SkTileMode tile) {
  RampArrays r = split(stops);
  const SkPoint pts[2] = {a, b};
  return shader(SkShaders::LinearGradient(pts, makeGradient(r, tile)));
}

Material Material::radial(SkPoint center, float radius, std::vector<Stop> stops,
                          SkTileMode tile) {
  RampArrays r = split(stops);
  return shader(
      SkShaders::RadialGradient(center, radius, makeGradient(r, tile)));
}

Material Material::sweep(SkPoint center, std::vector<Stop> stops, float startDeg,
                         float endDeg) {
  RampArrays r = split(stops);
  return shader(SkShaders::SweepGradient(center, startDeg, endDeg,
                                         makeGradient(r, SkTileMode::kClamp)));
}

Material Material::image(sk_sp<SkImage> image, SkTileMode tx, SkTileMode ty,
                         const SkMatrix &local, SkSamplingOptions sampling) {
  if (!image)
    return {};
  return shader(SkShaders::Image(std::move(image), tx, ty, sampling, &local));
}

Material Material::sksl(sk_sp<SkRuntimeEffect> effect,
                        std::vector<std::pair<std::string, float>> constants) {
  Material m;
  if (!effect)
    return m;
  m.m_live = std::make_shared<Live>();
  m.m_live->effect = std::move(effect);
  for (auto &[name, value] : constants) {
    if (!validUniform(m.m_live->effect, name, sizeof(float))) {
      warnUnknownUniform("sksl", name);
      continue;
    }
    m.m_live->constants.emplace_back(std::move(name), value);
  }
  m.m_shader = build(*m.m_live, nullptr); // static snapshot (constants only)
  return m;
}

Material Material::blend(std::vector<std::pair<Material, SkBlendMode>> layers) {
  if (layers.empty())
    return {};
  sk_sp<SkShader> acc = layers.front().first.asShader();
  for (size_t i = 1; i < layers.size(); ++i) {
    sk_sp<SkShader> src = layers[i].first.asShader();
    if (!acc) {
      acc = std::move(src);
      continue;
    }
    if (!src)
      continue;
    acc = SkShaders::Blend(layers[i].second, std::move(acc), std::move(src));
  }
  return shader(std::move(acc));
}

// uniform() mutations copy-on-write the recipe: Material is a VALUE — copies
// of a base material must never alias one another's uniforms (two HUD bars
// sharing one sksl base bind different Outputs; last-write-wins would corrupt
// both silently — the Fable audit's aliasing defect).
void Material::detachLive() {
  if (m_live && m_live.use_count() > 1)
    m_live = std::make_shared<Live>(*m_live);
}

Material &Material::uniform(std::string name, float value) {
  if (!m_live) {
    SkDebugf("Material::uniform(\"%s\", const): ignored — this material has no "
             "named uniforms (only sksl() does)\n",
             name.c_str());
    return *this;
  }
  if (!validUniform(m_live->effect, name, sizeof(float))) {
    warnUnknownUniform("uniform", name);
    return *this;
  }
  detachLive();
  m_live->constants.emplace_back(std::move(name), value);
  m_shader = build(*m_live, nullptr); // refresh the static snapshot
  return *this;
}

Material &Material::uniform(std::string name,
                            const choreograph::Output<float> *output) {
  if (!m_live) {
    SkDebugf("Material::uniform(\"%s\", &output): ignored — this material has "
             "no named uniforms (only sksl() does)\n",
             name.c_str());
    return *this;
  }
  if (!validUniform(m_live->effect, name, sizeof(float))) {
    warnUnknownUniform("uniform", name);
    return *this;
  }
  detachLive();
  m_live->binds.emplace_back(std::move(name), output);
  return *this; // now LIVE; painting resolves per frame (resolve())
}

bool Material::isLive() const { return m_live && !m_live->binds.empty(); }

sk_sp<SkShader> Material::asShader() const {
  // A live material's m_shader snapshot predates its binds — rebuild fresh so
  // bound Outputs contribute their CURRENT values (what blend() flattens; the
  // Fable audit's stale-snapshot defect).
  if (isLive())
    return build(*m_live, nullptr);
  if (m_shader)
    return m_shader;
  if (m_isSolid)
    return SkShaders::Color(m_solid, nullptr);
  return nullptr; // none
}

Fill Material::toFill() const {
  if (m_isSolid)
    return Fill::color(m_solid);
  if (m_shader)
    return Fill::shader(m_shader);
  return Fill::none();
}

Fill Material::resolve(const PaintContext &ctx) const {
  if (isLive())
    return Fill::shader(build(*m_live, &ctx));
  return toFill();
}

Element &Element::fill(Material m) {
  if (m.isLive()) {
    // Live materials re-resolve per frame — route to the volatile path and
    // clear any static fill so the painter reads the material.
    m_node->liveMaterial = std::move(m);
    m_node->paint.fill.reset();
  } else {
    m_node->paint.fill = PropValue<Fill>{m.toFill()};
    m_node->liveMaterial.reset();
  }
  return *this;
}

} // namespace sigil::compose
