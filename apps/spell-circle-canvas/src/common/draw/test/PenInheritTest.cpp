/** @file
 * The pen begins in the ink and the font it was given, and stops taking
 * them for whatever the program set itself.
 */

#include <gtest/gtest.h>
#include <include/core/SkColor.h>
#include <sigildraw/Draw.h>
#include <sigilweave/style/Type.h>

#include "support/Paper.h"

namespace {

using namespace sigil::draw;
using sigil::draw::testing::Paper;
namespace weave = sigil::weave;

/** A type at @p px in the instrument face, so an advance read off the ink
 *  is arithmetic rather than whatever face the machine has. */
weave::Type typeAt(float px) {
  weave::Type type = weave::initialType();
  type.face = sigil::test::instrument::sans();
  type.size = weave::Length{px};
  return type;
}

/** The frame begun on the paper's surface with NO FACE SET ON THE PEN:
 *  these cases are about the font a host hands over, so the pen has to
 *  arrive naming none — which the fixture's own `begin` does not, since
 *  every other text case wants the instrument set as a choice. */
void beginBare(Paper& paper, int frame = 1) {
  Frame f;
  f.width = (float)paper.width;
  f.height = (float)paper.height;
  f.deltaSeconds = 1.0 / 60.0;
  f.frameCount = frame;
  f.fonts = &sigil::test::fonts();
  paper.pen.begin(*paper.surface->getCanvas(), f);
}

TEST(PenInherit, TheFillTakesTheInkUntilTheProgramSetsOne) {
  Paper paper;
  paper.begin();
  paper.pen.inherit({0, 1, 0, 1}, typeAt(12));
  paper.pen.strokeWeight(0);
  paper.pen.rect(0, 0, 20, 20);
  paper.end();
  // A fresh pen fills white; this one fills the ink it was handed.
  EXPECT_EQ(paper.pixel(10, 10), SK_ColorGREEN);
}

TEST(PenInherit, TheStrokeTakesTheInkUntilTheProgramSetsOne) {
  Paper paper;
  paper.begin();
  paper.pen.inherit({0, 0, 1, 1}, typeAt(12));
  paper.pen.noSmooth();
  paper.pen.strokeWeight(4);
  paper.pen.line(0, 10, 100, 10);
  paper.end();
  // A fresh pen strokes black; this one strokes the ink.
  EXPECT_EQ(paper.pixel(50, 10), SK_ColorBLUE);
}

TEST(PenInherit, AFillTheProgramSetOutlivesEveryInkAfterIt) {
  Paper paper;
  paper.begin(1);
  paper.pen.inherit({0, 1, 0, 1}, typeAt(12));
  paper.pen.noStroke();
  paper.pen.fill(255, 0, 0);
  paper.pen.rect(0, 0, 20, 20);
  paper.end();

  // A SECOND FRAME UNDER ANOTHER INK, and a program that sets nothing
  // this time: the fill it chose once holds from frame to frame as p5's
  // does, so the ink cannot reach it again.
  paper.begin(2);
  paper.pen.inherit({0, 0, 1, 1}, typeAt(12));
  paper.pen.rect(20, 0, 20, 20);
  paper.end();
  EXPECT_EQ(paper.pixel(10, 10), SK_ColorRED);
  EXPECT_EQ(paper.pixel(30, 10), SK_ColorRED);
}

TEST(PenInherit, NoFillStillMeansNoFillWhateverTheInkIs) {
  Paper paper;
  paper.begin();
  paper.pen.noFill();
  paper.pen.noStroke();
  paper.pen.inherit({1, 0, 0, 1}, typeAt(12));
  paper.pen.rect(0, 0, 20, 20);
  paper.end();
  EXPECT_EQ(SkColorGetA(paper.pixel(10, 10)), 0u);
}

TEST(PenInherit, TheTypeTakesTheFontUntilTheProgramSetsOne) {
  Paper paper{200, 60};
  beginBare(paper);
  paper.pen.inherit({0, 0, 0, 1}, typeAt(40));
  const float inherited = paper.pen.textWidth("nn");
  paper.pen.textSize(10);
  const float chosen = paper.pen.textWidth("nn");
  paper.end();
  // One string in one face, so the advance is the size: the inherited
  // type set the first width and the program's own set the second.
  EXPECT_GT(inherited, chosen * 2.0f);
}

TEST(PenInherit, AFontTheProgramSetOutlivesEveryFontAfterIt) {
  Paper paper{200, 60};
  beginBare(paper, 1);
  paper.pen.inherit({0, 0, 0, 1}, typeAt(40));
  paper.pen.textSize(10);
  const float chosen = paper.pen.textWidth("nn");
  paper.end();

  beginBare(paper, 2);
  paper.pen.inherit({0, 0, 0, 1}, typeAt(40));
  const float after = paper.pen.textWidth("nn");
  paper.end();
  EXPECT_FLOAT_EQ(after, chosen);
}

TEST(PenInherit, TheLeadingComesWithTheInheritedSize) {
  Paper paper;
  beginBare(paper);
  paper.pen.inherit({0, 0, 0, 1}, typeAt(40));
  paper.end();
  EXPECT_FLOAT_EQ(paper.pen.textLeading(), 50.0f);
}

TEST(PenInherit, ThePairIsRememberedForWhateverIsSeededFromThePen) {
  Pen fresh;
  // A pen nobody told: black ink and the initial type, so whatever is
  // seeded from one reads a usable pair whether a host spoke or not.
  EXPECT_EQ(fresh.inheritedInk(), SkColor4f({0, 0, 0, 1}));
  EXPECT_TRUE(fresh.inheritedFont().size.has_value());

  Paper paper;
  paper.begin();
  paper.pen.inherit({0.25f, 0.5f, 0.75f, 1}, typeAt(18));
  paper.end();
  EXPECT_EQ(paper.pen.inheritedInk(), SkColor4f({0.25f, 0.5f, 0.75f, 1}));
  EXPECT_FLOAT_EQ(paper.pen.inheritedFont().size.value().value, 18.0f);
}

TEST(PenInherit, APenNobodyTellsKeepsP5sOwnDefaults) {
  Paper paper;
  paper.begin();
  paper.pen.strokeWeight(0);
  paper.pen.rect(0, 0, 20, 20);
  paper.end();
  EXPECT_EQ(paper.pixel(10, 10), SK_ColorWHITE);
}

}  // namespace
