#pragma once

/** @file
 * A node's animation lanes: the fixed rows every description carries a
 * slot for, and the list a host retargets and samples them through.
 *
 * The rows are FIXED and per node, so a lane keeps its meaning across a
 * patch that changed what the node holds — a turn that was ramping goes
 * on ramping when the geometry slot's value type changes underneath it.
 */

#include <sigilcore/reconcile/Lanes.h>
#include <sigilworld/element/Node.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sigil::world {

/** Where a node's motions live. One family: the fixed rows below. */
enum class LaneFamily : uint8_t { Slot };

/** The fixed lane rows, in the order lanesOf() fills them. */
enum Slot : size_t {
  kTranslateX,
  kTranslateY,
  kTranslateZ,
  kRotateX,
  kRotateY,
  kRotateZ,
  kScaleX,
  kScaleY,
  kScaleZ,
  kOriginX,
  kOriginY,
  kOriginZ,
  kAxisDegrees,
  kAlongDistance,
  kWindowHead,
  kWindowSpan,
  kLaneCount,
};

/** One lane of one node — SigilCore's, over this library's family. */
using Lane = core::Lane<LaneFamily>;

/** Fills @p out with @p node's lanes: always `kLaneCount` of them, in
 *  `Slot` order, with a null value on every row this description does
 *  not carry the block for. A caller-owned vector so a per-frame walk
 *  allocates nothing after the first node. */
void lanesOf(const ElementNode& node, std::vector<Lane>& out);

/** What a row's field defaults to — the endpoint a patch ramps from or
 *  to when one side of the diff lacks it. */
float standingValue(Slot slot);

}  // namespace sigil::world
