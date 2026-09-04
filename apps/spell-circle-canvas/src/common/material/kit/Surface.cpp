/** @file
 * The two surface bodies in each language a renderer speaks, the neutral
 * maps that fill an undressed slot, and the builders — including the one
 * that reads a decoded texture set into slots and channels.
 */

#include "sigilmaterial/kit/Surface.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkSurface.h>
#include <sigilio/hub/TextLibrary.h>

#include <string>
#include <string_view>
#include <utility>

#include "sigilmaterial/kit/Terms.h"

namespace sigil::material::kit {

namespace {

std::string shaderSource(std::string_view name) {
  static io::TextLibrary library("shader://material/kit/",
                                 SIGIL_MATERIAL_KIT_SHADER_DIR);
  return library.text("shader://material/kit/" + std::string(name))
      .value_or("");
}

/** The lit body for @p reflection: the shared text with the one line
 *  that says how the environment enters filled in. A negative weight is
 *  the split sum — the surface's own reflectance and its Fresnel decide
 *  — and a weight at or above zero is that much environment, added. */
std::string slangSurface(Reflection reflection) {
  std::string body = shaderSource("Surface.slang");
  const std::string_view mark = "REFLECTION_WEIGHT";
  const size_t at = body.find(mark);
  body.replace(at, mark.size(),
               reflection == Reflection::SplitSum
                   ? "-1.0"
                   : "max(reflectionWeight, 0.0)");
  return body;
}

Recipe define(std::string name, std::string_view bodyFile,
              const std::string& slangBody) {
  return Recipe::of<SurfaceParams>(std::move(name))
      .child(std::string(kBaseColorSlot))
      .child(std::string(kNormalSlot))
      .child(std::string(kRoughnessSlot))
      .child(std::string(kMetallicSlot))
      .child(std::string(kOcclusionSlot))
      .child(std::string(kEmissiveSlot))
      .child(std::string(kOpacitySlot))
      .body(Target::SkSL,
            shaderSource("SurfacePrelude.sksl") + shaderSource(bodyFile))
      // THE TERMS ARE NOT PREPENDED HERE. A Slang renderer loads them
      // once as the module `Shading` and imports it beside the body, so
      // the renderer's own shading and every material compiled with it
      // call one definition of each term rather than a copy apiece.
      .body(Target::Slang, shaderSource("SurfacePrelude.slang") + slangBody);
}

/** What every neutral fill's producer key starts with, so a reader can
 *  tell a dressing apart from a map a caller placed. */
constexpr char kFillPrefix[] = "material.kit.surface.";

/** A one-pixel texture of @p color, shared by key so two undressed
 *  surfaces compare equal. */
Texture flat(const char* key, SkColor4f color) {
  return Texture::produce(std::string(kFillPrefix) + key,
                          [color] {
                            sk_sp<SkSurface> s = SkSurfaces::Raster(
                                SkImageInfo::MakeN32Premul(1, 1));
                            s->getCanvas()->clear(color);
                            return s->makeImageSnapshot();
                          })
      .tile(SkTileMode::kClamp);
}

/** Every slot filled with the value that leaves the params speaking for
 *  themselves: white for the maps a scalar multiplies, a flat tangent
 *  normal for the normal map. */
Material dress(Material m) {
  const Texture white = flat("white", SkColors::kWhite);
  m.child(kBaseColorSlot, white);
  m.child(kNormalSlot, flat("normal", SkColor4f{0.5f, 0.5f, 1.0f, 1.0f}));
  m.child(kRoughnessSlot, white);
  m.child(kMetallicSlot, white);
  m.child(kOcclusionSlot, white);
  m.child(kEmissiveSlot, white);
  m.child(kOpacitySlot, white);
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

const std::shared_ptr<const Recipe>& unlitRecipe() {
  static const std::shared_ptr<const Recipe> recipe =
      std::make_shared<const Recipe>(
          define("unlit", "Unlit.sksl", shaderSource("Unlit.slang")));
  return recipe;
}

Material surface(const SurfaceParams& params, Reflection reflection) {
  return dress(Material(surfaceRecipe(reflection), params));
}

Material unlit(const SurfaceParams& params) {
  return dress(Material(unlitRecipe(), params));
}

bool isSurface(const Material& m) {
  return m.recipePtr() == surfaceRecipe(Reflection::SplitSum) ||
         m.recipePtr() == surfaceRecipe(Reflection::Additive) ||
         m.recipePtr() == unlitRecipe();
}

bool isUnlit(const Material& m) { return m.recipePtr() == unlitRecipe(); }

SurfaceParams SurfaceParams::chrome() {
  // Steel is not a mirror and not white: a slight cool bias and a
  // roughness a hand-polished object actually has.
  SurfaceParams p;
  p.baseColor = {0.92f, 0.95f, 1.0f, 1};
  p.metallic = 1;
  p.roughness = 0.04f;
  return p;
}

SurfaceParams SurfaceParams::gold() {
  // The measured reflectance of gold, which is what a metal's base
  // colour means.
  SurfaceParams p;
  p.baseColor = {1.0f, 0.766f, 0.336f, 1};
  p.metallic = 1;
  p.roughness = 0.18f;
  return p;
}

SurfaceParams SurfaceParams::metal(Color tint, float roughness) {
  SurfaceParams p;
  p.baseColor = tint;
  p.metallic = 1;
  p.roughness = roughness;
  return p;
}

SurfaceParams SurfaceParams::dielectric(Color baseColor, float roughness) {
  SurfaceParams p;
  p.baseColor = baseColor;
  p.metallic = 0;
  p.roughness = roughness;
  return p;
}

SurfaceParams SurfaceParams::glass() {
  SurfaceParams p;
  p.baseColor = {1, 1, 1, 1};
  p.metallic = 0;
  p.roughness = 0.02f;
  p.transmission = 1;
  p.ior = 1.5f;
  p.thickness = 0.35f;
  // A faint green cast in a thick edge, which is what soda-lime glass
  // does and what tells an eye it is glass rather than a hole.
  p.absorption = {0.55f, 0.12f, 0.35f, 1};
  return p;
}

const Texture* map(const Material& m, std::string_view slot) {
  const auto* texture = dynamic_cast<const Texture*>(m.leaf(slot));
  if (!texture) return nullptr;
  const auto* producer = texture->source().as<ProducerSource>();
  const bool fill = producer && producer->key().rfind(kFillPrefix, 0) == 0;
  return fill ? nullptr : texture;
}

Material surface(const textures::TextureMaps& maps, SurfaceParams base) {
  using textures::Role;
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
  const SurfaceParams stock;
  if (metallic && base.metallic == stock.metallic) base.metallic = 1;
  if (roughness && base.roughness == stock.roughness) base.roughness = 1;
  if (has(Role::Emissive)) {
    if (base.emissiveStrength <= 0) base.emissiveStrength = 1;
    if (base.emissive == stock.emissive) base.emissive = {1, 1, 1, 1};
  }
  base.normalDirectX = maps.normalDirectX ? 1.0f : 0.0f;

  Material m = surface(base);
  const auto place = [&](std::string_view slot, const Texture* t) {
    if (t) m.child(slot, *t);
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

}  // namespace sigil::material::kit
