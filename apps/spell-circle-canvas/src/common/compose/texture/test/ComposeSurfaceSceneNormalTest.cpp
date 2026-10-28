/** @file
 * Where a surface scene writes a stated normal: a canvas fill places its
 * normal where it places its colour, a glyph ink restarts its normal on
 * each glyph with its colour, and a turned node's normal is written in the
 * page as the node is turned.
 */

#include <sigilmaterial/texture/Image.h>

#include <algorithm>
#include <cmath>
#include <utility>

#include "support/Rows.h"
#include "support/SurfaceSceneFixture.h"

namespace {

/** A picture split down its middle: its left half the encoding of a
 *  normal leaning right, its right half one leaning left, so the same
 *  texture read as the base colour and as the normal says in both maps
 *  which half stands where. It is @p width pixels across, or the unit
 *  square across where @p width is zero — the square a unit ink
 *  restarts on. */
material::Material splitPicture(int width) {
  const int across = width > 0 ? width : 64;
  SkBitmap pixels;
  pixels.allocN32Pixels(across, 8, true);
  pixels.erase(SkColorSetRGB(191, 128, 230),
               SkIRect::MakeXYWH(0, 0, across / 2, 8));
  pixels.erase(SkColorSetRGB(64, 128, 230),
               SkIRect::MakeXYWH(across / 2, 0, across / 2, 8));
  pixels.setImmutable();
  material::Texture texture(sigil::media::PixelSource(
      CountedPicture{std::make_shared<int>(0), pixels.asImage()}));
  texture.tile(material::Repeat::Pad).sampling(material::Sampling::Nearest);
  if (width == 0) {
    glm::mat3 toUnitSquare(1.0f);
    toUnitSquare[0][0] = 1.0f / across;
    toUnitSquare[1][1] = 1.0f / 8;
    texture.uv(toUnitSquare);
  }
  return material::image(std::move(texture));
}

/** One texture as the colour and as the normal of one surface. */
material::Material splitFinish(int width) {
  return material::from(splitPicture(width))
      .surface({.roughness = .5f, .normal = splitPicture(width)});
}

/** Which half of the split picture a base colour shows: one for the left,
 *  zero for the right, and one half for neither. */
float leftHalfOf(const SkColor4f& colour) {
  if (std::abs(colour.fR - 191 / 255.f) < .05f) return 1;
  if (std::abs(colour.fR - 64 / 255.f) < .05f) return 0;
  return .5f;
}

/** Which way an encoded normal leans: one to the right, zero to the left,
 *  one half upright. */
float leansRight(const SkColor4f& normal) {
  if (normal.fR > .65f) return 1;
  if (normal.fR < .35f) return 0;
  return .5f;
}

}  // namespace

