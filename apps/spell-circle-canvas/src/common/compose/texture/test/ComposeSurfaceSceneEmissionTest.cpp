/** @file
 * What a surface scene paints into its emissive map: content stated
 * unlit is its own colour there over a black base while plain content is
 * lit; an emission past white is painted against the page's brightest;
 * and a stated range holds an emission past the brightest painted.
 */

#include <sigilcompose/brush/Ribbons.h>

#include <cmath>
#include <cstdint>
#include <vector>

#include "support/Rows.h"
#include "support/SurfaceSceneFixture.h"

namespace {

/** A page whose fill and keyline, whole-passage ink, per-glyph ink and
 *  ribbon all carry @p finish. */
Element ownColourPage(const material::Material& finish) {
  return box().width(kWidth).height(kHeight).children({
      box()
          .absolute()
          .rect(0, 0, 40, 40)
          .fill(finish)
          .stroke(finish,
                  {.width = 4, .position = material::StrokePosition::Inside}),
      text(u8"HH")
          .font({.size = 40})
          .absolute()
          .rect(44, 0, 84, 48)
          .ink(finish),
      text(u8"HH")
          .font({.size = 40})
          .absolute()
          .rect(0, 48, 64, 48)
          .ink(finish, PaintBox::Glyph),
      box().absolute().rect(76, 60, 40, 40).stroke(brush::ribbon(8, finish)),
  });
}

/** Whether, somewhere in @p region where the page painted at full
 *  coverage, @p shown reads @p colour and @p dark reads black. */
bool showsColourIn(const SkBitmap& shown, const SkBitmap& dark, SkIRect region,
                   material::Color colour) {
  for (int y = region.top(); y < region.bottom(); ++y)
    for (int x = region.left(); x < region.right(); ++x) {
      const SkColor4f in = shown.getColor4f(x, y);
      const SkColor4f out = dark.getColor4f(x, y);
      if (in.fA < .99f || out.fA < .99f) continue;
      if (std::abs(in.fR - colour.r) < .03f &&
          std::abs(in.fG - colour.g) < .03f &&
          std::abs(in.fB - colour.b) < .03f && out.fR < .03f && out.fG < .03f &&
          out.fB < .03f)
        return true;
    }
  return false;
}

}  // namespace

TEST(ComposeSurfaceScene,
     ContentStatedUnlitIsItsOwnColourAndPlainContentIsLit) {
  // Stated unlit — in either spelling — every fill, keyline, ink and ribbon
  // is its own colour: the colour in the emissive map over a black base.
  // The same content as a plain colour is the base colour and emits
  // nothing, so a scene's light falls on it.
  const material::Color colour{.9f, .3f, .1f, 1};
  struct Case {
    const char* name;
    material::Material finish;
    bool ownColour;
  };
  const std::vector<Case> cases{
      {"unlit surface options", material::from(colour).surface({.unlit = true}),
       true},
      {"unlit surface program", material::surface::unlit({.baseColor = colour}),
       true},
      {"plain colour", material::Material(colour), false},
  };
  for (const Case& each : cases) {
    SCOPED_TRACE(each.name);
    const auto scene = SurfaceScene::make({kWidth, kHeight}, fonts());
    ASSERT_NE(scene, nullptr);
    scene->render(ownColourPage(each.finish));
    const SkBitmap base = pixelsOf(*scene, Role::BaseColor);
    const SkBitmap emissive = pixelsOf(*scene, Role::Emissive);
    ASSERT_FALSE(base.empty());
    const SkBitmap& shown = each.ownColour ? emissive : base;
    const SkBitmap& dark = each.ownColour ? base : emissive;
    EXPECT_TRUE(
        showsColourIn(shown, dark, SkIRect::MakeLTRB(14, 14, 26, 26), colour))
        << "the fill";
    EXPECT_TRUE(
        showsColourIn(shown, dark, SkIRect::MakeLTRB(0, 14, 3, 26), colour))
        << "the keyline";
    EXPECT_TRUE(
        showsColourIn(shown, dark, SkIRect::MakeLTRB(44, 0, 128, 48), colour))
        << "the whole-passage ink";
    EXPECT_TRUE(
        showsColourIn(shown, dark, SkIRect::MakeLTRB(0, 48, 64, 96), colour))
        << "the per-glyph ink";
    EXPECT_TRUE(
        showsColourIn(shown, dark, SkIRect::MakeLTRB(74, 70, 79, 90), colour))
        << "the ribbon";
  }
}

