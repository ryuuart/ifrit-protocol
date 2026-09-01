/** @file
 * The two surface bodies in each language a renderer speaks, the neutral
 * maps that fill an undressed slot, and the builders — including the one
 * that reads a decoded texture set into slots and channels.
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

/** The same two readings in Slang, for a renderer that compiles it.
 *
 *  A Slang body answers ONE function — `float4 surface(float2 uv)` —
 *  returning the surface's own colour with straight alpha. Where the
 *  SkSL bodies evaluate a child slot as a shader, these sample it as a
 *  texture; the params, the slots and the channel conventions are the
 *  same ABI, because they are the same recipe.
 *
 *  A Slang renderer shades, and the lit body says three things about its
 *  surface that a colour cannot carry — `gSurfaceNormal` in tangent
 *  space, `gSurfaceGloss` as a Blinn exponent, `gSurfaceMetal` — and
 *  raises `gSurfacePerPixel` when it has. The renderer declares those
 *  four and reads them after the body has run. It writes them only where
 *  a MAP varies them across the surface: that is the case a shading
 *  evaluated once per vertex cannot carry, and a surface whose roughness
 *  and metallic are one number over the whole of it keeps the shading it
 *  already had. */
constexpr char kSlangPrelude[] = R"(
float chanS(float4 c, float which) {
  return which < 0.5 ? c.r : which < 1.5 ? c.g : which < 2.5 ? c.b : c.a;
}

// Written out rather than taken from the language's library: an
// intrinsic whose two targets are two different pieces of code is where
// one source stops producing one answer.
float mixS(float a, float b, float t) { return a + (b - a) * t; }

float cutS(float a) {
  return alphaCutoff > 0.0 ? (a < alphaCutoff ? 0.0 : 1.0) : a;
}

// IS THIS SLOT DRESSED? An undressed one reads one white texel — the
// neutral for every map a scalar multiplies, and the one value a
// tangent-space normal cannot mean, since such a normal's x and y are
// centred on a half and only its z reaches one. So white IS "no map
// here", exactly and with no threshold to pick.
bool dressedS(float4 c) { return c.r < 1.0 || c.g < 1.0 || c.b < 1.0; }
)";

constexpr char kSlangSurface[] = R"(
float4 surface(float2 uv) {
  float4 c = baseColor * baseColorMap.Sample(uv);
  float occ = mixS(1.0, chanS(occlusionMap.Sample(uv), occlusionChannel),
                   clamp(occlusionStrength, 0.0, 1.0));
  float3 rgb = c.rgb * occ + emissive.rgb * emissiveStrength *
                                 emissiveMap.Sample(uv).rgb;
  float a = cutS(c.a * chanS(opacityMap.Sample(uv), opacityChannel));

  // THE HALF OF THE MODEL A COLOUR CANNOT CARRY, handed to the renderer
  // that shades. Only a map reaches it: a surface whose roughness and
  // metallic are one number over the whole of it is already what a
  // per-vertex shading says it is.
  float4 nm = normalMap.Sample(uv);
  float4 rm = roughnessMap.Sample(uv);
  float4 mm = metallicMap.Sample(uv);
  if (dressedS(nm) || dressedS(rm) || dressedS(mm)) {
    if (dressedS(nm)) {
      float3 tn = nm.rgb * 2.0 - 1.0;
      float ty = normalDirectX > 0.5 ? -tn.y : tn.y;
      gSurfaceNormal = float3(tn.x * normalScale, ty * normalScale, tn.z);
    }
    float rough = clamp(
        roughness * (dressedS(rm) ? chanS(rm, roughnessChannel) : 1.0),
        0.0, 1.0);
    // ROUGHNESS AS A BLINN EXPONENT: the mirror end of the range is a
    // narrow highlight and the rough end is a wide one, squared so the
    // smooth half of the range spends most of it. It is a mapping and
    // not a microfacet distribution, which this model does not have.
    float smoothness = 1.0 - rough;
    gSurfaceGloss = 2.0 + 254.0 * smoothness * smoothness;
    gSurfaceMetal = clamp(
        metallic * (dressedS(mm) ? chanS(mm, metallicChannel) : 1.0),
        0.0, 1.0);
    gSurfacePerPixel = true;
  }
  return float4(min(rgb, float3(a, a, a)), a);
}
)";

constexpr char kSlangUnlit[] = R"(
float4 surface(float2 uv) {
  float4 c = baseColor * baseColorMap.Sample(uv);
  float a = cutS(c.a * chanS(opacityMap.Sample(uv), opacityChannel));
  return float4(min(c.rgb, float3(a, a, a)), a);
}
)";

Recipe define(std::string name, const char* body, const char* slangBody) {
  return Recipe::of<SurfaceParams>(std::move(name))
      .child(std::string(kBaseColorSlot))
      .child(std::string(kNormalSlot))
      .child(std::string(kRoughnessSlot))
      .child(std::string(kMetallicSlot))
      .child(std::string(kOcclusionSlot))
      .child(std::string(kEmissiveSlot))
      .child(std::string(kOpacitySlot))
      .body(Target::SkSL, std::string(kPrelude) + body)
      .body(Target::Slang, std::string(kSlangPrelude) + slangBody);
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
      std::make_shared<const Recipe>(
          define("surface", kSurface, kSlangSurface));
  return recipe;
}

const std::shared_ptr<const Recipe>& unlitRecipe() {
  static const std::shared_ptr<const Recipe> recipe =
      std::make_shared<const Recipe>(define("unlit", kUnlit, kSlangUnlit));
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
