/** @file
 * The two surface bodies in each language a renderer speaks, the neutral
 * maps that fill an undressed slot, and the builders — including the one
 * that reads a decoded texture set into slots and channels.
 */

#include "sigilmaterial/kit/Surface.h"

#include "sigilmaterial/kit/Terms.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkSurface.h>

#include <string>
#include <string_view>
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
 *  A Slang renderer shades, and the lit body tells it what this surface
 *  IS beyond its colour: how rough, how metallic, and — for glass — how
 *  much light passes through, at what index, through what thickness of
 *  what medium. Those are the surface's standing whether or not a map
 *  varies them, and a renderer with an environment map to sample cannot
 *  shade a mirror without them. A MAP that varies the normal, the
 *  roughness or the metallic across a face says one thing more, and
 *  raises `gSurfacePerPixel`: that is the case a shading evaluated once
 *  per vertex cannot carry, and a surface whose values are one number
 *  over the whole of it keeps the shading it already had. */
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

/** What both lit Slang bodies do, which is everything but the line that
 *  says how the environment reaches the surface. */
constexpr char kSlangSurfaceBody[] = R"(
float4 surface(float2 uv) {
  float4 c = baseColor * baseColorMap.Sample(uv);
  float occ = occlusion(chanS(occlusionMap.Sample(uv), occlusionChannel),
                        occlusionStrength);
  float3 rgb = c.rgb * occ + emission(emissive.rgb, emissiveStrength,
                                      emissiveMap.Sample(uv).rgb);
  float a = cutS(c.a * chanS(opacityMap.Sample(uv), opacityChannel));

  // WHAT THIS SURFACE IS, handed to the renderer that shades. Stated
  // whether or not a map varies it: a mirror with no maps at all still
  // has to reflect, and only the surface knows how rough it is.
  float4 rm = roughnessMap.Sample(uv);
  float4 mm = metallicMap.Sample(uv);
  float rough = clamp(
      roughness * (dressedS(rm) ? chanS(rm, roughnessChannel) : 1.0), 0.0, 1.0);
  gSurfaceRoughness = rough;
  gSurfaceMetal = clamp(
      metallic * (dressedS(mm) ? chanS(mm, metallicChannel) : 1.0), 0.0, 1.0);
  // ROUGHNESS AS A BLINN EXPONENT, for the highlight the emitters make:
  // the mirror end of the range is a narrow highlight and the rough end
  // is a wide one, squared so the smooth half of the range spends most
  // of it. It is a mapping and not a microfacet distribution.
  float smoothness = 1.0 - rough;
  gSurfaceGloss = 2.0 + 254.0 * smoothness * smoothness;
  gSurfaceTransmission = clamp(transmission, 0.0, 1.0);
  gSurfaceIor = max(ior, 1.0);
  gSurfaceThickness = max(thickness, 0.0);
  gSurfaceAbsorption = absorption.rgb;
  gSurfaceReflection = REFLECTION_WEIGHT;

  // THE HALF NO PER-VERTEX SHADING CAN CARRY: a normal that turns
  // under the pixel, or a roughness or a metallic that changes across
  // the face. Where a map varies one, the emitter loop runs again where
  // those values can be seen.
  float4 nm = normalMap.Sample(uv);
  if (dressedS(nm) || dressedS(rm) || dressedS(mm)) {
    if (dressedS(nm)) {
      float3 tn = nm.rgb * 2.0 - 1.0;
      float ty = normalDirectX > 0.5 ? -tn.y : tn.y;
      gSurfaceNormal = float3(tn.x * normalScale, ty * normalScale, tn.z);
    }
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

/** The lit body for @p reflection: the shared text with the one line
 *  that says how the environment enters filled in. A negative weight is
 *  the split sum — the surface's own reflectance and its Fresnel decide
 *  — and a weight at or above zero is that much environment, added. */
std::string slangSurface(Reflection reflection) {
  std::string body = kSlangSurfaceBody;
  const std::string_view mark = "REFLECTION_WEIGHT";
  const size_t at = body.find(mark);
  body.replace(at, mark.size(),
               reflection == Reflection::SplitSum ? "-1.0"
                                                  : "max(reflectionWeight, 0.0)");
  return body;
}

Recipe define(std::string name, const char* body,
              const std::string& slangBody) {
  return Recipe::of<SurfaceParams>(std::move(name))
      .child(std::string(kBaseColorSlot))
      .child(std::string(kNormalSlot))
      .child(std::string(kRoughnessSlot))
      .child(std::string(kMetallicSlot))
      .child(std::string(kOcclusionSlot))
      .child(std::string(kEmissiveSlot))
      .child(std::string(kOpacitySlot))
      .body(Target::SkSL, std::string(kPrelude) + body)
      // THE TERMS ARE NOT PREPENDED HERE. A Slang renderer loads them
      // once as the module `Shading` and imports it beside the body, so
      // the renderer's own shading and every material compiled with it
      // call one definition of each term rather than a copy apiece.
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

const std::shared_ptr<const Recipe>& surfaceRecipe(Reflection reflection) {
  static const std::shared_ptr<const Recipe> splitSum =
      std::make_shared<const Recipe>(
          define("surface", kSurface, slangSurface(Reflection::SplitSum)));
  static const std::shared_ptr<const Recipe> additive =
      std::make_shared<const Recipe>(define(
          "surface.additive", kSurface, slangSurface(Reflection::Additive)));
  return reflection == Reflection::SplitSum ? splitSum : additive;
}

const std::shared_ptr<const Recipe>& unlitRecipe() {
  static const std::shared_ptr<const Recipe> recipe =
      std::make_shared<const Recipe>(define("unlit", kUnlit, kSlangUnlit));
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
