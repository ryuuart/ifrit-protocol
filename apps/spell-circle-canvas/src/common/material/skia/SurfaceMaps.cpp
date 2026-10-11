/** @file
 * A surface's maps in 2D: the colour stack lowered once as it is painted
 * flat, and the surface's channels beside it, read back unshaded through
 * one runtime effect whose role is a uniform — the maps a renderer
 * bringing its own lights dresses a surface with.
 */

#include "sigilmaterial/skia/SurfaceMaps.h"

#include <include/core/SkString.h>
#include <include/core/SkTypes.h>  // SkDebugf
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilshaders/MaterialSkia.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <variant>

#include "PaintInternal.h"

namespace sigil::material::skia {

namespace {

/** The one program every surface map is read through; its role is a
 *  uniform, so a surface's six maps share one compiled effect. */
const sk_sp<SkRuntimeEffect>& surfaceMapEffect() {
  static const sk_sp<SkRuntimeEffect> effect = [] {
    auto [built, error] = SkRuntimeEffect::MakeForShader(
        SkString(std::string(shaderSource("SurfaceMap.sksl")).c_str()));
    if (!built)
      SkDebugf("sigilmaterial surface map shader: %s\n", error.c_str());
    return built;
  }();
  return effect;
}

/** The position of @p role among the maps a surface map pass answers, or
 *  none for a role it does not. */
std::optional<size_t> surfaceMapIndex(texture::Role role) {
  switch (role) {
    case texture::Role::BaseColor:
      return 0;
    case texture::Role::Normal:
      return 1;
    case texture::Role::Roughness:
      return 2;
    case texture::Role::Metallic:
      return 3;
    case texture::Role::Occlusion:
      return 4;
    case texture::Role::Emissive:
      return 5;
    case texture::Role::Packed:
      return 6;
    case texture::Role::Clearcoat:
      return 7;
    case texture::Role::Sheen:
      return 8;
    case texture::Role::SheenSpecular:
      return 9;
    case texture::Role::SpecularColor:
      return 10;
    case texture::Role::Anisotropy:
      return 11;
    case texture::Role::Iridescence:
      return 12;
    case texture::Role::ClearcoatNormal:
      return 13;
    case texture::Role::Transmission:
      return 14;
    case texture::Role::Thickness:
      return 15;
    case texture::Role::Distortion:
      return 16;
    default:
      return std::nullopt;
  }
}

/** The number of roles a surface map pass answers. */
constexpr size_t kSurfaceMapRoles = 17;

/** The listener standing on this thread, innermost first. */
thread_local LobeListener* tCurrentListener = nullptr;

/** A colour channel into the pass: its map in the slot and white, or its
 *  colour with no map. */
void colorChannel(Paint& pass, const ColorInput& value,
                  const std::optional<Paint>& lowered, const char* map,
                  const char* hasMap, const char* colour) {
  if (lowered) {
    PaintAccess::storeSlot(pass, map, *lowered);
    PaintAccess::storeUniform(pass, hasMap, 1.0f);
    PaintAccess::storeUniform(pass, colour, Color{1, 1, 1, 1});
  } else {
    PaintAccess::storeUniform(pass, hasMap, 0.0f);
    PaintAccess::storeUniform(pass, colour, std::get<Color>(value));
  }
}

/** A channel into the pass: its map in the slot and a factor of one, or
 *  its number with no map. */
void channel(Paint& pass, const Channel& value,
             const std::optional<Paint>& lowered, const char* map,
             const char* hasMap, const char* number) {
  if (lowered) {
    PaintAccess::storeSlot(pass, map, *lowered);
    PaintAccess::storeUniform(pass, hasMap, 1.0f);
    PaintAccess::storeUniform(pass, number, 1.0f);
  } else {
    PaintAccess::storeUniform(pass, hasMap, 0.0f);
    PaintAccess::storeUniform(pass, number, std::get<float>(value));
  }
}

/** How far @p surface, stating @p stated, reaches past a map's white. */
MapReach reachOf(const SurfaceOptions& surface, const Lobes& stated) {
  MapReach reach;
  reach.emission = std::max({surface.emission.r, surface.emission.g,
                             surface.emission.b, 0.0f}) *
                   std::max(surface.emissionStrength, 0.0f);
  if (stated.transmission) {
    const float* thickness = std::get_if<float>(&surface.thickness);
    reach.thickness = thickness ? std::max(*thickness, 0.0f) : 1.0f;
    reach.volume =
        texture::Volume{.ior = surface.ior,
                        .attenuationColor = surface.attenuationColor,
                        .attenuationDistance = surface.attenuationDistance,
                        .dispersion = surface.dispersion};
  }
  if (stated.distortion)
    reach.distortion = std::abs(surface.distortionStrength);
  return reach;
}

}  // namespace

struct SurfaceMaps::Inputs {
  Material material;
  Paint colours;
  std::optional<Paint> normal, roughness, metallic, occlusion, emission,
      clearcoat, clearcoatRoughness, sheenColor, sheenRoughness, specular,
      specularColor, anisotropyDirection, iridescence, iridescenceThickness,
      clearcoatNormal, transmission, thickness, distortion;

