/** @file
 * The tile mechanism and the stock generators: a tile bakes once and is
 * shared by copies, reseeding a copy leaves the original's bake alone, the
 * mapping enters equality without a rebake, a checker tiles seamlessly, a
 * sequence paints its runs in order, and grid lines take a pitch per axis.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/pattern/Tile.h>
#include <sigilmaterial/skia/Painted.h>
#include <sigilmaterial/skia/Texture.h>

using namespace sigil::material;

namespace {

SkBitmap fill(const Texture& t, int w, int h) {
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(w, h));
  SkCanvas canvas(bm);
  canvas.clear(SK_ColorTRANSPARENT);
  SkPaint paint;
  paint.setShader(skia::shader(t));
  canvas.drawPaint(paint);
  return bm;
}

}  // namespace

TEST(Tile, BakesOnceAndCopiesShareTheBake) {
  int draws = 0;
  pattern::Tile t =
      pattern::Tile::of({4, 4}, skia::painted([&](SkCanvas& c, SkSize, uint32_t) {
        ++draws;
        c.clear(SK_ColorRED);
      }));
  EXPECT_FALSE(t.baked());
  const sk_sp<SkImage> first = skia::image(t.texture());
  EXPECT_TRUE(t.baked());
  EXPECT_EQ(draws, 1);
  pattern::Tile copy = t;
  EXPECT_EQ(skia::image(copy.texture()).get(), first.get());
  EXPECT_EQ(draws, 1);
  EXPECT_EQ(copy, t);
  // Reseeding the COPY leaves the original's bake alone.
  copy.seed(7);
  EXPECT_TRUE(t.baked());
  EXPECT_FALSE(copy.baked());
  EXPECT_FALSE(copy == t);
  skia::image(copy.texture());
  EXPECT_EQ(draws, 2);
  // Reseeding the sole holder rebakes in place.
  t.seed(9);
  EXPECT_FALSE(t.baked());
  EXPECT_EQ(t.currentSeed(), 9u);
}

TEST(Tile, MappingIsPerObjectAndNeverRebakes) {
  int draws = 0;
  pattern::Tile t =
      pattern::Tile::of({4, 4}, skia::painted([&](SkCanvas& c, SkSize, uint32_t) {
        ++draws;
        c.clear(SK_ColorRED);
      }));
  skia::image(t.texture());
  pattern::Tile turned = t;
  turned.rotate(45).scale(2).offset({3, 1});
  EXPECT_FALSE(turned == t);
  EXPECT_TRUE(turned.baked());
  EXPECT_EQ(draws, 1);
  EXPECT_FLOAT_EQ(turned.mapping()[2][0], 3);
  const Texture tex = turned.texture();
  EXPECT_EQ(tex.tileX(), Repeat::Repeat);
  EXPECT_EQ(tex.uv(), turned.mapping());
  EXPECT_EQ(t.texture(), pattern::Tile(t).texture());
}

TEST(StockTiles, CheckerTilesSeamlessly) {
  const pattern::Tile checker = pattern::checker(4, {1, 0, 0, 1}, {0, 0, 1, 1});
  const SkBitmap bm =
      fill(checker.texture().sampling(Sampling::Nearest), 16, 16);
  EXPECT_EQ(bm.getColor(1, 1), SK_ColorRED);
  EXPECT_EQ(bm.getColor(5, 1), SK_ColorBLUE);
  EXPECT_EQ(bm.getColor(9, 1), SK_ColorRED);  // the repeat
  EXPECT_EQ(bm.getColor(13, 5), SK_ColorRED);
}

TEST(StockTiles, SequencePaintsRunsInOrderAndPhaseSlides) {
  const pattern::Tile runs = pattern::sequence(
      {{2, {1, 0, 0, 1}}, {3, {0, 1, 0, 1}}, {1, {0, 0, 1, 1}}});
  EXPECT_EQ(runs.size(), glm::vec2(6, 8));
  const SkBitmap bm =
      fill(runs.texture().sampling(Sampling::Nearest), 12, 2);
  EXPECT_EQ(bm.getColor(0, 0), SK_ColorRED);
  EXPECT_EQ(bm.getColor(2, 0), SK_ColorGREEN);
  EXPECT_EQ(bm.getColor(5, 0), SK_ColorBLUE);
  EXPECT_EQ(bm.getColor(6, 0), SK_ColorRED);
  const pattern::Tile slid = pattern::sequence(
      {{2, {1, 0, 0, 1}}, {3, {0, 1, 0, 1}}, {1, {0, 0, 1, 1}}}, 2);
  EXPECT_EQ(
      fill(slid.texture().sampling(Sampling::Nearest), 6, 2).getColor(0, 0),
      SK_ColorGREEN);
  // DOWN THE TILE is the same sett turned, and it is the same run order
  // read along y — a sett is woven both ways, and the tile has to carry
  // the second one rather than leave it to a rotation of the first.
  const pattern::Tile down = pattern::sequence(
      {{2, {1, 0, 0, 1}}, {3, {0, 1, 0, 1}}, {1, {0, 0, 1, 1}}}, 0,
      pattern::Axis::V);
  EXPECT_EQ(down.size(), glm::vec2(8, 6));
  const SkBitmap vertical =
      fill(down.texture().sampling(Sampling::Nearest), 2, 12);
  EXPECT_EQ(vertical.getColor(0, 0), SK_ColorRED);
  EXPECT_EQ(vertical.getColor(0, 2), SK_ColorGREEN);
  EXPECT_EQ(vertical.getColor(0, 5), SK_ColorBLUE);
  EXPECT_EQ(vertical.getColor(0, 6), SK_ColorRED);
  // …and nothing varies across it, which is what makes it a sett and not
  // a diagonal.
  EXPECT_EQ(vertical.getColor(1, 2), SK_ColorGREEN);

  // No positive run: draws nothing.
  EXPECT_EQ(fill(pattern::sequence({{0, {1, 0, 0, 1}}}).texture(), 4, 4)
                .getColor(1, 1),
            0u);
}

TEST(StockTiles, GridLinesTakeATwoAxisPitchAndSpeckleReseeds) {
  const pattern::Tile grid = pattern::gridLines(8, 4, 1, {1, 1, 1, 1});
  EXPECT_EQ(grid.size(), glm::vec2(8, 4));
  const SkBitmap bm =
      fill(grid.texture().sampling(Sampling::Nearest), 16, 8);
  EXPECT_EQ(bm.getColor(0, 2) & 0xff000000, 0xff000000u);  // the vertical
  EXPECT_EQ(bm.getColor(3, 0) & 0xff000000, 0xff000000u);  // the horizontal
  EXPECT_EQ(bm.getColor(3, 2) & 0xff000000, 0u);
  EXPECT_EQ(bm.getColor(8, 6) & 0xff000000, 0xff000000u);  // the repeat
  pattern::Tile speck = pattern::speckle(32, 20, 1, 2, {{1, 1, 1, 1}});
  const sk_sp<SkImage> a = skia::image(speck.texture());
  speck.seed(2);
  const sk_sp<SkImage> b = skia::image(speck.texture());
  EXPECT_NE(a.get(), b.get());
  EXPECT_TRUE(skia::image(pattern::halftone(6, 2, {0, 0, 0, 1}).texture()));
  EXPECT_TRUE(skia::image(pattern::stripes(2, 2, {0, 0, 0, 1}).texture()));
}
