/** @file
 * Selectors: what each term answers, how the combinators compose, and
 * the value equality a description depends on.
 */

#include <gtest/gtest.h>
#include <sigilworld/element/Selector.h>

#include <glm/vec4.hpp>
#include <memory>
#include <string>
#include <vector>

#include "TestMaterial.h"

using namespace sigil;
using namespace sigil::world;
using namespace sigil::world::test;

namespace {

struct Node {
  std::string key;
  std::vector<std::string> tags;
  std::vector<std::string> ancestors;
  const material::Material* material = nullptr;

  Subject subject() const { return {key, tags, ancestors, material}; }
};

}  // namespace

TEST(WorldSelector, TheDefaultMatchesEverything) {
  const Node node{"body", {}, {}, nullptr};
  EXPECT_TRUE(Selector{}.matches(node.subject()));
  EXPECT_EQ(Selector{}.op(), Selector::Op::All);
}

TEST(WorldSelector, TermsAnswerTheirOwnQuestion) {
  const Node node{"body", {"glow", "lit"}, {"stage", "rig"}, nullptr};
  EXPECT_TRUE(sel::tag("glow").matches(node.subject()));
  EXPECT_FALSE(sel::tag("dim").matches(node.subject()));
  EXPECT_TRUE(sel::key("body").matches(node.subject()));
  EXPECT_FALSE(sel::key("stage").matches(node.subject()));
  EXPECT_TRUE(sel::under("rig").matches(node.subject()));
  EXPECT_FALSE(sel::under("body").matches(node.subject()));
}

TEST(WorldSelector, AMaterialTermComparesByValue) {
  const material::Material red = paint({1, 0, 0, 1});
  const material::Material blue = paint({0, 0, 1, 1});
  Node node{"body", {}, {}, &red};
  EXPECT_TRUE(sel::material(paint({1, 0, 0, 1})).matches(node.subject()));
  EXPECT_FALSE(sel::material(blue).matches(node.subject()));
  node.material = nullptr;
  EXPECT_FALSE(sel::material(red).matches(node.subject()));
}

TEST(WorldSelector, AndOrAndNotCombineTheirTermsVerdicts) {
  const Node glowing{"body", {"glow"}, {"rig"}, nullptr};
  const Node plain{"body", {}, {"rig"}, nullptr};
  EXPECT_TRUE(
      (sel::tag("glow") | sel::key("nothing")).matches(glowing.subject()));
  EXPECT_FALSE(
      (sel::tag("glow") | sel::key("nothing")).matches(plain.subject()));
  EXPECT_TRUE(
      (sel::tag("glow") & sel::under("rig")).matches(glowing.subject()));
  EXPECT_FALSE(
      (sel::tag("glow") & sel::under("stage")).matches(glowing.subject()));
  EXPECT_TRUE((!sel::tag("glow")).matches(plain.subject()));
  EXPECT_FALSE((!sel::tag("glow")).matches(glowing.subject()));
}

TEST(WorldSelector, EqualityIsByValue) {
  EXPECT_EQ(sel::tag("glow"), sel::tag("glow"));
  EXPECT_NE(sel::tag("glow"), sel::tag("dim"));
  EXPECT_NE(sel::tag("glow"), sel::key("glow"));
  EXPECT_EQ(sel::tag("a") | sel::tag("b"), sel::tag("a") | sel::tag("b"));
  EXPECT_NE(sel::tag("a") | sel::tag("b"), sel::tag("a") & sel::tag("b"));
  EXPECT_EQ(sel::material(paint({1, 0, 0, 1})),
            sel::material(paint({1, 0, 0, 1})));
  EXPECT_NE(sel::material(paint({1, 0, 0, 1})),
            sel::material(paint({0, 1, 0, 1})));
  EXPECT_EQ(Selector{}, Selector::leaf(Selector::Op::All, ""));
}
