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
#include <sigilmaterial/advanced/Program.h>  // reportOnce
#include <sigilmaterial/advanced/Terms.h>
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
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "Environment.h"
#include "PaintInternal.h"

namespace sigil::material::skia {

namespace {

std::string lightUniform(std::string_view name, size_t index, size_t count) {
  return count == 1 ? std::string(name)
                    : std::string(name) + "_" + std::to_string(index);
}

sk_sp<SkRuntimeEffect> buildLitEffect(const std::vector<LightKind>& kinds,
                                      LightingFrame frame, bool placed) {
  const bool scene = frame == LightingFrame::Scene;
  std::string declarations;
  std::string direct;
  for (size_t i = 0; i < kinds.size(); ++i) {
    const auto uniform = [&](std::string_view name) {
      return lightUniform(name, i, kinds.size());
    };
    const bool point = kinds[i] == LightKind::Point;
    const bool spot = kinds[i] == LightKind::Spot;
    if (!point) {
      declarations += "uniform float " + uniform("uDirection") + ";\n";
      declarations += "uniform float " + uniform("uElevation") + ";\n";
    }
    declarations += "uniform float " + uniform("uIntensity") + ";\n";
    declarations += "uniform float4 " + uniform("uLightColor") + ";\n";
    direct += "  {\n";
    if (point) {
      direct += "    float3 l = float3(0, 0, 1);\n";
    } else {
      direct += "    float azimuth = radians(" + uniform("uDirection") +
                "), height = radians(" + uniform("uElevation") + ");\n";
      direct +=
          "    float3 l = normalize(float3(cos(height) * cos(azimuth),\n"
          "        -cos(height) * sin(azimuth), sin(height)));\n";
    }
    direct += "    float3 radiance = " + uniform("uLightColor") + ".rgb * " +
              uniform("uIntensity") + ";\n";
    const bool sourcePositioned = kinds[i] != LightKind::Directional;
    if (sourcePositioned) {
      declarations += "uniform float3 " + uniform("uLightPosition") + ";\n";
      declarations += "uniform float " + uniform("uLightRange") + ";\n";
      if (spot)
        declarations += "uniform float2 " + uniform("uConeCosines") + ";\n";
      direct += "    float falloff;\n    l = positionedLight(xy, l, " +
                uniform("uLightPosition") + ", " + uniform("uLightRange") +
                ", " + (spot ? "1.0" : "0.0") + ", " +
                (spot ? uniform("uConeCosines") : "float2(0)") +
                ", falloff);\n    radiance *= falloff;\n";
    }
    direct += "    float3 h = halfDirection(l, view);\n";
    direct += "    illuminate(" +
              std::string(sourcePositioned || scene ? "n" : "localNormal") +
              ", l, h, radiance, diffuse, f0, roughness, lit, coating);\n  }\n";
  }

  std::string body(shaderSource("LitSurface.sksl"));
  // A body missing a marker cannot take the lights: report it and build
  // nothing, as a body that fails to compile does, rather than throw out
  // of the effect cache.
  const auto insert = [&](std::string_view marker,
                          const std::string& replacement) {
    const size_t at = body.find(marker);
    if (at == std::string::npos) {
      reportOnce("litsurface.marker:" + std::string(marker),
                 "the lit surface body carries no \"" + std::string(marker) +
                     "\" marker, so its lights cannot be written into it; "
                     "the surface paints flat");
      return false;
    }
    body.replace(at, marker.size(), replacement);
    return true;
  };
  if (!insert("// SIGIL_LIGHT_DECLARATIONS", declarations) ||
      !insert("// SIGIL_DIRECT_LIGHTS", direct))
    return nullptr;
  std::string source(shaderSource("LitDirections.sksl"));
  source += shaderSource(placed ? "LitPositioned.sksl" : "LitDirectional.sksl");
  source += termsSource(Target::SkSL);
  source += shaderSource("EnvironmentKernel.sksl");
  source += body;
  auto [effect, error] =
      SkRuntimeEffect::MakeForShader(SkString(source.c_str()));
  if (!effect)
    SkDebugf("sigilmaterial lit surface shader (%zu sources): %s\n",
             kinds.size(), error.c_str());
  return effect;
}

sk_sp<SkRuntimeEffect> litEffect(const Lighting& lighting) {
  std::vector<LightKind> kinds;
  kinds.reserve(lighting.lights.size());
  for (const Light& light : lighting.lights) kinds.push_back(light.kind);
  const LightingFrame frame = lighting.frame;

  // Skia runtime effects use constant-size uniforms and ES2 control flow.
  // Specializing the exact source list leaves no fixed light-count ceiling.
  // A bounded LRU owns reusable variants; paints retain evicted effects.
  struct Cached {
    std::vector<LightKind> kinds;
    LightingFrame frame;
    sk_sp<SkRuntimeEffect> effect;
  };
  constexpr size_t kCapacity = 32;
  static std::mutex mutex;
  static std::vector<Cached> cache;
  const std::lock_guard lock(mutex);
  for (auto it = cache.begin(); it != cache.end(); ++it) {
    if (it->kinds != kinds || it->frame != frame) continue;
    Cached hit = std::move(*it);
    cache.erase(it);
    cache.push_back(std::move(hit));
    return cache.back().effect;
  }
  // Placement is read from the kinds and the frame alone, which key the
  // cache.
  sk_sp<SkRuntimeEffect> effect =
      buildLitEffect(kinds, frame, lighting.dependsOnPlacement());
  if (cache.size() == kCapacity) cache.erase(cache.begin());
  cache.push_back({std::move(kinds), frame, effect});
  return effect;
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

struct LitSurface::Inputs {
  Material material;
  Paint colours;
  std::optional<Paint> normal, roughness, metallic, occlusion, emission;

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
  }
};

struct LitSurface::PassCache {
  std::mutex environmentMutex;
  std::optional<Material> source;
  Paint lowered;
  std::shared_ptr<EnvironmentPreparation> preparation =
      std::make_shared<EnvironmentPreparation>();
  std::mutex passMutex;
  std::optional<Lighting> lighting;
  Paint pass;

