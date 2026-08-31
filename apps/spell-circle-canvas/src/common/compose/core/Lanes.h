#pragma once

/** @file
 * Internal to the kernel — a node's animation lanes: every Animatable<float>
 * a description carries that the kernel holds a motion for, addressed by
 * where that motion lives on the Instance. The fixed slot rows come first,
 * one per Instance::Slot whether or not this node carries the field; the
 * positional families — span endpoints, mask gates, fx() tracks — follow
 * in declaration order, and how many of each a node has is a property of
 * its description. The lane itself, and everything done with one, is
 * SigilCore's; this names the families.
 */

#include <sigilcore/reconcile/Lanes.h>

#include "Instance.h"

namespace sigil::compose::detail {

/** Where a lane's motion is held on the Instance. */
enum class LaneFamily : uint8_t {
  Slot,   ///< the fixed array `Instance::anims`, indexed by Instance::Slot
  Span,   ///< `Instance::spanAnims`: three per Spans term of a stroke pass
  Gate,   ///< `Instance::maskAnims`: a mask gate's Spans terms or Edge fraction
  Track,  ///< `Instance::trackAnims`: one per fx() track's progress
};

using LaneSlot = core::LaneSlot<LaneFamily>;
using Lane = core::Lane<LaneFamily>;

}  // namespace sigil::compose::detail
