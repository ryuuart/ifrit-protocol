/** @file
 * The shaper shelf and the oscillating width law beside it: the stock
 * values over the leaf's deviation and profile seams. Each answers the
 * seam it plugs, compares by its own dials, and actually moves the mark it
 * is given.
 */

#include <gtest/gtest.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkRect.h>
#include <sigilgeometry/kit/Shapers.h>

using namespace sigil::geometry;

namespace {

TEST(Shapers, EveryStockShaperAnswersTheSeamAndComparesByItsDials) {
  static_assert(path::ShaperScheme<shapers::Wave>);
  static_assert(path::ShaperScheme<shapers::Zigzag>);
  static_assert(path::ShaperScheme<shapers::Jitter>);
  static_assert(path::ShaperScheme<shapers::Offset>);
  static_assert(path::ShaperScheme<shapers::Rounded>);
  static_assert(path::ShaperScheme<shapers::Chamfer>);
  static_assert(path::ShaperScheme<shapers::Square>);
  EXPECT_TRUE(shapers::wave(6, 40) == shapers::wave(6, 40));
  EXPECT_FALSE(shapers::wave(6, 40) == shapers::wave(6, 41));
  // Bleed is how far the deviation reaches, so a cull can grow by it.
  EXPECT_FLOAT_EQ(shapers::wave(6, 40).bleed(), 6.0f);
  EXPECT_FLOAT_EQ(shapers::zigzag(4, 24).bleed(), 4.0f);
  EXPECT_FLOAT_EQ(shapers::offset(-9).bleed(), 9.0f);
}

TEST(Shapers, EachOneActuallyMovesTheMarkItIsGiven) {
  SkPathBuilder b;
  b.moveTo(0, 50);
  b.lineTo(200, 50);
  const SkPath run = b.detach();
  // A wave and a zigzag swing the run off its own axis.
  EXPECT_GT(shapers::wave(8, 40).shape(run).getBounds().height(), 8.0f);
  EXPECT_GT(shapers::zigzag(8, 40).shape(run).getBounds().height(),
            8.0f);
  EXPECT_GT(shapers::square(8, 40).shape(run).getBounds().height(),
            8.0f);
  // An offset moves it bodily, LEFT of travel, which on a west-to-east
  // run is upward on screen.
  EXPECT_NEAR(shapers::offset(10).shape(run).getBounds().centerY(),
              40.0f, 1.5f);
  // A corner treatment over a straight run has no corner to treat.
  EXPECT_EQ(shapers::rounded(6).shape(run).getBounds(), run.getBounds());
}

TEST(Shapers, AChamferCutsEveryCornerOfAClosedRun) {
  // A corner treatment is a shaper like any other, so it takes whatever
  // outline a caller hands it rather than a shape value: a closed 100x100
  // square chamfered at 30 comes back as the octagon, corners cut away and
  // the interior kept.
  SkPathBuilder sq;
  sq.moveTo(0, 0).lineTo(100, 0).lineTo(100, 100).lineTo(0, 100).close();
  const SkPath oct = shapers::chamfered(30).shape(sq.detach());
  int vertices = 0, closes = 0;
  SkPath::Iter iter(oct, false);
  SkPoint pts[4];
  for (SkPath::Verb verb = iter.next(pts); verb != SkPath::kDone_Verb;
       verb = iter.next(pts)) {
    vertices += verb == SkPath::kLine_Verb;
    closes += verb == SkPath::kClose_Verb;
  }
  EXPECT_EQ(closes, 1);
  EXPECT_EQ(vertices, 8);  // eight cut edges, the close synthesising none
  EXPECT_FALSE(oct.contains(2, 2)) << "the corner is cut away";
  EXPECT_TRUE(oct.contains(50, 50));
}

TEST(Shapers, TheOscillatingWidthLawIsZeroMeanAndPlugsTheProfileSeam) {
  const path::Profile w = path::profile::wave(9, 50);
  EXPECT_NEAR(w.max(), 9.0f, 1e-4f) << "max() is what a cull is sized from";
  EXPECT_TRUE(w == path::profile::wave(9, 50));
  EXPECT_FALSE(w == path::profile::wave(9, 51));
  // Zero-mean: it goes both ways, which is what makes it a centreline and
  // not a band width.
  bool positive = false, negative = false;
  for (int k = 0; k <= 64; ++k) {
    const float v = w.across((float)k / 64.0f);
    positive = positive || v > 0.5f;
    negative = negative || v < -0.5f;
  }
  EXPECT_TRUE(positive && negative);
}

}  // namespace
