/** @file
 * The reconciler over a fake host: mount and patch, the identity prune,
 * keyed reorder keeping its handles, identity change keeping the handle and
 * its lanes, retirement stamped with the pass, the remount rule, the memo
 * with its captured environment, slot content and the key index.
 */

#include <gtest/gtest.h>

#include <algorithm>

#include "FakeHost.h"

using namespace sigil::core;
using namespace sigil::core::test::reconcile;

namespace {

bool logged(const FakeHost& host, const std::string& entry) {
  return std::find(host.log.begin(), host.log.end(), entry) != host.log.end();
}

}  // namespace

TEST(Reconciler, MountsTheTreeAndCountsIt) {
  FakeHost host;
  host.render(desc("root", 0, {desc("a", 1), desc("b", 2)}));
  ASSERT_TRUE(host.root);
  EXPECT_EQ(host.childKeys(), (std::vector<std::string>{"a", "b"}));
  EXPECT_EQ(host.child(0)->parent, host.root.get());
  const ReconcileStats& s = host.reconciler.stats();
  EXPECT_EQ(s.describedNodes, 3);
  EXPECT_EQ(s.patchedNodes, 3);
  EXPECT_EQ(s.mounted, 3);
  EXPECT_EQ(s.retired, 0);
  EXPECT_EQ(host.reconciler.frame(), 1u);
  EXPECT_TRUE(logged(host, "patch a (mount)"));
  EXPECT_TRUE(logged(host, "create a #2 0/2"));
  EXPECT_TRUE(logged(host, "create b #3 1/2"));
}

TEST(Reconciler, IdenticalRedescribePrunesEveryNode) {
  FakeHost host;
  host.render(desc("root", 0, {desc("a", 1), desc("b", 2)}));
  host.log.clear();
  host.render(desc("root", 0, {desc("a", 1), desc("b", 2)}));
  const ReconcileStats& s = host.reconciler.stats();
  EXPECT_EQ(s.describedNodes, 3);
  EXPECT_EQ(s.patchedNodes, 0);
  EXPECT_EQ(s.mounted, 0);
  EXPECT_FALSE(logged(host, "patch a"));
  EXPECT_TRUE(logged(host, "reorder root"));  // never "changed"
  EXPECT_FALSE(logged(host, "reorder root changed"));
}

TEST(Reconciler, AChangedValuePatchesThatNodeAlone) {
  FakeHost host;
  host.render(desc("root", 0, {desc("a", 1), desc("b", 2)}));
  host.log.clear();
  host.render(desc("root", 0, {desc("a", 1), desc("b", 3)}));
  EXPECT_EQ(host.reconciler.stats().patchedNodes, 1);
  EXPECT_TRUE(logged(host, "patch b"));
  EXPECT_FALSE(logged(host, "patch a"));
}

TEST(Reconciler, KeyedReorderKeepsEveryHandle) {
  FakeHost host;
  host.render(desc("root", 0, {desc("a"), desc("b"), desc("c")}));
  FakeNode *a = host.child(0), *b = host.child(1), *c = host.child(2);
  host.log.clear();
  host.render(desc("root", 0, {desc("c"), desc("a"), desc("b")}));
  EXPECT_EQ(host.child(0), c);
  EXPECT_EQ(host.child(1), a);
  EXPECT_EQ(host.child(2), b);
  EXPECT_EQ(host.reconciler.stats().patchedNodes, 0);
  EXPECT_EQ(host.reconciler.stats().retired, 0);
  EXPECT_TRUE(logged(host, "reorder root changed"));
}

TEST(Reconciler, UnkeyedChildrenMatchByPosition) {
  FakeHost host;
  host.render(desc("root", 0, {desc("", 1), desc("", 2), desc("k", 3)}));
  FakeNode *first = host.child(0), *second = host.child(1),
           *keyed = host.child(2);
  host.render(desc("root", 0, {desc("k", 3), desc("", 5), desc("", 2)}));
  // The keyed child follows its key; the unkeyed take their positions
  // among the unkeyed, in order.
  EXPECT_EQ(host.child(0), keyed);
  EXPECT_EQ(host.child(1), first);
  EXPECT_EQ(host.child(2), second);
  EXPECT_EQ(host.reconciler.stats().patchedNodes, 1);  // first: 1 → 5
}

