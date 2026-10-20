/** @file
 * A surface material drawn by a composer rather than painted into maps:
 * what a composer holds for one map is never what it draws for another,
 * and the same tree drawn as a page paints every material's colours flat,
 * reading none of its surface channels.
 */

#include <sigilcompose/brush/Relief.h>
#include <sigilmaterial/texture/Image.h>

#include <algorithm>
#include <optional>

#include "support/SurfaceSceneFixture.h"

namespace {

/** A page whose fill, glyph ink, keyline and relief carry @p finish,
 *  which states a surface or does not. */
Element finishedPage(const material::Material& finish) {
  return box().width(kWidth).height(kHeight).children({
      box()
          .absolute()
          .rect(0, 0, 128, 48)
          .fill(finish)
          .stroke(finish,
                  {.width = 4, .position = material::StrokePosition::Inside}),
      text(u8"HH", whiteStyle(44))
          .absolute()
          .rect(0, 48, 64, 48)
          .ink(finish, PaintBox::Glyph),
      box()
          .absolute()
          .rect(64, 48, 64, 48)
          .background(relief(finish, {.shoulder = 6, .depth = 3})),
  });
}

}  // namespace

TEST(ComposeSurfaceScene, WhatAComposerHoldsForOneMapIsNotDrawnForAnother) {
  Host host(kWidth, kHeight);
  host.composer.render(box()
                           .width(kWidth)
                           .height(kHeight)
                           .cache(Cache::Picture)
                           .children({
                               box()
                                   .absolute()
                                   .rect(0, 0, 128, 48)
                                   .cache(Cache::Picture)
                                   .fill(plateFinish({.8f, .4f, .2f, 1})),
                               text(u8"HH", whiteStyle(44))
                                   .absolute()
                                   .rect(0, 48, 64, 48)
                                   .cache(Cache::Picture)
                                   .ink(glyphFinish()),
                           }));
  const auto at = [&](int x, int y) {
    return SkColor4f::FromColor(host.pixel(x, y));
  };
  // The strongest red across the glyphs, over the black the host clears to.
  const auto inkedRed = [&] {
    float strongest = 0;
    for (int y = 50; y < 96; ++y)
      for (int x = 0; x < 64; ++x) strongest = std::max(strongest, at(x, y).fR);
    return strongest;
  };
  host.composer.setSurfaceMap(Role::Roughness);
  host.frame();
  host.frame();
  expectNear(at(10, 10), .25f, .25f, .25f);
  EXPECT_NEAR(inkedRed(), .9f, .012f);
  host.composer.setSurfaceMap(Role::Metallic);
  host.frame();
  expectNear(at(10, 10), .75f, .75f, .75f);
  EXPECT_NEAR(inkedRed(), 0, .012f);  // the ink states no metal
  host.composer.setSurfaceMap(Role::BaseColor);
  host.frame();
  expectNear(at(10, 10), .8f, .4f, .2f);
  EXPECT_NEAR(inkedRed(), .2f, .012f);
  host.composer.setSurfaceMap(std::nullopt);
  host.frame();
  // With no map and no lighting the plate paints its colour stack flat.
  expectNear(at(10, 10), .8f, .4f, .2f);
}

TEST(ComposeSurfaceScene, APageDrawPaintsAStatedSurfaceAsItsColoursFlat) {
  auto colourReads = std::make_shared<int>(0);
  auto normalReads = std::make_shared<int>(0);
  auto roughnessReads = std::make_shared<int>(0);
  const auto picture = [](const std::shared_ptr<int>& reads, SkColor colour) {
    SkBitmap pixels;
    pixels.allocN32Pixels(8, 8, true);
    pixels.eraseColor(colour);
    pixels.setImmutable();
    return material::image(
        sigil::media::PixelSource(CountedPicture{reads, pixels.asImage()}),
        {.repeat = material::Repeat::Pad});
  };
  // A strongly tilted normal, a metal and an emission: none of them
  // changes what a page paints.
  // The colours are a picture, so the plain material and the surfaced one
  // both paint as a paint rather than one of them as a plain colour.
  const material::Material colours =
      picture(colourReads, SkColorSetRGB(77, 140, 204));
  const material::Material surfaced = material::from(colours).surface(
      {.metallic = 1.0f,
       .roughness = picture(roughnessReads, SkColorSetRGB(40, 40, 40)),
       .normal = picture(normalReads, SkColorSetRGB(230, 128, 160)),
       .emission = {1, 0, 0, 1},
       .emissionStrength = 1});
  Host lit(kWidth, kHeight), plain(kWidth, kHeight);
  lit.composer.render(finishedPage(surfaced));
  plain.composer.render(finishedPage(colours));
  lit.frame();
  plain.frame();
  EXPECT_TRUE(identicalPixels(lit, plain, kWidth, kHeight));
  EXPECT_EQ(*normalReads, 0);
  EXPECT_EQ(*roughnessReads, 0);

  // The same tree through a surface scene still yields its maps.
  const auto scene = SurfaceScene::make({kWidth, kHeight}, fonts());
  ASSERT_NE(scene, nullptr);
  scene->render(finishedPage(surfaced));
  EXPECT_GT(*normalReads, 0);
  EXPECT_GT(*roughnessReads, 0);
  const SkBitmap base = pixelsOf(*scene, Role::BaseColor);
  const SkBitmap normal = pixelsOf(*scene, Role::Normal);
  const SkBitmap metallic = channelOf(*scene, 2);
  const SkBitmap emissive = pixelsOf(*scene, Role::Emissive);
  ASSERT_FALSE(base.empty());
  expectNear(base.getColor4f(20, 20), 77 / 255.f, 140 / 255.f, 204 / 255.f);
  EXPECT_GT(normal.getColor4f(20, 20).fR, .8f);
  expectNear(metallic.getColor4f(20, 20), 1, 1, 1);
  expectNear(emissive.getColor4f(20, 20), 1, 0, 0);
}
