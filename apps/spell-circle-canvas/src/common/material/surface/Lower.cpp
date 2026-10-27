/** @file
 * The lowering of a material's stated response into the surface program:
 * the base into the base colour, each channel into its factor or its map
 * slot, and what a surface cannot carry said once.
 */

#include <sigilmaterial/advanced/Program.h>  // reportOnce
#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/core/Instance.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/surface/PaintedMap.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Texture.h>

#include <cstdint>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <optional>
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

/** Whether a device compiles @p material itself: a recipe with a Slang
 *  body, which a body inlines and plays. */
bool deviceCompiles(const Material& material) {
  return material.recipePointer() && material.recipe().has(Target::Slang);
}

/** Whether @p texture is read once across the unit square: the placing
 *  every map of the surface program is read at, since its body reads
 *  every slot at the mesh's own coordinates. What is painted past the
 *  edges is then the sampler's to say, as it is for every map the body
 *  binds. */
bool unplaced(const Texture& texture) {
  return texture.uv() == glm::mat3(1.0f);
}

/** THE PLACED MAP'S VALUES: the two rows of the affine map from a body's
 *  texture coordinates to the image's own, unit across, and per axis how
 *  the image carries on past its edges and one texel's share of it. */
struct PlacedParameters {
  glm::vec4 uAcross;
  glm::vec4 uDown;
  glm::vec2 uRepeat;
  glm::vec2 uTexel;
};

/** A TEXTURE READ WHERE ITS OWN PLACING SAYS, for a slot the surface
 *  program reads at the mesh's coordinates: the image at its matrix and
 *  its tiling per axis, played on the device as the body's child. A
 *  repeating axis is the sampler's to wrap; a mirrored one is folded
 *  here; a padded one is held half a texel inside its edges, because the
 *  sampler repeats once either axis does; an axis painting nothing past
 *  its ends reads clear there. */
const std::shared_ptr<const Recipe>& placedRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<PlacedParameters>("material.surface.placed")
          .slot("image")
          .body(Target::SkSL,
                "half4 main(float2 xy) { return image.eval(xy); }\n")
          .body(Target::Slang,
                "float placedAxis(float at, float repeat, float texel) {\n"
                "  if (repeat == 1.0) return at;\n"
                "  if (repeat == 2.0) {\n"
                "    float folded = at - 2.0 * floor(at * 0.5);\n"
                "    return folded > 1.0 ? 2.0 - folded : folded;\n"
                "  }\n"
                "  return clamp(at, texel * 0.5, 1.0 - texel * 0.5);\n"
                "}\n"
                "float4 surface(float2 uv) {\n"
                "  float3 point = float3(uv, 1.0);\n"
                "  float2 at = float2(dot(uAcross.xyz, point),\n"
                "                     dot(uDown.xyz, point));\n"
                "  if ((uRepeat.x == 3.0 && (at.x < 0.0 || at.x > 1.0)) ||\n"
                "      (uRepeat.y == 3.0 && (at.y < 0.0 || at.y > 1.0)))\n"
                "    return float4(0.0, 0.0, 0.0, 0.0);\n"
                "  return image.Sample(float2(\n"
                "      placedAxis(at.x, uRepeat.x, uTexel.x),\n"
                "      placedAxis(at.y, uRepeat.y, uTexel.y)));\n"
                "}\n"));
  return recipe;
}

/** The repeat's number in the placed body. */
float repeatNumber(Repeat repeat) {
  switch (repeat) {
    case Repeat::Pad:
      return 0;
    case Repeat::Repeat:
      return 1;
    case Repeat::Mirror:
      return 2;
    case Repeat::None:
      return 3;
  }
  return 0;
}

/** Whether @p slot holds a colour rather than numbers. */
bool colourSlot(std::string_view slot) {
  return slot == kBaseColorSlot || slot == kEmissiveSlot ||
         slot == kSheenColorSlot || slot == kSpecularColorSlot ||
         slot == kAttenuationSlot;
}

