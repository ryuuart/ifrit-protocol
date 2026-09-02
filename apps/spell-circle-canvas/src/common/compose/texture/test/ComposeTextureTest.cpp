// A compose scene as a material texture: what makes the version move,
// what makes it stand still, and that the value a consumer holds is an
// ordinary texture with every dial on it.

#include <sigilcompose/texture/Texture.h>

#ifdef SIGILCOMPOSE_TEXTURE_DEVICE
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilskia/graphite/GraphiteContext.h>
#endif

#include "support/Host.h"

namespace {

/** A tree with something to paint: a filled box the size of the scene. */
Element plate(SkColor4f colour) {
  return box().width(pct(100)).height(pct(100)).fill(colour);
}

TEST(ComposeTexture, FirstRenderPaintsAndVersionsFromOne) {
  const std::shared_ptr<TextureScene> scene =
      TextureScene::make({64, 48}, fonts());
  EXPECT_EQ(scene->version(), 0u);
  EXPECT_EQ(scene->image(), nullptr);

  scene->render(plate(SkColors::kRed));
  EXPECT_EQ(scene->version(), 1u);
  ASSERT_NE(scene->image(), nullptr);
  EXPECT_EQ(scene->image()->width(), 64);
  EXPECT_EQ(scene->image()->height(), 48);
}

TEST(ComposeTexture, AStillTreeBumpsNothing) {
  const std::shared_ptr<TextureScene> scene =
      TextureScene::make({32, 32}, fonts());
  const Element still = plate(SkColors::kBlue);
  scene->render(still);
  const uint64_t painted = scene->version();
  for (int frame = 0; frame < 8; ++frame)
    scene->render(still, (double)(frame + 1) / 60.0);
  EXPECT_EQ(scene->version(), painted);
  EXPECT_FALSE(scene->active());
}

TEST(ComposeTexture, ADescriptionThatChangedPaintsAgain) {
  const std::shared_ptr<TextureScene> scene =
      TextureScene::make({32, 32}, fonts());
  scene->render(plate(SkColors::kBlue));
  const uint64_t painted = scene->version();
  scene->render(plate(SkColors::kGreen));
  EXPECT_EQ(scene->version(), painted + 1);
}

TEST(ComposeTexture, TheMaterialComparesEqualAcrossAStillFrame) {
  const std::shared_ptr<TextureScene> scene =
      TextureScene::make({32, 32}, fonts());
  const Element still = plate(SkColors::kYellow);
  scene->render(still);
  const sigil::material::Texture first = scene->texture();
  scene->render(still, 1.0 / 60.0);
  const sigil::material::Texture second = scene->texture();
  EXPECT_TRUE(first == second);

  scene->render(plate(SkColors::kMagenta), 2.0 / 60.0);
  EXPECT_FALSE(first == scene->texture());
}

TEST(ComposeTexture, EverySamplingDialRidesTheValue) {
  const std::shared_ptr<TextureScene> scene =
      TextureScene::make({16, 16}, fonts());
  scene->render(plate(SkColors::kWhite));
  sigil::material::Texture tiled = scene->texture();
  tiled.tile(SkTileMode::kRepeat).filter(SkFilterMode::kNearest);
  EXPECT_EQ(tiled.tileX(), SkTileMode::kRepeat);
  EXPECT_EQ(tiled.filter(), SkFilterMode::kNearest);
  // A dial is part of the value, so a texture that differs only by one
  // is a different texture.
  EXPECT_FALSE(tiled == scene->texture());

  sigil::material::Texture cut = scene->texture();
  cut.region(SkIRect::MakeWH(8, 8));
  EXPECT_EQ(cut.size(), SkISize::Make(8, 8));
  EXPECT_NE(cut.shader(), nullptr);
}

TEST(ComposeTexture, TheScenePaintsWhatTheTreeDescribed) {
  const std::shared_ptr<TextureScene> scene =
      TextureScene::make({8, 8}, fonts());
  scene->render(plate(SkColors::kRed));
  SkBitmap read;
  read.allocPixels(SkImageInfo::MakeN32Premul(8, 8));
  ASSERT_TRUE(scene->image()->readPixels(nullptr, read.pixmap(), 0, 0));
  EXPECT_EQ(read.getColor(4, 4), SK_ColorRED);
}

TEST(ComposeTexture, TheOneShotVerbHoldsItsOwnScene) {
  const sigil::material::Texture value =
      texture(plate(SkColors::kCyan), {24, 24}, fonts());
  ASSERT_TRUE(value.valid());
  EXPECT_EQ(value.size(), SkISize::Make(24, 24));
  // The scene stands behind the value, so a copy of the value is the
  // same texture and compares equal to it.
  EXPECT_TRUE(sigil::material::Texture(value) == value);
  // …and a second call is a second scene, which is a different texture
  // however alike the two pictures are.
  EXPECT_FALSE(value == texture(plate(SkColors::kCyan), {24, 24}, fonts()));
}

TEST(ComposeTexture, NoDeviceMeansNoDeviceImage) {
  const std::shared_ptr<TextureScene> scene =
      TextureScene::make({8, 8}, fonts());
  scene->render(plate(SkColors::kWhite));
  EXPECT_FALSE((bool)scene->texture().deviceImage());
}

#ifdef SIGILCOMPOSE_TEXTURE_DEVICE

TEST(ComposeTexture, ADeviceTakesTheSceneAndSaysWhereItStands) {
  namespace skia = sigil::skia;
  namespace core = sigil::core;
  std::string error;
  const std::unique_ptr<core::hardware::GpuDevice> device =
      core::hardware::GpuDevice::createOwned(&error);
  if (!device) GTEST_SKIP() << error;
  const std::unique_ptr<skia::GraphiteContext> context =
      skia::GraphiteContext::create(*device);
  if (!context) GTEST_SKIP() << "no Graphite context on this device";

  const std::shared_ptr<TextureScene> scene =
      TextureScene::make({32, 32}, fonts());
  ASSERT_TRUE(scene->useDevice(*device, *context));
  scene->render(plate(SkColors::kRed));

  // The pixels stand on the device, and the value says so — which is
  // what lets a renderer on that same device bind them instead of
  // uploading a copy.
  const sigil::material::DeviceImage image = scene->texture().deviceImage();
  ASSERT_TRUE((bool)image);
  EXPECT_EQ(image.device, device.get());
  EXPECT_EQ(image.width, 32);
  EXPECT_EQ(image.height, 32);
  // …and the scene still answers with an image, so a host with no
  // interest in the device reads it the way it reads any texture.
  ASSERT_NE(scene->image(), nullptr);
  EXPECT_TRUE(scene->image()->isTextureBacked());

  // The version rule is the surface's, not the device's.
  const uint64_t painted = scene->version();
  scene->render(plate(SkColors::kRed), 1.0 / 60.0);
  EXPECT_EQ(scene->version(), painted);
}

#endif

}  // namespace
