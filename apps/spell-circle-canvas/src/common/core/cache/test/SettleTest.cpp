/** @file
 * The stability release, all three sides: the count that warms up where a
 * host draws, the release the proof performs, and the scan that
 * re-declares the frame a value moves from outside.
 */

#include <gtest/gtest.h>

#include <vector>

#include "FakeCacheHost.h"

using namespace sigil::core;
using namespace sigil::core::test;

namespace {

constexpr int kHold = 3;

TEST(CacheSettle, AWarmedUpHoldReleasesAndAMovingOneNeverDoes) {
  Settle<std::vector<float>> settle;
  std::vector<float> still{1.0f};
  int reads = 0;
  const auto read = [&] {
    ++reads;
    return still;
  };

  // Below the bar the release does not even read the values.
  for (int i = 0; i < kHold; ++i) {
    EXPECT_FALSE(settle.release(kHold, read));
    EXPECT_EQ(reads, 0);
    const bool crossed = settle.observe(true, still, kHold);
    EXPECT_EQ(crossed, i == kHold - 1);
  }
  EXPECT_TRUE(settle.release(kHold, read));
  EXPECT_EQ(reads, 1);
  // The crossing is announced ONCE, however long the hold goes on.
  EXPECT_FALSE(settle.observe(true, still, kHold));
}

TEST(CacheSettle, AnyInstabilityRestartsTheWarmup) {
  Settle<std::vector<float>> settle;
  std::vector<float> v{1.0f};
  for (int i = 0; i < kHold - 1; ++i) settle.observe(true, v, kHold);
  EXPECT_EQ(settle.frames(), kHold - 1);
  settle.observe(false, v, kHold);
  EXPECT_EQ(settle.frames(), 0);
}

TEST(CacheSettle, AReadingThatDiffersRestartsTheHoldFromIt) {
  Settle<std::vector<float>> settle;
  std::vector<float> a{1.0f};
  std::vector<float> b{2.0f};
  for (int i = 0; i < kHold; ++i) settle.observe(true, a, kHold);
  EXPECT_FALSE(settle.release(kHold, [&] { return b; }));
  EXPECT_EQ(settle.frames(), 0);
  EXPECT_EQ(settle.held(), b);
}

TEST(CacheSettle, TheScanReDeclaresTheFrameAnOutsideValueMoves) {
  Settle<std::vector<float>> settle;
  std::vector<float> a{1.0f};
  for (int i = 0; i < kHold; ++i) settle.observe(true, a, kHold);
  ASSERT_TRUE(settle.release(kHold, [&] { return a; }));
  EXPECT_FALSE(settle.moved(a));
  EXPECT_TRUE(settle.moved({7.0f}));
  EXPECT_EQ(settle.frames(), 0);
}

/** One frame of the host: prove, draw, scan. The first drawn frame has no
 *  previous reading to compare against, so a hold of N warms up over N+1
 *  of these. */
void frame(FakeHost& host, FakeNode& root) {
  host.proveTree(root);
  host.drawTree(root);
  host.scanReleased();
}

TEST(CacheHost, ASettledDrivenNodeStopsBlockingItsAncestors) {
  FakeHost host;
  host.hold = kHold;
  FakeNode root;
  root.key = "root";
  FakeNode& leaf = root.add("leaf");
  leaf.driven = true;
  leaf.lanes = {1.0f};

  // While the hold is warming up, the whole chain is volatile.
  for (int i = 0; i < kHold + 1; ++i) {
    frame(host, root);
    EXPECT_TRUE(root.verdict.subtreeVolatile) << "frame " << i;
  }
  ASSERT_TRUE(host.proofDirty);  // the crossing asked for one more proof
  host.proveTree(root);
  EXPECT_TRUE(leaf.released);
  EXPECT_FALSE(root.verdict.subtreeVolatile);
}

TEST(CacheHost, AReleasedLaneMovingReDeclaresBeforeAnythingReplays) {
  FakeHost host;
  host.hold = kHold;
  FakeNode root;
  root.key = "root";
  FakeNode& leaf = root.add("leaf");
  leaf.driven = true;
  leaf.lanes = {1.0f};

  for (int i = 0; i < kHold + 2; ++i) frame(host, root);
  ASSERT_TRUE(leaf.released);
  ASSERT_FALSE(root.verdict.subtreeVolatile);

  // An output assigned between two proofs.
  leaf.lanes = {9.0f};
  host.scanReleased();
  EXPECT_TRUE(host.proofDirty);
  EXPECT_TRUE(root.stale);  // the ancestor's artefact is refused
  host.proveTree(root);
  EXPECT_FALSE(leaf.released);
  EXPECT_TRUE(root.verdict.subtreeVolatile);
}

}  // namespace
