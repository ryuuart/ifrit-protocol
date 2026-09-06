/** @file
 * The pen's text, shaped by the text engine and set in the instrument
 * face: every letter on an advance of 0.6 em, the space on 0.3, a
 * capital's ink standing 0.7 em over the baseline and a lowercase
 * letter's 0.5, so a seat and a width are arithmetic here rather than a
 * property of the machine's installed families. The one case whose claim
 * IS that resolution takes the instrument back off.
 */

#include <gtest/gtest.h>
#include <sigildraw/Draw.h>

#include "support/Paper.h"

namespace {

using namespace sigil::draw;
using sigil::draw::testing::Paper;

TEST(Pen, TextIsShapedAndCentredByTheAlignment) {
  // "Hello" at 20 px: five advances of 12, each letter's ink inset 2 into
  // its own cell and 2 short of the next, so the ink runs 12..68 from a
  // pen set down at 10. The H stands 14 over the baseline and the
  // lowercase letters 10, none of them descends, so the band is 46..60.
  Paper left;
  left.begin();
  left.pen.textSize(20);
  left.pen.text("Hello", 10, 60);
  left.end();
  const SkIRect ink = left.inked();
  ASSERT_FALSE(ink.isEmpty());
  EXPECT_EQ(ink.left(), 12);
  EXPECT_EQ(ink.right(), 68);
  EXPECT_EQ(ink.top(), 46);
  EXPECT_EQ(ink.bottom(), 60);

  // CENTER on both axes seats the same block about the point instead: the
  // natural width is 60, so it starts at 20 and the ink runs 22..78, and
  // the baseline is the point plus half of what the ascent stands over
  // the descent — 50 + (16 - 4) / 2 — so the band is 42..56.
  Paper centred;
  centred.begin();
  centred.pen.textSize(20);
  centred.pen.textAlign(CENTER, CENTER);
  centred.pen.text("Hello", 50, 50);
  centred.end();
  const SkIRect box = centred.inked();
  ASSERT_FALSE(box.isEmpty());
  EXPECT_EQ(box.left(), 22);
  EXPECT_EQ(box.right(), 78);
  EXPECT_EQ(box.top(), 42);
  EXPECT_EQ(box.bottom(), 56);
}

TEST(Pen, TheBoxIsTheExtentTheVerticalAlignmentDistributesOver) {
  // One passage in a box deeper than it needs. The room left over is
  // what the alignment places, and only the box says how much room that
  // is — a distribution over an extent of nobody said has none to place
  // and seats the middle and the foot where the top would be.
  //
  // At 12 px the passage sets 86.4 wide (eleven letters of 7.2 and two
  // spaces of 3.6), so a measure of 60 breaks it after "two" and the
  // block is two lines of the 15 px leading.
  constexpr float kX = 4, kY = 4, kW = 60;
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
  // A capital at 30 px is a solid bar 12 wide and 21 deep, so the middle
  // of the ink box is ink and the colour read there is the glyph's.
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

TEST(PenMachineFace, TextIsSetInTheMachinesDefaultUntilAFaceIsNamed) {
  // Every case above hands the pen the instrument, which is the pen's
  // face-was-named path. This is the other one: no face and no family,
  // so the font context's default typeface answers and the machine's
  // families are what the text is set in. It measures nothing that face
  // decides — only that a pen given nothing still has something to set
  // with, which is what a runner without faces cannot supply.
  Paper paper;
  paper.begin();
  paper.useMachineFace();
  paper.pen.textSize(20);
  paper.pen.text("Hello", 10, 60);
  const float width = paper.pen.textWidth("Hello");
  const float ascent = paper.pen.textAscent();
  paper.end();
  EXPECT_FALSE(paper.inked().isEmpty());
  EXPECT_GT(width, 0.0f);
  EXPECT_GT(ascent, 0.0f);
}

}  // namespace
