#pragma once

/** @file
 * A host with nothing behind it: a tree of nodes that declare volatility,
 * hold a numbered artefact instead of pixels, and count every operation
 * the kernel asks of them. Enough to exercise the proof, the release and
 * the bake seam without a canvas anywhere in the link.
 */

#include <sigilcore/cache/Cache.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace sigil::core::test::cache {

/** The values this host's nodes hold still on: one lane per node, which is
 *  as much structure as the release protocol needs. */
using Lanes = std::vector<float>;

/** One retained node. Its declarations are set by the test, its verdict is
 *  written by the proof, and its artefact is an integer standing in for
 *  whatever a real host would rasterize.
 *
 *  Children are held by pointer so a parent link stays valid while a test
 *  grows the tree. */
struct FakeNode {
  std::string key;
  FakeNode* parent = nullptr;
  std::vector<std::unique_ptr<FakeNode>> children;

  /** What the host declares about this node, before its lane is folded in. */
  NodeVolatility declared;

  /** The lane values the node draws from, and the hold on them. `driven`
   *  is the DECLARATION — a binding is connected — and it stays true while
   *  the lane sits still, which is the whole reason the release exists. */
  bool driven = false;
  Lanes lanes;
  Settle<Lanes> settle;
  bool released = false;  ///< the proof proved it is holding still
  /** What the lane read on the previous drawn frame — this host's stand-in
   *  for "did my artefact stay exact", which is the question the write
   *  side of the hold is asking. */
  Lanes drawnLanes;
  bool drawnSeen = false;

  /** The bake, standing in for a recording or an image: the serial number
   *  of the take that produced it, or -1 for none. */
  int artefact = -1;
  bool stale = true;  ///< a patch landed, or a held value moved

  SubtreeVerdict verdict;

  /** Add a child and answer it, so a test can spell a tree in one
   *  expression. */
  FakeNode& add(std::string key) {
    auto node = std::make_unique<FakeNode>();
    node->key = std::move(key);
    node->parent = this;
    children.push_back(std::move(node));
    return *children.back();
  }

  /** This node and every ancestor must re-take. */
  void staleUp() {
    for (FakeNode* n = this; n; n = n->parent) n->stale = true;
  }
};

/** Every operation the host was asked to perform, since the fake exists to
 *  count them. */
struct Counters {
  int takes = 0;
  int replays = 0;
  int drops = 0;
  int lives = 0;
};

/** The host's one bake tier. Comparable through its counters, so two seam
 *  values built over one host answer equal. */
struct CountingBake : BakeOps<FakeNode> {
  explicit CountingBake(Counters* c) : counters(c) {}
  Counters* counters = nullptr;

  void take(FakeNode& n) const override {
    n.artefact = ++counters->takes;
    n.stale = false;
  }
  void replay(FakeNode&) const override { ++counters->replays; }
  void drop(FakeNode& n) const override {
    n.artefact = -1;
    ++counters->drops;
  }
  [[nodiscard]] bool held(const FakeNode& n) const override {
    return n.artefact >= 0;
  }
  bool operator==(const CountingBake& o) const {
    return counters == o.counters;
  }
};

/** The host itself: it walks its own tree, calls the proof on the way back
 *  up, and runs the bake seam over the answer. */
struct FakeHost {
  Counters counters;
  Bake<FakeNode> bake = CountingBake{&counters};
  /** Consecutive stable frames before a release, small so a test can spell
   *  the whole warmup out. */
  int hold = 3;
  /** Nodes the proof released, for the per-draw movement scan. */
  std::vector<FakeNode*> releasedNodes;
  /** Set when a released node moved or a hold warmed up: the host must
   *  re-run the proof before anything replays. */
  bool proofDirty = false;

  /** THE PROOF, over one subtree. A driven lane is content volatility
   *  unless the hold released it; everything else the node declared passes
   *  through untouched. */
  SubtreeVerdict prove(FakeNode& n) {
    ChildVolatility kids;
    for (auto& c : n.children) kids.add(prove(*c));

    NodeVolatility self = n.declared;
    n.released = false;
    if (n.driven) {
      // Read-side release: it costs a lane read only once the hold has
      // warmed up.
      if (n.settle.release(hold, [&] { return n.lanes; })) {
        n.released = true;
        releasedNodes.push_back(&n);
      } else {
        self.ownContent = true;
      }
    }
    n.verdict = foldSubtree(self, kids);
    return n.verdict;
  }

  /** One frame's proof. */
  void proveTree(FakeNode& root) {
    releasedNodes.clear();
    proofDirty = false;
    prove(root);
  }

  /** THE DRAW, over one subtree: the write side of the hold, then the bake
   *  decision at every node. A node whose subtree is settled bakes and its
   *  children are inside the artefact; everything else paints live and its
   *  children draw themselves. */
  void draw(FakeNode& n) {
    if (n.driven) {
      const bool stable = n.drawnSeen && n.drawnLanes == n.lanes;
      n.drawnLanes = n.lanes;
      n.drawnSeen = true;
      if (n.settle.observe(stable, n.lanes, hold)) proofDirty = true;
    }
    const BakeState state{.cacheable = !n.verdict.subtreeVolatile,
                          .held = n.artefact >= 0,
                          .stale = n.stale};
    const BakeAction action = runBake(bake, n, state);
    if (action == BakeAction::Live) {
      ++counters.lives;
      n.stale = false;
    }
    if (action != BakeAction::Live) return;  // children are inside the bake
    for (auto& c : n.children) draw(*c);
  }

  void drawTree(FakeNode& root) { draw(root); }

  /** THE PER-DRAW SCAN over the released nodes: a lane assigned from
   *  outside re-declares the frame it moves, before anything holding the
   *  old reading replays. */
  void scanReleased() {
    for (FakeNode* n : releasedNodes)
      if (n->settle.moved(n->lanes)) {
        n->staleUp();
        proofDirty = true;
      }
  }
};

}  // namespace sigil::core::test::cache
