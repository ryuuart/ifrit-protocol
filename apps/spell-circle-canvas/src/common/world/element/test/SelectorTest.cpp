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
  EXPECT_EQ(Selector{}.operation(), Selector::Operation::All);
}

TEST(WorldSelector, TermsAnswerTheirOwnQuestion) {
  const Node node{"body", {"glow", "lit"}, {"stage", "rig"}, nullptr};
  EXPECT_TRUE(selectors::tag("glow").matches(node.subject()));
  EXPECT_FALSE(selectors::tag("dim").matches(node.subject()));
  EXPECT_TRUE(selectors::key("body").matches(node.subject()));
  EXPECT_FALSE(selectors::key("stage").matches(node.subject()));
  EXPECT_TRUE(selectors::under("rig").matches(node.subject()));
  EXPECT_FALSE(selectors::under("body").matches(node.subject()));
}

TEST(WorldSelector, AMaterialTermComparesByValue) {
  const material::Material red = paint({1, 0, 0, 1});
  const material::Material blue = paint({0, 0, 1, 1});
  Node node{"body", {}, {}, &red};
  EXPECT_TRUE(selectors::material(paint({1, 0, 0, 1})).matches(node.subject()));
  EXPECT_FALSE(selectors::material(blue).matches(node.subject()));
  node.material = nullptr;
  EXPECT_FALSE(selectors::material(red).matches(node.subject()));
}

TEST(WorldSelector, AndOrAndNotCombineTheirTermsVerdicts) {
  const Node glowing{"body", {"glow"}, {"rig"}, nullptr};
  const Node plain{"body", {}, {"rig"}, nullptr};
  EXPECT_TRUE((selectors::tag("glow") | selectors::key("nothing"))
                  .matches(glowing.subject()));
  EXPECT_FALSE((selectors::tag("glow") | selectors::key("nothing"))
                   .matches(plain.subject()));
  EXPECT_TRUE((selectors::tag("glow") & selectors::under("rig"))
                  .matches(glowing.subject()));
  EXPECT_FALSE((selectors::tag("glow") & selectors::under("stage"))
                   .matches(glowing.subject()));
  EXPECT_TRUE((!selectors::tag("glow")).matches(plain.subject()));
  EXPECT_FALSE((!selectors::tag("glow")).matches(glowing.subject()));
}

TEST(WorldSelector, EqualityIsByValue) {
  EXPECT_EQ(selectors::tag("glow"), selectors::tag("glow"));
  EXPECT_NE(selectors::tag("glow"), selectors::tag("dim"));
  EXPECT_NE(selectors::tag("glow"), selectors::key("glow"));
  EXPECT_EQ(selectors::tag("a") | selectors::tag("b"),
            selectors::tag("a") | selectors::tag("b"));
  EXPECT_NE(selectors::tag("a") | selectors::tag("b"),
            selectors::tag("a") & selectors::tag("b"));
  EXPECT_EQ(selectors::material(paint({1, 0, 0, 1})),
            selectors::material(paint({1, 0, 0, 1})));
  EXPECT_NE(selectors::material(paint({1, 0, 0, 1})),
            selectors::material(paint({0, 1, 0, 1})));
  EXPECT_EQ(Selector{}, Selector::leaf(Selector::Operation::All, ""));
}
