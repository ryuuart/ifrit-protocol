/** @file
 * Material strokes: a lit stroke's live surface maps and an unlit stroke's
 * colours stay live under the composer's caching without reading their
 * pictures again.
 */

#include <sigilcompose/brush/Relief.h>
#include <sigilmaterial/texture/Image.h>

#include <memory>

#include "ReliefTestSupport.h"
#include "support/Rows.h"

TEST(ComposeLiveKeyline,
     AMaterialKeylineRepaintsWhenADeclaredInputMovesAndReadsNoStillInputAgain) {
  sigil::compose::test::checkRows({
      {"ALitKeylinesBoundRoughnessRepaintsItsMap",
       [&] {
         ASSERT_TRUE(scalarMap());
         auto colourReads = std::make_shared<int>(0);
         auto normalReads = std::make_shared<int>(0);
         auto roughness = motion::animatable(0.05f);
         material::Paint roughMap = material::skia::sksl(scalarMap());
         roughMap.bind("value", roughness);
         const material::Material finish =
             material::image(sigil::media::PixelSource(
                                 CountedPicture{colourReads, greyPicture()}))
                 .surface(
                     {.metallic = 1.0f,
                      .roughness = material::skia::base(roughMap),
                      .normal = material::image(sigil::media::PixelSource(
                          CountedPicture{normalReads, greyPicture(SkColorSetRGB(
                                                          128, 128, 255))}))});
         Host held(128, 100);
         held.composer.setSurfaceMap(material::texture::Role::Roughness);
         held.composer.render(box().children(
             {box()
                  .absolute()
                  .rect(20, 20, 80, 60)
                  .stroke(finish,
                          {.width = 8,
                           .position = material::StrokePosition::Inside})}));
         for (int frame = 0; frame < 16; ++frame) held.frame(1.0 / 60.0);
         const SkColor polished = held.pixel(60, 23);
         const int colours = *colourReads, normals = *normalReads;
         ASSERT_GT(colours, 0);
         // The roughness map samples the colour stack for its coverage and the
         // roughness, and no other channel, so the normal's picture is never
         // read.
         EXPECT_EQ(normals, 0);
         roughness = 1.0f;
         EXPECT_TRUE(held.composer.isRunning());
         held.frame();
         const SkColor matte = held.pixel(60, 23);
         EXPECT_GT(brightness(matte), brightness(polished) + 30);
         EXPECT_EQ(*colourReads, colours);
         EXPECT_EQ(*normalReads, normals);
         EXPECT_EQ(held.pixel(60, 50), SK_ColorBLACK);
       }},
      {"AnUnlitKeylineResolvesItsColourAtTheCurrentFrame",
       [&] {
         ASSERT_TRUE(timeColour());
         const material::Material colour =
             material::skia::base(material::skia::sksl(timeColour()))
                 .surface({.unlit = true});
         Host held(128, 100);
         held.composer.render(box().children(
             {box()
                  .absolute()
                  .rect(20, 20, 80, 60)
                  .stroke(colour,
                          {.width = 8,
                           .position = material::StrokePosition::Inside})}));
         held.frame();
         EXPECT_EQ(held.pixel(60, 23), SK_ColorRED);
         EXPECT_TRUE(held.composer.isRunning());
         held.frame(1.0);
         EXPECT_EQ(held.pixel(60, 23), SK_ColorBLUE);
         held.frame();
         EXPECT_EQ(held.pixel(60, 23), SK_ColorBLUE);
         EXPECT_EQ(held.pixel(60, 50), SK_ColorBLACK);
       }},
  });
}
