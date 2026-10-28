/** @file
 * The brushing a surface scene writes into its anisotropy map: a fill
 * brushed along its unit runs along the unit in the page however it is
 * turned, and a keyline or a ribbon brushed along itself follows its
 * outline.
 */

#include <sigilcompose/brush/Ribbons.h>

#include <cmath>
#include <utility>

#include "support/Rows.h"
#include "support/SurfaceSceneFixture.h"

TEST(ComposeSurfaceScene, ABrushingRunsAlongTheUnitItsMaterialReads) {
  sigil::compose::test::checkRows({
      {"AFillBrushedAlongItsBoxTurnsWithItInThePage",
       [&] {
         // A long strip turned a quarter clockwise on the page and brushed
         // along its unit: the brushing runs down the page — green down,
         // encoded — and a strip lying flat brushes across it.
         const auto brushingOf = [&](float turn) {
           const auto scene = SurfaceScene::make({kWidth, kHeight}, fonts());
           scene->render(box().width(kWidth).height(kHeight).children(
               {box()
                    .absolute()
                    .rect(24, 46, 80, 20)
                    .rotate(turn)
                    .fill(material::from(material::Color{.9f, .9f, .9f, 1})
                              .surface({.metallic = 1.0f, .roughness = .3f})
                              .brushed({.strength = .8f}))}));
           return pixelsOf(*scene, Role::Anisotropy);
         };
         expectNear(brushingOf(0).getColor4f(64, 56), 1, .5f, .8f);
         expectNear(brushingOf(90).getColor4f(64, 56), .5f, 0, .8f);
         // A quarter less, the strip leans down to the right and so does its
         // brushing.
         const SkColor4f leaning = brushingOf(45).getColor4f(64, 56);
         EXPECT_NEAR(leaning.fR, .5f + .5f * std::sqrt(.5f), .02f);
         EXPECT_NEAR(leaning.fG, .5f - .5f * std::sqrt(.5f), .02f);
       }},
      {"AKeylineAlongItsOutline",
       [&] {
         // A brushed keyline round a square: along the top and bottom it
         // brushes across the page, along the sides down it.
         const auto scene = SurfaceScene::make({kWidth, kHeight}, fonts());
         ASSERT_NE(scene, nullptr);
         scene->render(box().width(kWidth).height(kHeight).children(
             {box()
                  .absolute()
                  .rect(24, 16, 80, 80)
                  .stroke(material::from(material::Color{.9f, .9f, .9f, 1})
                              .surface({.metallic = 1.0f, .roughness = .3f})
                              .brushed({.strength = 1}),
                          {.width = 8,
                           .position = material::StrokePosition::Center})}));
         const SkBitmap brushing = pixelsOf(*scene, Role::Anisotropy);
         ASSERT_FALSE(brushing.empty());
         for (const auto& [x, y] : {std::pair{64, 16}, std::pair{64, 96}}) {
           const SkColor4f across = brushing.getColor4f(x, y);
           EXPECT_NEAR(std::abs(across.fR - .5f), .5f, .03f) << x << "," << y;
           EXPECT_NEAR(across.fG, .5f, .03f) << x << "," << y;
         }
         for (const auto& [x, y] : {std::pair{24, 56}, std::pair{104, 56}}) {
           const SkColor4f down = brushing.getColor4f(x, y);
           EXPECT_NEAR(down.fR, .5f, .03f) << x << "," << y;
           EXPECT_NEAR(std::abs(down.fG - .5f), .5f, .03f) << x << "," << y;
         }
       }},
      {"ARibbonAlongItsCurve",
       [&] {
         // A brushed ribbon round a circle: along its top and bottom it brushes
         // across the page, along its sides down it, as the curve runs.
         const auto scene = SurfaceScene::make({kWidth, kHeight}, fonts());
         ASSERT_NE(scene, nullptr);
         scene->render(box().width(kWidth).height(kHeight).children(
             {box()
                  .absolute()
                  .rect(24, 16, 80, 80)
                  .borderRadius(40)
                  .stroke(brush::ribbon(
                      8, material::from(material::Color{.9f, .9f, .9f, 1})
                             .surface({.metallic = 1.0f, .roughness = .3f})
                             .brushed({.strength = 1})))}));
         const SkBitmap brushing = pixelsOf(*scene, Role::Anisotropy);
         ASSERT_FALSE(brushing.empty());
         for (const auto& [x, y] : {std::pair{64, 16}, std::pair{64, 96}}) {
           const SkColor4f across = brushing.getColor4f(x, y);
           EXPECT_GT(std::abs(across.fR - .5f), .4f) << x << "," << y;
           EXPECT_NEAR(across.fG, .5f, .1f) << x << "," << y;
         }
         for (const auto& [x, y] : {std::pair{24, 56}, std::pair{104, 56}}) {
           const SkColor4f down = brushing.getColor4f(x, y);
           EXPECT_NEAR(down.fR, .5f, .1f) << x << "," << y;
           EXPECT_GT(std::abs(down.fG - .5f), .4f) << x << "," << y;
         }
       }},
  });
}
