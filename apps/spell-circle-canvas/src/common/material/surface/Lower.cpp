/** @file
 * The lowering of a material's stated response into the surface program:
 * the base into the base colour, each channel into its factor or its map
 * slot, and what a surface cannot carry said once.
 */

#include <sigilmaterial/core/Program.h>  // reportOnce
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Texture.h>

#include <memory>
#include <string_view>
#include <utility>
#include <variant>

namespace sigil::material::surface {

namespace {

/** The texture an `image(…)` material samples: the leaf in its one
 *  `image` slot, when the material is that and nothing more. */
const Texture* imageTexture(const Material& material) {
  if (!material.hasProgram() || material.isComposed()) return nullptr;
  return dynamic_cast<const Texture*>(material.leaf("image"));
}

/** @p source into @p slot: an image's own texture as the leaf the slot
 *  binds, anything else as the material the slot evaluates. */
void place(Material& program, std::string_view slot, const Material& source) {
  if (const Texture* texture = imageTexture(source))
    program.slot(slot, *texture);
  else
    program.slot(slot, source);
}

/** The base with its layers and nothing else: what the base colour of a
 *  composed material is. */
Material flat(const Material& material) {
  Material out = material.base();
  for (const Layer& layer : material.layers())
    out.layer(layer.source, layer.options);
  return out;
}

/** A channel's number, or one when a map carries it: the map is
 *  multiplied by the factor, so a factor of one lets it through. */
float factor(const Channel& channel) {
  const float* number = std::get_if<float>(&channel);
  return number ? *number : 1.0f;
}

}  // namespace

Material lower(const Material& material) {
  if (material.hasProgram() && !material.isComposed()) return material;
  if (material.effects())
    reportOnce("surface.lower.effects",
               "a surface has no coverage for an effects stage to read; the "
               "effects on a material drawn as a surface are not drawn");
  // A program base with nothing but effects around it is still that
  // program.
  if (material.hasProgram() && material.layers().empty() &&
      !material.surface())
    return material.base();

  const SurfaceOptions stated =
      material.surface() ? *material.surface() : SurfaceOptions{.unlit = true};
  if (stated.clearcoat != 0)
    reportOnce("surface.lower.clearcoat",
               "the surface program has no clearcoat term; a clearcoat is "
               "not drawn");

  SurfaceParameters parameters;
  const Color* colour =
      material.layers().empty() ? material.color() : nullptr;
  parameters.baseColor = colour ? *colour : Color{1, 1, 1, 1};
  parameters.metallic = factor(stated.metallic);
  parameters.roughness = factor(stated.roughness);
  // The program darkens by an occlusion MAP; a constant has nothing to
  // multiply, so only a material reaches it.
  if (factor(stated.occlusion) != 1.0f)
    reportOnce("surface.lower.occlusion",
               "the surface program reads occlusion from a map; a constant "
               "occlusion other than one is not drawn");
  parameters.normalScale = stated.normalScale;
  parameters.normalDirectX = stated.normalDirectX ? 1.0f : 0.0f;
  parameters.emissive = stated.emission;
  parameters.emissiveStrength = stated.emissionStrength;
  parameters.alphaCutoff = stated.alphaCutoff;
  parameters.transmission = stated.transmission;
  parameters.ior = stated.ior;
  parameters.thickness = stated.thickness;
  parameters.absorption = stated.absorption;
  parameters.reflectionWeight = stated.reflectionWeight;
  if (stated.emissionMap && parameters.emissiveStrength <= 0)
    parameters.emissiveStrength = 1;

  Material out = stated.unlit ? unlit(parameters) : program(parameters);
  if (!colour) place(out, kBaseColorSlot, flat(material));
  const auto channel = [&](std::string_view slot, const Channel& value) {
    if (const Material* map = std::get_if<Material>(&value))
      place(out, slot, *map);
  };
  channel(kMetallicSlot, stated.metallic);
  channel(kRoughnessSlot, stated.roughness);
  channel(kOcclusionSlot, stated.occlusion);
  if (stated.normal) place(out, kNormalSlot, *stated.normal);
  if (stated.emissionMap) place(out, kEmissiveSlot, *stated.emissionMap);
  return out;
}

}  // namespace sigil::material::surface
