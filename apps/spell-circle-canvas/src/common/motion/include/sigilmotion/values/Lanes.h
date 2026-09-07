#pragma once

/** @file
 * A node's animation lanes — every Animatable<float> a description
 * carries that the host holds a motion for, ADDRESSED by where that
 * motion lives on the node — and the retarget of a patch: bending the
 * running motions of one storage onto the endpoints the next description
 * asks for.
 *
 * A lane names no host type: `Family` is the host's own enumeration of
 * its storages, and everything else here is an animatable, a held motion
 * and a ticker. That is why it sits with the values it retargets rather
 * than with the reconciler that calls it.
 */

#include <cstddef>
#include <memory>
#include <optional>
#include <span>

#include "sigilmotion/clock/Ticker.h"
#include "sigilmotion/values/Animatable.h"
#include "sigilmotion/values/Animated.h"
#include "sigilmotion/values/Transition.h"

namespace sigil::motion {

/** Where a lane's motion is held on the node. `Family` is the host's
 *  enumeration of its storages: one fixed slot array whose rows are a
 *  property of the host, and any number of positional families whose
 *  length is a property of the description. */
template <class Family>
struct LaneSlot {
  Family family;
  size_t index;  ///< a slot row for the fixed family, a position otherwise
};

/** One transitionable float on a node: what the description asks for, and
 *  which of the host's storages holds the motion that serves it. A host
 *  fills a list of these grouped by family, so `familyLanes` can hand back
 *  one family's run as a contiguous span. */
template <class Family>
struct Lane {
  /** The description's animatable, or nullptr on a fixed-slot lane whose
   *  node does not carry the block that holds it. A positional lane
   *  always has one. */
  const Animatable<float>* value;
  LaneSlot<Family> slot;
  /** The endpoint a patch ramps from or to when `value` is null on one
   *  side of the diff — the field's own default. Meaningful for
   *  fixed-slot lanes only. */
  float standing;
};

/** The contiguous run of @p family's lanes in a list a host filled. */
template <class Family>
std::span<const Lane<Family>> familyLanes(std::span<const Lane<Family>> lanes,
                                          Family family) {
  size_t begin = 0;
  while (begin < lanes.size() && lanes[begin].slot.family != family) ++begin;
  size_t end = begin;
  while (end < lanes.size() && lanes[end].slot.family == family) ++end;
  return lanes.subspan(begin, end - begin);
}

/** The fixed slots between two descriptions: every row both lists carry
 *  is retargeted; a row neither carries is skipped; a row one side lacks
 *  ramps from or to the lane's standing value. */
template <class Family>
void retargetSlots(Ticker& ticker,
                   std::span<std::unique_ptr<AnimatedFloat>> anims,
                   std::span<const Lane<Family>> prev,
                   std::span<const Lane<Family>> next,
                   const std::optional<Transition>& nodeDefault) {
  for (size_t i = 0; i < next.size(); ++i) {
    if (!prev[i].value && !next[i].value)
      continue;  // neither description carries it: nothing to ramp
    const Animatable<float> standing = next[i].standing;
    transitionFloatAt(ticker, anims[next[i].slot.index],
                      prev[i].value ? *prev[i].value : standing,
                      next[i].value ? *next[i].value : standing, nodeDefault);
  }
}

/** A positional family between two descriptions. The lane list is
 *  positional, so a description that changes the SHAPE of the family
 *  drops the running motions rather than carrying them onto endpoints
 *  that now mean something else — the same rule keys enforce for whole
 *  nodes. A family of equal shape retargets lane by lane. */
template <class Family>
void retargetFamily(Ticker& ticker, AnimatedFloats& anims,
                    std::span<const Lane<Family>> prev,
                    std::span<const Lane<Family>> next,
                    const std::optional<Transition>& nodeDefault) {
  if (prev.size() != next.size()) {
    anims.clear();
    anims.resize(next.size());
  } else {
    // The mount sizes this vector — and a host that skips mount entrances
    // leaves it empty, so a patch is the first thing to touch it there.
    // Size it here too rather than indexing an empty vector.
    if (anims.size() != next.size()) anims.resize(next.size());
    for (size_t i = 0; i < next.size(); ++i)
      transitionFloatAt(ticker, anims[i], *prev[i].value, *next[i].value,
                        nodeDefault);
  }
}

}  // namespace sigil::motion
