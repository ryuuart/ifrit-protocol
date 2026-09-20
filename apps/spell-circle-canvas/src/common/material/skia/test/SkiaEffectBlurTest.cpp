/** @file
 * THE TWO BLURS PAINTED OUT: the one whose sigma a parameter map varies
 * across the node, which must reach its own box and stop at the clip,
 * and the one whose sigma is bound, which must ride inside the pyramid
 * the effect declared rather than rebuilding it every frame.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>
#include <include/core/SkSurface.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/Paint.h>

#include <cmath>
#include <cstdint>
#include <memory>

using namespace sigil::material;

namespace {

/** A 32x32 white layer with a black square in the middle, painted
 *  through @p filter as one layer — the smallest picture a blur changes. */
SkBitmap squareThrough(const sk_sp<SkImageFilter>& filter) {
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(32, 32));
  SkCanvas canvas(bm);
  canvas.clear(SK_ColorTRANSPARENT);
  SkPaint layer;
  layer.setImageFilter(filter);
  canvas.saveLayer(nullptr, &layer);
  canvas.clear(SK_ColorWHITE);
  SkPaint ink;
  ink.setColor(SK_ColorBLACK);
  canvas.drawRect(SkRect::MakeXYWH(12, 12, 8, 8), ink);
  canvas.restore();
  return bm;
}

}  // namespace

TEST(SkiaEffect, AParameterBlurReachesItsOwnBoxAndNotTheClip) {
  // A runtime shader may write any pixel, so Skia treats a filter built
  // from one as covering the whole clip and hands it a clip-sized layer.
  // A parameter blur declares its reach instead: the box the map is
  // defined over, grown by the support of the largest level. Asked what
  // it touches under two clips of different sizes, the resolved filter
  // answers the same rectangle — the box and its reach — rather than
  // either clip.
  // A bound sigma is what makes the resolve build against the frame; an
  // unbound blur answers its store-time snapshot, which knows no box.
  skia::PaintFrame frame;
  frame.size = SkSize::Make(120, 120);
  choreograph::Output<float> sigma(5.0f);
  skia::Effect blur =
      skia::Effect::blur(skia::Paint::solid({1, 1, 1, 1}), 8.0f);
  blur.uniform("maxSigma", &sigma);
  const sk_sp<SkImageFilter> filter = blur.resolvedImageFilter(&frame);
  ASSERT_NE(filter, nullptr);
  const SkIRect small =
      filter->filterBounds(SkIRect::MakeWH(300, 300), SkMatrix::I(),
                           SkImageFilter::kForward_MapDirection, nullptr);
  const SkIRect large =
      filter->filterBounds(SkIRect::MakeWH(1200, 1200), SkMatrix::I(),
                           SkImageFilter::kForward_MapDirection, nullptr);
  EXPECT_EQ(small, large) << "the reach depends on the clip";
  EXPECT_NE(large, SkIRect::MakeWH(1200, 1200)) << "the reach is the clip";
  const SkIRect declared =
      SkRect::MakeWH(120, 120).makeOutset(24, 24).roundOut();
  EXPECT_EQ(large, declared);
}

TEST(SkiaEffect, ABoundBlurSigmaRidesInsideTheDeclaredPyramid) {
  // The declared range builds the pyramid once; a bound sigma re-wraps
  // only the mix, so two resolves at different sigmas share their blur
  // inputs by identity — which is what lets Skia's filter cache keep the
  // blurred layers between frames while the sigma breathes.
  choreograph::Output<float> sigma(2.0f);
  skia::Effect blur =
      skia::Effect::blur(skia::Paint::solid({1, 1, 1, 1}), 8.0f);
  blur.uniform("maxSigma", &sigma);
  EXPECT_TRUE(blur.isAnimated());
  const sk_sp<SkImageFilter> at2 = blur.resolvedImageFilter(nullptr);
  sigma = 6.0f;
  const sk_sp<SkImageFilter> at6 = blur.resolvedImageFilter(nullptr);
  ASSERT_NE(at2, nullptr);
  ASSERT_NE(at6, nullptr);
  EXPECT_NE(at2, at6);  // the mix is re-wrapped for the new sigma
  ASSERT_EQ(at2->countInputs(), 3);
  ASSERT_EQ(at6->countInputs(), 3);
  EXPECT_EQ(at2->getInput(1), at6->getInput(1));
  EXPECT_EQ(at2->getInput(2), at6->getInput(2));

  // Exact at a pass sigma: a white map bound to half the declared range
  // IS the half-range pass, so it paints what a blur declared at that
  // sigma with no binding paints.
  sigma = 4.0f;
  const SkBitmap ridden = squareThrough(blur.resolvedImageFilter(nullptr));
  const SkBitmap declared =
      squareThrough(skia::Effect::blur(skia::Paint::solid({1, 1, 1, 1}), 4.0f)
                        .resolvedImageFilter(nullptr));
  for (int y = 0; y < 32; ++y)
    for (int x = 0; x < 32; ++x)
      EXPECT_NEAR((int)SkColorGetR(ridden.getColor(x, y)),
                  (int)SkColorGetR(declared.getColor(x, y)), 1)
          << "at " << x << "," << y;
  // The edge of the square is softened, so the picture is a blur at all.
  EXPECT_GT(SkColorGetR(ridden.getColor(11, 16)), 0u);
  EXPECT_LT(SkColorGetR(ridden.getColor(11, 16)), 255u);

  // Above the declared range the sigma clamps to it: the top of the
  // pyramid is the widest the effect ever paints.
  sigma = 40.0f;
  const SkBitmap clamped = squareThrough(blur.resolvedImageFilter(nullptr));
  const SkBitmap top =
      squareThrough(skia::Effect::blur(skia::Paint::solid({1, 1, 1, 1}), 8.0f)
                        .resolvedImageFilter(nullptr));
  for (int y = 0; y < 32; ++y)
    for (int x = 0; x < 32; ++x)
      EXPECT_NEAR((int)SkColorGetR(clamped.getColor(x, y)),
                  (int)SkColorGetR(top.getColor(x, y)), 1)
          << "at " << x << "," << y;
}

