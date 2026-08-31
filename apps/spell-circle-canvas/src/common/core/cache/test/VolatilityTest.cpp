/** @file
 * The settled-subtree proof over a fake host: what a still tree promises,
 * what one driven lane costs every node above it, what an opted-out node
 * costs, and the two halves of the hold question.
 */

#include <gtest/gtest.h>

#include "FakeCacheHost.h"

using namespace sigil::core;
using namespace sigil::core::test;

namespace {

/** root → mid → leaf, the shape every question below is asked of. */
struct Chain {
  FakeNode root;
  FakeNode* mid = nullptr;
  FakeNode* leaf = nullptr;
  Chain() {
    root.key = "root";
    mid = &root.add("mid");
    leaf = &mid->add("leaf");
  }
};

TEST(CacheProof, AStillTreeIsSettledEverywhere) {
  FakeHost host;
  Chain t;
  host.proveTree(t.root);
  for (const FakeNode* n : {&t.root, t.mid, t.leaf}) {
    EXPECT_FALSE(n->verdict.subtreeVolatile) << n->key;
    EXPECT_FALSE(n->verdict.volatileAbove) << n->key;
    EXPECT_FALSE(n->verdict.ownContentVolatile) << n->key;
    EXPECT_TRUE(n->verdict.memoSafe) << n->key;
  }
}

TEST(CacheProof, ADrivenLaneDeepInASubtreeUnsettlesEveryAncestor) {
  FakeHost host;
  Chain t;
  t.leaf->driven = true;
  t.leaf->lanes = {1.0f};
  host.proveTree(t.root);
  EXPECT_TRUE(t.leaf->verdict.ownContentVolatile);
  EXPECT_TRUE(t.leaf->verdict.subtreeVolatile);
  EXPECT_TRUE(t.mid->verdict.subtreeVolatile);
  EXPECT_TRUE(t.root.verdict.subtreeVolatile);
  // …and none of them owns the volatility: the ancestors' own content is
  // as static as it was.
  EXPECT_FALSE(t.mid->verdict.ownContentVolatile);
  EXPECT_FALSE(t.root.verdict.ownContentVolatile);
}

TEST(CacheProof, CompositeMotionSparesTheNodeAndBlocksItsAncestors) {
  FakeHost host;
  Chain t;
  t.mid->declared.ownPaint = true;  // an opacity or a transform, nothing else
  host.proveTree(t.root);
  // The node's own artefact survives — it is drawn THROUGH the motion.
  EXPECT_FALSE(t.mid->verdict.subtreeVolatile);
  // …but the ancestor's would contain it.
  EXPECT_TRUE(t.mid->verdict.volatileAbove);
  EXPECT_TRUE(t.root.verdict.subtreeVolatile);
}

TEST(CacheProof, DeclaredVolatilityOptsANodeOutForever) {
  FakeHost host;
  Chain t;
  t.leaf->declared.policy = Cache::Never;
  host.proveTree(t.root);
  EXPECT_TRUE(t.leaf->verdict.ownContentVolatile);
  EXPECT_TRUE(t.root.verdict.subtreeVolatile);
  // …and it is blind to a value memo too, so no ancestor can hold it.
  EXPECT_FALSE(t.leaf->verdict.memoSafe);
  EXPECT_FALSE(t.root.verdict.memoSafe);
}

TEST(CacheProof, AlwaysDoesNotOverruleAVolatileVerdict) {
  FakeHost host;
  Chain t;
  t.leaf->declared.policy = Cache::Always;
  t.leaf->driven = true;
  t.leaf->lanes = {1.0f};
  host.proveTree(t.root);
  EXPECT_TRUE(t.leaf->verdict.subtreeVolatile);
}

TEST(CacheProof, ABackdropReadRefusesToBeInsideAnyBake) {
  FakeHost host;
  Chain t;
  t.leaf->declared.readsBackdrop = true;
  host.proveTree(t.root);
  EXPECT_TRUE(t.leaf->verdict.subtreeReadsBackdrop);
  EXPECT_TRUE(t.root.verdict.subtreeReadsBackdrop);
  EXPECT_FALSE(t.leaf->verdict.memoSafe);
  EXPECT_FALSE(t.root.verdict.memoSafe);
  // It is not volatility: the node's pixels are as static as ever.
  EXPECT_FALSE(t.root.verdict.subtreeVolatile);
}

TEST(CacheProof, AHoldRootMayBlendButMayNotSampleTheDestination) {
  FakeHost host;
  Chain t;
  t.root.declared.holdSubtree = true;
  t.root.declared.readsBackdrop = true;  // a blend, applied outside the bake
  host.proveTree(t.root);
  EXPECT_TRUE(t.root.verdict.holdRootOK);
  EXPECT_FALSE(t.root.verdict.memoSafe);  // …but nobody may hold IT

  t.root.declared.samplesDestination = true;  // applied INSIDE the bake
  host.proveTree(t.root);
  EXPECT_FALSE(t.root.verdict.holdRootOK);
}

TEST(CacheProof, OneMemoBlindDescendantRefusesTheWholeHold) {
  FakeHost host;
  Chain t;
  t.root.declared.holdSubtree = true;
  host.proveTree(t.root);
  ASSERT_TRUE(t.root.verdict.holdRootOK);

  t.leaf->declared.memoOpaque = true;  // moves pixels with no value to compare
  host.proveTree(t.root);
  EXPECT_FALSE(t.root.verdict.holdRootOK);
  // …while a driven lane, which IS a value, does not.
  t.leaf->declared.memoOpaque = false;
  t.leaf->driven = true;
  t.leaf->lanes = {1.0f};
  host.proveTree(t.root);
  EXPECT_TRUE(t.root.verdict.holdRootOK);
  EXPECT_TRUE(t.root.verdict.subtreeVolatile);  // which is what a hold is for
}

TEST(CacheProof, TheFoldIsOrderIndependent) {
  const NodeVolatility self;
  ChildVolatility a;
  ChildVolatility b;
  const SubtreeVerdict moving{.volatileAbove = true};
  const SubtreeVerdict blind{.memoSafe = false};
  const SubtreeVerdict still{.memoSafe = true};
  a.add(moving);
  a.add(blind);
  a.add(still);
  b.add(still);
  b.add(blind);
  b.add(moving);
  const SubtreeVerdict va = foldSubtree(self, a);
  const SubtreeVerdict vb = foldSubtree(self, b);
  EXPECT_EQ(va.subtreeVolatile, vb.subtreeVolatile);
  EXPECT_EQ(va.memoSafe, vb.memoSafe);
}

}  // namespace