  Paint lower(const Material& image) {
    const std::lock_guard lock(environmentMutex);
    if (!source || *source != image) {
      Material description = image;
      Paint prepared = PaintAccess::prepareMaterial(image);
      // Solid child resolution otherwise creates a fresh color shader at
      // every call, despite the unchanged panorama.
      PaintAccess::retainSolidShader(prepared);
      lowered = std::move(prepared);
      source.emplace(std::move(description));
    }
    return lowered;
  }
};

LitSurface::LitSurface(const Material& material)
    : m_inputs(std::make_shared<const Inputs>(material)),
      m_pass(std::make_shared<PassCache>()) {}

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
  if (surface->lighting && *surface->lighting &&
      surface->lighting->dependsOnPlacement())
    return true;
  for (const Channel* channel :
       {&surface->metallic, &surface->roughness, &surface->occlusion})
    if (const auto* map = std::get_if<Material>(channel);
        map && detail::paintUsesWorldSpace(*map))
      return true;
  if ((surface->normal && detail::paintUsesWorldSpace(*surface->normal)) ||
      (surface->emissionMap &&
       detail::paintUsesWorldSpace(*surface->emissionMap)))
    return true;
  return surface->lighting && surface->lighting->environment &&
         surface->lighting->environment->image &&
         detail::paintUsesWorldSpace(*surface->lighting->environment->image);
}

Lighting lightingFor(const Material& material, const Lighting& inForce) {
  if (!isLit(material)) return {};
  const SurfaceOptions& surface = *material.surface();
  return surface.lighting ? *surface.lighting : inForce;
}

Paint lit(const Material& material, const Lighting& lighting) {
  if (!isLit(material) || !lighting) return paint(material);
  return LitSurface(material).under(lighting);
}

Paint LitSurface::under(const Lighting& lighting) const {
  const Material& material = m_inputs->material;
  const auto flat = [&] {
    Paint paint = m_inputs->colours;
    PaintAccess::retainSnapshot(paint);
    return paint;
  };
  if (!isLit(material) || !lighting) return flat();
  Paint pass = lightingPass(lighting);
  if (pass.isNone()) return flat();
  PaintAccess::refresh(pass);
  return pass;
}

sk_sp<SkShader> LitSurface::shader(const Lighting& lighting,
                                   const FrameData& nodeFrame,
                                   const glm::mat3& paintToLocal) const {
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
  if (!isLit(m_inputs->material) || !lighting) return mapped(m_inputs->colours);
  Paint pass = lightingPass(lighting);
  if (pass.isNone()) return mapped(m_inputs->colours);
  std::array<PaintAccess::ChildOverride, 6> children;
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
  const PaintFrame frame = paintFrameOf(nodeFrame);
  return PaintAccess::build(*PaintAccess::live(pass), &frame, false,
                            std::span(children).first(count));
}

Paint LitSurface::lightingPass(const Lighting& lighting) const {
  {
    const std::lock_guard lock(m_pass->passMutex);
    if (m_pass->lighting && *m_pass->lighting == lighting) return m_pass->pass;
  }
  Paint pass = makeLightingPass(lighting);
  if (pass.isNone()) return pass;
  {
    const std::lock_guard lock(m_pass->passMutex);
    if (m_pass->lighting && *m_pass->lighting == lighting) return m_pass->pass;
    m_pass->lighting.emplace(lighting);
    m_pass->pass = pass;
  }
  return pass;
}

Paint LitSurface::makeLightingPass(const Lighting& lighting) const {
  const Material& material = m_inputs->material;
  const sk_sp<SkRuntimeEffect> effect = litEffect(lighting);
  if (!effect) return {};
  const SurfaceOptions& surface = *material.surface();

  Paint pass = PaintAccess::unresolvedSksl(effect);
  PaintAccess::storeSlot(pass, "uColor", m_inputs->colours);
  PaintAccess::storeUniform(pass, "uHasNormal", surface.normal ? 1.0f : 0.0f);
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
                            surface.emissionMap ? 1.0f : 0.0f);
  if (m_inputs->emission)
    PaintAccess::storeSlot(pass, "uEmissionMap", *m_inputs->emission);
  PaintAccess::storeUniform(pass, "uEmission", surface.emission);
  PaintAccess::storeUniform(pass, "uEmissionStrength",
                            surface.emissionStrength);
  PaintAccess::storeUniform(pass, "uReflection", surface.reflectionWeight);
  PaintAccess::storeUniform(pass, "uClearcoat",
                            std::isfinite(surface.clearcoat)
                                ? std::clamp(surface.clearcoat, 0.0f, 1.0f)
                                : 0.0f);

