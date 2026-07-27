#pragma once

/** @file
 * SigilWorld's public components — the EnTT face of the world. Every
 * surface addSurface()/scene:: creates IS an entity in an
 * entt::registry (the proven ECS the repo already runs its scene
 * decoding on), carrying:
 *
 *   TransformComponent  the world matrix (mutate freely; the renderer
 *                       reads it every frame)
 *   MaterialComponent   the shading parameters (colors, metallic,
 *                       roughness, emissive are LIVE on mutation;
 *                       swapping `texture` needs a remove/re-add or a
 *                       scene re-describe — the SRB is baked)
 *
 * plus a private GPU component (buffers, SRB) owned by World.cpp.
 * World::registry() hands out the registry itself, so systems compose
 * the EnTT way: animate transforms in a view, tag entities with your
 * own components, drive gameplay state alongside rendering state —
 * surface ids returned by addSurface() are entt entity values, usable
 * with the registry directly (via World::entity()).
 *
 * Lights and cameras are entities too: attach a LightComponent to any
 * entity (or use World::addLight) and render() gathers up to
 * kLightBudget of them each frame on top of the Lighting sun +
 * hemisphere base; attach a CameraComponent with `active = true` and
 * it overrides World::setCamera until deactivated or destroyed.
 */

#include "sigilworld/World.h"

#include <entt/entt.hpp>

namespace sigil::world {

struct TransformComponent {
  SkM44 model;
};

struct MaterialComponent {
  Material material;
};

/** render() gathers at most this many LightComponent entities per
 *  frame (registry iteration order; excess lights are ignored). */
inline constexpr int kLightBudget = 8;

/** A dynamic light the renderer reads LIVE each frame, layered on top
 *  of the Lighting sun + hemisphere ambient (setLighting) — scenes
 *  without light entities render exactly as before. */
struct LightComponent {
  enum class Type : uint8_t { Directional, Point };
  Type type = Type::Point;
  SkColor4f color = {1, 1, 1, 1};
  float intensity = 1;
  /** Directional only: world-space direction toward the scene. */
  SkV3 direction = {0, -1, 0};
  /** Point only: world-space position. */
  SkV3 position = {0, 0, 0};
  /** Point only: falloff radius — intensity fades smoothly to zero at
   *  this distance ((1 - (d/range)^2)^2, 1 at the light). */
  float range = 600;

  bool operator==(const LightComponent &) const = default;
};

/** An active camera entity overrides World::setCamera; deactivate or
 *  destroy it to fall back. With several active cameras the first the
 *  registry iterates wins — keep one active. */
struct CameraComponent {
  shape::space::Camera camera;
  bool active = true;
};

/** A surface id as the entity it is. */
inline entt::entity entity(uint32_t surfaceId) {
  return (entt::entity)surfaceId;
}

} // namespace sigil::world
