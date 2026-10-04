#pragma once

#include <sigilmaterial/core/Lighting.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmaterial/texture/EnvironmentMap.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmotion/values/Animatable.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>

namespace painted_fields {

namespace material = sigil::material;
namespace motion = sigil::motion;

enum class Source { Point, Spot, Sun, Softbox, Strip, Window, Ring, Rig };
inline constexpr std::array<std::string_view, 8> kSourceNames{
    "POINT", "SPOT", "SUN", "SOFTBOX", "STRIP", "WINDOW", "RING", "RIG"};

struct Lights {
  material::EnvironmentMap environment;
  std::array<material::EnvironmentMap, 4> shaped;
  motion::Animatable<float> bearing = motion::animatable(132.f);
  motion::Animatable<float> rotation = motion::animatable(0.f);
  glm::vec3 position{852, 440, 420};
  glm::vec3 fillPosition{632, 359, 360};
  motion::Animatable<float> rigKeyX = motion::animatable(532.f);
  motion::Animatable<float> rigKeyY = motion::animatable(215.f);
  motion::Animatable<float> rigFillX = motion::animatable(632.f);
  motion::Animatable<float> rigFillY = motion::animatable(359.f);

  void setup() {
    environment = material::EnvironmentMap::baked(256, [](float u, float v) {
      const float a = std::sin((u - .09f) * kPi) / .29f;
      const float b = (v - .38f) / .27f;
      const float c = (u - .73f) / .032f;
      const float d = (v - .47f) / .3f;
      return glm::vec3(.35f, .39f, .38f) +
             std::exp(-a * a - b * b) * glm::vec3(1.6f, 1.35f, .98f) +
             std::exp(-c * c - d * d) * glm::vec3(.67f, .93f, 1.15f);
    });
    for (int i = 0; i < 4; ++i) {
      const auto source = static_cast<Source>(i + 3);
      shaped[i] = material::EnvironmentMap::baked(
          512, [source](float u, float v) { return radiance(source, u, v); });
    }
  }

  void update(float t) {
    bearing = 132 + 54 * std::sin(t * 2 * kPi / 32);
    position = {852 + 340 * std::sin(t * 2 * kPi / 32),
                440 + 65 * std::cos(t * 2 * kPi / 16), 420};
    rotation = 27 * std::sin(t * 2 * kPi / 32 + .7f);
    fillPosition = {852 - 280 * std::sin(t * 2 * kPi / 32 + .9f),
                    440 - 90 * std::cos(t * 2 * kPi / 16 + .45f), 360};
    rigKeyX = position.x - 320;
    rigKeyY = position.y - 225;
    rigFillX = fillPosition.x;
    rigFillY = fillPosition.y;
  }

  std::array<material::Light, 2> rigSources() const {
    return {material::studio({.elevation = 90.f,
                              .color = material::Color{1, .63f, .35f, 1},
                              .intensity = .24f,
                              .ambient = .12f,
                              .kind = material::LightKind::Point,
                              .position = {0, 0, 420},
                              .range = 1400}),
            material::studio({.elevation = 90.f,
                              .color = material::Color{.35f, .7f, 1, 1},
                              .intensity = .20f,
                              .ambient = .08f,
                              .kind = material::LightKind::Point,
                              .position = {0, 0, 360},
                              .range = 1400})};
  }

  material::Environment rigEnvironment() const {
    return material::environment(environment.texture().source(),
                                 {.rotation = rotation, .intensity = .35f});
  }

