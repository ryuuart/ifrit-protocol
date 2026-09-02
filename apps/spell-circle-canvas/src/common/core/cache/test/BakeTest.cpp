/** @file
 * The bake seam: the three-way decision, the operations it runs through,
 * and the counts that say a settled tree bakes once and replays after.
 */

#include <gtest/gtest.h>

#include "FakeCacheHost.h"

using namespace sigil::core;
using namespace sigil::core::test::cache;

namespace {

TEST(CacheBake, TheDecisionIsThreeAnswersOverThreeFacts) {
  EXPECT_EQ(decideBake({.cacheable = false, .held = true, .stale = false}),
            BakeAction::Live);
  EXPECT_EQ(decideBake({.cacheable = true, .held = false, .stale = false}),
            BakeAction::Take);
  EXPECT_EQ(decideBake({.cacheable = true, .held = true, .stale = true}),
            BakeAction::Take);
  EXPECT_EQ(decideBake({.cacheable = true, .held = true, .stale = false}),
            BakeAction::Replay);
}

TEST(CacheBake, AnEmptySeamDraws) {
  Counters counters;
  FakeNode node;
  const Bake<FakeNode> none;
  EXPECT_EQ(
      runBake(none, node, {.cacheable = true, .held = false, .stale = false}),
      BakeAction::Live);
  EXPECT_EQ(counters.takes, 0);
}

TEST(CacheBake, ASeamValueComparesByItsModel) {
  Counters a;
  Counters b;
  const Bake<FakeNode> one = CountingBake{&a};
  const Bake<FakeNode> same = CountingBake{&a};
  const Bake<FakeNode> other = CountingBake{&b};
  EXPECT_EQ(one, same);
  EXPECT_NE(one, other);
}

TEST(CacheBake, ASettledTreeBakesOnceAndReplaysAfter) {
  FakeHost host;
  FakeNode root;
  root.key = "root";
  root.add("a");
  root.add("b");

  host.proveTree(root);
  host.drawTree(root);
  EXPECT_EQ(host.counters.takes, 1);  // the root's bake contains both children
  EXPECT_EQ(host.counters.replays, 1);

  for (int i = 0; i < 4; ++i) {
    host.proveTree(root);
    host.drawTree(root);
  }
  EXPECT_EQ(host.counters.takes, 1);
  EXPECT_EQ(host.counters.replays, 5);
  EXPECT_EQ(host.counters.lives, 0);
}

TEST(CacheBake, AnInvalidationRebakesExactlyOnce) {
  FakeHost host;
  FakeNode root;
  root.key = "root";
  FakeNode& leaf = root.add("leaf");

  for (int i = 0; i < 3; ++i) {
    host.proveTree(root);
    host.drawTree(root);
  }
  ASSERT_EQ(host.counters.takes, 1);

  leaf.staleUp();  // a patch landed on the leaf
  for (int i = 0; i < 3; ++i) {
    host.proveTree(root);
    host.drawTree(root);
  }
  EXPECT_EQ(host.counters.takes, 2);
  EXPECT_EQ(host.counters.replays, 6);
}

TEST(CacheBake, AVolatileNodeDropsWhateverItHeld) {
  FakeHost host;
  FakeNode root;
  root.key = "root";
  FakeNode& leaf = root.add("leaf");

  host.proveTree(root);
  host.drawTree(root);
  ASSERT_EQ(host.counters.takes, 1);
  ASSERT_GE(root.artefact, 0);

  // The leaf starts moving: nothing above it may hold pixels any more.
  leaf.declared.policy = Cache::Never;
  host.proveTree(root);
  host.drawTree(root);
  EXPECT_EQ(host.counters.drops, 1);
  EXPECT_LT(root.artefact, 0);
  EXPECT_EQ(host.counters.lives, 2);  // root and leaf both painted
}

}  // namespace
