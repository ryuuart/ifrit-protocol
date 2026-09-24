// The root box and the viewport it stands in: the root fills the canvas
// along any axis it states no size on, and a size it does state is its
// own, as CSS honours one on the root element.

#include "support/CoreTestSupport.h"

TEST(ComposeRootBox, ARootFillsTheCanvasWhereItStatesNoSize) {
  Host host(120, 60);
  host.composer.render(box().key("root"));
  host.frame();
  const SkRect root = require(host.composer.bounds("root"));
  EXPECT_FLOAT_EQ(root.width(), 120);
  EXPECT_FLOAT_EQ(root.height(), 60);
}

TEST(ComposeRootBox, ARootLeafsStatedWidthIsItsMeasure) {
  // A root text leaf set seventy pixels wide wraps at seventy, inside a
  // canvas of a hundred and twenty, and still fills the canvas's height.
  Host host(120, 60);
  host.composer.render(text(u8"a measure seventy pixels wide", whiteStyle(10))
                           .key("leaf")
                           .width(70));
  host.frame();
  const SkRect leaf = require(host.composer.bounds("leaf"));
  EXPECT_FLOAT_EQ(leaf.left(), 0);
  EXPECT_FLOAT_EQ(leaf.width(), 70);
  EXPECT_FLOAT_EQ(leaf.height(), 60);
}

TEST(ComposeRootBox, ARootsPercentageIsOfTheCanvas) {
  Host host(120, 60);
  host.composer.render(box().key("root").width(pct(50)).height(pct(25)));
  host.frame();
  const SkRect root = require(host.composer.bounds("root"));
  EXPECT_FLOAT_EQ(root.width(), 60);
  EXPECT_FLOAT_EQ(root.height(), 15);
}