/** @p texture as the placed child of @p slot. The image is read as its
 *  slot reads it — numbers in a data slot, colour in a colour slot —
 *  unless the texture says otherwise, since the child's own slot carries
 *  no role a renderer could read it by. The placing is the one a base
 *  colour map is read at: the image's pixels laid over the unit square
 *  scaled to the image's size, its matrix applied there. */
Material placed(Texture texture, std::string_view slot) {
  if (!texture.colorSpace())
    texture.colorSpace(colourSlot(slot) ? media::ColorSpace::Srgb
                                        : media::ColorSpace::Data);
  if (!texture.mipmaps()) texture.mipmaps(true);
  const glm::ivec2 size = texture.size();
  const glm::vec2 pixels = size.x > 0 && size.y > 0
                               ? glm::vec2((float)size.x, (float)size.y)
                               : glm::vec2(1, 1);
  glm::mat3 toPixels(1.0f);
  toPixels[0][0] = pixels.x;
  toPixels[1][1] = pixels.y;
  glm::mat3 toUnit(1.0f);
  toUnit[0][0] = 1.0f / pixels.x;
  toUnit[1][1] = 1.0f / pixels.y;
  const glm::mat3 lookup = toUnit * glm::inverse(texture.uv()) * toPixels;
  const PlacedParameters parameters{
      {lookup[0][0], lookup[1][0], lookup[2][0], 0},
      {lookup[0][1], lookup[1][1], lookup[2][1], 0},
      {repeatNumber(texture.tileX()), repeatNumber(texture.tileY())},
      {1.0f / pixels.x, 1.0f / pixels.y}};
  Material out(placedRecipe(), parameters);
  out.slot("image", std::move(texture));
  return out;
}

/** WHETHER @p slot IS READ AS A LEAF beside the body: the base colour map
 *  is the scaffold's, which places it itself, and the four a transmissive
 *  body's shadow tint reads take the texture where it stands. */
bool readAsLeaf(std::string_view slot) {
  return slot == kBaseColorSlot || slot == kTransmissionSlot ||
         slot == kThicknessSlot || slot == kVolumeSlot ||
         slot == kAttenuationSlot;
}

/** @p source into @p slot, as the device can read it: an image's own
 *  texture as the leaf the slot binds, or as a placed child where the
 *  texture's matrix is its own; a material the device compiles as the
 *  child the body plays; and anything else — a gradient, a colour stack,
 *  a program only Skia runs — painted once into a map the slot binds. */
