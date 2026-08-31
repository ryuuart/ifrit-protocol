#pragma once

/** @file
 * The studies, listed. One factory per file under this directory, and
 * this header is the outline of what there is — a study joins by adding
 * its factory here and its file to the binary's sources.
 */

#include <sigilworld/testing/Study.h>

#include <span>

namespace sigil::world::testing {

/** A lit set: a tube swept along a closed loop, a comet of stamps riding
 *  a window of that same loop, and a camera on a rail of its own. */
Study firstLight();

/** The passes, made visible: a set drawn once and then reached three
 *  ways — a masked grade on what is tagged, the same tag culled into a
 *  target of its own and softened, and that softening laid over its own
 *  output from the frame before. */
Study glowTrail();

/** What a surface is made of: five cards over a textured floor — one
 *  plain, one a stack through a mask, one glass, one emissive, and one
 *  wearing a map — under the kit's rig and turntable. */
Study materialLab();

/** A live 2D scene as a texture on a swept band: a compose tree
 *  reconciled and painted into an image, repeating along a ribbon. */
Study wovenCard();

/** The emitter's dials: one still set whose key light's strength and
 *  colour are bound to live values, so what moves is the lane and never
 *  the description. */
Study keyLight();

/** Every study, in the order a sweep walks them. */
std::span<const Study> registry();

}  // namespace sigil::world::testing
