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

#include <array>
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
    default:
      return std::nullopt;
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

}  // namespace

struct SurfaceMaps::Inputs {
  Material material;
  Paint colours;
  std::optional<Paint> normal, roughness, metallic, occlusion, emission,
      clearcoat, clearcoatRoughness;

  static Paint prepare(const Material& material) {
    Paint paint = PaintAccess::prepareMaterial(material);
    PaintAccess::retainSolidShader(paint);
    return paint;
  }

  explicit Inputs(const Material& from)
      : material(from), colours(prepare(material)) {
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
  }
};

struct SurfaceMaps::MapCache {
  std::mutex mutex;
  /** One unshaded pass per surface map role, made on first request. */
  std::array<std::optional<Paint>, 8> maps;
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
        &surface->clearcoat, &surface->clearcoatRoughness})
    if (const auto* map = std::get_if<Material>(channel);
        map && detail::paintUsesWorldSpace(*map))
      return true;
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
  std::array<PaintAccess::ChildOverride, 8> children;
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
  const PaintFrame frame = paintFrameOf(nodeFrame);
  return PaintAccess::build(*PaintAccess::live(pass), &frame, false,
                            std::span(children).first(count));
}

Color surfaceMapGround(texture::Role role) {
  switch (role) {
    case texture::Role::Normal:
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
  PaintAccess::storeSlot(pass, "uColor", m_inputs->colours);
  PaintAccess::storeUniform(pass, "uRole", float(*index));
  const bool unlit = !isLit(material);
  PaintAccess::storeUniform(pass, "uUnlit", unlit ? 1.0f : 0.0f);
  // What a lit surface reads where it states nothing is what the stock
  // surface options say; a material with no surface takes them too, and
  // the unlit branch of the program never reads them.
  static const SurfaceOptions stock;
  const SurfaceOptions& surface = unlit ? stock : *material.surface();
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
  const std::lock_guard lock(m_maps->mutex);
  std::optional<Paint>& held = m_maps->maps[*index];
  if (!held) held = std::move(pass);
  return *held;
}

}  // namespace sigil::material::skia
