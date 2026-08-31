#pragma once

/** @file
 * The settled-subtree proof: what a host reports about one node, how a
 * subtree's children fold into it, and what the answer promises.
 */

#include <sigilcore/cache/Policy.h>

namespace sigil::core {

/** WHAT A HOST REPORTS ABOUT ONE NODE, before its children are folded in.
 *
 *  Every field is a DECLARATION read off the node this frame — never a
 *  difference between two frames. That is the contract's sharp edge: a
 *  host that reports a term one frame late has already replayed a stale
 *  artefact, while a host that reports one early pays a re-bake and
 *  nothing else. When in doubt, declare.
 *
 *  The split between the four volatility terms is not a taxonomy for its
 *  own sake — each one is asked a different question by a different
 *  consumer, and collapsing any two of them loses a bake somebody wants. */
struct NodeVolatility {
  /** What the author asked for. `Cache::Never` forces every volatility
   *  answer below to true; a host may also carry the same fact in its own
   *  terms, and the two agree. */
  Cache policy = Cache::Auto;
  /** The author asked for this node's WHOLE SUBTREE to be held by a value
   *  memo instead of by this proof. It is the one request that looks past
   *  a permanently-volatile verdict: a subtree of pieces each carrying an
   *  entrance that runs once and then holds is volatile forever by
   *  declaration and unchanging in fact for all but a few frames of its
   *  life. The proof answers only whether such a hold is SOUND
   *  (`holdRootOK`); whether the values actually held still this frame is
   *  the host's comparison, not this one. */
  bool holdSubtree = false;
  /** Volatility that changes how the node COMPOSITES without changing what
   *  it draws — its opacity, its transform. The node's own artefact still
   *  replays: it is drawn through the moving composite rather than
   *  containing it. An ancestor's artefact may not span the node, because
   *  the ancestor's would contain it. */
  bool ownPaint = false;
  /** Volatility that rebuilds what the node draws. Blocks the node's own
   *  artefact and every ancestor's.
   *
   *  A host that runs a value memo reports the RELEASED reading here — a
   *  term whose inputs it has proven are holding still is not reported. */
  bool ownContent = false;
  /** Volatility NO value comparison can see: a shader reading the clock, a
   *  decoded frame arriving off the host's own schedule, a program the
   *  host cannot resolve outside drawing.
   *
   *  This is the term `holdSubtree` turns on, and the one place to be
   *  conservative. A held subtree is kept by comparing values; something
   *  inside it that moves pixels with no value to compare would blit one
   *  frame's picture indefinitely, so it refuses the hold outright rather
   *  than approximating it. Every host term that is opaque in that sense
   *  belongs here whether or not it is also in `ownContent`. */
  bool memoOpaque = false;
  /** The node composites against what is ALREADY on the canvas — a blend
   *  that is not source-over, a filter that samples the destination. A
   *  bake is a transparent layer, so such a node resolves against
   *  transparent black inside one and the pixels differ. It therefore
   *  refuses to be INSIDE any bake, its own included. */
  bool readsBackdrop = false;
  /** …and the half of that which refuses to be the bake's ROOT as well. A
   *  root's blend and opacity are applied OUTSIDE its bake, exactly as
   *  they would be applied outside a live paint, so they cost the root
   *  nothing; a filter that samples the destination is applied INSIDE it
   *  and would sample the bake instead of the canvas.
   *
   *  Implies `readsBackdrop`. */
  bool samplesDestination = false;
};

/** ONE SUBTREE'S VERDICT — what the proof promises about the node it was
 *  asked about, once its children are in.
 *
 *  The promise is one-sided and that is what makes it usable: a subtree
 *  the proof calls settled has no way to change its pixels without the
 *  host being told first, either by a description change or by one of the
 *  declarations above turning true. A subtree the proof calls volatile may
 *  well be standing perfectly still — proving otherwise is the value
 *  memo's job, not this one's. */
struct SubtreeVerdict {
  /** This node's own content changes. The half of `subtreeVolatile` that
   *  is about the node itself: the two differ exactly when the children
   *  are what makes the node uncacheable, which is the whole premise of a
   *  bake that replaces the node's own paint and draws live children over
   *  the result. */
  bool ownContentVolatile = false;
  /** This node's content changes, or anything below it does. Blocks every
   *  artefact that would contain the subtree. */
  bool subtreeVolatile = false;
  /** What an ANCESTOR folds in: `subtreeVolatile`, plus the node's own
   *  composite motion, which an ancestor's artefact would freeze. */
  bool volatileAbove = false;
  /** This node composites against the canvas, or something below it does.
   *  Whole-subtree bakes ask this; a bake of the node's own paint alone
   *  asks `NodeVolatility::readsBackdrop`, because children drawn over the
   *  blit composite against it exactly as they would against freshly
   *  rasterized pixels. */
  bool subtreeReadsBackdrop = false;
  /** May a value memo hold this subtree at all — is everything in it
   *  either a value the host can read back and compare, or a change that
   *  reaches the host as a description edit? This is what a PARENT asks of
   *  a candidate child. */
  bool memoSafe = false;
  /** …and may THIS node be the root of such a hold: it asked for one, its
   *  own paint is memo-visible, every child is `memoSafe`, and it does not
   *  sample the destination. Deliberately does not consult the node's own
   *  blend or opacity — those apply outside the bake. */
  bool holdRootOK = false;
};

/** WHAT A NODE'S CHILDREN CONTRIBUTE, folded in the host's own child walk.
 *
 *  Three accumulators rather than a list, because a host recurses into its
 *  children for reasons of its own — threading down what an ancestor's
 *  motion means for each of them — and should not have to materialize a
 *  vector of verdicts just to hand them here. */
struct ChildVolatility {
  bool anyVolatile = false;
  bool anyReadsBackdrop = false;
  bool allMemoSafe = true;

  /** Fold one child's verdict in. Order-independent, so a host may walk
   *  its children in whatever order it draws them. */
  constexpr void add(const SubtreeVerdict& child) {
    anyVolatile |= child.volatileAbove;
    anyReadsBackdrop |= child.subtreeReadsBackdrop;
    allMemoSafe &= child.memoSafe;
  }
};

/** THE PROOF. Given what a host declares about one node and what its
 *  children answered, decide what the subtree rooted there promises.
 *
 *  Pure, cheap and total: no allocation, no state, no traversal of its
 *  own. A host walks its tree once, calls this at each node on the way
 *  back up, and folds the answer into its parent's `ChildVolatility`. */
[[nodiscard]] SubtreeVerdict foldSubtree(const NodeVolatility& self,
                                         const ChildVolatility& children);

}  // namespace sigil::core