TEST(ComposeSurfaceScene,
     TheEmissiveMapsWhiteStandsForTheBrightestEmissionLearnedOrStated) {
  sigil::compose::test::checkRows({
      {"LearnedFromTheBrightestPainted",
       [&] {
         const auto glowing = [](float strength) {
           return material::from(material::Color{0, 0, 0, 1})
               .surface(
                   {.emission = {1, 1, 1, 1}, .emissionStrength = strength});
         };
         const auto twoGlows = [&](float left, float right) {
           return box().width(kWidth).height(kHeight).children({
               box().absolute().rect(0, 0, 64, 112).fill(glowing(left)),
               box().absolute().rect(64, 0, 64, 112).fill(glowing(right)),
           });
         };
         // A page whose brightest emission is one paints its emission as
         // stated, and its map is read at the stock strength.
         const auto plain = SurfaceScene::make({kWidth, kHeight}, fonts());
         ASSERT_NE(plain, nullptr);
         plain->render(twoGlows(1, .5f));
         EXPECT_EQ(plain->maps().emissiveStrength, 1);
         expectNear(pixelsOf(*plain, Role::Emissive).getColor4f(32, 56), 1, 1,
                    1);
         expectNear(pixelsOf(*plain, Role::Emissive).getColor4f(96, 56), .5f,
                    .5f, .5f);

         // Four is painted white and one as a quarter of that light, encoded
         // for display, and the map is read at four: both keep the light they
         // state.
         const auto bright = SurfaceScene::make({kWidth, kHeight}, fonts());
         ASSERT_NE(bright, nullptr);
         bright->render(twoGlows(4, 1));
         EXPECT_EQ(bright->maps().emissiveStrength, 4);
         const SkBitmap emissive = pixelsOf(*bright, Role::Emissive);
         expectNear(emissive.getColor4f(32, 56), 1, 1, 1);
         // The display encoding of a quarter.
         const float quarter = 1.055f * std::pow(.25f, 1 / 2.4f) - .055f;
         expectNear(emissive.getColor4f(96, 56), quarter, quarter, quarter);
         // The strength stands once raised, as a stated lobe's maps do.
         bright->render(twoGlows(1, .5f), 1);
         EXPECT_EQ(bright->maps().emissiveStrength, 4);
       }},
      {"StatedByASurfaceRange",
       [&] {
         const auto glowing = [](float strength) {
           return box().width(kWidth).height(kHeight).children(
               {box()
                    .absolute()
                    .rect(0, 0, 64, 64)
                    .fill(material::from(material::Color{.2f, .2f, .2f, 1})
                              .surface({.emission = {1, 1, 1, 1},
                                        .emissionStrength = strength}))});
         };
         // Stated: the emissive map's white is eight from the first paint, so a
         // strength that rises from two to six is read as itself, and the page
         // paints once for each description rather than again for a reading.
         const auto stated =
             SurfaceScene::make({kWidth, kHeight}, fonts(), {.emission = 8});
         ASSERT_NE(stated, nullptr);
         stated->render(glowing(2));
         EXPECT_FLOAT_EQ(stated->maps().emissiveStrength, 8);
         const float dim =
             pixelsOf(*stated, Role::Emissive).getColor4f(10, 10).fR;
         const uint64_t before = stated->revision();
         stated->render(glowing(6), 1.0 / 60.0);
         EXPECT_EQ(stated->revision(), before + 1);
         EXPECT_FLOAT_EQ(stated->maps().emissiveStrength, 8);
         const float bright =
             pixelsOf(*stated, Role::Emissive).getColor4f(10, 10).fR;
         EXPECT_GT(bright, dim);
         EXPECT_LT(bright, .97f) << "six of eight is not the map's white";

         // Learned: the reading rises to the brightest painted, which is white.
         const auto learned = SurfaceScene::make({kWidth, kHeight}, fonts());
         ASSERT_NE(learned, nullptr);
         learned->render(glowing(2));
         learned->render(glowing(6), 1.0 / 60.0);
         EXPECT_FLOAT_EQ(learned->maps().emissiveStrength, 6);
         EXPECT_GT(pixelsOf(*learned, Role::Emissive).getColor4f(10, 10).fR,
                   .99f);
       }},
  });
}
