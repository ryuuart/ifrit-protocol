/** @file
 * The rebuild guard: when a build fires, what a throw leaves behind, and
 * the value that holds what the last build answered — plus the snap that
 * keeps a drifting input posing one problem.
 */

#include <gtest/gtest.h>
#include <sigilcore/cache/Rebuild.h>

#include <stdexcept>
#include <string>

using namespace sigil::core;

TEST(RebuildGuard, FiresOnFirstUseThenOnlyOnKeyChange) {
  RebuildGuard<std::string, float> guard;
  int builds = 0;
  auto build = [&] { ++builds; };

  EXPECT_TRUE(guard.ensure({"a", 1.0f}, build));
  EXPECT_FALSE(guard.ensure({"a", 1.0f}, build));
  EXPECT_TRUE(guard.ensure({"a", 2.0f}, build));
  EXPECT_TRUE(guard.ensure({"b", 2.0f}, build));
  EXPECT_FALSE(guard.ensure({"b", 2.0f}, build));
  EXPECT_EQ(builds, 3);

  guard.invalidate();
  EXPECT_TRUE(guard.ensure({"b", 2.0f}, build));
  EXPECT_EQ(builds, 4);
}

TEST(RebuildGuard, ThrowingBuildStaysInvalidAndRetries) {
  RebuildGuard<int> guard;
  EXPECT_THROW(guard.ensure({1}, [] { throw std::runtime_error("boom"); }),
               std::runtime_error);
  EXPECT_FALSE(guard.built());
  int builds = 0;
  EXPECT_TRUE(guard.ensure({1}, [&] { ++builds; }));
  EXPECT_EQ(builds, 1);
}

TEST(CachedValue, TheBuilderRunsAgainOnlyWhenTheKeyChanges) {
  CachedValue<int, int> cached;
  EXPECT_EQ(cached.ensure({10}, [] { return 100; }), 100);
  // Same key: the stale-looking callable must not run.
  EXPECT_EQ(cached.ensure({10}, [] { return 999; }), 100);
  EXPECT_EQ(cached.ensure({20}, [] { return 200; }), 200);
  EXPECT_EQ(cached.value(), 200);
}

TEST(CachedValue, KeylessEnsureBuildsOnce) {
  CachedValue<int> lazy;
  int builds = 0;
  auto build = [&] {
    ++builds;
    return 7;
  };
  EXPECT_EQ(lazy.ensure(build), 7);
  EXPECT_EQ(lazy.ensure(build), 7);
  EXPECT_EQ(builds, 1);
}

TEST(QuantizeKey, AValueSnapsToTheNearestMultipleOfItsStep) {
  EXPECT_FLOAT_EQ(quantizeKey(10.3f), 10.0f);
  EXPECT_FLOAT_EQ(quantizeKey(10.6f), 11.0f);
  EXPECT_FLOAT_EQ(quantizeKey(103.0f, 8.0f), 104.0f);
  EXPECT_FLOAT_EQ(quantizeKey(-2.6f), -3.0f);
}
