/** @file
 * A transmitting region in a surface scene: it paints its weight, its
 * depth and its distortion, and two regions each carry their own volume
 * in the page's maps rather than one volume for the whole page.
 */

#include <array>

#include "support/SurfaceSceneFixture.h"

TEST(ComposeSurfaceScene, ATransmittingRegionPaintsItsWeightDepthAndVolume) {
  const auto scene = SurfaceScene::make({kWidth, kHeight}, fonts());
  ASSERT_NE(scene, nullptr);
  const material::Material glass = material::from(material::Color{1, 1, 1, 1})
                                       .surface({.roughness = .05f,
                                                 .transmission = 1.0f,
                                                 .ior = 1.33f,
                                                 .thickness = 20.0f});
  const material::Material shallow =
      material::from(material::Color{1, 1, 1, 1})
          .surface({.transmission = .5f, .ior = 1.33f, .thickness = 5.0f});
  // A swirl moving what stands behind right and down, at a strength of
  // two scene units.
  const material::Material haze =
      material::from(material::Color{.5f, .5f, .5f, 1})
          .surface(
              {.distortion = material::from(material::Color{1, .25f, .5f, 1}),
               .distortionStrength = 2.0f});
  scene->render(box().width(kWidth).height(kHeight).children({
      box().absolute().rect(0, 0, 64, 56).fill(glass),
      box().absolute().rect(0, 56, 64, 56).fill(shallow),
      box().absolute().rect(64, 0, 64, 56).fill(haze),
  }));
  EXPECT_TRUE(scene->lobes().transmission);
  EXPECT_TRUE(scene->lobes().distortion);
  const material::texture::TextureMaps maps = scene->maps();
  ASSERT_NE(maps.map(Role::Transmission), nullptr);
  ASSERT_NE(maps.map(Role::Thickness), nullptr);
  ASSERT_NE(maps.map(Role::Distortion), nullptr);
  // The deepest volume stated is what the thickness map's white stands
  // for, and the strongest distortion what its full swing does; the
  // volume itself is each region's own, in its maps.
  EXPECT_EQ(maps.thickness, 20);
  EXPECT_EQ(maps.distortionStrength, 2);
  EXPECT_FALSE(maps.volume.has_value());
  ASSERT_NE(maps.map(Role::Volume), nullptr);
  ASSERT_NE(maps.map(Role::Attenuation), nullptr);

  const SkBitmap weight = pixelsOf(*scene, Role::Transmission);
  expectNear(weight.getColor4f(32, 28), 1, 1, 1);
  expectNear(weight.getColor4f(32, 84), .5f, .5f, .5f);
  expectNear(weight.getColor4f(96, 84), 0, 0, 0);
  const SkBitmap depth = pixelsOf(*scene, Role::Thickness);
  expectNear(depth.getColor4f(32, 28), 1, 1, 1);
  expectNear(depth.getColor4f(32, 84), .25f, .25f, .25f);
  expectNear(depth.getColor4f(96, 84), 0, 0, 0);
  // Red the full two units across, green one unit down; still elsewhere.
  const SkBitmap moved = pixelsOf(*scene, Role::Distortion);
  expectNear(moved.getColor4f(96, 28), 1, .25f, 0);
  expectNear(moved.getColor4f(32, 28), .5f, .5f, 0);
  expectNear(moved.getColor4f(96, 84), .5f, .5f, 0);
}

TEST(ComposeSurfaceScene, TwoTransmittingRegionsCarryTheirOwnVolumes) {
  const auto scene = SurfaceScene::make({kWidth, kHeight}, fonts());
  ASSERT_NE(scene, nullptr);
  // Water on the left, a dense flint glass on the right: different
  // indices, spreads, tints and distances.
  const material::Material water =
      material::from(material::Color{1, 1, 1, 1})
          .surface({.transmission = 1.0f,
                    .ior = 1.33f,
                    .attenuationColor = {0.2f, 0.6f, 0.9f, 1},
                    .attenuationDistance = 3.0f});
  const material::Material flint =
      material::from(material::Color{1, 1, 1, 1})
          .surface({.transmission = 1.0f,
                    .ior = 1.9f,
                    .attenuationColor = {0.9f, 0.8f, 0.3f, 1},
                    .attenuationDistance = 0.5f,
                    .dispersion = 0.25f});
  scene->render(box().width(kWidth).height(kHeight).children({
      box().absolute().rect(0, 0, 64, kHeight).fill(water),
      box().absolute().rect(64, 0, 64, kHeight).fill(flint),
  }));
  ASSERT_TRUE(scene->lobes().transmission);
  const material::texture::TextureMaps maps = scene->maps();
  EXPECT_FALSE(maps.volume.has_value()) << "no one volume stands for the page";
  ASSERT_NE(maps.map(Role::Volume), nullptr);
  ASSERT_NE(maps.map(Role::Attenuation), nullptr);

  // The index as 1 - 1 / index, the spread and the distance each as
  // value / (value + 1).
  const auto volumeOf = [](float index, float spread, float distance) {
    return std::array<float, 3>{1 - 1 / index, spread / (spread + 1),
                                distance / (distance + 1)};
  };
  const SkBitmap volume = pixelsOf(*scene, Role::Volume);
  const auto left = volumeOf(1.33f, 0, 3);
  const auto right = volumeOf(1.9f, 0.25f, 0.5f);
  expectNear(volume.getColor4f(32, 56), left[0], left[1], left[2]);
  expectNear(volume.getColor4f(96, 56), right[0], right[1], right[2]);
  const SkBitmap tint = pixelsOf(*scene, Role::Attenuation);
  expectNear(tint.getColor4f(32, 56), 0.2f, 0.6f, 0.9f);
  expectNear(tint.getColor4f(96, 56), 0.9f, 0.8f, 0.3f);

  // A body dressed with the page's maps reads each region's volume from
  // them rather than one volume for the whole page.
  const material::Material dressed = material::surface::program(maps);
  EXPECT_NE(material::surface::map(dressed, material::surface::kVolumeSlot),
            nullptr);
  EXPECT_NE(
      material::surface::map(dressed, material::surface::kAttenuationSlot),
      nullptr);
}
