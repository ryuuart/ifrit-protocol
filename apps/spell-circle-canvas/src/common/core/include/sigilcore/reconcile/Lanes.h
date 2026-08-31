#pragma once

/** @file
 * A node's animation lanes — every Animatable<float> a description carries
 * that the host holds a motion for, addressed by where that motion lives
 * on the node — and the two things done with them: retargeting a running
 * ramp when a patch moves a lane's plain target, and starting a mount
 * entrance where a description declares one.
 */

#include <sigilmotion/clock/Ticker.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilmotion/values/Transition.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace sigil::core {

/** One float property that can transition: the Choreograph output is the
 *  source of truth while a motion is connected. */
struct AnimatedFloat {
  choreograph::Output<float> value{0.0f};
  bool started = false;
  // Where the running motion is headed — lets a patch that does not change
  // this slot's target leave the motion ALONE (no hitch, no re-held delay).
  float target = 0.0f;
};

/** The motions of one positional family on a node, in declaration order. */
using AnimatedFloats = std::vector<std::unique_ptr<AnimatedFloat>>;

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
  const motion::Animatable<float>* value;
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

/** Constant, binding, or transitioned — flattened for the reconciler. */
template <typename T>
struct ResolvedProp {
  T target{};
  const choreograph::Output<T>* binding = nullptr;
  const motion::Transition* transition = nullptr;  // the slot's or the node's
};

/** Reads one animatable against the transition the node declares for its
 *  whole self: a plain value takes that default, a transitioned value keeps
 *  its own spec instead, and a binding takes neither — it is already a
 *  running curve. */
template <typename T>
ResolvedProp<T> resolveProp(
    const motion::Animatable<T>& v,
    const std::optional<motion::Transition>& nodeDefault) {
  ResolvedProp<T> out;
  if (const T* plain = v.plain()) {
    out.target = *plain;
    if (nodeDefault) out.transition = &*nodeDefault;
  } else if (const motion::Transitioned<T>* tr = v.transitioned()) {
    out.target = tr->value;
    out.transition = &tr->spec;
  } else {
    out.binding = v.binding();
  }
  return out;
}

/** The value a lane resolves to this frame: a bound Output wins (shaped
 *  through its map when it has one), then a running ramp, then the plain
 *  value. One body, so every reader of a lane agrees. */
float resolveFloatAt(const AnimatedFloat* anim,
                     const motion::Animatable<float>& v);

/** Starts (or retargets) a ramp held in `slotAnim` when the plain target
 *  changed. Returns true if a motion is running. The slot is passed as
 *  the HELD MOTION rather than as an index because a positional family's
 *  count is a property of the description, not of the host — one body,
 *  two storages (the fixed array and the family vectors). */
bool transitionFloatAt(motion::Ticker& ticker,
                       std::unique_ptr<AnimatedFloat>& slotAnim,
                       const motion::Animatable<float>& prevValue,
                       const motion::Animatable<float>& nextValue,
                       const std::optional<motion::Transition>& nodeDefault);

/** The fixed slots between two descriptions: every row both lists carry
 *  is retargeted; a row neither carries is skipped; a row one side lacks
 *  ramps from or to the lane's standing value. */
template <class Family>
void retargetSlots(motion::Ticker& ticker,
                   std::span<std::unique_ptr<AnimatedFloat>> anims,
                   std::span<const Lane<Family>> prev,
                   std::span<const Lane<Family>> next,
                   const std::optional<motion::Transition>& nodeDefault) {
  for (size_t i = 0; i < next.size(); ++i) {
    if (!prev[i].value && !next[i].value)
      continue;  // neither description carries it: nothing to ramp
    const motion::Animatable<float> standing = next[i].standing;
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
void retargetFamily(motion::Ticker& ticker, AnimatedFloats& anims,
                    std::span<const Lane<Family>> prev,
                    std::span<const Lane<Family>> next,
                    const std::optional<motion::Transition>& nodeDefault) {
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

/** A mount entrance: an animate(from(a).to(b)) value plays `from → value`
 *  when the node FIRST appears — there is no prev to diff against, so
 *  this is the "prev" the author declared — and a waypoint list plays
 *  its segments in turn. `extraDelaySeconds` is what the host adds
 *  before the declared delay (a staggered mount). A value with no
 *  entrance starts nothing. */
void mountEntrance(motion::Ticker& ticker,
                   std::unique_ptr<AnimatedFloat>& slotAnim,
                   const motion::Animatable<float>& v, float extraDelaySeconds);

}  // namespace sigil::core
