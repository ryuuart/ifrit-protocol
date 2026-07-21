// Material — eager construction of the polymorphic paint value. Each factory
// resolves to a solid color or one flattened SkShader (gradients, blend stacks,
// image/sprite, SkSL), so a Material collapses to a Fill (toFill()) and rides
// the kernel's existing fill caching/prune/paint path unchanged. See
// Material.h for the surface and the "static first cut" rationale.

#include "sigilcompose/Material.h"

#include "ComposeInternal.h" // ElementNode, for Element::fill(Material)

#include <include/core/SkImage.h>
#include <include/core/SkShader.h>
#include <include/effects/SkGradient.h>
#include <include/effects/SkRuntimeEffect.h>

namespace sigil::compose {

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

// SkGradient's color/position spans are non-owning — keep `r` alive across the
// SkShaders::*Gradient call (the shader copies the stops during construction).
SkGradient makeGradient(const RampArrays &r, SkTileMode tile) {
  return SkGradient({{r.colors.data(), r.colors.size()},
                     {r.positions.data(), r.positions.size()}, tile},
                    {});
}

} // namespace

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
                        std::vector<std::pair<std::string, float>> uniforms) {
  if (!effect)
    return {};
  SkRuntimeShaderBuilder builder(std::move(effect));
  for (const auto &[name, value] : uniforms)
    builder.uniform(name.c_str()) = value;
  return shader(builder.makeShader());
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

sk_sp<SkShader> Material::asShader() const {
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

Element &Element::fill(Material m) {
  m_node->paint.fill = PropValue<Fill>{m.toFill()};
  return *this;
}

} // namespace sigil::compose
