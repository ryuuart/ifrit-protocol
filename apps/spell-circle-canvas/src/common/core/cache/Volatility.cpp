/** @file
 * The settled-subtree proof: one node's declarations and its children's
 * verdicts folded into what the subtree promises.
 */

#include <sigilcore/cache/Volatility.h>

namespace sigil::core {

SubtreeVerdict foldSubtree(const NodeVolatility& self,
                           const ChildVolatility& children) {
  // An opted-out node is volatile by fiat and blind to every memo. A host
  // that carries the same fact in its own terms agrees with this line
  // rather than duplicating it; a host that forgets is still safe.
  const bool never = self.policy == Cache::Never;

  SubtreeVerdict v;
  v.ownContentVolatile = self.ownContent || never;
  // Children paint INSIDE anything this node bakes, transforms included,
  // so a child that moves at all blocks the node's artefact — which is why
  // what folds up is `volatileAbove` and not `subtreeVolatile`.
  v.subtreeVolatile = v.ownContentVolatile || children.anyVolatile;
  // …and what this node hands ITS parent adds its own composite motion.
  // The node's artefact survives that motion (it is drawn through it); an
  // ancestor's would contain it, and would freeze it.
  v.volatileAbove = self.ownPaint || v.subtreeVolatile;
  v.subtreeReadsBackdrop = self.readsBackdrop || children.anyReadsBackdrop;

  // The two halves of the hold question, which are NOT the same predicate.
  //
  // `memoSafe` is what a parent asks of this subtree, and it includes the
  // node's own backdrop read: inside a hold, a node that blends with the
  // canvas resolves against the layer's transparent black.
  //
  // `holdRootOK` is what the node asks of ITSELF, and deliberately does
  // not consult its own blend or opacity — a root's composite is applied
  // outside its bake, exactly as it would be applied outside a live paint.
  // A filter that samples the destination is still fatal there, because
  // that one is applied inside.
  const bool memoBlind = self.memoOpaque || never;
  v.memoSafe = !memoBlind && !self.readsBackdrop && children.allMemoSafe;
  v.holdRootOK = self.holdSubtree && !memoBlind && children.allMemoSafe &&
                 !self.samplesDestination;
  return v;
}

}  // namespace sigil::core
