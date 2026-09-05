/** @file
 * The pen's text, shaped by the text engine against the machine's system
 * fonts: what these pin are relations no face changes.
 */

#include <gtest/gtest.h>
#include <sigildraw/Draw.h>

#include "support/Paper.h"

namespace {

using namespace sigil::draw;
using sigil::draw::testing::Paper;

TEST(Pen, TextIsShapedAndCentredByTheAlignment) {
  Paper left;
  left.begin();
  left.pen.textSize(20);
  left.pen.text("Hello", 10, 60);
  left.end();
  const SkIRect ink = left.inked();
  ASSERT_FALSE(ink.isEmpty());
  EXPECT_GE(ink.left(), 9);
  EXPECT_LT(ink.top(), 60);  // the ascent stands above the baseline
  EXPECT_GE(ink.bottom(), 44);

  Paper centred;
  centred.begin();
  centred.pen.textSize(20);
  centred.pen.textAlign(CENTER, CENTER);
  centred.pen.text("Hello", 50, 50);
  centred.end();
  const SkIRect box = centred.inked();
  ASSERT_FALSE(box.isEmpty());
  EXPECT_NEAR((box.left() + box.right()) / 2.0, 50.0, 4.0);
  EXPECT_NEAR((box.top() + box.bottom()) / 2.0, 50.0, 6.0);
}

TEST(Pen, TheBoxIsTheExtentTheVerticalAlignmentDistributesOver) {
  // One passage in a box deeper than it needs. The room left over is
  // what the alignment places, and only the box says how much room that
  // is — a distribution over an extent of nobody said has none to place
  // and seats the middle and the foot where the top would be.
  constexpr float kX = 4, kY = 4, kW = 92;
  const auto ink = [](Constant vertical, float height) {
    Paper paper(100, 140);
    paper.begin();
    paper.pen.textSize(12);
    paper.pen.textAlign(LEFT, vertical);
    paper.pen.text("one two three", kX, kY, kW, height);
    paper.end();
    const SkIRect box = paper.inked();
    EXPECT_FALSE(box.isEmpty());
    return box;
  };
  const SkIRect top = ink(TOP, 92);
  const SkIRect middle = ink(CENTER, 92);
  const SkIRect foot = ink(BOTTOM, 92);

  // The passage is the same passage: only its seat moves.
  EXPECT_NEAR(middle.height(), top.height(), 1);
  EXPECT_NEAR(foot.height(), top.height(), 1);
  EXPECT_EQ(middle.left(), top.left());

  // HALF OF THE ROOM, AND ALL OF IT.
  const int all = foot.top() - top.top();
  const int half = middle.top() - top.top();
  EXPECT_GT(all, 8) << "the box is deeper than the passage, so there is room";
  EXPECT_NEAR(half * 2, all, 2);

  // AND THE ROOM IS THE BOX'S. A box twenty pixels deeper leaves twenty
  // more for the foot to take and none for the top, which stacks from
  // the near edge whatever stands past the last line.
  const SkIRect deeperFoot = ink(BOTTOM, 112);
  const SkIRect deeperTop = ink(TOP, 112);
  EXPECT_EQ(deeperFoot.top() - foot.top(), 20);
  EXPECT_EQ(deeperTop.top(), top.top());
}

TEST(Pen, TextIsBlackUntilAFillIsSet) {
  Paper paper;
  paper.begin();
  paper.pen.textSize(30);
  paper.pen.text("I", 40, 70);
  paper.end();
  const SkIRect ink = paper.inked();
  ASSERT_FALSE(ink.isEmpty());
  const SkColor c = paper.pixel((ink.left() + ink.right()) / 2,
                                (ink.top() + ink.bottom()) / 2);
  EXPECT_EQ(SkColorGetR(c), 0u);
  EXPECT_EQ(SkColorGetG(c), 0u);
  EXPECT_EQ(SkColorGetB(c), 0u);
}

}  // namespace
