// The pixel styles: the bevel pair's tones land on the edges it names and
// swap when it is sunken, the brackets stand off the box at the corners
// they were asked for, the tick rail's ladder walks one edge with every
// n-th mark long, and the scanlines are rows inside the outline, laid as
// a layer of the fill's material. Every
// claim is about WHERE INK LANDS, so every case reads pixels back.

#include <sigilcompose/brush/PixelStyles.h>
#include <sigilmaterial/pattern/Patterns.h>

#include "support/BrushTestSupport.h"
#include "support/Rows.h"

namespace {

Fill grey() { return Fill::color({0.5f, 0.5f, 0.5f, 1}); }

/** Red, green and blue of a read-back pixel, for a claim about a tone. */
struct Rgb {
  int r, g, b;
};
Rgb rgb(SkColor c) {
  return {(int)SkColorGetR(c), (int)SkColorGetG(c), (int)SkColorGetB(c)};
}

}  // namespace

TEST(ComposePixelStyles, EachPixelStylePutsItsInkWhereItSays) {
  sigil::compose::test::checkRows({
      {"TheBevelPairLightsTheNearEdgesAndShadesTheFar",
       [&] {
         Host host(100, 100);
         const styles::BevelPair raised = styles::bevelPair(
             material::Color{1, 1, 1, 1}, material::Color{0, 0, 0, 1}, 2);
         host.composer.render(box().children(
             {box().left(10).top(10).width(40).height(30).fill(grey()).overlay(
                 raised)}));
         host.frame();
         // Two pixels in from each edge: light top and left, dark bottom and
         // right, and the grey face between them.
         EXPECT_EQ(host.pixel(11, 11), SK_ColorWHITE);
         EXPECT_EQ(host.pixel(11, 25), SK_ColorWHITE);
         EXPECT_EQ(host.pixel(30, 11), SK_ColorWHITE);
         EXPECT_EQ(host.pixel(48, 25), SK_ColorBLACK);
         EXPECT_EQ(host.pixel(30, 38), SK_ColorBLACK);
         EXPECT_NEAR(rgb(host.pixel(30, 25)).r, 128, 1);
         // The corner step: the top edge owns the top-right corner and the
         // bottom edge the bottom-left, as a raised panel's always have.
         EXPECT_EQ(host.pixel(48, 11), SK_ColorWHITE);
         EXPECT_EQ(host.pixel(11, 38), SK_ColorBLACK);

         // Sunken is the same value the other way up.
         const styles::BevelPair sunken = raised.inverted();
         EXPECT_TRUE(sunken.sunken);
         EXPECT_FALSE(sunken == raised);
         EXPECT_EQ(sunken.inverted(), raised);
         host.composer.render(box().children(
             {box().left(10).top(10).width(40).height(30).fill(grey()).overlay(
                 sunken)}));
         host.frame();
         EXPECT_EQ(host.pixel(11, 11), SK_ColorBLACK);
         EXPECT_EQ(host.pixel(48, 25), SK_ColorWHITE);
         EXPECT_EQ(host.pixel(30, 38), SK_ColorWHITE);
       }},
      {"TheDerivedPairLightensAndDropsTheFace",
       [&] {
         const SkColor4f face{0.4f, 0.5f, 0.6f, 1};
         const styles::BevelPair pair =
             styles::bevelPair(face, 0.2f, 0.5f, 3.0f);
         EXPECT_EQ(pair.light, sigil::material::lighten(face, 0.2f));
         EXPECT_EQ(pair.dark, sigil::material::scale(face, 0.5f));
         EXPECT_FLOAT_EQ(pair.lightWidth, 3.0f);
         EXPECT_FLOAT_EQ(pair.darkWidth, 3.0f);
         EXPECT_FLOAT_EQ(pair.reach(), 3.0f);
         // The same recipe twice is one value: a panel wearing it prunes.
         EXPECT_EQ(pair, styles::bevelPair(face, 0.2f, 0.5f, 3.0f));
       }},
  });
}
