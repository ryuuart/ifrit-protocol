/** @file
 * The two surface bodies in each language a renderer speaks, the neutral
 * maps that fill an undressed slot, and the builders — including the one
 * that reads a decoded texture set into slots and channels.
 */

#include "sigilmaterial/surface/Surface.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkSurface.h>
#include <sigilmaterial/advanced/Program.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilshaders/MaterialSurface.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace sigil::material::surface {

namespace {

/** The text between `// <name> {` and `// } <name>`, its two marking
 *  lines included, taken out of @p body: a lobe the program does not
 *  carry is not in its body at all, so its maps are never declared to the
 *  program and nothing downstream compiles its term. */
void strike(std::string& body, std::string_view name) {
  const std::string open = "// " + std::string(name) + " {";
  const std::string close = "// } " + std::string(name);
  for (size_t at = body.find(open); at != std::string::npos;
       at = body.find(open, at)) {
    const size_t start = body.rfind('\n', at);
    const size_t end = body.find(close, at);
    if (end == std::string::npos) {
      reportOnce("surface.lobe:" + std::string(name),
                 "the Slang surface body opens the " + std::string(name) +
                     " lobe and never closes it; the body keeps it");
      return;
    }
    const size_t after = body.find('\n', end);
    const size_t from = start == std::string::npos ? 0 : start;
    body.erase(from, (after == std::string::npos ? body.size() : after) - from);
  }
}

/** The lit body for @p reflection carrying @p carried: the shared text
 *  with the one line that says how the environment enters filled in and
 *  every lobe it does not carry struck out. A negative weight is the split
 *  sum — the surface's own reflectance and its Fresnel decide — and a
 *  weight at or above zero is that much environment, added. */
std::string slangSurface(Reflection reflection, Lobes carried) {
  std::string body(shaderSource("Surface.slang"));
  if (!carried.sheen) strike(body, "sheen");
  if (!carried.anisotropy) strike(body, "anisotropy");
  if (!carried.iridescence) strike(body, "iridescence");
  if (!carried.specular) strike(body, "specular");
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
      .slot(std::string(kClearcoatSlot))
      .slot(std::string(kClearcoatRoughnessSlot))
      .slot(std::string(kClearcoatNormalSlot))
      .slot(std::string(kSheenColorSlot))
      .slot(std::string(kSheenRoughnessSlot))
      .slot(std::string(kAnisotropySlot))
      .slot(std::string(kIridescenceSlot))
      .slot(std::string(kIridescenceThicknessSlot))
      .slot(std::string(kSpecularSlot))
      .slot(std::string(kSpecularColorSlot))
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
                       sk_sp<SkSurface> s =
                           SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1, 1));
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
  m.slot(kClearcoatSlot, white);
  m.slot(kClearcoatRoughnessSlot, white);
  m.slot(kClearcoatNormalSlot,
         flat("normal", material::Color{0.5f, 0.5f, 1.0f, 1.0f}));
  for (std::string_view slot :
       {kSheenColorSlot, kSheenRoughnessSlot, kAnisotropySlot, kIridescenceSlot,
        kIridescenceThicknessSlot, kSpecularSlot, kSpecularColorSlot})
    m.slot(slot, white);
  return m;
}

}  // namespace

Material blendNormals(Material base, Material detail,
                      NormalBlendOptions options) {
  struct Parameters {
    float baseGreen, detailGreen, outputGreen;
  };
  // The surface program takes one map. Reorient the fine relief into
  // the coarse normal's tangent frame before supplying that channel.
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<Parameters>("surface.blendNormals")
          .slot("baseNormal")
          .slot("detailNormal")
          .body(Target::SkSL, std::string(shaderSource("BlendNormals.sksl")))
          .body(Target::Slang,
                std::string(shaderSource("BlendNormals.slang"))));
  Material result(recipe, Parameters{options.baseDirectX ? -1.0f : 1.0f,
                                     options.detailDirectX ? -1.0f : 1.0f,
                                     options.outputDirectX ? -1.0f : 1.0f});
  result.slot("baseNormal", std::move(base));
  result.slot("detailNormal", std::move(detail));
  return result;
}

