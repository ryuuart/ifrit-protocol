#pragma once

/** @file
 * @ingroup world-node
 * A NULL: a placement with a key and nothing under it — no children, no
 * geometry, no light. It exists to be referred to: a pivot, or the point
 * a light or a camera looks at.
 */

#include <sigilworld/advanced/Node.h>
#include <sigilworld/node/Verbs.h>

namespace sigil::world {

/** A PLACEMENT AND NOTHING ELSE. It stands where its placement and its
 *  parents put it, answers to its key and joins its collections; it
 *  draws nothing, lights nothing and holds nothing. */
class Null : public Node,
             public PlacementVerbs<Null>,
             public IdentityVerbs<Null> {
 public:
  Null() = default;
};

/** A null: name it with `key` so something can refer to it. */
Null null();

}  // namespace sigil::world
