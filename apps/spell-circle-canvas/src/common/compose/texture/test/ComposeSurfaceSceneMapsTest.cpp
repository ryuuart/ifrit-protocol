/** @file
 * A compose tree painted as the maps of a surface: what each map reads
 * under a lit fill, lit glyph ink, a relief and a flat label, and where
 * nothing paints; a stated lobe or a stated coat normal adding its maps
 * in the render that states it; a still tree settling and a changed
 * material painting again; and a region's maps reading that part of the
 * page whole.
 */

#include <sigilcompose/brush/Relief.h>

#include <cstdint>

#include "support/SurfaceSceneFixture.h"

namespace {

material::Material reliefFinish() {
  return material::from(material::Color{.6f, .6f, .6f, 1})
      .surface({.roughness = .8f});
}

/** A lit plate with a flat label on it, lit glyph ink, a relief, and an
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

}  // namespace

TEST(ComposeSurfaceScene, EachMapReadsWhatTheTreePaintedThere) {
  const auto scene = SurfaceScene::make({kWidth, kHeight}, fonts());
  ASSERT_NE(scene, nullptr);
  scene->render(page());
  const SkBitmap base = pixelsOf(*scene, Role::BaseColor);
  const SkBitmap normal = pixelsOf(*scene, Role::Normal);
  const SkBitmap roughness = channelOf(*scene, 1);
  const SkBitmap metallic = channelOf(*scene, 2);
  const SkBitmap occlusion = channelOf(*scene, 0);
  const SkBitmap emissive = pixelsOf(*scene, Role::Emissive);
  ASSERT_FALSE(base.empty());

  // The lit plate: its colour, numbers and a flat normal; it emits nothing.
  expectNear(base.getColor4f(10, 10), .8f, .4f, .2f);
  expectNear(normal.getColor4f(10, 10), .5f, .5f, 1);
  expectNear(roughness.getColor4f(10, 10), .25f, .25f, .25f);
  expectNear(metallic.getColor4f(10, 10), .75f, .75f, .75f);
  expectNear(occlusion.getColor4f(10, 10), 1, 1, 1);
  expectNear(emissive.getColor4f(10, 10), 0, 0, 0);

  // The flat label over it states no surface: it is the stock surface, its
  // colour the base colour, and it covers the plate and emits nothing.
  expectNear(base.getColor4f(108, 16), .9f, .1f, .1f);
  expectNear(emissive.getColor4f(108, 16), 0, 0, 0);
  expectNear(normal.getColor4f(108, 16), .5f, .5f, 1);
  expectNear(roughness.getColor4f(108, 16), .5f, .5f, .5f);
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
  // Five maps: the three scalars share one and the coat's two another.
  EXPECT_EQ(maps.maps.size(), 5u);
  EXPECT_FALSE(maps.normalDirectX);
  EXPECT_NE(maps.map(Role::Emissive), nullptr);
  EXPECT_NE(maps.map(Role::Packed), nullptr);
  EXPECT_NE(maps.map(Role::Clearcoat), nullptr);
  // A plate stating no coat has none.
  expectNear(pixelsOf(*scene, Role::Clearcoat).getColor4f(10, 10), 0, 0, 0);
}

TEST(ComposeSurfaceScene, AStatedLobeAddsItsMapsInTheRenderThatStatesIt) {
  const auto scene = SurfaceScene::make({kWidth, kHeight}, fonts());
  ASSERT_NE(scene, nullptr);
  // A page whose materials state no optional lobe paints the five maps
  // and nothing else.
  scene->render(page());
  EXPECT_EQ(scene->maps().maps.size(), 5u);
  EXPECT_EQ(scene->lobes(), material::Lobes{});

  // A sheened, brushed plate: the render that paints it adds the sheen's
  // two maps and the brushing's, and paints them at once.
  const Element lobed = box().width(kWidth).height(kHeight).children({
      box()
          .absolute()
          .rect(0, 0, 64, 48)
          .fill(material::from(material::Color{.3f, .1f, .1f, 1})
                    .surface({.sheenColor = material::Color{.8f, .6f, .4f, 1},
                              .sheenRoughness = .5f})),
      box()
          .absolute()
          .rect(64, 0, 64, 48)
          .fill(material::from(material::Color{.9f, .9f, .9f, 1})
                    .surface({.metallic = 1.0f,
                              .anisotropy = .75f,
                              .anisotropyRotation = 90})),
  });
  scene->render(lobed);
  EXPECT_EQ(scene->lobes(),
            (material::Lobes{.sheen = true, .anisotropy = true}));
  const material::texture::TextureMaps maps = scene->maps();
  EXPECT_EQ(maps.maps.size(), 8u);
  EXPECT_NE(maps.map(Role::Sheen), nullptr);
  EXPECT_NE(maps.map(Role::SheenSpecular), nullptr);
  EXPECT_NE(maps.map(Role::Anisotropy), nullptr);
  EXPECT_EQ(maps.map(Role::Iridescence), nullptr);
  expectNear(pixelsOf(*scene, Role::Sheen).getColor4f(20, 20), .8f, .6f, .4f);
  // The sheen's roughness in red, the stock specular strength in green.
  expectNear(pixelsOf(*scene, Role::SheenSpecular).getColor4f(20, 20), .5f, 1,
             0);
  // A quarter turn from the page's width: the direction (0, 1), encoded,
  // and the strength in blue; where nothing brushes, no strength.
  const SkBitmap brushing = pixelsOf(*scene, Role::Anisotropy);
  expectNear(brushing.getColor4f(96, 20), .5f, 1, .75f);
  expectNear(brushing.getColor4f(5, 105), 1, .5f, 0);
  // The maps stay once added, though the next tree states no lobe.
  scene->render(page());
  EXPECT_EQ(scene->maps().maps.size(), 8u);
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

TEST(ComposeSurfaceScene, AStatedCoatNormalAddsItsMapWhereTheCoatIsPainted) {
  const auto scene = SurfaceScene::make({kWidth, kHeight}, fonts());
  ASSERT_NE(scene, nullptr);
  scene->render(page());
  EXPECT_EQ(scene->maps().map(Role::ClearcoatNormal), nullptr);
  // A coated plate whose coat leans right on its own while the surface
  // beneath faces the viewer.
  scene->render(box().width(kWidth).height(kHeight).children(
      {box()
           .absolute()
           .rect(0, 0, 64, 48)
           .fill(material::from(material::Color{.3f, .3f, .3f, 1})
                     .surface({.clearcoat = 1.0f,
                               .clearcoatNormal = material::from(
                                   material::Color{.75f, .5f, .9f, 1})}))}));
  ASSERT_NE(scene->maps().map(Role::ClearcoatNormal), nullptr);
  const SkBitmap coat = pixelsOf(*scene, Role::ClearcoatNormal);
  const SkBitmap normal = pixelsOf(*scene, Role::Normal);
  expectNear(coat.getColor4f(20, 20), .765f, .5f, .924f);
  expectNear(normal.getColor4f(20, 20), .5f, .5f, 1);
  // Where nothing paints, the coat's normal faces the viewer.
  expectNear(coat.getColor4f(100, 100), .5f, .5f, 1);
}

TEST(ComposeSurfaceScene, ARegionsMapsReadThatPartOfThePageWhole) {
  const auto halves = [](material::Color left, material::Color right) {
    return box().width(kWidth).height(kHeight).children({
        box().absolute().rect(0, 0, 64, kHeight).fill(left),
        box().absolute().rect(64, 0, 64, kHeight).fill(right),
    });
  };
  const material::Color red{.9f, .1f, .1f, 1};
  const material::Color blue{.1f, .1f, .9f, 1};
  const auto scene = SurfaceScene::make({kWidth, kHeight}, fonts());
  ASSERT_NE(scene, nullptr);
  const auto left = scene->maps({0, 0, 64, kHeight});
  const auto right = scene->maps({64, 0, 64, kHeight});
  const auto colourOf = [](const material::texture::TextureMaps& maps, int x,
                           int y) {
    const material::Texture* map = maps.map(Role::BaseColor);
    EXPECT_NE(map, nullptr);
    const auto* source = map ? map->source().as<SceneSource>() : nullptr;
    EXPECT_NE(source, nullptr);
    SkBitmap pixels;
    if (!source || !source->scene()->image()) return SkColor4f{};
    const sk_sp<SkImage> image = source->scene()->image();
    EXPECT_EQ(image->width(), 64);
    pixels.allocN32Pixels(image->width(), image->height());
    image->readPixels(nullptr, pixels.pixmap(), 0, 0);
    return pixels.getColor4f(x, y);
  };
  scene->render(halves(red, blue));
  // Each part's map is that part alone, its corner at the map's corner.
  expectNear(colourOf(left, 2, 2), .9f, .1f, .1f);
  expectNear(colourOf(left, 61, 100), .9f, .1f, .1f);
  expectNear(colourOf(right, 2, 2), .1f, .1f, .9f);
  expectNear(colourOf(right, 61, 100), .1f, .1f, .9f);
  // The parts follow what the page paints.
  scene->render(halves(blue, red), 1.0 / 60.0);
  expectNear(colourOf(left, 30, 30), .1f, .1f, .9f);
  expectNear(colourOf(right, 30, 30), .9f, .1f, .1f);
  EXPECT_TRUE(scene->paints(
      left.map(Role::BaseColor)->source().as<SceneSource>()->scene()));
}
