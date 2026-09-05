/** @file
 * The grain source: a texture that stands still in the pen's space, or
 * one that rides each stamp.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkImage.h>
#include <sigildraw/Draw.h>
#include <sigildraw/brush/Deposit.h>

#include <array>

#include "support/Paper.h"

namespace {

namespace brush = sigil::draw::brush;
using namespace sigil::draw;
using sigil::draw::testing::Paper;

/** A tile whose left half is white and right half black: eight pixels
 *  across, so at a scale of two its period in the pen's space is
 *  sixteen. */
sk_sp<SkImage> stripes() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(8, 8, true);
  bitmap.eraseColor(SK_ColorWHITE);
  for (int y = 0; y < 8; ++y)
    for (int x = 4; x < 8; ++x) *bitmap.getAddr32(x, y) = SK_ColorBLACK;
  bitmap.setImmutable();
  return SkImages::RasterFromBitmap(bitmap);
}

sk_sp<SkImage> block(int side) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(side, side, true);
  bitmap.eraseColor(SK_ColorWHITE);
  bitmap.setImmutable();
  return SkImages::RasterFromBitmap(bitmap);
}

/** A shape tool that stamps a plain square at full load, so what the
 *  pixels say is the grain and nothing else. */
brush::Tool stamper() {
  brush::Tool tool = brush::marker(SkColors::kBlack, 32.0f);
  tool.tip = brush::Tip::Image;
  tool.opacity = 1.0f;
  tool.markerTip = false;
  tool.pressure = {1, 1, 1};
  tool.pressure.variation.reset();
  tool.sizeJitter = 0.0f;
  tool.opacityJitter = 0.0f;
  tool.rotation = brush::Rotation::Fixed;
  tool.shape = brush::Shape{.image = block(16), .mask = brush::ImageMask::Alpha};
  return tool;
}

/** Two stamps twenty-four apart — one and a half periods of the grain,
 *  so a texture fixed in the pen's space meets them at opposite phases
 *  and a texture riding each stamp meets them at the same one. */
constexpr std::array<brush::Dab, 2> kPair{{
    {.position = {40, 40}},
    {.position = {64, 40}},
}};

TEST(Grain, StrokeSpaceStandsStillInThePensSpace) {
  Paper paper(120, 80, SK_ColorWHITE);
  paper.begin();
  brush::Tool tool = stamper();
  tool.grain = brush::Grain{.image = stripes(),
                            .space = brush::GrainSpace::Stroke,
                            .scale = 2.0f};
  brush::deposit(paper.pen, tool, kPair);
  paper.end();

  const SkBitmap pixels = paper.pixels();
  // One period apart inside the first stamp: the same phase, so the
  // same amount of pigment survives.
  EXPECT_EQ(pixels.getColor(30, 40), pixels.getColor(46, 40));
  // Half a period apart: the opposite phase.
  EXPECT_NE(pixels.getColor(30, 40), pixels.getColor(38, 40));
  // The same offset into each of the two stamps meets a different phase,
  // because the texture belongs to the surface and not to the stamp.
  EXPECT_NE(pixels.getColor(46, 40), pixels.getColor(70, 40));
}

TEST(Grain, DabSpaceRidesTheStamp) {
  Paper paper(120, 80, SK_ColorWHITE);
  paper.begin();
  brush::Tool tool = stamper();
  tool.grain = brush::Grain{.image = stripes(),
                            .space = brush::GrainSpace::Dab,
                            .scale = 2.0f};
  brush::deposit(paper.pen, tool, kPair);
  paper.end();

  const SkBitmap pixels = paper.pixels();
  // The same offset into each stamp meets the same phase: the texture
  // travelled with the mark.
  EXPECT_EQ(pixels.getColor(46, 40), pixels.getColor(70, 40));
  EXPECT_NE(pixels.getColor(46, 40), pixels.getColor(38, 40));
}

TEST(Grain, DepthDecidesHowMuchTheTextureMayTakeAway) {
  const auto markAt = [](float depth) {
    Paper paper(120, 80, SK_ColorWHITE);
    paper.begin();
    brush::Tool tool = stamper();
    tool.grain = brush::Grain{.image = stripes(),
                              .space = brush::GrainSpace::Stroke,
                              .scale = 2.0f,
                              .depth = depth};
    const std::array<brush::Dab, 1> one{{{.position = {40, 40}}}};
    brush::deposit(paper.pen, tool, one);
    paper.end();
    return paper.pixels().getColor(46, 40);
  };

  // At full depth the texture's black half erases the mark; at no depth
  // the mark is untouched wherever the texture is.
  EXPECT_EQ(markAt(1.0f), SK_ColorWHITE);
  EXPECT_EQ(markAt(0.0f), SK_ColorBLACK);
  EXPECT_NE(markAt(0.5f), markAt(1.0f));
  EXPECT_NE(markAt(0.5f), markAt(0.0f));
}

TEST(Grain, AProceduralTipTakesItsGrainStandingStill) {
  Paper paper(200, 80, SK_ColorWHITE);
  paper.begin();
  brush::Tool tool = brush::marker(SkColors::kBlack, 24.0f);
  tool.tip = brush::Tip::Nib;
  tool.opacity = 1.0f;
  tool.markerTip = false;
  tool.scatter = 0.0f;
  tool.pressure = {1, 1, 1};
  tool.pressure.variation.reset();
  tool.sizeJitter = 0.0f;
  tool.opacityJitter = 0.0f;
  tool.spacingJitter = 0.0f;
  tool.noise = 0.0f;
  tool.grain = brush::Grain{.image = stripes(),
                            .space = brush::GrainSpace::Dab,
                            .scale = 2.0f};
  brush::line(paper.pen, tool, {20, 40}, {180, 40});
  paper.end();

  const SkBitmap pixels = paper.pixels();
  // A nib deposits as one sprite batch, so a dab-space grain has no
  // stamp to ride and lands in the pen's space: one period apart is the
  // same phase, half a period apart is not.
  const bool erasedAt98 = pixels.getColor(98, 40) == SK_ColorWHITE;
  const bool erasedAt114 = pixels.getColor(114, 40) == SK_ColorWHITE;
  const bool erasedAt106 = pixels.getColor(106, 40) == SK_ColorWHITE;
  EXPECT_EQ(erasedAt98, erasedAt114);
  EXPECT_NE(erasedAt98, erasedAt106);
}

}  // namespace
