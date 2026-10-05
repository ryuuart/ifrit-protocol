#pragma once

/** @file
 * @ingroup material-core
 * WHAT A MATERIAL READS OF THE INSTANCE IT IS DRAWN ON. A body drawn as
 * instanced geometry is one stamp drawn many times, and each copy carries
 * a colour, its index and a few named scalars — an age, a phase, a cell.
 * A device program reads them through the scaffold it is compiled into:
 *
 *     float4 instanceColor()          // white for a body not instanced
 *     float  instanceIndex()          // 0 for a body not instanced
 *     float  instanceAttribute(float key)  // 0 where no scalar has the key
 *
 * A program cannot hold a name, so a scalar is asked for by the number its
 * name folds to here, which a material carries as an ordinary parameter
 * and a renderer matches against the names the instances were packed
 * with.
 */

#include <string_view>

namespace sigil::material {

/** THE NUMBER @p name IS ASKED FOR BY in a program: a fold of its bytes
 *  into the whole numbers a float holds exactly, and never zero, so zero
 *  can stand for "no attribute". Two names folding to one number are
 *  read as one; a material and a renderer agree because both ask here. */
[[nodiscard]] float instanceAttributeKey(std::string_view name);

}  // namespace sigil::material
