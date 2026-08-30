/** @file
 * The field-for-field handover: World's material struct into the
 * surface recipe's params and slots, its masks and layers into the
 * stacking combinator, and its light components into emitter values.
 */

#include "sigilworld/Adapt.h"

#include <sigilmaterial/core/Combine.h>
#include <sigilmaterial/kit/Mask.h>
#include <sigilmaterial/kit/Surface.h>

#include <utility>

namespace sigil::world {

namespace {

material::Color colorOf(const glm::vec4& c) { return {c.x, c.y, c.z, c.w}; }

/** The sampling a world material's uv window asks for: the image is
 *  sampled at `uv * scale + offset`, so texture space enters the sampled
 *  space through that map inverted. */
SkMatrix windowOf(SkV2 scale, SkV2 offset) {
  const float sx = scale.x != 0 ? scale.x : 1;
  const float sy = scale.y != 0 ? scale.y : 1;
  return SkMatrix::Concat(SkMatrix::Scale(1 / sx, 1 / sy),
                          SkMatrix::Translate(-offset.x, -offset.y));
}

material::Texture textureOf(const sk_sp<SkImage>& image, bool tile, SkV2 scale,
                            SkV2 offset) {
  return material::Texture::of(image)
      .tile(tile ? SkTileMode::kRepeat : SkTileMode::kClamp)
      .uv(windowOf(scale, offset));
}

material::Material blend(material::Material base, material::Material top,
                         const Mask& mask, Blend rule) {
  material::Blend as = material::Blend::Mix;
  if (rule == Blend::Add) as = material::Blend::Add;
  if (rule == Blend::Multiply) as = material::Blend::Multiply;
  return material::over(std::move(base), std::move(top), maskOf(mask), as);
}

/** The material without its layers, which is what the recipe's params
 *  and slots describe. */
material::Material flatOf(const Material& m) {
  material::kit::SurfaceParams p;
  p.baseColor = colorOf(m.baseColor);
  p.metallic = m.metallic;
  p.roughness = m.roughness;
  p.emissive = colorOf(m.emissive);
  p.emissiveStrength = m.emissiveStrength;
  p.normalScale = m.normalScale;
  p.normalDirectX = m.normalMapDirectX ? 1.0f : 0.0f;
  p.roughnessChannel = (float)m.roughnessChannel;
  p.metallicChannel = (float)m.metallicChannel;
  p.occlusionChannel = (float)m.occlusionChannel;
  p.occlusionStrength = m.occlusionStrength;
  p.opacityChannel = (float)m.opacityChannel;
  p.alphaCutoff = m.alphaCutoff;
  p.transmission = m.transmission;
  p.ior = m.ior;
  p.thickness = m.thickness;

  material::Material out =
      m.unlit ? material::kit::unlit(p) : material::kit::surface(p);
  const auto place = [&](std::string_view slot, const sk_sp<SkImage>& image) {
    if (image) out.child(slot, textureOf(image, m.tile, m.uvScale, m.uvOffset));
  };
  place(material::kit::kBaseColorSlot, m.texture);
  place(material::kit::kNormalSlot, m.normalMap);
  place(material::kit::kRoughnessSlot, m.roughnessMap);
  place(material::kit::kMetallicSlot, m.metallicMap);
  place(material::kit::kOcclusionSlot, m.occlusionMap);
  place(material::kit::kEmissiveSlot, m.emissiveMap);
  place(material::kit::kOpacitySlot, m.opacityMap);
  return out;
}

}  // namespace

material::Material maskOf(const Mask& mask) {
  material::Material out = material::kit::maskConstant(mask.value);
  if (mask.source == Mask::Source::Map && mask.map)
    out = material::kit::maskMap(
        textureOf(mask.map, mask.tile, mask.uvScale, mask.uvOffset),
        mask.channel);
  else if (mask.source != Mask::Source::Constant)
    out = material::kit::maskConstant(1.0f);
  out = material::kit::fit(std::move(out), mask.low, mask.high);
  return mask.inverted ? material::kit::invert(std::move(out)) : std::move(out);
}

material::Material surfaceOf(const Material& m) {
  material::Material out = flatOf(m.flat());
  for (const Material::Layer& layer : m.layers)
    out =
        blend(std::move(out), flatOf(layer.material), layer.mask, layer.blend);
  return out;
}

light::Light lightOf(const LightComponent& light) {
  if (light.type == LightComponent::Type::Directional)
    return light::sun(light.direction, light.color, light.intensity);
  return light::point(light.position, light.color, light.intensity,
                      light.range);
}

light::Light sunOf(const Lighting& lighting) {
  return light::sun(lighting.sunDirection, lighting.sunColor,
                    lighting.sunIntensity);
}

}  // namespace sigil::world
