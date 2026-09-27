#pragma once

/** @file
 * @ingroup motion-clock
 *
 * THE RECONCILER'S SEAM, below a sketch's vocabulary: an
 * `Animatable<float>` while it is MOVING — the held motion an engine runs
 * for it, the value it reads as this frame, whether it is running, the
 * retarget that bends a running ramp onto a new endpoint, and the entrance
 * it plays the first time it appears — and the lanes a retained host
 * retargets through.
 *
 * `Animatable<T>` is the value a description carries; `HeldMotion` is what
 * a consumer that retains state holds beside it while a motion runs.
 * Everything here is stated over one held motion, so a consumer's storage
 * — a fixed array, a vector, one member — is its own business. A lane
 * names no host type: `Family` is the host's own enumeration of its
 * storages.
 */

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "sigilmotion/clock/Engine.h"
#include "sigilmotion/schedule/Stagger.h"
#include "sigilmotion/values/Animatable.h"
#include "sigilmotion/values/Tween.h"

namespace sigil::motion {

/** ONE FLOAT PROPERTY'S MOTION, held by the consumer that retains it: a
 *  live value that is the source of truth while a motion is writing it. */
struct HeldMotion {
  Animatable<float> live = animatable(0.0f);
  bool started = false;
  // Where the running motion is headed — lets a patch that does not change
  // this value's target leave the motion ALONE (no hitch, no re-held delay).
  float target = 0.0f;

  /** A motion is writing the value now. */
  [[nodiscard]] bool isRunning() const { return live.cell()->moving; }
  /** Stops whatever motion is writing the value; the number stays where
   *  the motion left it. */
  void stop();
  /** The value this frame. */
  [[nodiscard]] float value() const { return live.value(); }

