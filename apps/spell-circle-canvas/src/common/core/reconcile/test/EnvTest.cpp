/** @file
 * The inherited-value channel: binding, reading, shadowing, the default
 * when nothing is bound, the out-of-order scope's warning, and the snapshot
 * equality and restore a memo is built on.
 */

#include <gtest/gtest.h>
#include <sigilcore/reconcile/Env.h>

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

TEST(Env, ProvideBindsForItsScopeAndUnbindsAfter) {
  EXPECT_FALSE(env::bound<Palette>());
  EXPECT_EQ(env::inherited<Palette>(), nullptr);
  EXPECT_EQ(env::inheritedOr(Palette{9}).surface, 9);
  {
    env::Provide<Palette> theme(Palette{1});
    ASSERT_TRUE(env::bound<Palette>());
    EXPECT_EQ(env::inherited<Palette>()->surface, 1);
    EXPECT_EQ(env::inheritedOr(Palette{9}).surface, 1);
  }
  EXPECT_FALSE(env::bound<Palette>());
}

TEST(Env, AnInnerBindingShadowsAndOtherTypesAreUntouched) {
  env::Provide<Palette> outer(Palette{1});
  {
    env::Provide<Palette> inner(Palette{2});
    env::Provide<Other> other(Other{7});
    EXPECT_EQ(env::inherited<Palette>()->surface, 2);
    EXPECT_EQ(env::inherited<Other>()->v, 7);
  }
  EXPECT_EQ(env::inherited<Palette>()->surface, 1);
  EXPECT_FALSE(env::bound<Other>());
}

TEST(Env, AScopeDestroyedOutOfOrderRemovesOnlyItsOwnBindingAndWarns) {
  auto first = std::make_unique<env::Provide<Palette>>(Palette{1});
  auto second = std::make_unique<env::Provide<Palette>>(Palette{2});
  ::testing::internal::CaptureStderr();
  first.reset();  // misuse: the outer scope ends first
  EXPECT_NE(::testing::internal::GetCapturedStderr().find("env::Provide"),
            std::string::npos);
  ASSERT_TRUE(env::bound<Palette>());
  EXPECT_EQ(env::inherited<Palette>()->surface, 2);  // the sibling survives
  second.reset();
  EXPECT_FALSE(env::bound<Palette>());
}

TEST(Env, SnapshotsCompareByBindingValueInOrder) {
  detail::EnvSnapshot empty;
  EXPECT_TRUE(detail::envEqual(empty, empty));
  detail::EnvSnapshot one, sameValue, otherValue, otherType, longer;
  {
    env::Provide<Palette> p(Palette{1});
    one = detail::envStack();
    {
      env::Provide<Other> o(Other{1});
      longer = detail::envStack();
    }
  }
  {
    env::Provide<Palette> p(Palette{1});
    sameValue = detail::envStack();
  }
  {
    env::Provide<Palette> p(Palette{2});
    otherValue = detail::envStack();
  }
  {
    env::Provide<Other> o(Other{1});
    otherType = detail::envStack();
  }
  EXPECT_TRUE(detail::envEqual(one, one));  // the same holder short-circuits
  EXPECT_TRUE(detail::envEqual(one, sameValue));
  EXPECT_FALSE(detail::envEqual(one, otherValue));
  EXPECT_FALSE(detail::envEqual(one, otherType));
  EXPECT_FALSE(detail::envEqual(one, longer));
  EXPECT_FALSE(detail::envEqual(one, empty));
}

TEST(Env, RestoreSwapsACapturedStackInAndBackOut) {
  detail::EnvSnapshot captured;
  {
    env::Provide<Palette> p(Palette{3});
    captured = detail::envStack();
  }
  env::Provide<Other> ambient(Other{1});
  {
    detail::EnvRestore restore(captured);
    // The deferred call sees exactly the author's scope: not the ambient
    // one on top of it.
    EXPECT_EQ(env::inherited<Palette>()->surface, 3);
    EXPECT_FALSE(env::bound<Other>());
  }
  EXPECT_FALSE(env::bound<Palette>());
  EXPECT_EQ(env::inherited<Other>()->v, 1);
}
