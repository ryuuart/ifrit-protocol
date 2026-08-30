#pragma once

/** @file
 * The bake seam: the decision — bake now, replay, or paint live — is the
 * kernel's, and the artefact it decides about is the host's.
 */

#include <sigilcore/reconcile/Erased.h>

#include <cstdint>

namespace sigil::core {

/** THE OPERATIONS A HOST IMPLEMENTS OVER ONE KIND OF BAKE.
 *
 *  `Target` is whatever the host bakes — its retained node, a node paired
 *  with the canvas it is drawing into, a recorder. The kernel never looks
 *  inside it and never stores one.
 *
 *  A host with several tiers of bake — a recorded command list, a
 *  rasterized image, a whole subtree composited into one layer — writes
 *  one model per tier. They cost different amounts and go stale under
 *  different rules, which is exactly why the artefact stays on the host's
 *  side of the seam: the kernel's three-way answer is the same for all of
 *  them. */
template <typename Target>
struct BakeOps {
  BakeOps() = default;
  BakeOps(const BakeOps&) = default;
  BakeOps(BakeOps&&) = default;
  BakeOps& operator=(const BakeOps&) = default;
  BakeOps& operator=(BakeOps&&) = default;
  virtual ~BakeOps() = default;

  /** Turn the settled subtree at @p target into a replayable artefact,
   *  replacing whatever it held. */
  virtual void take(Target& target) const = 0;
  /** Draw the artefact `take` produced. Called only when `held` answers
   *  true. */
  virtual void replay(Target& target) const = 0;
  /** Drop the artefact. Called when the subtree stopped being cacheable,
   *  so the host cannot reach a stale one by any other path. */
  virtual void drop(Target& target) const = 0;
  /** Is an artefact in hand? */
  [[nodiscard]] virtual bool held(const Target& target) const = 0;
};

/** A bake seam value: a host's operations, carried and compared like any
 *  other value on a description. */
template <typename Target>
using Bake = Erased<BakeOps<Target>>;

/** WHAT TO DO WITH ONE NODE'S BAKE THIS FRAME. */
enum class BakeAction : uint8_t {
  /** Paint it. No artefact may be held afterwards — a cached one that
   *  stays reachable is how a fall-through path blits last frame's pixels
   *  on the frame the proof just said not to. */
  Live,
  /** Bake now, then draw the result. Covers both the first bake and every
   *  re-bake; they are the same act, and a host that told them apart would
   *  be keeping a second staleness rule that could disagree with this
   *  one. */
  Take,
  /** The artefact in hand is exact. Draw it. */
  Replay,
};

/** WHAT THE HOST KNOWS ABOUT ONE NODE'S ARTEFACT beside its verdict. */
struct BakeState {
  /** May this node hold an artefact at all? The proof's verdict, plus
   *  whatever the host's own tier adds — a cost floor, a size cap, a
   *  value memo that looked past a volatile verdict. */
  bool cacheable = false;
  /** Is one in hand? */
  bool held = false;
  /** …and has anything invalidated it — a patch, a memo whose values
   *  moved, a bake taken against a different size? */
  bool stale = false;
};

/** THE DECISION. Three inputs, three answers, no state: which is the
 *  point. Every tier of every host asks this same question, and a tier
 *  that answered it locally would be a second copy of the rule that can
 *  drift from this one. */
[[nodiscard]] constexpr BakeAction decideBake(const BakeState& state) {
  if (!state.cacheable) return BakeAction::Live;
  return state.held && !state.stale ? BakeAction::Replay : BakeAction::Take;
}

/** The decision, run through a host's operations: takes the bake when one
 *  is due, drops a held artefact the node may no longer keep, and draws
 *  whichever way the answer says. @return what it did.
 *
 *  An empty @p ops means the host offers no bake at this tier, which is
 *  `Live` — a seam with nothing behind it draws rather than refusing. */
template <typename Target>
BakeAction runBake(const Bake<Target>& ops, Target& target,
                   const BakeState& state) {
  if (!ops) return BakeAction::Live;
  const BakeAction action = decideBake(state);
  switch (action) {
    case BakeAction::Live:
      if (ops->held(target)) ops->drop(target);
      break;
    case BakeAction::Take:
      ops->take(target);
      ops->replay(target);
      break;
    case BakeAction::Replay:
      ops->replay(target);
      break;
  }
  return action;
}

}  // namespace sigil::core