void place(Material& program, std::string_view slot, const Material& source) {
  if (const Texture* texture = imageTexture(source)) {
    if (readAsLeaf(slot) || unplaced(*texture))
      program.slot(slot, *texture);
    else
      program.slot(slot, placed(*texture, slot));
    return;
  }
  if (deviceCompiles(source)) {
    program.slot(slot, source);
    return;
  }
  program.slot(
      slot, Texture(media::PixelSource(PaintedMap(source, !colourSlot(slot)))));
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

/** A colour channel's colour, or white when a map carries it. */
Color factor(const ColorInput& channel) {
  const Color* colour = std::get_if<Color>(&channel);
  return colour ? *colour : Color{1, 1, 1, 1};
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
  if (material.hasProgram() && material.layers().empty() && !material.surface())
    return material.base();

  const SurfaceOptions stated =
      material.surface() ? *material.surface() : SurfaceOptions{.unlit = true};
  SurfaceParameters parameters;
  const Color* colour = material.layers().empty() ? material.color() : nullptr;
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
  parameters.transmission = factor(stated.transmission);
  parameters.ior = stated.ior;
  parameters.thickness = factor(stated.thickness);
  parameters.thicknessAttribute =
      instanceAttributeKey(stated.thicknessAttribute);
  parameters.attenuationColor = stated.attenuationColor;
  parameters.attenuationDistance = stated.attenuationDistance;
  parameters.dispersion = stated.dispersion;
  parameters.flatBack = stated.flatBack ? 1.0f : 0.0f;
  parameters.distortionStrength = stated.distortionStrength;
  parameters.reflectionWeight = stated.reflectionWeight;
  parameters.clearcoat = factor(stated.clearcoat);
  parameters.clearcoatRoughness = factor(stated.clearcoatRoughness);
  // A coat roughness given as a material is read through its own slot,
  // by its red as every other channel here is.
  if (std::holds_alternative<Material>(stated.clearcoatRoughness))
    parameters.clearcoatRoughnessChannel = 0;
  // THE OPTIONAL LOBES. A channel given as a material is read through its
  // own slot by its red, as every other channel here is; a lobe whose
  // only statement is a map is carried because the map states it.
  parameters.sheenColor = factor(stated.sheenColor);
  parameters.sheenRoughness = factor(stated.sheenRoughness);
  if (std::holds_alternative<Material>(stated.sheenRoughness))
    parameters.sheenRoughnessChannel = 0;
  parameters.anisotropy = stated.anisotropy;
  parameters.anisotropyRotation = stated.anisotropyRotation;
  parameters.iridescence = factor(stated.iridescence);
  parameters.iridescenceIor = stated.iridescenceIor;
  parameters.iridescenceThickness = stated.iridescenceThickness;
  parameters.iridescenceThicknessMinimum = stated.iridescenceThicknessMinimum;
  if (stated.iridescenceThicknessMap)
    parameters.iridescenceThicknessChannel = 0;
  parameters.specular = factor(stated.specular);
  if (std::holds_alternative<Material>(stated.specular))
    parameters.specularChannel = 0;
  parameters.specularColorFactor = factor(stated.specularColor);
  const Lobes carried = lobes(stated);
  if (stated.emissionMap && parameters.emissiveStrength <= 0)
    parameters.emissiveStrength = 1;
  parameters.blend = (float)stated.blend;
  parameters.depthWrite =
      stated.depthWrite ? (*stated.depthWrite ? 1.0f : 0.0f) : -1.0f;
  parameters.depthFade = stated.depthFade;

  Material out = stated.unlit
                     ? unlit(parameters)
                     : program(parameters, Reflection::SplitSum, carried);
  const Lobes carriedNow = lobes(out);
  if (!colour) place(out, kBaseColorSlot, flat(material));
  const auto channel = [&](std::string_view slot, const Channel& value) {
    if (const Material* map = std::get_if<Material>(&value))
      place(out, slot, *map);
  };
  channel(kMetallicSlot, stated.metallic);
  channel(kRoughnessSlot, stated.roughness);
  channel(kOcclusionSlot, stated.occlusion);
  channel(kClearcoatSlot, stated.clearcoat);
  channel(kClearcoatRoughnessSlot, stated.clearcoatRoughness);
  if (stated.normal) place(out, kNormalSlot, *stated.normal);
  if (stated.clearcoatNormal)
    place(out, kClearcoatNormalSlot, *stated.clearcoatNormal);
  if (stated.emissionMap) place(out, kEmissiveSlot, *stated.emissionMap);
  if (carriedNow.sheen) {
    if (const Material* map = std::get_if<Material>(&stated.sheenColor))
      place(out, kSheenColorSlot, *map);
    channel(kSheenRoughnessSlot, stated.sheenRoughness);
  }
  if (carriedNow.anisotropy && stated.anisotropyDirection)
    place(out, kAnisotropySlot, *stated.anisotropyDirection);
  if (carriedNow.iridescence) {
    channel(kIridescenceSlot, stated.iridescence);
    if (stated.iridescenceThicknessMap)
      place(out, kIridescenceThicknessSlot, *stated.iridescenceThicknessMap);
  }
  if (carriedNow.transmission) {
    channel(kTransmissionSlot, stated.transmission);
    channel(kThicknessSlot, stated.thickness);
  }
  if (carriedNow.distortion && stated.distortion)
    place(out, kDistortionSlot, *stated.distortion);
  if (carriedNow.specular) {
    channel(kSpecularSlot, stated.specular);
    if (const Material* map = std::get_if<Material>(&stated.specularColor))
      place(out, kSpecularColorSlot, *map);
  }
  return out;
}

}  // namespace sigil::material::surface
