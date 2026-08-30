#pragma once

/** @file
 * TRANSITIONAL. World's own material and light structs handed over as
 * the values that own those subjects: a `Material` as the material
 * library's metallic-roughness surface, a `LightComponent` and a
 * `Lighting`'s sun as the light feature's emitters. It exists so a
 * caller holding World's values can still reach a consumer that takes
 * the upstream ones, and goes away with the structs it adapts.
 *
 * Two losses are unavoidable and stated rather than hidden. A layer's
 * mask adapts as far as an image can carry it: a painted map keeps its
 * channel, its window and its fit, while a mask read off the surface
 * itself — vertex colour, slope, height — has no image to hand over and
 * becomes a mask that covers everything, since the adapted value has no
 * surface to read it from. And the hemisphere ambient and the
 * environment panorama of a `Lighting` are not emitters: only its sun
 * crosses.
 */

#include <sigilmaterial/core/Material.h>
#include <sigilworld/Components.h>
#include <sigilworld/World.h>
#include <sigilworld/light/Light.h>

namespace sigil::world {

/** @p m as a material-library surface: the params by name, one texture
 *  per map slot, and each layer stacked on with `over()`. An `unlit`
 *  material becomes an instance of the unlit recipe. */
material::Material surfaceOf(const Material& m);

/** @p mask as a scalar-output material. */
material::Material maskOf(const Mask& mask);

/** @p light as an emitter value. */
light::Light lightOf(const LightComponent& light);

/** The sun of @p lighting as an emitter value. */
light::Light sunOf(const Lighting& lighting);

}  // namespace sigil::world
