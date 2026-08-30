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

/** Every study, in the order a sweep walks them. */
std::span<const Study> registry();

}  // namespace sigil::world::testing