TEST(ComposeSurfaceScene, EveryChannelStandsOnTheUnitItsColourDoes) {
  sigil::compose::test::checkRows({
      {"ACanvasFillPlacesItsNormalWhereItPlacesItsColour",
       [&] {
         // A card inset in the page, from 32 to 80 across, filled with a split
         // picture as wide as the canvas laid from the canvas's corner: the
         // split falls at 64, the canvas's middle, in the base colour and the
         // normal alike. Laid from the card's own corner it would fall at 96,
         // past the card, which would show only the left half.
         const auto pageWith = [](PaintBox over) {
           return box().width(kWidth).height(kHeight).children(
               {box()
                    .absolute()
                    .rect(32, 24, 48, 40)
                    .fill(splitFinish(kWidth), over)});
         };
         const auto scene = SurfaceScene::make({kWidth, kHeight}, fonts());
         ASSERT_NE(scene, nullptr);
         scene->render(pageWith(PaintBox::Canvas));
         SkBitmap base = pixelsOf(*scene, Role::BaseColor);
         SkBitmap normal = pixelsOf(*scene, Role::Normal);
         ASSERT_FALSE(base.empty());
         for (const int x : {40, 60, 68, 76}) {
           const float expected = x < 64 ? 1.0f : 0.0f;
           EXPECT_EQ(leftHalfOf(base.getColor4f(x, 44)), expected) << "x " << x;
           EXPECT_EQ(leansRight(normal.getColor4f(x, 44)), expected)
               << "x " << x;
         }
         // On the card's own box both maps move together with the card.
         scene->render(pageWith(PaintBox::Element), 1);
         base = pixelsOf(*scene, Role::BaseColor);
         normal = pixelsOf(*scene, Role::Normal);
         for (const int x : {40, 60, 68, 76}) {
           EXPECT_EQ(leftHalfOf(base.getColor4f(x, 44)), 1) << "x " << x;
           EXPECT_EQ(leansRight(normal.getColor4f(x, 44)), 1) << "x " << x;
         }
       }},
      {"AGlyphInkRestartsItsNormalOnEachGlyph",
       [&] {
         // Two capitals inked with the split picture restarting on each glyph:
         // every letter shows both halves, and wherever the base colour shows a
         // half the normal leans that half's way.
         const auto scene = SurfaceScene::make({kWidth, kHeight}, fonts());
         ASSERT_NE(scene, nullptr);
         scene->render(box().width(kWidth).height(kHeight).children(
             {text(u8"HH", whiteStyle(44))
                  .absolute()
                  .rect(0, 32, 128, 48)
                  .ink(splitFinish(0), PaintBox::Glyph)}));
         const SkBitmap base = pixelsOf(*scene, Role::BaseColor);
         const SkBitmap normal = pixelsOf(*scene, Role::Normal);
         ASSERT_FALSE(base.empty());
         // Each letter's columns, split where the second one begins: the inked
         // columns' middle.
         int first = kWidth, last = -1;
         for (int y = 32; y < 80; ++y)
           for (int x = 0; x < kWidth; ++x)
             if (base.getColor4f(x, y).fA > .99f) {
               first = std::min(first, x);
               last = std::max(last, x);
             }
         ASSERT_LT(first, last);
         const int between = (first + last) / 2;
         int halves[2][2] = {{0, 0}, {0, 0}};
         int agreeing = 0, inked = 0;
         for (int y = 32; y < 80; ++y)
           for (int x = 0; x < kWidth; ++x) {
             const SkColor4f colour = base.getColor4f(x, y);
             if (colour.fA < .99f) continue;
             const float half = leftHalfOf(colour);
             if (half == .5f) continue;
             ++inked;
             ++halves[x < between ? 0 : 1][half == 1 ? 0 : 1];
             if (leansRight(normal.getColor4f(x, y)) == half) ++agreeing;
           }
         ASSERT_GT(inked, 50);
         EXPECT_EQ(agreeing, inked);
         for (const auto& letter : halves) {
           EXPECT_GT(letter[0], 10) << "a letter shows no left half";
           EXPECT_GT(letter[1], 10) << "a letter shows no right half";
         }
       }},
  });
}

TEST(ComposeSurfaceScene, ANormalIsWrittenInThePageAsItsNodeIsTurned) {
  // A plate whose stated normal leans toward its own right: lying flat it
  // leans right on the page; turned a quarter clockwise its right runs
  // down the page, and so does the lean — green down, encoded.
  const auto normalOf = [&](float turn) {
    const auto scene = SurfaceScene::make({kWidth, kHeight}, fonts());
    scene->render(box().width(kWidth).height(kHeight).children(
        {box()
             .absolute()
             .rect(24, 46, 80, 20)
             .rotate(turn)
             .fill(material::from(material::Color{.6f, .6f, .6f, 1})
                       .surface({.roughness = .5f,
                                 .normal = material::from(
                                     material::Color{.75f, .5f, .9f, 1})}))}));
    return pixelsOf(*scene, Role::Normal).getColor4f(64, 56);
  };
  const SkColor4f flat = normalOf(0);
  EXPECT_GT(flat.fR, .6f);
  EXPECT_NEAR(flat.fG, .5f, .02f);
  const SkColor4f turned = normalOf(90);
  EXPECT_NEAR(turned.fR, .5f, .02f);
  EXPECT_LT(turned.fG, .4f);
  EXPECT_NEAR(turned.fB, flat.fB, .02f) << "the lean keeps its steepness";
}