  /** The motion writing the value, while there is one — what a blended
   *  retarget adds its change onto. */
  std::shared_ptr<detail::Stepped> running;
};

/** A run of held motions, in declaration order — what a consumer keeps
 *  for a list of animatables whose length is a property of the
 *  description rather than of the consumer. */
using HeldMotions = std::vector<std::unique_ptr<HeldMotion>>;

/** Constant, live, or described — one animatable flattened. */
template <typename T>
struct ResolvedProperty {
  T target{};
  /** The slot, when it holds a live or shaped value: it is already a
   *  running number, so it takes no transition. */
  const Animatable<T>* live = nullptr;
  /** How a change to `target` eases: a described motion's own timing, or
   *  the default for a constant — a tween read for its timing alone, with
   *  every staggered field resolved; nothing where a change snaps. */
  std::optional<Tween<float>> transition;
};

/** A described motion's timing as the transition a change eases by, for
 *  the child at @p place: its duration and delay resolved there, its
 *  curve and its composition, and no endpoints. */
template <typename T>
Tween<float> transitionOf(const Tween<T>& tween, Place place = {}) {
  return {.duration = tween.duration.at(place),
          .delay = tween.delay.at(place),
          .ease = tween.easing(),
          .composition = tween.composition};
}

/** Reads one animatable against a transition the caller supplies as its
 *  default: a constant takes that default, a described motion keeps its
 *  own timing instead — either resolved for the child at @p place — and
 *  a live value takes neither: it is already a running number. */
template <typename T>
ResolvedProperty<T> resolveProperty(const Animatable<T>& property,
                                    const std::optional<Tween<float>>& fallback,
                                    Place place = {}) {
  ResolvedProperty<T> out;
  if (const T* constant = property.constant()) {
    out.target = *constant;
    if (fallback) out.transition = transitionOf(*fallback, place);
  } else if (const Tween<T>* described = property.described()) {
    out.target = described->resolved(place).rest();
    out.transition = transitionOf(*described, place);
  } else {
    out.live = &property;
  }
  return out;
}

/** The value an animatable reads as this frame: a live value wins
 *  (shaped through its stages when it has any), then a running ramp, then
 *  the constant. One body, so every reader agrees. */
float valueOf(const HeldMotion* animated, const Animatable<float>& property);

/** Starts (or retargets) the ramp held in `held` when the target
 *  changed. Returns true if a motion is running. The motion is passed
 *  rather than an index into a store, because how many of these a
 *  consumer keeps and where is the consumer's business — one body,
 *  every storage. @p place is where the value's owner stands among its
 *  siblings, which a staggered tween resolves against. */
bool retarget(Engine& engine, std::unique_ptr<HeldMotion>& held,
                       const Animatable<float>& previousValue,
                       const Animatable<float>& nextValue,
                       const std::optional<Tween<float>>& fallback,
                       Place place = {});

/** An entrance: a tween that names `.from` plays from there — through its
 *  keyframes, or to `.to` — when it FIRST appears: there is no previous
 *  value to diff against, so `from` is the "previous" the author
 *  declared. It repeats as its `loop` and `alternate` say, and every
 *  staggered field resolves against @p place, where the owner stands among
 *  its siblings. A value with no entrance starts nothing. */
void enter(Engine& engine, std::unique_ptr<HeldMotion>& held,
                   const Animatable<float>& property, Place place = {});

/** IS THIS VALUE MOVING RIGHT NOW? A live value always is — the hand that
 *  writes it can stop at any frame and nothing here can see when — and a
 *  held ramp is moving while a motion is writing it.
 *
 *  This is the DECLARED half of stillness, and it is the half a
 *  description can answer on its own. What it cannot answer is whether a
 *  moving value is actually changing the number — a wave held at one
 *  phase moves nothing — which is what `settled()` is for. */
bool isRunning(const HeldMotion* animated, const Animatable<float>& property);

/** A SYNTHESIZED 0→1 PROGRESS: hold at 0 for the delay, then ramp to 1
 *  over the transition's duration on its curve.
 *
 *  For the value a host has to interpolate ITSELF because the description
 *  carries no float to point at — a colour crossfade, a shape morph, a
 *  two-image dissolve. The host keeps the endpoints and reads this
 *  progress between them, and because the ramp is authored here rather
 *  than at each such site, the mount and the retarget of one cannot drift
 *  apart. */
void progress(Engine& engine, std::unique_ptr<HeldMotion>& held,
                  const Tween<float>& spec);

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
 *  ramps from or to the lane's standing value. @p place is where the
 *  node stands among its siblings, for a staggered tween. */
template <class Family>
void retargetFixed(Engine& engine,
                   std::span<std::unique_ptr<HeldMotion>> animated,
                   std::span<const Lane<Family>> previous,
                   std::span<const Lane<Family>> next,
                   const std::optional<Tween<float>>& nodeDefault,
                   Place place = {}) {
  for (size_t i = 0; i < next.size(); ++i) {
    if (!previous[i].value && !next[i].value)
      continue;  // neither description carries it: nothing to ramp
    const Animatable<float> standing = next[i].standing;
    retarget(engine, animated[next[i].slot.index],
                      previous[i].value ? *previous[i].value : standing,
                      next[i].value ? *next[i].value : standing, nodeDefault,
                      place);
  }
}

/** A positional family between two descriptions. The lane list is
 *  positional, so a description that changes the SHAPE of the family
 *  drops the running motions rather than carrying them onto endpoints
 *  that now mean something else — the same rule keys enforce for whole
 *  nodes. A family of equal shape retargets lane by lane. */
template <class Family>
void retargetPositional(Engine& engine, HeldMotions& animated,
                    std::span<const Lane<Family>> previous,
                    std::span<const Lane<Family>> next,
                    const std::optional<Tween<float>>& nodeDefault,
                    Place place = {}) {
  if (previous.size() != next.size()) {
    animated.clear();
    animated.resize(next.size());
  } else {
    // The mount sizes this vector — and a host that skips mount entrances
    // leaves it empty, so a patch is the first thing to touch it there.
    // Size it here too rather than indexing an empty vector.
    if (animated.size() != next.size()) animated.resize(next.size());
    for (size_t i = 0; i < next.size(); ++i)
      retarget(engine, animated[i], *previous[i].value, *next[i].value,
                        nodeDefault, place);
  }
}

}  // namespace sigil::motion
