#pragma once

/** @file
 * Internal to the kernel — the state of the volatility walk: whether it
 * must run again, what the root's verdict was, the nodes whose scalar
 * volatility it released, what it threads down to a child about the planes
 * above it, and the scratch the subtree value memo compares into.
 */

#include <vector>

namespace sigil::compose::detail {

struct Instance;

/** THE VOLATILITY WALK'S STATE: one part of the composer's retained state,
 *  read off it by each field's own name. */
// fields are grouped by what they belong to, not by size
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
struct VolatilityState {
  bool volatileDirty = true;  // recompute needed (render or animation)
  // Whether a motion ran at the last settle, so the frame it lands on
  // walks once more.
  bool engineWasRunning = false;
  // The root verdict's volatileAbove bit: unlike Instance::subtreeVolatile,
  // this includes the root's own opacity and transform, which can change the
  // composited pixels without invalidating any content cache below it.
  bool rootVolatile = false;
  // Instances whose scalar volatility is RELEASED (settled bound gates,
  // glyph progress and the other memoized scalar lanes). Rebuilt by every
  // computeVolatile walk, and scanned once per draw so an EXTERNALLY-driven
  // output that starts moving again re-declares volatility the same frame:
  // the walk itself only re-runs on reconcile or while the engine is
  // active, so without this scan a released node driven from outside the
  // library would never notice it had resumed. Guarded by !volatileDirty —
  // a pending recompute means the tree changed and these pointers may be
  // stale.
  std::vector<Instance*> releasedScalars;
  /** What the walk threads down to a child about the planes above it:
   *  whether a bound or transitioning transform is connected on some
   *  ancestor, whether the shared space it stands in is moving (its
   *  host's transform, or the host's own space), whether the view its
   *  parent declares is live, and whether it stands in a space at all — a
   *  node whose projection moves for any of those reasons is moving
   *  exactly as one whose own lane is.
   *
   *  A node carrying a world-space material under a moving ancestor has
   *  its node→root matrix changing off the describe clock, which is
   *  CONTENT volatility for that node — and it joins the memoized scalar
   *  lane, because that matrix is six floats, so the recording survives
   *  between ticks and the flag releases when the motion settles. */
  struct Above {
    bool moving = false;           ///< a connected transform on an ancestor
    bool spaceMoving = false;      ///< the space this node stands in moves
    bool perspectiveLive = false;  ///< the parent's perspective lane is live
    bool inSpace = false;          ///< the parent hosts a shared space
  };
  // Scratch for the subtree value memo, swapped with the group root's
  // `groupPrev` each frame so a settled group allocates nothing at all.
  std::vector<float> groupScratch;
};

}  // namespace sigil::compose::detail
