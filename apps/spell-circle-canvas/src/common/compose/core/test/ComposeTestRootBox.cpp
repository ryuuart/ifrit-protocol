// The root box and the viewport it stands in: the root fills the canvas
// along any axis it states no size on, and a size it does state is its
// own, as CSS honours one on the root element.

#include "support/CoreTestSupport.h"
#include "support/Rows.h"

using sigil::compose::test::checkRows;
using sigil::compose::test::forEachRow;

// The root fills an axis on which it states no size, an explicit root size
// remains its own, and an empty viewport asks for intrinsic layout on every
// axis the root leaves unstated.
TEST(ComposeRootBox, TheRootFillsTheCanvasOnEveryAxisItStatesNoSizeOn) {
  struct PolicyRow {
    const char* name;
    bool statesWidth;
    bool statesHeight;
  };
  const PolicyRow policies[] = {
      {"AnEmptyViewportRestoresIntrinsicUnstatedAxesStatingNeither", false,
       false},
      {"AnEmptyViewportRestoresIntrinsicUnstatedAxesStatingTheWidth", true,
       false},
      {"AnEmptyViewportRestoresIntrinsicUnstatedAxesStatingTheHeight", false,
       true},
      {"AnEmptyViewportRestoresIntrinsicUnstatedAxesStatingBoth", true, true},
  };
  forEachRow(policies, [](const PolicyRow& policy) {
    Host host(120, 60);
    Element root = box().key("root").children({box().width(40).height(20)});
    if (policy.statesWidth) root.width(70);
    if (policy.statesHeight) root.height(35);
    host.composer.render(root);
    host.frame();
    EXPECT_FLOAT_EQ(require(host.composer.bounds("root")).width(),
                    policy.statesWidth ? 70 : 120);
    EXPECT_FLOAT_EQ(require(host.composer.bounds("root")).height(),
                    policy.statesHeight ? 35 : 60);

    host.composer.setSize({0, 0});
    host.frame();
    EXPECT_FLOAT_EQ(require(host.composer.bounds("root")).width(),
                    policy.statesWidth ? 70 : 40);
    EXPECT_FLOAT_EQ(require(host.composer.bounds("root")).height(),
                    policy.statesHeight ? 35 : 20);

    host.composer.setSize({100, 50});
    host.frame();
    EXPECT_FLOAT_EQ(require(host.composer.bounds("root")).width(),
                    policy.statesWidth ? 70 : 100);
    EXPECT_FLOAT_EQ(require(host.composer.bounds("root")).height(),
                    policy.statesHeight ? 35 : 50);
  });
  checkRows({
      {"ARootFillsTheCanvasWhereItStatesNoSize",
       [] {
         Host host(120, 60);
         host.composer.render(box().key("root"));
         host.frame();
         const SkRect root = require(host.composer.bounds("root"));
         EXPECT_FLOAT_EQ(root.width(), 120);
         EXPECT_FLOAT_EQ(root.height(), 60);
       }},
      {"ARootLeafsStatedWidthIsItsMeasure",
       [] {
         // A root text leaf set seventy pixels wide wraps at seventy, inside a
         // canvas of a hundred and twenty, and still fills the canvas's height.
         Host host(120, 60);
         host.composer.render(
             text(u8"a measure seventy pixels wide", whiteStyle(10))
                 .key("leaf")
                 .width(70));
         host.frame();
         const SkRect leaf = require(host.composer.bounds("leaf"));
         EXPECT_FLOAT_EQ(leaf.left(), 0);
         EXPECT_FLOAT_EQ(leaf.width(), 70);
         EXPECT_FLOAT_EQ(leaf.height(), 60);
       }},
      {"ARootsPercentageIsOfTheCanvas",
       [] {
         Host host(120, 60);
         host.composer.render(
             box().key("root").width(percent(50)).height(percent(25)));
         host.frame();
         const SkRect root = require(host.composer.bounds("root"));
         EXPECT_FLOAT_EQ(root.width(), 60);
         EXPECT_FLOAT_EQ(root.height(), 15);
       }},
  });
}