  material::Lighting lighting(Source source) const {
    material::Light key = material::studio({.direction = bearing,
                                            .elevation = 82.f,
                                            .intensity = .32f,
                                            .ambient = .2f,
                                            .kind = material::LightKind::Point,
                                            .position = position,
                                            .range = 1400,
                                            .innerAngle = 18,
                                            .outerAngle = 38});
    switch (source) {
      case Source::Spot:
        key.kind = material::LightKind::Spot;
        key.elevation = 90.f;
        return key;
      case Source::Sun:
        key.kind = material::LightKind::Directional;
        key.elevation = 12.f;
        return key;
      case Source::Softbox:
      case Source::Strip:
      case Source::Window:
      case Source::Ring:
        return material::environment(
            shaped[static_cast<int>(source) - 3].texture().source(),
            {.rotation = rotation, .intensity = kShapedIntensity});
      case Source::Point:
      default:
        return {key, material::environment(
                         environment.texture().source(),
                         {.rotation = rotation, .intensity = .55f})};
    }
  }

  material::Material preview(Source source, glm::vec2 extent) const {
    const int index = static_cast<int>(source) - 3;
    if (index < 0 || index >= 4) return material::Color{0, 0, 0, 0};
    const material::Texture panorama = shaped[index].texture();
    struct Preview {
      glm::vec2 extent;
      glm::vec2 mapSize;
      float intensity;
    };
    return material::shader(
               R"(
half4 main(float2 p) {
  float2 plane = (p / extent - .5) * float2(4, -2);
  float3 d = normalize(float3(plane, 1));
  float2 uv = float2(.5 + atan(d.x, -d.z) / 6.2831853,
                    acos(clamp(d.y, -1, 1)) / 3.14159265);
  half4 sampled = panorama.eval(uv * mapSize);
  return half4(sampled.rgb * half(intensity), sampled.a);
})",
               Preview{extent, glm::vec2(panorama.size()), kShapedIntensity},
               {.textures = {{"panorama", {}}}})
        .slot("panorama", panorama);
  }

 private:
  static constexpr float kPi = 3.14159265358979323846f;
  static constexpr float kShapedIntensity = .45f;

  static float feather(float distance, float width) {
    const float t = std::clamp(.5f - distance / width, 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
  }

  static float rectangle(glm::vec2 p, glm::vec2 halfSize, float edge) {
    const float distance =
        std::max(std::abs(p.x) - halfSize.x, std::abs(p.y) - halfSize.y);
    return feather(distance, edge);
  }

  static glm::vec3 radiance(Source source, float u, float v) {
    const glm::vec3 direction = material::equirectangularDirection({u, v});
    const glm::vec3 room{.055f, .065f, .075f};
    if (direction.z <= 0.f) return room;
    // A direction meets a virtual front plane. This gives each emitter its
    // angular shape; there is no nearby-source distance or parallax.
    const glm::vec2 p{direction.x / direction.z, direction.y / direction.z};
    float coverage = 0;
    glm::vec3 color{1.65f, 1.6f, 1.48f};
    switch (source) {
      case Source::Softbox:
        coverage = rectangle(p - glm::vec2{-.08f, .12f}, {1.f, .56f}, .12f);
        break;
      case Source::Strip:
        coverage = rectangle(p, {1.2f, .105f}, .045f);
        color = {1.4f, 1.75f, 2.f};
        break;
      case Source::Window: {
        const glm::vec2 pane = p - glm::vec2{.15f, .2f};
        const float frame = rectangle(pane, {.95f, .7f}, .04f);
        const float vertical = feather(std::abs(pane.x) - .035f, .018f);
        const float horizontal = feather(std::abs(pane.y) - .035f, .018f);
        coverage = frame * (1.f - vertical) * (1.f - horizontal);
        color = {1.3f, 1.7f, 2.1f};
        break;
      }
      case Source::Ring:
        coverage = feather(
            std::abs(std::sqrt(p.x * p.x + p.y * p.y) - .44f) - .18f, .045f);
        color = {1.85f, 1.6f, 1.3f};
        break;
      default:
        break;
    }
    // Edge softness is authored into the source. Surface roughness changes
    // reflection strength but does not filter this panorama in the 2D pass.
    return room + coverage * color;
  }
};

}  // namespace painted_fields