Material normalFromHeight(Material height, HeightNormalOptions options) {
  if (!std::isfinite(options.depth) || options.depth == 0 ||
      !std::isfinite(options.step) || options.step <= 0 ||
      !std::isfinite(options.depth / options.step))
    return Color{0.5f, 0.5f, 1, 1};
  struct Parameters {
    float depth, sampleDistance, green;
  };
  // Skia's contour bevel accepts a path. A sampled height can instead come
  // from an image, a composed material or a consumer's rendered element tree.
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<Parameters>("surface.normalFromHeight")
          .frame(FrameInput::LocalToSample)
          .slot("height")
          .body(Target::SkSL,
                std::string(shaderSource("NormalFromHeight.sksl"))));
  Material result(recipe, Parameters{options.depth, options.step,
                                     options.directX ? -1.0f : 1.0f});
  result.slot("height", std::move(height));
  return result;
}

namespace {

/** Every lit recipe made so far, one per model and set of lobes, made on
 *  first asking and kept for the process. */
struct SurfaceRecipes {
  std::mutex mutex;
  std::map<int, std::shared_ptr<const Recipe>> made;
};

SurfaceRecipes& surfaceRecipes() {
  static SurfaceRecipes recipes;
  return recipes;
}

int lobeBits(Lobes carried) {
  return (carried.sheen ? 1 : 0) | (carried.anisotropy ? 2 : 0) |
         (carried.iridescence ? 4 : 0) | (carried.specular ? 8 : 0);
}

Lobes lobesOfBits(int bits) {
  return {(bits & 1) != 0, (bits & 2) != 0, (bits & 4) != 0, (bits & 8) != 0};
}

/** The lit recipe a material is an instance of, by its key — the model
 *  times sixteen plus the lobe bits — or none. */
std::optional<int> surfaceKey(const Material& material) {
  SurfaceRecipes& recipes = surfaceRecipes();
  const std::lock_guard lock(recipes.mutex);
  for (const auto& [key, recipe] : recipes.made)
    if (material.recipePointer() == recipe) return key;
  return std::nullopt;
}

}  // namespace

const std::shared_ptr<const Recipe>& surfaceRecipe(Reflection reflection,
                                                   Lobes carried) {
  const int key =
      (reflection == Reflection::SplitSum ? 0 : 16) | lobeBits(carried);
  SurfaceRecipes& recipes = surfaceRecipes();
  const std::lock_guard lock(recipes.mutex);
  std::shared_ptr<const Recipe>& recipe = recipes.made[key];
  if (!recipe) {
    // A plain recipe is named for its model alone; one carrying a lobe
    // says which.
    std::string name =
        reflection == Reflection::SplitSum ? "surface" : "surface.additive";
    if (carried.sheen) name += ".sheen";
    if (carried.anisotropy) name += ".anisotropy";
    if (carried.iridescence) name += ".iridescence";
    if (carried.specular) name += ".specular";
    recipe = std::make_shared<const Recipe>(define(
        std::move(name), "Surface.sksl", slangSurface(reflection, carried)));
  }
  return recipe;
}

Lobes lobes(const SurfaceParameters& parameters) {
  Lobes out;
  out.sheen = parameters.sheenColor.r != 0 || parameters.sheenColor.g != 0 ||
              parameters.sheenColor.b != 0;
  out.anisotropy = parameters.anisotropy != 0;
  out.iridescence = parameters.iridescence != 0;
  const SurfaceParameters stock;
  out.specular =
      parameters.specular != stock.specular ||
      parameters.specularColorFactor.r != stock.specularColorFactor.r ||
      parameters.specularColorFactor.g != stock.specularColorFactor.g ||
      parameters.specularColorFactor.b != stock.specularColorFactor.b;
  return out;
}

Lobes lobes(const Material& material) {
  const std::optional<int> key = surfaceKey(material);
  return key ? lobesOfBits(*key & 15) : Lobes{};
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
  return program(parameters, reflection, {});
}

Material program(const SurfaceParameters& parameters, Reflection reflection,
                 Lobes carried) {
  const Lobes stated = lobes(parameters);
  carried.sheen |= stated.sheen;
  carried.anisotropy |= stated.anisotropy;
  carried.iridescence |= stated.iridescence;
  carried.specular |= stated.specular;
  return dress(Material(surfaceRecipe(reflection, carried), parameters));
}

Material unlit(const SurfaceParameters& parameters) {
  return dress(Material(unlitRecipe(), parameters));
}

bool isSurface(const Material& material) {
  return material.recipePointer() == unlitRecipe() ||
         surfaceKey(material).has_value();
}

