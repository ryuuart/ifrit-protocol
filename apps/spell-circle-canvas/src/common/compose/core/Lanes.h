#pragma once

/** @file
 * Internal to the kernel — a node's animation lanes: every Animatable<float>
 * a description carries that the kernel holds a motion for, addressed by
 * where that motion lives on the Instance. The fixed slot rows come first,
 * one per Instance::Slot whether or not this node carries the field; the
 * positional families — span endpoints, mask gates, fx() tracks — follow
 * in declaration order, and how many of each a node has is a property of
 * its description.
 */

#include "Instance.h"

namespace sigil::compose::detail {

/** Where a lane's motion is held on the Instance. */
enum class LaneFamily : uint8_t {
  Slot,   ///< the fixed array `Instance::anims`, indexed by Instance::Slot
  Span,   ///< `Instance::spanAnims`: three per Spans term of a stroke pass
  Gate,   ///< `Instance::maskAnims`: a mask gate's Spans terms or Edge fraction
  Track,  ///< `Instance::trackAnims`: one per fx() track's progress
};

struct LaneSlot {
  LaneFamily family;
  size_t index;  ///< an Instance::Slot for Slot, a position otherwise
};

struct Lane {
  /** The description's animatable, or nullptr on a Slot lane whose node does
   *  not carry the block that holds it (a node with no `travel()` has no
   *  `t`). A positional lane always has one. */
  const Animatable<float>* value;
  LaneSlot slot;
  /** The endpoint a patch ramps from or to when `value` is null on one side
   *  of the diff — the field's own default. Meaningful for Slot lanes only. */
  float standing;
};

}  // namespace sigil::compose::detail
