/** @file
 * A lit surface in 2D: the colour stack lowered once as it is painted
 * flat, and the lighting pass over it — a runtime effect whose children
 * are that stack and the surface's maps, and whose uniforms are the
 * surface's numbers and the lighting's, the moving ones bound so the
 * pass re-resolves on its own while its children stay put.
 */

#include "sigilmaterial/skia/Lit.h"

#include <include/core/SkString.h>
#include <include/core/SkTypes.h>  // SkDebugf
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilshaders/MaterialSkia.h>

#include <string>
#include <utility>
#include <variant>

namespace sigil::material::skia {

namespace {

const sk_sp<SkRuntimeEffect>& litEffect() {
  static const sk_sp<SkRuntimeEffect> effect = [] {
    auto [built, error] = SkRuntimeEffect::MakeForShader(
        SkString(std::string(shaderSource("LitSurface.sksl")).c_str()));
    if (!built) SkDebugf("sigilmaterial lit surface shader: %s\n", error.c_str());
    return built;
  }();
  return effect;
}

/** A channel into the pass: its map in the slot and a factor of one, or
 *  its number with no map. */
void channel(Paint& pass, const Channel& value, const char* map,
             const char* hasMap, const char* number) {
  if (const auto* stated = std::get_if<Material>(&value)) {
    pass.slot(map, paint(*stated));
    pass.set(hasMap, 1.0f);
    pass.set(number, 1.0f);
  } else {
    pass.set(hasMap, 0.0f);
    pass.set(number, std::get<float>(value));
  }
}

}  // namespace

bool isLit(const Material& material) {
  const SurfaceOptions* surface = material.surface();
  return surface != nullptr && !surface->unlit;
}

Lighting lightingFor(const Material& material, const Lighting& inForce) {
  if (!isLit(material)) return {};
  const SurfaceOptions& surface = *material.surface();
  return surface.lighting ? *surface.lighting : inForce;
}

Paint lit(const Material& material, const Lighting& lighting) {
  Paint colours = paint(material);
  if (!isLit(material) || !lighting || !litEffect()) return colours;
  const SurfaceOptions& surface = *material.surface();

  Paint pass = sksl(litEffect());
  pass.slot("uColor", std::move(colours));
  pass.set("uHasNormal", surface.normal ? 1.0f : 0.0f);
  if (surface.normal) pass.slot("uNormal", paint(*surface.normal));
  pass.set("uNormalScale", surface.normalScale);
  pass.set("uNormalDirectX", surface.normalDirectX ? 1.0f : 0.0f);
  channel(pass, surface.roughness, "uRoughnessMap", "uHasRoughnessMap",
          "uRoughness");
  channel(pass, surface.metallic, "uMetallicMap", "uHasMetallicMap",
          "uMetallic");
  channel(pass, surface.occlusion, "uOcclusionMap", "uHasOcclusionMap",
          "uOcclusion");
  pass.set("uHasEmissionMap", surface.emissionMap ? 1.0f : 0.0f);
  if (surface.emissionMap) pass.slot("uEmissionMap", paint(*surface.emissionMap));
  pass.set("uEmission", surface.emission);
  pass.set("uEmissionStrength", surface.emissionStrength);
  pass.set("uReflection", surface.reflectionWeight);

  pass.set("uHasLight", lighting.light ? 1.0f : 0.0f);
  const Light light = lighting.light.value_or(Light{});
  pass.bind("uDirection", light.direction);
  pass.bind("uElevation", light.elevation);
  pass.bind("uIntensity", light.intensity);
  pass.set("uLightColor", light.color);
  pass.set("uAmbient", lighting.light ? light.ambient : 0.0f);

  pass.set("uHasEnvironment", lighting.environment ? 1.0f : 0.0f);
  if (lighting.environment && lighting.environment->image) {
    const Environment& around = *lighting.environment;
    pass.slot("uEnvironment", paint(*around.image));
    pass.bind("uEnvironmentRotation", around.options.rotation);
    pass.set("uEnvironmentIntensity", around.options.intensity);
    pass.set("uEnvironmentSize",
             std::array<float, 2>{around.options.size.x, around.options.size.y});
    // Where the environment alone lights the surface, its ambient share is
    // the whole of the diffuse light.
    if (!lighting.light) pass.set("uAmbient", 1.0f);
  } else {
    pass.set("uHasEnvironment", 0.0f);
    pass.set("uEnvironmentRotation", 0.0f);
    pass.set("uEnvironmentIntensity", 0.0f);
    pass.set("uEnvironmentSize", std::array<float, 2>{1.0f, 1.0f});
  }
  return pass;
}

}  // namespace sigil::material::skia
