/** @file
 * The coverage mask and the distance field over it: the tolerance that
 * decides what counts as ink, the distance being the true Euclidean one on
 * the diagonal as well as on the axes, and the disc a threshold of it cuts.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkImageInfo.h>
#include <sigilimage/field/DistanceField.h>

#include <cmath>

using namespace sigil::image;

namespace {

/// An 8-bit alpha raster the caller paints by hand.
SkBitmap alphaRaster(int width, int height, uint8_t fill = 0) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeA8(width, height));
  bitmap.eraseColor(SkColorSetARGB(fill, 0, 0, 0));
  return bitmap;
}

}  // namespace

TEST(CoverageMask, TheThresholdIsWhatCountsAsInk) {
  SkBitmap raster = alphaRaster(4, 1);
  const uint8_t alphas[4] = {0, 64, 128, 255};
  for (int x = 0; x < 4; ++x) *raster.getAddr8(x, 0) = alphas[x];

  const Mask any = coverageMask(raster.pixmap(), 0.0f);
  EXPECT_FALSE(any.at(0, 0));
  EXPECT_TRUE(any.at(1, 0)) << "any paint at all is inside at a zero tolerance";

  const Mask half = coverageMask(raster.pixmap(), 0.5f);
  EXPECT_FALSE(half.at(1, 0));
  // Half is the rule an unantialiased rasteriser uses: the paint reached at
  // least half the pixel, which for an 8-bit alpha is 128 and up.
  EXPECT_TRUE(half.at(2, 0));
  EXPECT_TRUE(half.at(3, 0));
}

TEST(DistanceFieldOverAMask, MeasuresTheTrueEuclideanDistanceOffTheAxes) {
  SkBitmap raster = alphaRaster(9, 9);
  *raster.getAddr8(4, 4) = 255;
  const DistanceField field =
      distanceField(coverageMask(raster.pixmap(), 0.5f));

  EXPECT_FLOAT_EQ(field.at(4, 4), 0.0f);
  EXPECT_FLOAT_EQ(field.at(7, 4), 3.0f);
  // The diagonal is where a chamfer approximation and a square dilation
  // both go wrong: three across and four down is five, not seven.
  EXPECT_FLOAT_EQ(field.at(7, 8), 5.0f);
  EXPECT_NEAR(field.at(5, 5), std::sqrt(2.0f), 1e-5f);
}

TEST(DistanceFieldOverAMask,
     ThresholdingItDilatesTheShapeByADiscAndNotASquare) {
  // A quarter plane, so its boundary is one 45-degree edge. Every point at
  // distance m from that edge is m of PERPENDICULAR standoff — where a
  // square dilation would put the same point m·root-two away.
  constexpr int kSize = 64;
  SkBitmap raster = alphaRaster(kSize, kSize);
  for (int y = 0; y < kSize; ++y)
    for (int x = 0; x < kSize; ++x)
      if (x + y >= kSize) *raster.getAddr8(x, y) = 255;
  const DistanceField field =
      distanceField(coverageMask(raster.pixmap(), 0.5f));

  constexpr float kMargin = 8.0f;
  // The point (32, 32) is on the edge's own diagonal; step back along the
  // perpendicular by the margin and it must be exactly inside the dilation,
  // and a step further out of it.
  const float step = kMargin / std::sqrt(2.0f);
  const int inx = (int)std::lround(32 - step + 1),
            iny = (int)std::lround(32 - step + 1);
  const int outx = (int)std::lround(32 - step - 1),
            outy = (int)std::lround(32 - step - 1);
  EXPECT_LE(field.at(inx, iny), kMargin);
  EXPECT_GT(field.at(outx, outy), kMargin);
}

TEST(DistanceFieldOverAMask, ANonSquareRasterMeasuresTheSameEitherWayRound) {
  // A row pass and a column pass over one buffer is where a width used
  // where a height belongs hides: on a square raster the two strides
  // agree and the swap does not show. The same shape is measured in a
  // 5x2 raster and in its transpose, and every distance must match.
  SkBitmap wide = alphaRaster(5, 2);
  *wide.getAddr8(0, 0) = 255;
  SkBitmap tall = alphaRaster(2, 5);
  *tall.getAddr8(0, 0) = 255;
  const DistanceField across = distanceField(coverageMask(wide.pixmap(), 0.5f));
  const DistanceField down = distanceField(coverageMask(tall.pixmap(), 0.5f));
  ASSERT_EQ(across.width, 5);
  ASSERT_EQ(across.height, 2);
  ASSERT_EQ(down.width, 2);
  ASSERT_EQ(down.height, 5);
  for (int y = 0; y < 2; ++y)
    for (int x = 0; x < 5; ++x)
      EXPECT_NEAR(across.at(x, y), down.at(y, x), 1e-5f)
          << "at " << x << ", " << y;
  EXPECT_FLOAT_EQ(across.at(0, 0), 0.0f);
  EXPECT_FLOAT_EQ(across.at(4, 0), 4.0f);
  EXPECT_FLOAT_EQ(across.at(4, 1), std::sqrt(17.0f));
}

TEST(DistanceFieldOverAMask, OnePixelIsARasterLikeAnyOther) {
  SkBitmap covered = alphaRaster(1, 1, 255);
  const DistanceField inside =
      distanceField(coverageMask(covered.pixmap(), 0.5f));
  ASSERT_FALSE(inside.empty());
  EXPECT_FLOAT_EQ(inside.at(0, 0), 0.0f);
  EXPECT_EQ(inside.at(1, 0), DistanceField::kOutside);

  SkBitmap bare = alphaRaster(1, 1);
  const DistanceField empty = distanceField(coverageMask(bare.pixmap(), 0.5f));
  ASSERT_FALSE(empty.empty());
  EXPECT_EQ(empty.at(0, 0), DistanceField::kOutside);
}

TEST(DistanceFieldOverAMask, AMaskThatCoversEverythingIsZeroThroughout) {
  SkBitmap raster = alphaRaster(5, 3, 255);
  const DistanceField field =
      distanceField(coverageMask(raster.pixmap(), 0.5f));
  ASSERT_FALSE(field.empty());
  for (int y = 0; y < 3; ++y)
    for (int x = 0; x < 5; ++x)
      EXPECT_FLOAT_EQ(field.at(x, y), 0.0f) << "at " << x << ", " << y;
}

TEST(DistanceFieldOverAMask, AMaskThatCoversNothingIsEverywhereOutside) {
  SkBitmap raster = alphaRaster(8, 8);
  const DistanceField field =
      distanceField(coverageMask(raster.pixmap(), 0.5f));
  ASSERT_FALSE(field.empty());
  EXPECT_EQ(field.at(0, 0), DistanceField::kOutside);
  EXPECT_EQ(field.at(7, 7), DistanceField::kOutside);
}