  float ambient = 0;
  for (size_t i = 0; i < lighting.lights.size(); ++i) {
    const Light& light = lighting.lights[i];
    const auto uniform = [&](std::string_view name) {
      return lightUniform(name, i, lighting.lights.size());
    };
    if (light.kind != LightKind::Point) {
      PaintAccess::storeBinding(pass, uniform("uDirection"), light.direction);
      PaintAccess::storeBinding(pass, uniform("uElevation"), light.elevation);
    }
    PaintAccess::storeBinding(pass, uniform("uIntensity"), light.intensity);
    PaintAccess::storeBinding(pass, uniform("uLightColor"), light.color);
    ambient += light.ambient;
    if (light.kind == LightKind::Directional) continue;
    const bool valid = std::isfinite(light.position.x) &&
                       std::isfinite(light.position.y) &&
                       std::isfinite(light.position.z) &&
                       std::isfinite(light.range) && light.range > 0;
    PaintAccess::storeUniform(
        pass, uniform("uLightPosition"),
        std::vector<float>{valid ? light.position.x : 0.0f,
                           valid ? light.position.y : 0.0f,
                           valid ? light.position.z : 0.0f});
    PaintAccess::storeUniform(pass, uniform("uLightRange"),
                              valid ? light.range : 0.0f);
    if (light.kind != LightKind::Spot) continue;
    const float outer = std::isfinite(light.outerAngle)
                            ? std::clamp(light.outerAngle, 0.0f, 180.0f)
                            : 0.0f;
    const float inner = std::isfinite(light.innerAngle)
                            ? std::clamp(light.innerAngle, 0.0f, outer)
                            : 0.0f;
    constexpr float radiansPerDegree = 3.14159265f / 180;
    PaintAccess::storeUniform(
        pass, uniform("uConeCosines"),
        std::array<float, 2>{std::cos(inner * radiansPerDegree),
                             std::cos(outer * radiansPerDegree)});
  }
  PaintAccess::storeUniform(pass, "uAmbient", ambient);

  // An environment with no picture is no environment: it neither
  // reflects nor takes the ambient share over.
  if (lighting.environment && lighting.environment->image) {
    const Environment& around = *lighting.environment;
    PaintAccess::storeUniform(pass, "uHasEnvironment", 1.0f);
    PaintAccess::storeSlot(pass, "uEnvironment", m_pass->lower(*around.image));
    PaintAccess::storeBinding(pass, "uEnvironmentRotation",
                              around.options.rotation);
    PaintAccess::storeUniform(pass, "uEnvironmentIntensity",
                              around.options.intensity);
    PaintAccess::storeUniform(
        pass, "uEnvironmentSize",
        std::array<float, 2>{around.options.size.x, around.options.size.y});
    PaintAccess::prepareEnvironment(pass, m_pass->preparation);
    // Where the environment alone lights the surface, its ambient share is
    // the whole of the diffuse light.
    if (lighting.lights.empty())
      PaintAccess::storeUniform(pass, "uAmbient", 1.0f);
  } else {
    PaintAccess::storeUniform(pass, "uHasEnvironment", 0.0f);
    PaintAccess::storeUniform(pass, "uEnvironmentRotation", 0.0f);
    PaintAccess::storeUniform(pass, "uEnvironmentIntensity", 0.0f);
    PaintAccess::storeUniform(pass, "uEnvironmentSize",
                              std::array<float, 2>{1.0f, 1.0f});
  }
  return pass;
}

}  // namespace sigil::material::skia
