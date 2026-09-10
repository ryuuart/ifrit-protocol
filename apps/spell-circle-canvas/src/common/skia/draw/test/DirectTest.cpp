/** @file
 * The band split a nine-slice decomposes into: divs cut the source into
 * alternating fixed and stretchable intervals starting FIXED, the
 * stretchable ones share whatever destination space is left, and a
 * destination too small for the fixed sum scales the fixed bands down
 * instead of overflowing.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkSurface.h>
#include <sigilskia/draw/Direct.h>

#include <span>
#include <vector>

using namespace sigil::skia::draw;

namespace {

/** The edges for one axis, as the decomposition computes them. */
struct Split {
  std::vector<float> src, dst;
};

Split edgesOf(const std::vector<int>& divs, float srcLen, float dstLen,
              float density = 1.0f) {
  Split s;
  detail::latticeEdges(divs, srcLen, dstLen, s.src, s.dst, density);
  return s;
}

}  // namespace

TEST(LatticeEdges, TheBandsAlternateFromFixedAndSpanTheWholeAxis) {
  // A 90 px source cut at 30 and 60: fixed [0,30), stretch [30,60),
  // fixed [60,90).
  const Split s = edgesOf({30, 60}, 90.0f, 200.0f);
  ASSERT_EQ(s.src.size(), 4u);
  EXPECT_FLOAT_EQ(s.src.front(), 0.0f);
  EXPECT_FLOAT_EQ(s.src.back(), 90.0f);
  ASSERT_EQ(s.dst.size(), 4u);
  EXPECT_FLOAT_EQ(s.dst.front(), 0.0f);
  EXPECT_FLOAT_EQ(s.dst.back(), 200.0f);
  // The two fixed bands keep their source size; the stretch band absorbs
  // the rest, which is the whole point of a nine-slice.
  EXPECT_FLOAT_EQ(s.dst[1] - s.dst[0], 30.0f);
  EXPECT_FLOAT_EQ(s.dst[3] - s.dst[2], 30.0f);
  EXPECT_FLOAT_EQ(s.dst[2] - s.dst[1], 140.0f);
}

TEST(LatticeEdges, ADestinationSmallerThanTheFixedSumScalesTheFixedBands) {
  // 60 px of fixed corner into a 30 px destination: the corners halve
  // rather than overflowing, and the stretch band collapses.
  const Split s = edgesOf({30, 60}, 90.0f, 30.0f);
  EXPECT_FLOAT_EQ(s.dst.back(), 30.0f);
  EXPECT_FLOAT_EQ(s.dst[1] - s.dst[0], 15.0f);
  EXPECT_FLOAT_EQ(s.dst[2] - s.dst[1], 0.0f);
  EXPECT_FLOAT_EQ(s.dst[3] - s.dst[2], 15.0f);
}

TEST(LatticeEdges, DensityShrinksTheFixedBandsAndTheStretchAbsorbsIt) {
  // A frame authored at 2x: its corners land at half their pixel count,
  // sharp on a 2x device instead of twice the intended width.
  const Split s = edgesOf({30, 60}, 90.0f, 200.0f, 2.0f);
  EXPECT_FLOAT_EQ(s.dst[1] - s.dst[0], 15.0f);
  EXPECT_FLOAT_EQ(s.dst[3] - s.dst[2], 15.0f);
  EXPECT_FLOAT_EQ(s.dst[2] - s.dst[1], 170.0f);
  EXPECT_FLOAT_EQ(s.dst.back(), 200.0f);
  // A non-positive density is the caller having no opinion, not a
  // degenerate frame.
  EXPECT_FLOAT_EQ(edgesOf({30, 60}, 90.0f, 200.0f, 0.0f).dst[1], 30.0f);
}

