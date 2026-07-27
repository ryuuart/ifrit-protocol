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

/** A surface id as the entity it is. */
inline entt::entity entity(uint32_t surfaceId) {
  return (entt::entity)surfaceId;
}

} // namespace sigil::world
