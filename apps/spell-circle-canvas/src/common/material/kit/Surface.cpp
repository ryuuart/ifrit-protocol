/** @file
 * The two surface bodies in SkSL, the neutral maps that fill an
 * undressed slot, and the builders — including the one that reads a
 * decoded texture set into slots and channels.
 */

#include "sigilmaterial/kit/Surface.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkSurface.h>

#include <string>
#include <utility>

namespace sigil::material::kit {

namespace {

/** Reading one channel of a map by its uniform index, and the cutout
 *  rule both bodies share. */
constexpr char kPrelude[] = R"(
half chan(half4 c, float which) {
  return which < 0.5 ? c.r : which < 1.5 ? c.g : which < 2.5 ? c.b : c.a;
}

half cut(half a) {
  return alphaCutoff > 0.0 ? (a < half(alphaCutoff) ? half(0.0) : half(1.0))
                           : a;
}
)";

/** Lit: albedo darkened by the occlusion map at its strength, plus the
 *  emission. The result is clamped against its own alpha, which is what
 *  a premultiplied colour means. */
constexpr char kSurface[] = R"(
half4 main(float2 xy) {
  half4 c = half4(baseColor) * baseColorMap.eval(xy);
  half occ = mix(half(1.0), chan(occlusionMap.eval(xy), occlusionChannel),
                 half(clamp(occlusionStrength, 0.0, 1.0)));
  half3 rgb = c.rgb * occ + half3(emissive.rgb) * half(emissiveStrength) *
                                emissiveMap.eval(xy).rgb;
  half a = cut(c.a * chan(opacityMap.eval(xy), opacityChannel));
  return half4(min(rgb, half3(a)), a);
}
)";

/** Self-lit: the albedo IS the answer. */
constexpr char kUnlit[] = R"(
half4 main(float2 xy) {
  half4 c = half4(baseColor) * baseColorMap.eval(xy);
  half a = cut(c.a * chan(opacityMap.eval(xy), opacityChannel));
  return half4(min(c.rgb, half3(a)), a);
}
)";

Recipe define(std::string name, const char* body) {
  return Recipe::of<SurfaceParams>(std::move(name))
      .child(std::string(kBaseColorSlot))
      .child(std::string(kNormalSlot))
      .child(std::string(kRoughnessSlot))
      .child(std::string(kMetallicSlot))
      .child(std::string(kOcclusionSlot))
      .child(std::string(kEmissiveSlot))
      .child(std::string(kOpacitySlot))
      .body(Target::SkSL, std::string(kPrelude) + body);
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

const std::shared_ptr<const Recipe>& surfaceRecipe() {
  static const std::shared_ptr<const Recipe> recipe =
      std::make_shared<const Recipe>(define("surface", kSurface));
  return recipe;
}

const std::shared_ptr<const Recipe>& unlitRecipe() {
  static const std::shared_ptr<const Recipe> recipe =
      std::make_shared<const Recipe>(define("unlit", kUnlit));
  return recipe;
}

Material surface(const SurfaceParams& params) {
  return dress(Material(surfaceRecipe(), params));
}

Material unlit(const SurfaceParams& params) {
  return dress(Material(unlitRecipe(), params));
}

bool isSurface(const Material& m) {
  return m.recipePtr() == surfaceRecipe() || m.recipePtr() == unlitRecipe();
}

bool isUnlit(const Material& m) { return m.recipePtr() == unlitRecipe(); }

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