TEST(LatticeEdges, ADivOutsideTheSourceIsClampedIntoIt) {
  // A caller's div past the image's own width would otherwise emit a band
  // that samples nothing.
  const Split s = edgesOf({-10, 500}, 90.0f, 90.0f);
  EXPECT_FLOAT_EQ(s.src[1], 0.0f);
  EXPECT_FLOAT_EQ(s.src[2], 90.0f);
  EXPECT_FLOAT_EQ(s.dst.back(), 90.0f);
}

TEST(LatticeEdges, AnAxisWithNoDivsIsOneStretchableBand) {
  const Split s = edgesOf({}, 40.0f, 100.0f);
  ASSERT_EQ(s.src.size(), 2u);
  ASSERT_EQ(s.dst.size(), 2u);
  // An undivided axis fills its destination, the way an image drawn to a
  // rect does. Read as one FIXED band it would instead leave the rest of
  // the destination unclaimed — and a lattice with neither axis divided
  // is a plain image draw, so the same lattice would stretch or not
  // depending on what the other axis carried.
  EXPECT_FLOAT_EQ(s.dst.back(), 100.0f);
  // …and it is still one band: nothing is cut where nothing was divided.
  EXPECT_FLOAT_EQ(s.dst.front(), 0.0f);
}

// ---------------------------------------------------------------------------
// The two draws over the split: a nine-slice and a sprite sheet, decomposed
// onto a raster canvas — no device, no context, which is the point of the
// feature standing apart from the bring-up.

namespace {

/** A 4x4 sheet: the left half white, the right half red, so a sprite that
 *  samples one half is told apart from one that samples the other. */
sk_sp<SkImage> twoHalves() {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(4, 4));
  bitmap.eraseArea(SkIRect::MakeLTRB(0, 0, 2, 4), SK_ColorWHITE);
  bitmap.eraseArea(SkIRect::MakeLTRB(2, 0, 4, 4), SK_ColorRED);
  bitmap.setImmutable();
  return bitmap.asImage();
}

/** A canvas over pixels a case can read back. */
struct Raster {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 200));
  SkCanvas& canvas() { return *surface->getCanvas(); }
  SkColor pixel(int x, int y) {
    SkBitmap out;
    out.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
    surface->readPixels(out, x, y);
    return out.getColor(0, 0);
  }
};

}  // namespace

TEST(DirectDraws, ANineSliceKeepsItsCornersAndStretchesTheMiddle) {
  Raster raster;
  raster.canvas().clear(SK_ColorBLACK);
  Promoted cache;
  drawLattice(raster.canvas(), cache, twoHalves(), {2}, {},
              SkRect::MakeXYWH(0, 0, 100, 100), SkFilterMode::kNearest);
  // The x axis is cut at 2: a fixed white band 2 units wide, then the red
  // half stretched over the rest. The y axis is undivided, so it fills.
  EXPECT_EQ(raster.pixel(1, 50), SK_ColorWHITE);
  EXPECT_EQ(raster.pixel(60, 50), SK_ColorRED);
  EXPECT_EQ(raster.pixel(60, 99), SK_ColorRED) << "the undivided axis fills";
}

TEST(DirectDraws, EverySpriteIsDrawnAcrossTheChunkBoundary) {
  // One vertex list indexes at most 16383 sprites, so a bigger run is cut
  // into chunks — and the LAST chunk is the one an off-by-one loses. The
  // run is one sprite past the cut, with the last one somewhere else, so
  // a lost tail is a pixel that is not there.
  constexpr size_t kCount = 16001;
  std::vector<SkRSXform> xforms(kCount,
                                SkRSXform::MakeFromRadians(1, 0, 10, 10, 0, 0));
  std::vector<SkRect> tex(kCount, SkRect::MakeWH(2, 4));  // the white half
  xforms.back() = SkRSXform::MakeFromRadians(1, 0, 150, 150, 0, 0);
  tex.back() = SkRect::MakeLTRB(2, 0, 4, 4);  // …and the red one

  Raster raster;
  raster.canvas().clear(SK_ColorBLACK);
  Promoted cache;
  drawSpriteAtlas(raster.canvas(), cache, twoHalves(),
                  SpriteBatch{.xforms = xforms, .tex = tex},
                  SkSamplingOptions());
  EXPECT_EQ(raster.pixel(11, 11), SK_ColorWHITE) << "the first chunk";
  EXPECT_EQ(raster.pixel(151, 151), SK_ColorRED) << "…and the tail";
}