  static Paint prepare(const Material& material) {
    Paint paint = PaintAccess::prepareMaterial(material);
    PaintAccess::retainSolidShader(paint);
    return paint;
  }

  /** The colour stack placed in the frame every channel shares: a
   *  material anchored to the root frame anchors the whole map, so its
   *  colours are lowered in that frame rather than anchored twice. */
  static Paint prepareColours(const Material& material) {
    if (!material.worldSpace()) return prepare(material);
    Material local = material;
    local.worldSpace(false);
    return prepare(local);
  }

  explicit Inputs(const Material& from)
      : material(from), colours(prepareColours(material)) {
    if (!isLit(material)) return;
    const SurfaceOptions& surface = *material.surface();
    const auto lower = [](const Channel& value) -> std::optional<Paint> {
      if (const auto* map = std::get_if<Material>(&value)) return prepare(*map);
      return std::nullopt;
    };
    if (surface.normal) normal = prepare(*surface.normal);
    roughness = lower(surface.roughness);
    metallic = lower(surface.metallic);
    occlusion = lower(surface.occlusion);
    if (surface.emissionMap) emission = prepare(*surface.emissionMap);
    clearcoat = lower(surface.clearcoat);
    clearcoatRoughness = lower(surface.clearcoatRoughness);
    if (surface.clearcoatNormal)
      clearcoatNormal = prepare(*surface.clearcoatNormal);
    const auto lowerColour =
        [](const ColorInput& value) -> std::optional<Paint> {
      if (const auto* map = std::get_if<Material>(&value)) return prepare(*map);
      return std::nullopt;
    };
    sheenColor = lowerColour(surface.sheenColor);
    sheenRoughness = lower(surface.sheenRoughness);
    specular = lower(surface.specular);
    specularColor = lowerColour(surface.specularColor);
    if (surface.anisotropyDirection)
      anisotropyDirection = prepare(*surface.anisotropyDirection);
    iridescence = lower(surface.iridescence);
    if (surface.iridescenceThicknessMap)
      iridescenceThickness = prepare(*surface.iridescenceThicknessMap);
    transmission = lower(surface.transmission);
    thickness = lower(surface.thickness);
    if (surface.distortion) distortion = prepare(*surface.distortion);
    // A page painted as a surface's maps learns from here which of the
    // optional lobes its materials state, and whether one states a normal
    // of the coat's own.
    const Lobes stated = lobes(surface);
    LobeListener::hear(stated);
    if (clearcoatNormal) LobeListener::hearClearcoatNormal();
    LobeListener::hearReach(reachOf(surface, stated));
  }
};

LobeListener::LobeListener() : m_outer(tCurrentListener) {
  tCurrentListener = this;
}

LobeListener::~LobeListener() { tCurrentListener = m_outer; }

void LobeListener::hear(const Lobes& lobes) {
  for (LobeListener* listener = tCurrentListener; listener;
       listener = listener->m_outer) {
    listener->m_heard.sheen |= lobes.sheen;
    listener->m_heard.anisotropy |= lobes.anisotropy;
    listener->m_heard.iridescence |= lobes.iridescence;
    listener->m_heard.specular |= lobes.specular;
    listener->m_heard.transmission |= lobes.transmission;
    listener->m_heard.distortion |= lobes.distortion;
  }
}

void LobeListener::hearReach(const MapReach& reach) {
  for (LobeListener* listener = tCurrentListener; listener;
       listener = listener->m_outer) {
    MapReach& heard = listener->m_reach;
    heard.emission = std::max(heard.emission, reach.emission);
    heard.thickness = std::max(heard.thickness, reach.thickness);
    heard.distortion = std::max(heard.distortion, reach.distortion);
    if (!reach.volume) continue;
    if (!heard.volume)
      heard.volume = reach.volume;
    else if (*heard.volume != *reach.volume)
      heard.volumesDiffer = true;
  }
}

void LobeListener::hearClearcoatNormal() {
  for (LobeListener* listener = tCurrentListener; listener;
       listener = listener->m_outer)
    listener->m_heardClearcoatNormal = true;
}

struct SurfaceMaps::MapCache {
  std::mutex mutex;
  /** One unshaded pass per surface map role, made on first request. */
  std::array<std::optional<Paint>, kSurfaceMapRoles> maps;
};

SurfaceMaps::SurfaceMaps(const Material& material)
    : m_inputs(std::make_shared<const Inputs>(material)),
      m_maps(std::make_shared<MapCache>()) {}

bool isLit(const Material& material) {
  const SurfaceOptions* surface = material.surface();
  return surface != nullptr && !surface->unlit;
}

bool usesWorldSpace(const Material& material) {
  if (detail::paintUsesWorldSpace(material)) return true;
  if (const Filter* effects = material.effects();
      effects && effects->usesWorldSpace())
    return true;
  const SurfaceOptions* surface = material.surface();
  if (!surface || surface->unlit) return false;
  for (const Channel* channel :
       {&surface->metallic, &surface->roughness, &surface->occlusion,
        &surface->clearcoat, &surface->clearcoatRoughness,
        &surface->sheenRoughness, &surface->specular, &surface->iridescence,
        &surface->transmission, &surface->thickness})
    if (const auto* map = std::get_if<Material>(channel);
        map && detail::paintUsesWorldSpace(*map))
      return true;
  for (const ColorInput* channel :
       {&surface->sheenColor, &surface->specularColor})
    if (const auto* map = std::get_if<Material>(channel);
        map && detail::paintUsesWorldSpace(*map))
      return true;
  for (const std::optional<Material>* map :
       {&surface->anisotropyDirection, &surface->iridescenceThicknessMap,
        &surface->distortion})
    if (*map && detail::paintUsesWorldSpace(**map)) return true;
  return (surface->normal && detail::paintUsesWorldSpace(*surface->normal)) ||
         (surface->emissionMap &&
          detail::paintUsesWorldSpace(*surface->emissionMap));
}

sk_sp<SkShader> SurfaceMaps::mapShader(texture::Role role,
                                       const FrameData& nodeFrame,
                                       const glm::mat3& paintToLocal) const {
  const Paint pass = mapPass(role);
  if (pass.isNone()) return nullptr;
  const SkMatrix mapping = toSkMatrix(paintToLocal);
  SkMatrix inverse;
  if (!mapping.isFinite() || !mapping.invert(&inverse)) return nullptr;
  FrameData inputFrame = nodeFrame;
  inputFrame.resolution = {1.0f, 1.0f};
  inputFrame.world = nodeFrame.world * paintToLocal;
  // A node offset steps the node's sampling coordinates by
  // `localToSample`, and those reach the unit-mapped input through the
  // inverse mapping, so the input's differential is the two composed.
  inputFrame.localToSample = toMatrix(inverse) * nodeFrame.localToSample;
  // A constant differential describes affine sampling only. A projective
  // mapping keeps its colour and encoded normals, with flat height relief.
  if (mapping.hasPerspective() || !inverse.isFinite())
    inputFrame.localToSample = glm::mat3{0};
  inputFrame.localToSample[2] = {0, 0, 1};
  const PaintFrame inputs = paintFrameOf(inputFrame);
  const auto mapped = [&](const Paint& input) {
    sk_sp<SkShader> result = detail::childShader(input, &inputs);
    if (!result || input.isSolid()) return result;
    return result->makeWithLocalMatrix(mapping);
  };
  std::array<PaintAccess::ChildOverride, 20> children;
  size_t count = 0;
  const auto input = [&](const char* name, const Paint& lowered) {
    children[count++] = {name, mapped(lowered)};
  };
  input("uColor", m_inputs->colours);
  if (m_inputs->normal) input("uNormal", *m_inputs->normal);
  if (m_inputs->roughness) input("uRoughnessMap", *m_inputs->roughness);
  if (m_inputs->metallic) input("uMetallicMap", *m_inputs->metallic);
  if (m_inputs->occlusion) input("uOcclusionMap", *m_inputs->occlusion);
  if (m_inputs->emission) input("uEmissionMap", *m_inputs->emission);
  if (m_inputs->clearcoat) input("uClearcoatMap", *m_inputs->clearcoat);
  if (m_inputs->clearcoatRoughness)
    input("uClearcoatRoughnessMap", *m_inputs->clearcoatRoughness);
  if (m_inputs->sheenColor) input("uSheenColorMap", *m_inputs->sheenColor);
  if (m_inputs->sheenRoughness)
    input("uSheenRoughnessMap", *m_inputs->sheenRoughness);
  if (m_inputs->specular) input("uSpecularMap", *m_inputs->specular);
  if (m_inputs->specularColor)
    input("uSpecularColorMap", *m_inputs->specularColor);
  if (m_inputs->anisotropyDirection)
    input("uAnisotropyMap", *m_inputs->anisotropyDirection);
  if (m_inputs->iridescence) input("uIridescenceMap", *m_inputs->iridescence);
  if (m_inputs->iridescenceThickness)
    input("uIridescenceThicknessMap", *m_inputs->iridescenceThickness);
  if (m_inputs->clearcoatNormal)
    input("uClearcoatNormal", *m_inputs->clearcoatNormal);
  if (m_inputs->transmission)
    input("uTransmissionMap", *m_inputs->transmission);
  if (m_inputs->thickness) input("uThicknessMap", *m_inputs->thickness);
  if (m_inputs->distortion) input("uDistortionMap", *m_inputs->distortion);
  const PaintFrame frame = paintFrameOf(nodeFrame);
  return PaintAccess::build(*PaintAccess::live(pass), &frame, false,
                            std::span(children).first(count));
}

Color flatContentMap(texture::Role role) {
  switch (role) {
    case texture::Role::BaseColor:
      return {0, 0, 0, 0};
    case texture::Role::Roughness:
      return {0.5f, 0.5f, 0.5f, 1};
    case texture::Role::Packed:
      return {1, 0.5f, 0, 1};
    default:
      return surfaceMapGround(role);
  }
}

Color surfaceMapGround(texture::Role role) {
  switch (role) {
    case texture::Role::Normal:
    case texture::Role::ClearcoatNormal:
      return {0.5f, 0.5f, 1, 1};
    case texture::Role::Roughness:
    case texture::Role::Occlusion:
      return {1, 1, 1, 1};
    case texture::Role::Metallic:
    case texture::Role::Emissive:
    case texture::Role::Clearcoat:
      return {0, 0, 0, 1};
    case texture::Role::Packed:
      return {1, 1, 0, 1};
    case texture::Role::Sheen:
    case texture::Role::Iridescence:
      return {0, 0, 0, 1};
    case texture::Role::SheenSpecular:
      return {0, 1, 0, 1};
    case texture::Role::SpecularColor:
      return {1, 1, 1, 1};
    case texture::Role::Anisotropy:
      return {1, 0.5f, 0, 1};
    case texture::Role::Transmission:
    case texture::Role::Thickness:
      return {0, 0, 0, 1};
    case texture::Role::Distortion:
      return {0.5f, 0.5f, 0, 1};
    default:
      return {0, 0, 0, 0};
  }
}

bool isSurfaceMapRole(texture::Role role) {
  return surfaceMapIndex(role).has_value();
}

Paint asMap(const Material& material, texture::Role role) {
  return SurfaceMaps(material).asMap(role);
}

Paint SurfaceMaps::asMap(texture::Role role) const {
  Paint pass = mapPass(role);
  if (pass.isNone()) return pass;
  PaintAccess::refresh(pass);
  return pass;
}

Paint SurfaceMaps::mapPass(texture::Role role) const {
  const std::optional<size_t> index = surfaceMapIndex(role);
  if (!index) return {};
  {
    const std::lock_guard lock(m_maps->mutex);
    if (const std::optional<Paint>& held = m_maps->maps[*index]) return *held;
  }
  const sk_sp<SkRuntimeEffect>& effect = surfaceMapEffect();
  if (!effect) return {};
  const Material& material = m_inputs->material;
  Paint pass = PaintAccess::unresolvedSksl(effect);
  // EVERY CHANNEL STANDS WHERE THE COLOUR DOES: a material anchored to the
  // root frame places its normal, its numbers and its lobes' maps there
  // too, because the map samples each of them at the point it samples
  // the colour.
  pass.worldSpace(material.worldSpace());
  PaintAccess::storeSlot(pass, "uColor", m_inputs->colours);
  PaintAccess::storeUniform(pass, "uRole", float(*index));
  // A material that states no surface is a surface all the same, with
  // the stock options: its colour is its base colour and it takes light.
  // Only a surface stated unlit is its own colour.
  const SurfaceOptions* stated = material.surface();
  const bool unlit = stated != nullptr && stated->unlit;
  PaintAccess::storeUniform(pass, "uUnlit", unlit ? 1.0f : 0.0f);
  static const SurfaceOptions stock;
  const SurfaceOptions& surface = stated && !unlit ? *stated : stock;
  PaintAccess::storeUniform(pass, "uHasNormal",
                            !unlit && surface.normal ? 1.0f : 0.0f);
  if (m_inputs->normal)
    PaintAccess::storeSlot(pass, "uNormal", *m_inputs->normal);
  PaintAccess::storeUniform(pass, "uNormalScale", surface.normalScale);
  PaintAccess::storeUniform(pass, "uNormalDirectX",
                            surface.normalDirectX ? 1.0f : 0.0f);
  channel(pass, surface.roughness, m_inputs->roughness, "uRoughnessMap",
          "uHasRoughnessMap", "uRoughness");
  channel(pass, surface.metallic, m_inputs->metallic, "uMetallicMap",
          "uHasMetallicMap", "uMetallic");
  channel(pass, surface.occlusion, m_inputs->occlusion, "uOcclusionMap",
          "uHasOcclusionMap", "uOcclusion");
  PaintAccess::storeUniform(pass, "uHasEmissionMap",
                            !unlit && surface.emissionMap ? 1.0f : 0.0f);
  if (m_inputs->emission)
    PaintAccess::storeSlot(pass, "uEmissionMap", *m_inputs->emission);
  PaintAccess::storeUniform(pass, "uEmission", surface.emission);
  PaintAccess::storeUniform(pass, "uEmissionStrength",
                            surface.emissionStrength);
  channel(pass, surface.clearcoat, m_inputs->clearcoat, "uClearcoatMap",
          "uHasClearcoatMap", "uClearcoat");
  channel(pass, surface.clearcoatRoughness, m_inputs->clearcoatRoughness,
          "uClearcoatRoughnessMap", "uHasClearcoatRoughnessMap",
          "uClearcoatRoughness");
  colorChannel(pass, surface.sheenColor, m_inputs->sheenColor, "uSheenColorMap",
               "uHasSheenColorMap", "uSheenColor");
  channel(pass, surface.sheenRoughness, m_inputs->sheenRoughness,
          "uSheenRoughnessMap", "uHasSheenRoughnessMap", "uSheenRoughness");
  channel(pass, surface.specular, m_inputs->specular, "uSpecularMap",
          "uHasSpecularMap", "uSpecular");
  colorChannel(pass, surface.specularColor, m_inputs->specularColor,
               "uSpecularColorMap", "uHasSpecularColorMap", "uSpecularColor");
  PaintAccess::storeUniform(pass, "uAnisotropy", surface.anisotropy);
  PaintAccess::storeUniform(pass, "uAnisotropyRotation",
                            surface.anisotropyRotation);
  PaintAccess::storeUniform(pass, "uHasAnisotropyMap",
                            m_inputs->anisotropyDirection ? 1.0f : 0.0f);
  if (m_inputs->anisotropyDirection)
    PaintAccess::storeSlot(pass, "uAnisotropyMap",
                           *m_inputs->anisotropyDirection);
  channel(pass, surface.iridescence, m_inputs->iridescence, "uIridescenceMap",
          "uHasIridescenceMap", "uIridescence");
  PaintAccess::storeUniform(
      pass, "uIridescenceThickness",
      surface.iridescenceThickness / texture::kIridescenceThicknessRange);
  PaintAccess::storeUniform(pass, "uIridescenceThicknessMinimum",
                            surface.iridescenceThicknessMinimum /
                                texture::kIridescenceThicknessRange);
  PaintAccess::storeUniform(pass, "uHasIridescenceThicknessMap",
                            m_inputs->iridescenceThickness ? 1.0f : 0.0f);
  if (m_inputs->iridescenceThickness)
    PaintAccess::storeSlot(pass, "uIridescenceThicknessMap",
                           *m_inputs->iridescenceThickness);
  PaintAccess::storeUniform(pass, "uHasClearcoatNormal",
                            m_inputs->clearcoatNormal ? 1.0f : 0.0f);
  if (m_inputs->clearcoatNormal)
    PaintAccess::storeSlot(pass, "uClearcoatNormal",
                           *m_inputs->clearcoatNormal);
  channel(pass, surface.transmission, m_inputs->transmission,
          "uTransmissionMap", "uHasTransmissionMap", "uTransmission");
  channel(pass, surface.thickness, m_inputs->thickness, "uThicknessMap",
          "uHasThicknessMap", "uThickness");
  // A distortion is carried only with its map, at a strength other than
  // zero; without one, what stands behind stays still.
  const bool distorts = m_inputs->distortion && surface.distortionStrength != 0;
  PaintAccess::storeUniform(pass, "uHasDistortionMap", distorts ? 1.0f : 0.0f);
  PaintAccess::storeUniform(pass, "uDistortionStrength",
                            surface.distortionStrength);
  if (m_inputs->distortion)
    PaintAccess::storeSlot(pass, "uDistortionMap", *m_inputs->distortion);
  const std::lock_guard lock(m_maps->mutex);
  std::optional<Paint>& held = m_maps->maps[*index];
  if (!held) held = std::move(pass);
  return *held;
}

}  // namespace sigil::material::skia
