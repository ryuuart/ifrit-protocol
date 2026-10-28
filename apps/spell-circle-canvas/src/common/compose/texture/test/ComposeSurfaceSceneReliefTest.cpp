/** @file
 * A relief cut into a surface scene's normal map from the unit its ink
 * paints: per glyph and per word it differs only between letters, and a
 * span ink stating no unit cuts it from the glyphs the span paints.
 */

#include <cmath>
#include <utility>
#include <vector>

#include "support/Rows.h"
#include "support/SurfaceSceneFixture.h"

namespace {

/** The columns where row @p y of @p base is inked opaque, as runs of
 *  [first, last]. */
std::vector<std::pair<int, int>> inkedRuns(const SkBitmap& base, int y) {
  std::vector<std::pair<int, int>> runs;
  for (int x = 0; x < base.width(); ++x) {
    if (base.getColor4f(x, y).fA < .99f) continue;
    if (!runs.empty() && runs.back().second == x - 1)
      runs.back().second = x;
    else
      runs.push_back({x, x});
  }
  return runs;
}

}  // namespace

TEST(ComposeSurfaceScene, AReliefIsCutFromTheUnitItsInkPaints) {
  sigil::compose::test::checkRows({
      {"PerGlyphAndPerWordDifferOnlyBetweenLetters",
       [&] {
         // One grey that raises the unit it is painted into, inked on two
         // capitals restarting on each glyph and on each word: the shoulders at
         // the word's outer edges fall alike, and between the two letters the
         // word's one height runs across the gap where each glyph's falls away.
         const material::Material raised =
             material::from(material::Color{.6f, .6f, .6f, 1})
                 .surface({.roughness = .8f})
                 .relief({.shoulder = 16, .depth = .5f});
         const auto normalsUnder = [&](PaintBox unit) {
           const auto scene = SurfaceScene::make({kWidth, kHeight}, fonts());
           scene->render(box().width(kWidth).height(kHeight).children(
               {text(u8"HH", whiteStyle(64))
                    .absolute()
                    .rect(4, 16, 120, 80)
                    .ink(raised, unit)}));
           return std::pair{pixelsOf(*scene, Role::BaseColor),
                            pixelsOf(*scene, Role::Normal)};
         };
         const auto [glyphBase, glyphNormal] = normalsUnder(PaintBox::Glyph);
         const auto [wordBase, wordNormal] = normalsUnder(PaintBox::Word);
         ASSERT_FALSE(glyphBase.empty());
         // A row through the middle of both letters.
         int top = -1, bottom = -1;
         for (int y = 0; y < kHeight; ++y)
           if (!inkedRuns(glyphBase, y).empty()) {
             if (top < 0) top = y;
             bottom = y;
           }
         ASSERT_GE(top, 0);
         const int row = (top + bottom) / 2;
         const auto runs = inkedRuns(glyphBase, row);
         ASSERT_EQ(runs.size(), 2u);
         // The covered pixels are the same: a relief moves no ink.
         for (int x = 0; x < kWidth; ++x)
           EXPECT_EQ(glyphBase.getColor4f(x, row).fA > .5f,
                     wordBase.getColor4f(x, row).fA > .5f)
               << "x " << x;
         // The word's outer edges: the first letter's left side and the last
         // letter's right side lean alike under both units, and lean.
         const int outerLeft = runs[0].first + 1,
                   outerRight = runs[1].second - 1;
         EXPECT_LT(glyphNormal.getColor4f(outerLeft, row).fR, .42f);
         EXPECT_NEAR(glyphNormal.getColor4f(outerLeft, row).fR,
                     wordNormal.getColor4f(outerLeft, row).fR, .02f);
         EXPECT_GT(glyphNormal.getColor4f(outerRight, row).fR, .58f);
         EXPECT_NEAR(glyphNormal.getColor4f(outerRight, row).fR,
                     wordNormal.getColor4f(outerRight, row).fR, .02f);
         // Between the letters: each glyph falls to its own edge, so its
         // shoulder leans out more steeply than the word's, whose height the
         // neighbouring letter holds up across the gap.
         const int innerLeft = runs[0].second - 1,
                   innerRight = runs[1].first + 1;
         EXPECT_GT(glyphNormal.getColor4f(innerLeft, row).fR,
                   wordNormal.getColor4f(innerLeft, row).fR + .04f);
         EXPECT_LT(glyphNormal.getColor4f(innerRight, row).fR,
                   wordNormal.getColor4f(innerRight, row).fR - .04f);
       }},
      {"ASpanInkOnNoUnitCutsFromItsGlyphs",
       [&] {
         // The first word inked with a raised stone and no unit: the stretch of
         // glyphs the span paints is the unit its relief is cut from, so its
         // letters' edges lean in the normal map.
         const material::Material raised =
             material::from(material::Color{.6f, .6f, .6f, 1})
                 .surface({.roughness = .8f})
                 .relief({.shoulder = 6, .depth = .5f});
         const auto scene = SurfaceScene::make({kWidth, kHeight}, fonts());
         scene->render(box().width(kWidth).height(kHeight).children(
             {text(u8"HH", whiteStyle(64))
                  .absolute()
                  .rect(4, 16, 120, 80)
                  .span(sigil::weave::selectors::word(0),
                        SpanStyle().ink(raised))}));
         const SkBitmap base = pixelsOf(*scene, Role::BaseColor);
         const SkBitmap normal = pixelsOf(*scene, Role::Normal);
         ASSERT_FALSE(normal.empty());
         int leaning = 0;
         for (int y = 0; y < kHeight; ++y)
           for (int x = 0; x < kWidth; ++x)
             if (base.getColor4f(x, y).fA > .9f &&
                 std::abs(normal.getColor4f(x, y).fR - .5f) > .08f)
               ++leaning;
         EXPECT_GT(leaning, 20);
       }},
  });
}