TEST(Reconciler, IdentityChangeKeepsTheHandleAndItsLanes) {
  FakeHost host;
  host.render(desc("root", 0, {desc("a", 1)}));
  FakeNode* a = host.child(0);
  a->lane = 42;  // a motion the node holds
  auto changed = desc("a", 1);
  changed->kind = "text";
  host.render(desc("root", 0, {changed}));
  EXPECT_EQ(host.child(0), a);
  EXPECT_EQ(a->kind, "text");
  EXPECT_EQ(a->lane, 42);
  EXPECT_EQ(host.reconciler.stats().retired, 0);
  EXPECT_TRUE(logged(host, "patch a"));
}

TEST(Reconciler, ARemovedChildRetiresStampedWithThePass) {
  FakeHost host;
  host.render(desc("root", 0, {desc("a"), desc("b"), desc("c")}));
  const int idB = host.child(1)->id;
  host.render(desc("root", 0, {desc("a"), desc("c")}));
  EXPECT_EQ(host.reconciler.frame(), 2u);
  ASSERT_EQ(host.retired.size(), 1u);
  EXPECT_EQ(host.retired[0], std::make_pair(idB, uint64_t{2}));
  EXPECT_EQ(host.reconciler.stats().retired, 1);
  EXPECT_EQ(host.childKeys(), (std::vector<std::string>{"a", "c"}));
  // Retirement follows the reorder, so the host has detached the node
  // from whatever it keeps children attached to before it is destroyed.
  const auto reorder =
      std::find(host.log.begin(), host.log.end(), "reorder root changed");
  const auto destroy = std::find(host.log.begin(), host.log.end(),
                                 "destroy #" + std::to_string(idB));
  ASSERT_NE(reorder, host.log.end());
  ASSERT_NE(destroy, host.log.end());
  EXPECT_LT(reorder, destroy);
}

TEST(Reconciler, ARemountRuleRetiresTheMatchAndMountsAfresh) {
  FakeHost host;
  host.render(desc("root", 0, {desc("a", 1)}));
  const int idA = host.child(0)->id;
  auto positioned = desc("root", 0, {desc("a", 1)});
  positioned->positioned = true;
  host.render(positioned);
  ASSERT_EQ(host.retired.size(), 1u);
  EXPECT_EQ(host.retired[0].first, idA);
  EXPECT_NE(host.child(0)->id, idA);
  EXPECT_TRUE(host.child(0)->positionedMode);
  EXPECT_EQ(host.reconciler.stats().mounted, 1);
}

namespace {

/** A memo over an int, producing a keyed leaf whose value is that int;
 *  `calls` counts the deferred describes that actually ran. */
Desc memoOf(std::string key, int props, int* calls) {
  auto shell = desc(key);
  Memo<Desc> memo;
  memo.props = props;
  memo.equal = [](const std::any& a, const std::any& b) {
    return std::any_cast<int>(a) == std::any_cast<int>(b);
  };
  memo.invoke = [key = std::move(key), calls](const std::any& p) {
    ++*calls;
    return desc(key, std::any_cast<int>(p));
  };
  memo.env = env::capture();
  shell->memo = std::move(memo);
  return shell;
}

struct Theme {
  int tone = 0;
  bool operator==(const Theme&) const = default;
};

}  // namespace

TEST(Reconciler, AMemoHitsOnEqualPropsAndMissesOnChangedOnes) {
  FakeHost host;
  int calls = 0;
  host.render(desc("root", 0, {memoOf("m", 7, &calls)}));
  EXPECT_EQ(calls, 1);
  EXPECT_EQ(host.child(0)->desc->value, 7);
  EXPECT_EQ(host.child(0)->memoShell->key, "m");
  host.render(desc("root", 0, {memoOf("m", 7, &calls)}));
  EXPECT_EQ(calls, 1);  // hit: the payload stands
  EXPECT_EQ(host.reconciler.stats().memoHits, 1);
  EXPECT_EQ(host.reconciler.stats().patchedNodes, 0);
  host.render(desc("root", 0, {memoOf("m", 8, &calls)}));
  EXPECT_EQ(calls, 2);
  EXPECT_EQ(host.reconciler.stats().memoHits, 0);
  EXPECT_EQ(host.child(0)->desc->value, 8);
}

