/** @file
 * The two surface bodies in each language a renderer speaks, the neutral
 * maps that fill an undressed slot, and the builders — including the one
 * that reads a decoded texture set into slots and channels.
 */

#include "sigilmaterial/surface/Surface.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkSurface.h>
#include <sigilmaterial/core/Program.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilshaders/MaterialSurface.h>

#include <string>
#include <string_view>
#include <utility>

namespace sigil::material::surface {

namespace {

/** The lit body for @p reflection: the shared text with the one line
 *  that says how the environment enters filled in. A negative weight is
 *  the split sum — the surface's own reflectance and its Fresnel decide
 *  — and a weight at or above zero is that much environment, added. */
std::string slangSurface(Reflection reflection) {
  std::string body(shaderSource("Surface.slang"));
  const std::string_view mark = "REFLECTION_WEIGHT";
  const size_t at = body.find(mark);
  // A body with no mark in it is a body the reflection cannot be written
  // into: report it and hand back what was read, rather than throwing out
  // of a recipe definition on a shader edit.
  if (at == std::string::npos) {
    reportOnce("surface.mark",
               "the Slang surface body carries no REFLECTION_WEIGHT mark, so "
               "the reflection choice could not be written into it; the body "
               "compiles as it stands");
    return body;
  }
  body.replace(at, mark.size(),
               reflection == Reflection::SplitSum
                   ? "-1.0"
                   : "max(reflectionWeight, 0.0)");
  return body;
}

Recipe define(std::string name, std::string_view bodyFile,
              const std::string& slangBody) {
  return Recipe::of<SurfaceParameters>(std::move(name))
      .slot(std::string(kBaseColorSlot))
      .slot(std::string(kNormalSlot))
      .slot(std::string(kRoughnessSlot))
      .slot(std::string(kMetallicSlot))
      .slot(std::string(kOcclusionSlot))
      .slot(std::string(kEmissiveSlot))
      .slot(std::string(kOpacitySlot))
      .body(Target::SkSL, std::string(shaderSource("SurfacePrelude.sksl"))
                              .append(shaderSource(bodyFile)))
      // THE TERMS ARE NOT PREPENDED HERE. A Slang renderer loads them
      // once as the module `Shading` and imports it beside the body, so
      // the renderer's own shading and every material compiled with it
      // call one definition of each term rather than a copy apiece.
      .body(
          Target::Slang,
          std::string(shaderSource("SurfacePrelude.slang")).append(slangBody));
}

/** What every neutral fill's producer key starts with, so a reader can
 *  tell a dressing apart from a map a caller placed. */
constexpr char kFillPrefix[] = "material.surface.";

/** A one-pixel texture of @p color, shared by key so two undressed
 *  surfaces compare equal. */
Texture flat(const char* key, material::Color color) {
  return Texture(media::PixelSource::produce(
                     std::string(kFillPrefix) + key,
                     [color]() -> sk_sp<SkImage> {
                       sk_sp<SkSurface> s = SkSurfaces::Raster(
                           SkImageInfo::MakeN32Premul(1, 1));
                       if (!s) return nullptr;
                       s->getCanvas()->clear(skia::toSkColor(color));
                       return s->makeImageSnapshot();
                     }))
      .tile(Repeat::Pad);
}

/** Every slot filled with the value that leaves the parameters speaking for
 *  themselves: white for the maps a scalar multiplies, a flat tangent
 *  normal for the normal map. */
Material dress(Material m) {
  const Texture white = flat("white", SkColors::kWhite);
  m.slot(kBaseColorSlot, white);
  m.slot(kNormalSlot, flat("normal", material::Color{0.5f, 0.5f, 1.0f, 1.0f}));
  m.slot(kRoughnessSlot, white);
  m.slot(kMetallicSlot, white);
  m.slot(kOcclusionSlot, white);
  m.slot(kEmissiveSlot, white);
  m.slot(kOpacitySlot, white);
  return m;
}

}  // namespace

const std::shared_ptr<const Recipe>& surfaceRecipe(Reflection reflection) {
  static const std::shared_ptr<const Recipe> splitSum =
      std::make_shared<const Recipe>(define(
          "surface", "Surface.sksl", slangSurface(Reflection::SplitSum)));
  static const std::shared_ptr<const Recipe> additive =
      std::make_shared<const Recipe>(
          define("surface.additive", "Surface.sksl",
                 slangSurface(Reflection::Additive)));
  return reflection == Reflection::SplitSum ? splitSum : additive;
}

namespace {

const std::shared_ptr<const Recipe>& unlitRecipe() {
  static const std::shared_ptr<const Recipe> recipe =
      std::make_shared<const Recipe>(define(
          "unlit", "Unlit.sksl", std::string(shaderSource("Unlit.slang"))));
  return recipe;
}

}  // namespace

Material program(const SurfaceParameters& parameters, Reflection reflection) {
  return dress(Material(surfaceRecipe(reflection), parameters));
}

Material unlit(const SurfaceParameters& parameters) {
  return dress(Material(unlitRecipe(), parameters));
}

bool isSurface(const Material& material) {
  return material.recipePointer() == surfaceRecipe(Reflection::SplitSum) ||
         material.recipePointer() == surfaceRecipe(Reflection::Additive) ||
         material.recipePointer() == unlitRecipe();
}

bool isUnlit(const Material& material) { return material.recipePointer() == unlitRecipe(); }

const Texture* map(const Material& material, std::string_view slot) {
  const auto* texture = dynamic_cast<const Texture*>(material.leaf(slot));
  if (!texture) return nullptr;
  const auto* producer = texture->source().as<media::Produced>();
  const bool fill = producer && producer->key().rfind(kFillPrefix, 0) == 0;
  return fill ? nullptr : texture;
}

Material program(const texture::TextureMaps& maps, SurfaceParameters base) {
  using texture::Role;
  const auto has = [&](Role role) { return maps.map(role) != nullptr; };
  // A packed occlusion-roughness-metallic image stands in for whichever
  // of the three a set did not ship separately, at glTF's channel order.
  const Texture* packed = maps.map(Role::Packed);
  const auto pick = [&](Role role, int packedChannel,
                        float& channel) -> const Texture* {
    if (const Texture* t = maps.map(role)) {
      channel = 0;
      return t;
    }
    if (!packed) return nullptr;
    channel = (float)packedChannel;
    return packed;
  };
  const Texture* roughness = pick(Role::Roughness, 1, base.roughnessChannel);
  const Texture* metallic = pick(Role::Metallic, 2, base.metallicChannel);
  const Texture* occlusion = pick(Role::Occlusion, 0, base.occlusionChannel);

  // A map's values must be able to reach the shader: the scalar it
  // multiplies starts at one when the set carries that map, unless the
  // caller's base already moved it.
  const SurfaceParameters stock;
  if (metallic && base.metallic == stock.metallic) base.metallic = 1;
  if (roughness && base.roughness == stock.roughness) base.roughness = 1;
  if (has(Role::Emissive)) {
    if (base.emissiveStrength <= 0) base.emissiveStrength = 1;
    if (base.emissive == stock.emissive) base.emissive = {1, 1, 1, 1};
  }
  base.normalDirectX = maps.normalDirectX ? 1.0f : 0.0f;

  Material m = program(base);
  const auto place = [&](std::string_view slot, const Texture* t) {
    if (t) m.slot(slot, *t);
  };
  place(kBaseColorSlot, maps.map(Role::BaseColor));
  place(kNormalSlot, maps.map(Role::Normal));
  place(kRoughnessSlot, roughness);
  place(kMetallicSlot, metallic);
  place(kOcclusionSlot, occlusion);
  place(kEmissiveSlot, maps.map(Role::Emissive));
  place(kOpacitySlot, maps.map(Role::Opacity));
  return m;
}

}  // namespace sigil::material::surface
