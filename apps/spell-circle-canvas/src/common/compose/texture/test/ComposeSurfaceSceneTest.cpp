// A compose tree painted as the maps of a surface: what each map reads
// under a lit fill, lit glyph ink, a relief and an unlit label, where
// nothing paints, when it stands still and when it paints again, and that
// what a composer holds for one map is never what it draws for another.

#include <include/core/SkBitmap.h>
#include <include/core/SkImage.h>
#include <sigilcompose/brush/Relief.h>
#include <sigilcompose/testing/Scene.h>
#include <sigilcompose/texture/SurfaceScene.h>
#include <sigilmaterial/core/Material.h>

#include <algorithm>
#include <cmath>
#include <optional>

#include "support/Host.h"

namespace {

using sigil::material::texture::Role;

constexpr int kWidth = 128;
constexpr int kHeight = 112;

material::Material plateFinish(material::Color colour) {
  return material::from(colour).surface({.metallic = .75f, .roughness = .25f});
}

material::Material glyphFinish() {
  // A constant tilted normal: (0.5, 0, 0.8) normalised, green up.
  return material::from(material::Color{.2f, .6f, .4f, 1})
      .surface({.roughness = .9f,
                .normal = material::from(material::Color{.75f, .5f, .9f, 1})});
}

material::Material reliefFinish() {
  return material::from(material::Color{.6f, .6f, .6f, 1})
      .surface({.roughness = .8f});
}

/** A lit plate with an unlit label on it, lit glyph ink, a relief, and an
 *  empty band along the bottom. */
Element page(material::Color plate = {.8f, .4f, .2f, 1}) {
  return box().width(kWidth).height(kHeight).children({
      box().absolute().rect(0, 0, 128, 48).fill(plateFinish(plate)),
      box()
          .absolute()
          .rect(96, 4, 24, 24)
          .fill(material::Color{.9f, .1f, .1f, 1}),
      text(u8"HH", whiteStyle(44))
          .absolute()
          .rect(0, 48, 64, 48)
          .ink(glyphFinish(), PaintBox::Glyph),
      box()
          .absolute()
          .rect(64, 48, 64, 48)
          .background(relief(reliefFinish(), {.shoulder = 6, .depth = 3})),
  });
}

SkBitmap pixelsOf(const SurfaceScene& scene, Role role) {
  SkBitmap pixels;
  const TextureScene* map = scene.scene(role);
  if (!map || !map->image()) return pixels;
  pixels.allocN32Pixels(map->image()->width(), map->image()->height());
  map->image()->readPixels(nullptr, pixels.pixmap(), 0, 0);
  return pixels;
}

void expectNear(const SkColor4f& actual, float red, float green, float blue,
                float alpha = 1, float tolerance = .012f) {
  EXPECT_NEAR(actual.fR, red, tolerance);
  EXPECT_NEAR(actual.fG, green, tolerance);
  EXPECT_NEAR(actual.fB, blue, tolerance);
  EXPECT_NEAR(actual.fA, alpha, tolerance);
}

}  // namespace

TEST(ComposeSurfaceScene, EachMapReadsWhatTheTreePaintedThere) {
  const auto scene = SurfaceScene::make({kWidth, kHeight}, fonts());
  ASSERT_NE(scene, nullptr);
  scene->render(page());
  const SkBitmap base = pixelsOf(*scene, Role::BaseColor);
  const SkBitmap normal = pixelsOf(*scene, Role::Normal);
  const SkBitmap roughness = pixelsOf(*scene, Role::Roughness);
  const SkBitmap metallic = pixelsOf(*scene, Role::Metallic);
  const SkBitmap occlusion = pixelsOf(*scene, Role::Occlusion);
  const SkBitmap emissive = pixelsOf(*scene, Role::Emissive);
  ASSERT_FALSE(base.empty());

  // The lit plate: its colour, numbers and a flat normal; it emits nothing.
  expectNear(base.getColor4f(10, 10), .8f, .4f, .2f);
  expectNear(normal.getColor4f(10, 10), .5f, .5f, 1);
  expectNear(roughness.getColor4f(10, 10), .25f, .25f, .25f);
  expectNear(metallic.getColor4f(10, 10), .75f, .75f, .75f);
  expectNear(occlusion.getColor4f(10, 10), 1, 1, 1);
  expectNear(emissive.getColor4f(10, 10), 0, 0, 0);

  // The unlit label over it is its own colour and covers the plate.
  expectNear(base.getColor4f(108, 16), 0, 0, 0);
  expectNear(emissive.getColor4f(108, 16), .9f, .1f, .1f);
  expectNear(normal.getColor4f(108, 16), .5f, .5f, 1);
  expectNear(roughness.getColor4f(108, 16), 1, 1, 1);
  expectNear(metallic.getColor4f(108, 16), 0, 0, 0);

  // Lit glyph ink: somewhere inside a glyph the base is the ink's colour
  // and the normal its tilted one.
  bool inked = false;
  for (int y = 50; y < 96 && !inked; ++y)
    for (int x = 0; x < 64 && !inked; ++x) {
      const SkColor4f colour = base.getColor4f(x, y);
      if (colour.fA < .99f) continue;
      inked = true;
      expectNear(colour, .2f, .6f, .4f);
      expectNear(normal.getColor4f(x, y), .765f, .5f, .924f);
      expectNear(roughness.getColor4f(x, y), .9f, .9f, .9f);
      expectNear(emissive.getColor4f(x, y), 0, 0, 0);
    }
  EXPECT_TRUE(inked);

  // The relief: its colour throughout, flat across its middle and tilted
  // along its shoulder.
  expectNear(base.getColor4f(96, 72), .6f, .6f, .6f);
  expectNear(normal.getColor4f(96, 72), .5f, .5f, 1, 1, .03f);
  const SkColor4f shoulder = normal.getColor4f(66, 72);
  EXPECT_GT(std::abs(shoulder.fR - .5f), .08f);

  // Where nothing paints, every map reads its ground.
  expectNear(base.getColor4f(5, 105), 0, 0, 0, 0);
  expectNear(normal.getColor4f(5, 105), .5f, .5f, 1);
  expectNear(roughness.getColor4f(5, 105), 1, 1, 1);
  expectNear(metallic.getColor4f(5, 105), 0, 0, 0);
  expectNear(emissive.getColor4f(5, 105), 0, 0, 0);

  const material::texture::TextureMaps maps = scene->maps();
  EXPECT_EQ(maps.maps.size(), 6u);
  EXPECT_FALSE(maps.normalDirectX);
  EXPECT_NE(maps.map(Role::Emissive), nullptr);
}

TEST(ComposeSurfaceScene, AStillTreeSettlesAndAChangedMaterialPaintsAgain) {
  const auto scene = SurfaceScene::make({kWidth, kHeight}, fonts());
  ASSERT_NE(scene, nullptr);
  const Element still = page();
  scene->render(still);
  const uint64_t painted = scene->revision();
  EXPECT_EQ(painted, 1u);
  for (int frame = 0; frame < 6; ++frame)
    scene->render(still, (frame + 1) / 60.0);
  EXPECT_EQ(scene->revision(), painted);
  EXPECT_FALSE(scene->isRunning());

  scene->render(page({.1f, .3f, .9f, 1}), 1);
  EXPECT_EQ(scene->revision(), painted + 1);
  expectNear(pixelsOf(*scene, Role::BaseColor).getColor4f(10, 10), .1f, .3f,
             .9f);
}

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
