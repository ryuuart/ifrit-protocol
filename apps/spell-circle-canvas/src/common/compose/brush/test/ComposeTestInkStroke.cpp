// A mark that names no colour is painted in the ink in force — the rule a
// stroke, a line and a ribbon share with a text leaf that names no style.

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/Lines.h>

#include "support/BrushTestSupport.h"

TEST(ComposeInkStroke, AStrokeThatNamesNoColourIsPaintedInTheInk) {
  // stroke(width) alone: the nearest ancestor's ink, green here, is the
  // colour of the band.
  Host host;
  host.composer.render(box().padding(20).ink({0, 1, 0, 1}).child(
      box().width(100).height(100).stroke(stroke(10))));
  host.frame();
  // The band straddles the outline: five pixels inside the box's left edge
  // at x = 20 are the stroke's, well clear of its antialiased rim.
  EXPECT_EQ(host.pixel(22, 70), SkColorSetARGB(255, 0, 255, 0));
  EXPECT_EQ(host.pixel(18, 70), SkColorSetARGB(255, 0, 255, 0));
}

TEST(ComposeInkStroke, AStrokeInTheInkFollowsARecolouredAncestor) {
  Host host;
  const auto page = [](SkColor4f ink) {
    return box().padding(20).ink(ink).child(
        box().key("c").width(100).height(100).stroke(stroke(10)));
  };
  host.composer.render(page({1, 0, 0, 1}));
  host.frame();
  EXPECT_EQ(host.pixel(22, 70), SkColorSetARGB(255, 255, 0, 0));
  host.composer.render(page({0, 0, 1, 1}));
  host.frame();
  EXPECT_EQ(host.pixel(22, 70), SkColorSetARGB(255, 0, 0, 255));
}

TEST(ComposeInkStroke, AStrokeThatNamesAColourKeepsIt) {
  Host host;
  host.composer.render(box().padding(20).ink({0, 1, 0, 1}).child(
      box().width(100).height(100).stroke(stroke(10, red()))));
  host.frame();
  EXPECT_EQ(host.pixel(22, 70), SkColorSetARGB(255, 255, 0, 0));
}

TEST(ComposeInkStroke, ALineWrittenAsTheInkTakesIt) {
  // The cartography stroke's fill, written as the ink, resolves through
  // the same context a plain stroke's does.
  Host host;
  lines::Line line;
  line.width = 10;
  line.fill = Fill::currentInk();
  host.composer.render(box().padding(20).ink({0, 0, 1, 1}).child(
      box().width(100).height(100).stroke(line)));
  host.frame();
  EXPECT_EQ(host.pixel(22, 70), SkColorSetARGB(255, 0, 0, 255));
}
