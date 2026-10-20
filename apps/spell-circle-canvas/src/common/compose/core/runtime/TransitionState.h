#pragma once

/** @file
 * Internal to the kernel — what a node holds while its values move: the
 * fixed property slots and the motion each one transitions through, the
 * positional families a description decides the count of (span endpoints,
 * mask gates, textFx() tracks), the fill's path and the ink lane, and
 * whether the node is leaving.
 */

#include <sigilmaterial/color/Color.h>
#include <sigilmotion/advanced/Held.h>

#include <memory>
#include <optional>
#include <vector>

#include "FillPath.h"
#include "description/ComposeInternal.h"

namespace sigil::compose::detail {

/** One float property that can transition: the Choreograph output is the
 *  source of truth while a motion is connected. SigilMotion's, because a
 *  moving animatable is the motion library's business. */
using HeldMotion = motion::HeldMotion;

/** THE TRANSITION STATE OF A NODE: one part of the retained node, read off
 *  the instance by each field's own name, so `Instance::Slot` and its
 *  enumerators are spelled through the instance. */
// fields are grouped by what they belong to, not by size
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
struct TransitionState {
  // Transition state, keyed by property slot
  // The FIXED property slots — one per property every node can carry, so the
  // count is a property of the KERNEL. Mask gates and textFx() tracks are
  // deliberately not here: how many a node has is a property of its
  // description, so they live in the maskAnims and trackAnims vectors
  // instead.
  //
  // ADDING A SLOT HERE IS A BUILD FAILURE until it also gets a row in
  // kSlotSpecs, the one table every consumer of this enum walks.
  enum Slot : int {
    kOpacity,
    kTx,
    kTy,
    kRotate,
    kScale,
    kFillLerp,
    kSkewX,
    kSkewY,
    kScaleX,
    kScaleY,
    kMotionT,
    kTextPathAt,
    // The depth lanes (DepthData): the plane's turn about its two in-plane
    // axes, its depth, its depth scale, and the view it declares for its
    // children.
    kRotateX,
    kRotateY,
    kTranslateZ,
    kScaleZ,
    kPerspective,
    // The ink easing from one colour to another under this node's
    // transition, whether the node states the colour or inherits it; the
    // pass reads the ramp back into the colour of `font` each frame, so
    // everything under the node with no transition of its own follows it.
    kInkLerp,
    kSlots
  };
  std::unique_ptr<HeldMotion> anims[kSlots];
  /** Where this node stood among the siblings that MOUNTED WITH IT — the
   *  place every staggered tween on it resolves against, set when it
   *  mounts. Unlike `place`, which a selector reads, it counts only the
   *  children new in that patch, so one row appended to a live list enters
   *  at once rather than waiting out its whole-list step. */
  motion::Place mountPlace;
  /** WHETHER THIS NODE IS LEAVING: a keyed node stating `exit()` that the
   *  last description of its parent left out, kept in its place while its
   *  entrances play backwards. A leaving node answers no hit, and nothing
   *  under it is retargeted. */
  bool leaving = false;
  /** The values the exit's motions write, one per entrance under the node
   *  played backwards: the node is dropped once none of them is moving. */
  std::vector<motion::Animatable<float>> exitRuns;
  /** Whether a leaving node's exit has landed: none of its motions is still
   *  writing its value. */
  [[nodiscard]] bool exitLanded() const {
    for (const motion::Animatable<float>& run : exitRuns)
      if (run.cell()->moving) return false;
    return true;
  }
  // The path kFillLerp's progress runs along: two stops for a change
  // eased by the fill's own curve, one per keyframe after the start for
  // a path.
  std::vector<FillStop> fillPath;
  material::Color inkFrom{0, 0, 0, 1};  // where kInkLerp started
  // THE INK TARGET IN FORCE, as the cascade pass last resolved it: the
  // colour kInkLerp is headed for, and the one a freshly resolved colour
  // is compared against to decide the lane moves. It is the RESOLVED
  // colour and not the declared one, so a change arriving through a
  // class, a matched rule or a custom property is a change here too; for
  // a node that inherits its ink it is the parent's target, never the
  // colour a running ramp stands at. Unset until the pass has resolved
  // this node once, and wherever it resolves no colour at all — a lane
  // with no previous target has nothing to ease from.
  std::optional<material::Color> inkTarget;
  // THE TRANSITION A MATCHED RULE STATES for this node — the strongest of
  // the rules that matched it at the last cascade pass and state one —
  // which the node's own `transition()` stands over.
  std::optional<motion::Tween<float>> ruleTransition;
  // Animated span endpoints (THREE per term — begin, end, offset — per
  // pass, in declaration order). A vector rather than the fixed Slot array
  // because the count is a property of the description, not of the kernel.
  std::vector<std::unique_ptr<HeldMotion>> spanAnims;
  // Animated MASK GATE scalars, in declaration order: three per Spans term
  // (begin, end, offset), one per Edge fraction, none for Shape or Alpha.
  //
  // SEPARATELY INDEXED PER MASK, and that is the whole point rather than an
  // implementation detail. Three masks on one node may carry three motions
  // at three rates; if they shared a slot the second would retarget the
  // first and a composition the designer asked for by name would be a race.
  // Positional like spanAnims: a description that changes the SHAPE of its
  // mask list drops the running motions rather than carrying them onto
  // numbers that now mean something else.
  std::vector<std::unique_ptr<HeldMotion>> maskAnims;
  // Animated textFx() TRACK progresses, one per track in declaration order.
  //
  // Positional and separately indexed for the same reason mask gates are:
  // three tracks on one text node may run at three rates, and a shared slot
  // would make the third retarget the first. A description that changes the
  // NUMBER of tracks drops the running motions rather than carrying them
  // onto a progress that now drives a different effect.
  std::vector<std::unique_ptr<HeldMotion>> trackAnims;
  /** WHEN THIS LEAF MOUNTED, on the engine's clock, while a track's
   *  progress waits for its span: a progress that runs over the schedule's
   *  span (`Track::progressOverSpan`) holds at its start until the text is
   *  laid out and the span is known, and then runs as though it had started
   *  at the mount. Empty once every such progress has started. */
  std::optional<motion::Duration> trackSpansOwedSince;
};

}  // namespace sigil::compose::detail