TEST(Reconciler, AMemoIsKeyedByItsEnvironmentToo) {
  FakeHost host;
  int calls = 0;
  {
    env::Provide<Theme> theme(Theme{1});
    host.render(desc("root", 0, {memoOf("m", 7, &calls)}));
  }
  EXPECT_EQ(calls, 1);
  {
    env::Provide<Theme> theme(Theme{1});
    host.render(desc("root", 0, {memoOf("m", 7, &calls)}));
  }
  EXPECT_EQ(calls, 1);  // same props, equal environment: a hit
  {
    env::Provide<Theme> theme(Theme{2});
    host.render(desc("root", 0, {memoOf("m", 7, &calls)}));
  }
  EXPECT_EQ(calls, 2);  // same props, different environment: a miss
}

TEST(Reconciler, ADeferredDescribeRunsUnderTheEnvironmentItWasWrittenIn) {
  FakeHost host;
  Desc shell;
  {
    env::Provide<Theme> theme(Theme{5});
    shell = desc("m");
    Memo<Desc> memo;
    memo.props = 0;
    memo.equal = [](const std::any&, const std::any&) { return true; };
    memo.invoke = [](const std::any&) {
      const Theme* t = env::inherited<Theme>();
      return desc("m", t ? t->tone : -1);
    };
    memo.env = env::capture();
    shell->memo = std::move(memo);
  }
  ASSERT_FALSE(env::bound<Theme>());  // the author's scope is gone
  host.render(desc("root", 0, {shell}));
  EXPECT_EQ(host.child(0)->desc->value, 5);
  EXPECT_FALSE(env::bound<Theme>());  // and restored after the invoke
}

TEST(Reconciler, SlotContentIsNotWalkedAndReplaceContentFillsIt) {
  FakeHost host;
  auto slot = desc("slot");
  slot->slot = true;
  slot->children = {desc("ignored")};
  host.render(desc("root", 0, {slot}));
  FakeNode* slotNode = host.child(0);
  EXPECT_TRUE(slotNode->children.empty());

  host.log.clear();
  host.reconciler.replaceContent(*slotNode, desc("content", 1));
  ASSERT_EQ(slotNode->children.size(), 1u);
  FakeNode* content = slotNode->children.front().get();
  EXPECT_EQ(content->parent, slotNode);
  EXPECT_TRUE(logged(host, "reorder slot changed"));
  EXPECT_TRUE(logged(host, "invalidate slot"));
  EXPECT_EQ(host.reconciler.frame(), 2u);

  // The single child is patched in place next time, and the slot is
  // invalidated either way.
  host.log.clear();
  host.reconciler.replaceContent(*slotNode, desc("content", 2));
  EXPECT_EQ(slotNode->children.front().get(), content);
  EXPECT_EQ(content->desc->value, 2);
  EXPECT_TRUE(logged(host, "patch content"));
  EXPECT_TRUE(logged(host, "invalidate slot"));
}

TEST(Reconciler, TheKeyIndexAddressesAMemoShellByTheShellsKeyElseThePayloads) {
  FakeHost host;
  int calls = 0;
  auto unkeyedShell = memoOf("", 3, &calls);  // the payload is keyed ""
  ASSERT_TRUE(unkeyedShell->memo.has_value());
  unkeyedShell->memo->invoke = [](const std::any& p) {
    return desc("payload", std::any_cast<int>(p));
  };
  host.render(
      desc("root", 0, {desc("a"), memoOf("m", 1, &calls), unkeyedShell}));
  FakeHost::Reconciler::KeyIndex byKey;
  std::vector<std::string> visited;
  host.reconciler.indexKeys(*host.root, byKey, [&](FakeNode& n) {
    visited.push_back(host.reconciler.keyOf(n));
  });
  EXPECT_EQ(visited, (std::vector<std::string>{"root", "a", "m", "payload"}));
  EXPECT_EQ(byKey.at("a"), host.child(0));
  EXPECT_EQ(byKey.at("m"), host.child(1));
  EXPECT_EQ(byKey.at("payload"), host.child(2));
  // Matching, unlike addressing, reads the shell alone.
  EXPECT_EQ(host.reconciler.matchKeyOf(*host.child(2)), "");
}

TEST(Reconciler, StatsReportIntoNamedCounters) {
  FakeHost host;
  host.render(desc("root", 0, {desc("a"), desc("b")}));
  sigil::measure::Counters counters;
  host.reconciler.stats().report(counters);
  EXPECT_EQ(counters.get("reconcile.described"), 3);
  EXPECT_EQ(counters.get("reconcile.mounted"), 3);
  EXPECT_EQ(counters.get("reconcile.patched"), 3);
  EXPECT_EQ(counters.get("reconcile.memoHits"), 0);
  EXPECT_EQ(counters.get("reconcile.retired"), 0);
}
