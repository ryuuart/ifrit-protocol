/** @file
 * Collections: what a named term and a `/key` term answer, how include
 * and exclude combine, and the value equality a description depends on.
 */

#include <gtest/gtest.h>
#include <sigilworld/node/Collection.h>

#include <string>
#include <vector>

using namespace sigil::world;

namespace {

/** One node as a collection reads it. */
struct Asked {
  std::string key;
  std::vector<std::string> collections;
  std::vector<std::string> ancestors;

  Member member() const { return {key, collections, ancestors}; }
};

}  // namespace

TEST(WorldCollection, TheDefaultHoldsEveryNode) {
  const Asked node{"body", {}, {}};
  EXPECT_TRUE(Collection{}.contains(node.member()));
  EXPECT_TRUE(Collection{}.everything());
}

TEST(WorldCollection, ANameIsTheMembersOfThatCollection) {
  const Asked node{"body", {"glow", "lit"}, {"stage", "rig"}};
  EXPECT_TRUE((Collection{.include = {"glow"}}).contains(node.member()));
  EXPECT_FALSE((Collection{.include = {"dim"}}).contains(node.member()));
  // Any one included name is enough.
  EXPECT_TRUE((Collection{.include = {"dim", "lit"}}).contains(node.member()));
}

TEST(WorldCollection, AKeyTermIsThatNodeAndEverythingUnderIt) {
  const Asked node{"body", {}, {"stage", "rig"}};
  EXPECT_TRUE((Collection{.include = {"/rig"}}).contains(node.member()));
  EXPECT_TRUE((Collection{.include = {"/body"}}).contains(node.member()));
  EXPECT_FALSE((Collection{.include = {"/lamp"}}).contains(node.member()));
  // A key term never reads a collection of the same name.
  const Asked named{"lamp", {"rig"}, {}};
  EXPECT_FALSE((Collection{.include = {"/rig"}}).contains(named.member()));
}

TEST(WorldCollection, ExcludeTakesOutWhateverIncludedIt) {
  const Asked ground{"floor", {"ground", "lit"}, {"set"}};
  const Asked prop{"vase", {"lit"}, {"set"}};
  const Collection litButNotGround{.include = {"lit"}, .exclude = {"ground"}};
  EXPECT_FALSE(litButNotGround.contains(ground.member()));
  EXPECT_TRUE(litButNotGround.contains(prop.member()));
  // Excluding from everything.
  EXPECT_FALSE((Collection{.exclude = {"/set"}}).contains(prop.member()));
}

TEST(WorldCollection, EqualityIsByValue) {
  EXPECT_EQ(Collection{.include = {"glow"}}, Collection{.include = {"glow"}});
  EXPECT_NE(Collection{.include = {"glow"}}, Collection{.include = {"dim"}});
  EXPECT_NE(Collection{.include = {"glow"}}, Collection{.exclude = {"glow"}});
  EXPECT_EQ(Collection{}, Collection{});
}
