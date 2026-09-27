// The rules cut to what a block of type actually occupies.

#include <sigilcompose/kit/Typeset.h>

#include <string>
#include <vector>

#include "support/ShapeTestSupport.h"

namespace kit = sigil::compose::kit;

TEST(KitRules, ARuleStandsWhereTheBlockIsAndTheThreeArmsDifferInWhere) {
  Host host(300, 200);
  const auto tree = [](Element overlay) {
    return box().padding(20).children(
        {text(u8"EPIGRAPH", whiteStyle(20)).key("epigraph"),
         std::move(overlay).absolute().inset(0)});
  };
  host.composer.render(tree(positioned()));
  host.frame();
  const SkRect block = require(host.composer.bounds("epigraph"));

  const auto ruleAt = [&](kit::BlockRule::Where where) {
    Host run(300, 200);
    run.composer.render(tree(positioned()));
    run.frame();
    run.composer.render(
        tree(kit::rules(run.composer, "epigraph",
                        sigil::weave::selectors::each(sigil::weave::Unit::Line),
                        {.where = where,
                         .thickness = 3,
                         .gap = 4,
                         .bleed = 4,
                         .colour = {1, 0, 0, 1}})));
    run.frame();
    const std::string key = where == kit::BlockRule::Where::Behind
                                ? "epigraph-shade"
                                : "epigraph-rule";
    return require(run.composer.bounds(key));
  };

  const SkRect above = ruleAt(kit::BlockRule::Where::Above);
  const SkRect below = ruleAt(kit::BlockRule::Where::Below);
  const SkRect behind = ruleAt(kit::BlockRule::Where::Behind);
  EXPECT_LT(above.bottom(), block.top() + 1);
  EXPECT_GT(below.top(), block.bottom() - 1);
  // Behind covers the run of lines rather than standing off it, and is
  // grown by its bleed at both ends.
  EXPECT_LT(behind.top(), block.top());
  EXPECT_GT(behind.bottom(), block.bottom());
  // Every arm is as wide as the LINES, not as wide as the box holding
  // them.
  EXPECT_LT(above.width(), 260);
  EXPECT_NEAR(above.width(), behind.width(), 1.0f);
}
