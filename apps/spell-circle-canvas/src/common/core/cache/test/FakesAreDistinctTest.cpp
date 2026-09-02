// The two fake hosts under core/ are compiled into one binary each, so
// nothing ever links them together — which is precisely why a collision
// between them would go unnoticed until a third binary wanted both. This
// translation unit is that third place: it includes both headers, so the
// compiler is the thing that says a `FakeHost` and a `FakeNode` are two
// different types living side by side rather than one type redefined.

#include <gtest/gtest.h>

#include <type_traits>

#include "../../reconcile/test/FakeHost.h"
#include "FakeCacheHost.h"

namespace {

namespace cache = sigil::core::test::cache;
namespace reconcile = sigil::core::test::reconcile;

// One name per fake per namespace. Spelled as a static assertion because
// the failure being guarded against is a redefinition — a compile error,
// not a wrong answer — and the assertion is what states that two hosts
// sharing a spelling was deliberate rather than an accident nobody saw.
static_assert(!std::is_same_v<cache::FakeNode, reconcile::FakeNode>);
static_assert(!std::is_same_v<cache::FakeHost, reconcile::FakeHost>);

TEST(CoreFakes, BothHostsStandUpInOneTranslationUnit) {
  cache::FakeHost cached;
  cache::FakeNode root;
  root.declared.ownPaint = true;
  cached.proveTree(root);

  reconcile::FakeHost reconciled;
  reconciled.render(reconcile::desc("root"));

  EXPECT_EQ(cached.counters.takes, 0);
  EXPECT_FALSE(reconciled.log.empty());
}

}  // namespace
