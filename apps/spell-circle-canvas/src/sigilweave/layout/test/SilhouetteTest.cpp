/** @file
 * The silhouettes an exclusion flow subtracts: the margin as a disc rather
 * than a square, an image's own alpha with the tolerance that decides what
 * counts as ink, and a shape that moves being re-answered every pass.
 */

#include <gtest/gtest.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPathBuilder.h>

#include <cmath>
#include <vector>

#include "support/LayoutSupport.h"

using namespace sigil::weave;
using namespace sigil::weave::test;

namespace {

constexpr float kSide = 400;

/// The half-plane x + y >= kSide: one straight boundary, at 45 degrees, so
/// the standoff a margin buys is a pure perpendicular distance.
SkPath diagonalHalfPlane() {
  SkPathBuilder wedge;
  wedge.moveTo(kSide, 0);
  wedge.lineTo(kSide, kSide);
  wedge.lineTo(0, kSide);
  wedge.close();
  return wedge.detach();
}

/// Where the free interval ends on the band that starts at `bandStart`.
float freeEndAt(ExclusionFlow& flow, int index, float pitch) {
  std::vector<LineInterval> out;
  if (!flow.lineIntervals(index, pitch, pitch * 0.8f, out) || out.empty())
    return -1;
  return out.front().origin.x() + out.front().length;
}

/// An image whose alpha ramps left to right across `width`, so the column
/// where a tolerance cuts is a number the caller can compute.
sk_sp<SkImage> alphaRamp(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(width, height));
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x) {
      const uint8_t alpha =
          (uint8_t)std::lround(255.0 * x / std::max(width - 1, 1));
      // Premultiplied black: the colour is irrelevant, the alpha is the
      // silhouette.
      *bitmap.getAddr32(x, y) = (uint32_t)alpha << 24;
    }
  bitmap.setImmutable();
  return SkImages::RasterFromBitmap(bitmap);
}

}  // namespace

TEST(Silhouette, AMarginOnADiagonalEdgeIsTheDistanceItAsksForAndNotItsDiagonal) {
  constexpr float kMargin = 20;
  constexpr float kPitch = 4;
  constexpr int kBand = 25;  // the band [100, 104]

  ExclusionFlow bare(SkRect::MakeWH(kSide, kSide));
  bare.exclusions().push_back({silhouette::path(diagonalHalfPlane())});
  ExclusionFlow stood(SkRect::MakeWH(kSide, kSide));
  stood.exclusions().push_back(
      {silhouette::path(diagonalHalfPlane()), kMargin});

  const float bareEnd = freeEndAt(bare, kBand, kPitch);
  const float stoodEnd = freeEndAt(stood, kBand, kPitch);
  ASSERT_GT(bareEnd, 0);
  ASSERT_GT(stoodEnd, 0);

  // The band's own depth is what the bare answer already gives up, so the
  // standoff is what the margin took ON TOP of it, measured perpendicular
  // to the edge — which on a 45-degree edge is the along-distance over
  // root two.
  const float standoff = (bareEnd - stoodEnd) / std::sqrt(2.0f);
  EXPECT_NEAR(standoff, kMargin, 1.5f)
      << "the margin is a disc: exactly what was asked, perpendicular to "
         "the edge";
  EXPECT_LT(standoff, kMargin * std::sqrt(2.0f) - 3.0f)
      << "a square dilation would stand the text off by margin times root "
         "two on this edge";
}

TEST(Silhouette, ARectanglesCornerIsRoundedByItsMargin) {
  constexpr float kMargin = 30;
  const SkRect block = SkRect::MakeXYWH(200, 100, 100, 100);
  ExclusionFlow flow(SkRect::MakeWH(kSide, kSide));
  flow.exclusions().push_back({silhouette::rectangle(block), kMargin});

  std::vector<LineInterval> out;
  // A band level with the rectangle: the whole margin stands beside it.
  ASSERT_TRUE(flow.lineIntervals(30, 5, 4, out));  // band [150, 155]
  ASSERT_FALSE(out.empty());
  EXPECT_NEAR(out.front().length, block.left() - kMargin, 0.5f);

  // A band most of the way up the margin above it: the disc has almost run
  // out, so far less than the margin stands beside the corner.
  ASSERT_TRUE(flow.lineIntervals(15, 5, 4, out));  // band [75, 80]
  ASSERT_FALSE(out.empty());
  EXPECT_GT(out.front().length, block.left() - kMargin);
  EXPECT_LT(out.front().length, block.left());
}

TEST(Silhouette, ASoftAlphaEdgeAdmitsWordsUpToTheTolerance) {
  const sk_sp<SkImage> ramp = alphaRamp(100, 100);
  ASSERT_TRUE(ramp);
  const SkRect box = SkRect::MakeXYWH(0, 0, 100, 100);

  const auto freeEndAtThreshold = [&](float threshold) {
    ExclusionFlow flow(SkRect::MakeWH(200, 100));
    flow.exclusions().push_back({silhouette::coverage(ramp, box, threshold)});
    return freeEndAt(flow, 10, 4);
  };

  // The ramp is inside where its alpha exceeds the tolerance, so the free
  // room ahead of it is that fraction of the picture's width.
  EXPECT_NEAR(freeEndAtThreshold(0.25f), 25.0f, 3.0f);
  EXPECT_NEAR(freeEndAtThreshold(0.5f), 50.0f, 3.0f);
  EXPECT_NEAR(freeEndAtThreshold(0.75f), 75.0f, 3.0f);
}

TEST(Silhouette, AMovingShapeIsAnsweredWhereItStandsThisPass) {
  ExclusionFlow flow(SkRect::MakeWH(kSide, kSide));
  flow.exclusions().push_back({silhouette::path(diagonalHalfPlane()), 8});

  const float atRest = freeEndAt(flow, 25, 4);
  ASSERT_GT(atRest, 0);
  // Rigid motion: the silhouette keeps everything it measured and the flow
  // simply asks it about a band 40 further back.
  flow.exclusions()[0].offset = {40, 0};
  EXPECT_NEAR(freeEndAt(flow, 25, 4), atRest + 40.0f, 0.5f);
  flow.exclusions()[0].offset = {-30, 0};
  EXPECT_NEAR(freeEndAt(flow, 25, 4), atRest - 30.0f, 0.5f);
}
