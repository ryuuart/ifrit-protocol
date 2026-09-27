#pragma once

/** @file
 * @ingroup material-core
 *
 * THE LIGHT A LIT SURFACE IS SHADED UNDER, in 2D as in 3D: one
 * directional light and an environment, each optional, as one value a
 * scene states once and everything under it inherits — `studio()` for
 * the light, `environment()` for the picture around the surface that it
 * reflects. Every angle and strength is an animatable, so a bound light
 * turns the relief of a normal map frame by frame while the colours
 * beneath it stay as they were painted.
 */

#include <sigilmaterial/color/Color.h>
#include <sigilmotion/values/Animatable.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <cstdint>

#include <memory>
#include <optional>
#include <utility>

namespace sigil::material {

class Material;

/** What an emitter is. A surface lit in the plane answers to the
 *  directional reading alone; a set in three dimensions also places the
 *  other two where they stand. */
enum class LightKind : uint8_t {
  Directional,  ///< a direction only: a key light, the sun
  Point,        ///< a position, falling off to nothing at a range
  Spot,         ///< a position and a direction, within a cone
};

/** ONE LIGHT, as a studio key light is set: where it comes from, how high
 *  above the surface, its colour and strength, and the ambient share
 *  every point receives whatever way it faces. A point or a spot also
 *  stands somewhere and reaches so far; the fields a kind does not read
 *  keep their defaults and are ignored. */
struct Light {
  /** Where the light comes FROM on the page, in degrees counter-clockwise
   *  from three o'clock: 120 is the upper left a bevel is lit from. */
  motion::Animatable<float> direction = 120.0f;
  /** How far above the page it stands, in degrees: 90 is straight on,
   *  0 grazes the surface. */
  motion::Animatable<float> elevation = 45.0f;
  Color color = {1, 1, 1, 1};
  motion::Animatable<float> intensity = 1.0f;
  /** The light every point receives whichever way it faces, as a share of
   *  the surface's own colour. */
  float ambient = 0.3f;
  /** Directional by default: every point is lit from one direction. */
  LightKind kind = LightKind::Directional;
  /** Point and spot: where the emitter stands, in the scene's units. */
  glm::vec3 position = {0, 0, 0};
  /** Point and spot: the distance at which the light has fallen to
   *  nothing. */
  float range = 600;
  /** Spot: full strength within this many degrees of where it aims, dark
   *  beyond `outerAngle`, and smoothly between. */
  float innerAngle = 0;
  float outerAngle = 45;

  /** Whether an angle or the strength is moving. */
  bool isRunning() const;
  bool operator==(const Light&) const = default;
};

/** THE STOCK KEY LIGHT: `light`'s fields, each defaulting to a studio
 *  light from the upper left at 45 degrees. */
inline Light studio(Light light = {}) { return light; }

/** How an environment surrounds the surface. */
struct EnvironmentOptions {
  /** Turns the environment about the vertical axis, in degrees. */
  motion::Animatable<float> rotation = 0.0f;
  /** Scales what the environment gives, 1 as it is. */
  float intensity = 1;
  /** The extent the latitude-longitude picture spans in its material's
   *  own units — its pixel size for an image. Zero takes the image's own
   *  size where the environment is made from one. */
  glm::vec2 size = {0, 0};
  bool operator==(const EnvironmentOptions&) const = default;
};

/** THE PICTURE AROUND A SURFACE, as a latitude-longitude (equirectangular)
 *  image the surface reflects by its normal and its roughness, and takes
 *  its ambient colour from. Any material stands for the picture, read
 *  across `options.size`. */
struct Environment {
  std::shared_ptr<const Material> image;
  EnvironmentOptions options;

  /** Whether the picture or its rotation is moving. */
  bool isRunning() const;
  /** Equal when the pictures are equal materials under equal options. */
  bool operator==(const Environment& other) const;
};

/** @p image as an environment. `environment(media::PixelSource, …)`
 *  beside the image base takes a picture directly. */
Environment environment(Material image, EnvironmentOptions options = {});

/** THE LIGHT IN FORCE: a directional light, an environment, or both.
 *  A light converts, and so does an environment, so either is written
 *  where a lighting is taken. */
struct Lighting {
  std::optional<Light> light;
  std::optional<Environment> environment;

  Lighting() = default;
  // NOLINTNEXTLINE(google-explicit-constructor)
  Lighting(Light key) : light(std::move(key)) {}
  // NOLINTNEXTLINE(google-explicit-constructor)
  Lighting(Environment around) : environment(std::move(around)) {}
  Lighting(Light key, Environment around)
      : light(std::move(key)), environment(std::move(around)) {}

  /** Whether anything lights at all. */
  explicit operator bool() const {
    return light.has_value() || environment.has_value();
  }
  /** Whether the light or the environment is moving. */
  bool isRunning() const;
  bool operator==(const Lighting&) const = default;
};

}  // namespace sigil::material