Compositing compositing(const Material& material) {
  Compositing out;
  const Schema& fields = material.recipe().parameters();
  const auto number = [&](std::string_view name, float otherwise) {
    const Field* field = fields.find(name);
    return field && field->floats == 1 ? material.get<float>(name) : otherwise;
  };
  const float blend = number("blend", 0.0f);
  if (blend >= 0.0f && blend <= (float)BlendMode::Xor)
    out.blend = (BlendMode)(int)std::lround(blend);
  const float depthWrite = number("depthWrite", -1.0f);
  if (depthWrite >= 0.0f) out.depthWrite = depthWrite > 0.5f;
  out.depthFade = std::max(number("depthFade", 0.0f), 0.0f);
  return out;
}

bool isUnlit(const Material& material) {
  return material.recipePointer() == unlitRecipe();
}

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
  // A clear coat image carries the weight in red and the roughness in
  // green, so it fills both of the coat's slots.
  const Texture* coat = maps.map(Role::Clearcoat);
  if (coat) {
    if (base.clearcoat == stock.clearcoat) base.clearcoat = 1;
    if (base.clearcoatRoughness == stock.clearcoatRoughness)
      base.clearcoatRoughness = 1;
    base.clearcoatChannel = 0;
    base.clearcoatRoughnessChannel = 1;
  }
  // THE OPTIONAL LOBES a set carries, each starting its factors at one as
  // the maps above do, and carried by the program because a map states
  // them. The sheen's roughness and the specular strength share one image
  // in red and green; a thin film's thickness runs over the whole range
  // the image can say unless the base moved it.
  Lobes carried;
  const Texture* sheen = maps.map(Role::Sheen);
  const Texture* sheenSpecular = maps.map(Role::SheenSpecular);
  const Texture* specularColor = maps.map(Role::SpecularColor);
  const Texture* anisotropy = maps.map(Role::Anisotropy);
  const Texture* iridescence = maps.map(Role::Iridescence);
  if (sheen) {
    carried.sheen = true;
    if (base.sheenColor == stock.sheenColor) base.sheenColor = {1, 1, 1, 1};
    if (sheenSpecular) {
      if (base.sheenRoughness == stock.sheenRoughness) base.sheenRoughness = 1;
      base.sheenRoughnessChannel = 0;
    }
  }
  if (specularColor) {
    carried.specular = true;
    if (sheenSpecular) base.specularChannel = 1;
  }
  if (anisotropy) {
    carried.anisotropy = true;
    if (base.anisotropy == stock.anisotropy) base.anisotropy = 1;
  }
  if (iridescence) {
    carried.iridescence = true;
    if (base.iridescence == stock.iridescence) base.iridescence = 1;
    base.iridescenceChannel = 0;
    base.iridescenceThicknessChannel = 1;
    if (base.iridescenceThickness == stock.iridescenceThickness &&
        base.iridescenceThicknessMinimum == stock.iridescenceThicknessMinimum) {
      base.iridescenceThicknessMinimum = 0;
      base.iridescenceThickness = texture::kIridescenceThicknessRange;
    }
  }

  Material m = program(base, Reflection::SplitSum, carried);
  const auto place = [&](std::string_view slot, const Texture* t) {
    if (t) m.slot(slot, *t);
  };
  // A set's maps other than its colours are DATA, and are stated so where
  // the set did not say otherwise: a normal or a packed map read as colour
  // would have its alpha multiplied into it.
  const auto data = [&](std::string_view slot, const Texture* t) {
    if (!t) return;
    Texture stated = *t;
    if (!stated.colorSpace()) stated.colorSpace(media::ColorSpace::Data);
    m.slot(slot, std::move(stated));
  };
  place(kBaseColorSlot, maps.map(Role::BaseColor));
  data(kNormalSlot, maps.map(Role::Normal));
  data(kRoughnessSlot, roughness);
  data(kMetallicSlot, metallic);
  data(kOcclusionSlot, occlusion);
  place(kEmissiveSlot, maps.map(Role::Emissive));
  data(kOpacitySlot, maps.map(Role::Opacity));
  data(kClearcoatSlot, coat);
  data(kClearcoatRoughnessSlot, coat);
  if (sheen) {
    place(kSheenColorSlot, sheen);
    data(kSheenRoughnessSlot, sheenSpecular);
  }
  if (specularColor) {
    place(kSpecularColorSlot, specularColor);
    data(kSpecularSlot, sheenSpecular);
  }
  data(kAnisotropySlot, anisotropy);
  data(kIridescenceSlot, iridescence);
  data(kIridescenceThicknessSlot, iridescence);
  return m;
}

}  // namespace sigil::material::surface