TEST(DirectDraws, APerSpriteSizeScalesTheQuadTheXformCannot) {
  Raster raster;
  raster.canvas().clear(SK_ColorBLACK);
  Promoted cache;
  const SkRSXform xform = SkRSXform::MakeFromRadians(1, 0, 20, 20, 0, 0);
  const SkRect tex = SkRect::MakeWH(2, 4);
  // The quad is centred on the cell, so a 2x4 cell at (20, 20) has its
  // centre at (21, 22): twenty times as wide is 40 units across it, and
  // its own height stands.
  const SkSize size{20.0f, 1.0f};
  drawSpriteAtlas(
      raster.canvas(), cache, twoHalves(),
      SpriteBatch{.xforms = {&xform, 1}, .tex = {&tex, 1}, .sizes = {&size, 1}},
      SkSamplingOptions(), SkBlendMode::kSrcOver);
  EXPECT_EQ(raster.pixel(35, 22), SK_ColorWHITE) << "wide";
  EXPECT_EQ(raster.pixel(35, 30), SK_ColorBLACK) << "…and not tall";
  EXPECT_EQ(raster.pixel(45, 22), SK_ColorBLACK) << "…and bounded";
}

TEST(DirectDraws, ABatchWithAShortLaneDrawsNothingRatherThanReadingPastIt) {
  // The four lanes were four pointers and a count, with nothing holding
  // them to one length. As one value the batch can be ASKED, and a draw
  // that cannot trust its lanes refuses the whole batch: the alternative
  // is reading past the end of the short one, which draws a sprite out of
  // whatever memory followed it.
  const std::vector<SkRSXform> xforms(
      4, SkRSXform::MakeFromRadians(1, 0, 10, 10, 0, 0));
  const std::vector<SkRect> tex(4, SkRect::MakeWH(2, 4));
  const std::vector<SkColor> colors(4, SK_ColorWHITE);
  const std::vector<SkSize> sizes(4, SkSize{1, 1});

  EXPECT_TRUE((SpriteBatch{.xforms = xforms, .tex = tex}.consistent()));
  EXPECT_FALSE((SpriteBatch{.xforms = xforms, .tex = std::span(tex).first(3)}
                    .consistent()));
  EXPECT_FALSE((SpriteBatch{
      .xforms = xforms, .tex = tex, .colors = std::span(colors).first(3)}
                    .consistent()));
  EXPECT_FALSE((SpriteBatch{
      .xforms = xforms, .tex = tex, .sizes = std::span(sizes).first(3)}
                    .consistent()));

  Raster raster;
  raster.canvas().clear(SK_ColorBLACK);
  Promoted cache;
  drawSpriteAtlas(raster.canvas(), cache, twoHalves(),
                  SpriteBatch{.xforms = xforms, .tex = std::span(tex).first(3)},
                  SkSamplingOptions());
  EXPECT_EQ(raster.pixel(11, 11), SK_ColorBLACK) << "nothing was drawn";
}

TEST(DirectDraws, ReadyHandsBackTheImageWhereThereIsNoRecorder) {
  // A raster canvas promotes nothing: the image is already what a draw
  // can sample, and the cache stays empty rather than holding a copy.
  Raster raster;
  Promoted cache;
  const sk_sp<SkImage> sheet = twoHalves();
  EXPECT_EQ(ready(cache, sheet, raster.canvas()), sheet);
  EXPECT_EQ(cache.image, nullptr);
  EXPECT_EQ(cache.sourceId, 0u);
  // …and a null image is answered with null rather than promoted.
  EXPECT_EQ(ready(cache, nullptr, raster.canvas()), nullptr);
}
