/** @file
 * The inherited-value channel: binding, reading, shadowing, the default
 * when nothing is bound, the out-of-order scope's warning, and the snapshot
 * equality and restore a memo is built on.
 */

#include <gtest/gtest.h>
#include <sigilcore/reconcile/Environment.h>

#include <memory>

using namespace sigil::core;

namespace {

struct Palette {
  int surface = 0;
  bool operator==(const Palette&) const = default;
};
struct Other {
  int v = 0;
  bool operator==(const Other&) const = default;
};

}  // namespace

TEST(Environment, ProvideBindsForItsScopeAndUnbindsAfter) {
  EXPECT_FALSE(environment::bound<Palette>());
  EXPECT_EQ(environment::inherited<Palette>(), nullptr);
  EXPECT_EQ(environment::inheritedOr(Palette{9}).surface, 9);
  {
    environment::Provide<Palette> theme(Palette{1});
    ASSERT_TRUE(environment::bound<Palette>());
    EXPECT_EQ(environment::inherited<Palette>()->surface, 1);
    EXPECT_EQ(environment::inheritedOr(Palette{9}).surface, 1);
  }
  EXPECT_FALSE(environment::bound<Palette>());
}

TEST(Environment, AnInnerBindingShadowsAndOtherTypesAreUntouched) {
  environment::Provide<Palette> outer(Palette{1});
  {
    environment::Provide<Palette> inner(Palette{2});
    environment::Provide<Other> other(Other{7});
    EXPECT_EQ(environment::inherited<Palette>()->surface, 2);
    EXPECT_EQ(environment::inherited<Other>()->v, 7);
  }
  EXPECT_EQ(environment::inherited<Palette>()->surface, 1);
  EXPECT_FALSE(environment::bound<Other>());
}

TEST(Environment, AScopeDestroyedOutOfOrderRemovesOnlyItsOwnBindingAndWarns) {
  auto first = std::make_unique<environment::Provide<Palette>>(Palette{1});
  auto second = std::make_unique<environment::Provide<Palette>>(Palette{2});
  ::testing::internal::CaptureStderr();
  first.reset();  // misuse: the outer scope ends first
  EXPECT_NE(
      ::testing::internal::GetCapturedStderr().find("environment::Provide"),
      std::string::npos);
  ASSERT_TRUE(environment::bound<Palette>());
  EXPECT_EQ(environment::inherited<Palette>()->surface,
            2);  // the sibling survives
  second.reset();
  EXPECT_FALSE(environment::bound<Palette>());
}

TEST(Environment, SnapshotsCompareByBindingValueInOrder) {
  environment::Snapshot empty;
  EXPECT_TRUE(empty == empty);
  environment::Snapshot one, sameValue, otherValue, otherType, longer;
  {
    environment::Provide<Palette> p(Palette{1});
    one = environment::capture();
    {
      environment::Provide<Other> o(Other{1});
      longer = environment::capture();
    }
  }
  {
    environment::Provide<Palette> p(Palette{1});
    sameValue = environment::capture();
  }
  {
    environment::Provide<Palette> p(Palette{2});
    otherValue = environment::capture();
  }
  {
    environment::Provide<Other> o(Other{1});
    otherType = environment::capture();
  }
  EXPECT_TRUE(one == one);  // the same holder short-circuits
  EXPECT_TRUE(one == sameValue);
  EXPECT_FALSE(one == otherValue);
  EXPECT_FALSE(one == otherType);
  EXPECT_FALSE(one == longer);
  EXPECT_FALSE(one == empty);
}

TEST(Environment, RestoreSwapsACapturedStackInAndBackOut) {
  environment::Snapshot captured;
  {
    environment::Provide<Palette> p(Palette{3});
    captured = environment::capture();
  }
  environment::Provide<Other> ambient(Other{1});
  {
    environment::Restore restore(captured);
    // The deferred call sees exactly the author's scope: not the ambient
    // one on top of it.
    EXPECT_EQ(environment::inherited<Palette>()->surface, 3);
    EXPECT_FALSE(environment::bound<Other>());
  }
  EXPECT_FALSE(environment::bound<Palette>());
  EXPECT_EQ(environment::inherited<Other>()->v, 1);
}
