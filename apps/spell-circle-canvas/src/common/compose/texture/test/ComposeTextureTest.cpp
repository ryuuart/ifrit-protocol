// A compose scene as a material texture: what makes the version move,
// what makes it stand still, and that the value a consumer holds is an
// ordinary texture with every dial on it.

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColorSpace.h>
#include <include/core/SkPaint.h>
#include <include/core/SkString.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkRuntimeEffect.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilgeometry/device/Device.h>
#include <sigilmaterial/advanced/FrameData.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/skia/Texture.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/Readback.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <utility>

#include "OnDevice.h"
#include "support/Host.h"

namespace {

/** A tree with something to paint: a filled box the size of the scene. */
Element plate(SkColor4f colour) {
  return box().width(pct(100)).height(pct(100)).fill(colour);
}

SkBitmap readFloat(const sk_sp<SkImage>& image) {
  SkBitmap pixels;
  if (!image ||
      !pixels.tryAllocPixels(SkImageInfo::Make(
          image->width(), image->height(), kRGBA_F32_SkColorType,
          kPremul_SkAlphaType, image->refColorSpace())) ||
      !image->readPixels(nullptr, pixels.pixmap(), 0, 0))
    return {};
  return pixels;
}

std::array<float, 4> premul(const SkBitmap& pixels, int x, int y) {
  const auto* values = static_cast<const float*>(pixels.pixmap().addr(x, y));
  return {values[0], values[1], values[2], values[3]};
}

void expectPremul(const SkBitmap& pixels, int x, int y,
                  const std::array<float, 4>& expected,
                  float tolerance = .0001f) {
  const auto actual = premul(pixels, x, y);
  for (size_t channel = 0; channel < actual.size(); ++channel)
    EXPECT_NEAR(actual[channel], expected[channel], tolerance)
        << "channel " << channel;
}

Element layeredHdr() {
  namespace material = sigil::material;
  const material::Material finish =
      material::from(material::Color{4, 1, .5f, .5f})
          .layer(material::Color{.5f, 4, 2, .25f}, {.opacity = .5f});
  return box()
      .width(32)
      .height(24)
      .cache(Cache::Picture)
      .children(
          {box().width(32).height(24).cache(Cache::Texture).fill(finish)});
}

SkBitmap sampledRaster(const sigil::material::Texture& texture,
                       sk_sp<SkColorSpace> colorSpace = nullptr) {
  const auto surface = SkSurfaces::Raster(
      SkImageInfo::Make(16, 16, kRGBA_F32_SkColorType, kPremul_SkAlphaType,
                        std::move(colorSpace)));
  if (!surface) return {};
  surface->getCanvas()->clear(SK_ColorTRANSPARENT);
  SkPaint paint;
  paint.setShader(sigil::material::skia::shader(texture));
  if (!paint.getShader()) return {};
  surface->getCanvas()->drawPaint(paint);
  return readFloat(surface->makeImageSnapshot());
}

TEST(ComposeTexture, FirstRenderPaintsAndVersionsFromOne) {
  const std::shared_ptr<TextureScene> scene =
      TextureScene::make({64, 48}, fonts());
  EXPECT_EQ(scene->revision(), 0u);
  EXPECT_EQ(scene->image(), nullptr);

  scene->render(plate(SkColors::kRed));
  EXPECT_EQ(scene->revision(), 1u);
  ASSERT_NE(scene->image(), nullptr);
  EXPECT_EQ(scene->image()->width(), 64);
  EXPECT_EQ(scene->image()->height(), 48);
}

TEST(ComposeTexture, AStillTreeBumpsNothing) {
  const std::shared_ptr<TextureScene> scene =
      TextureScene::make({32, 32}, fonts());
  const Element still = plate(SkColors::kBlue);
  scene->render(still);
  const uint64_t painted = scene->revision();
  for (int frame = 0; frame < 8; ++frame)
    scene->render(still, (double)(frame + 1) / 60.0);
  EXPECT_EQ(scene->revision(), painted);
  EXPECT_FALSE(scene->isRunning());
}

TEST(ComposeTexture, AScaleReadingMaterialLetsTheSceneSleep) {
  // The scene's scale does not change between renders, so a material that
  // reads it leaves the scene still: no revision, the same image.
  auto [effect, err] = SkRuntimeEffect::MakeForShader(
      SkString("uniform float uContentScale;"
               "half4 main(float2 p) {"
               "  return half4(uContentScale * 0.25, 0, 0, 1); }"));
  ASSERT_TRUE(effect) << err.c_str();
  const std::shared_ptr<TextureScene> scene =
      TextureScene::make({32, 32}, fonts());
  const Element still = box().width(32).height(32).fill(
      sigil::material::skia::base(sigil::material::skia::sksl(effect)));
  for (int frame = 0; frame < 4; ++frame)
    scene->render(still, (double)frame / 60.0);
  const uint64_t settled = scene->revision();
  const sk_sp<SkImage> held = scene->image();
  for (int frame = 4; frame < 12; ++frame)
    scene->render(still, (double)frame / 60.0);
  EXPECT_EQ(scene->revision(), settled);
  EXPECT_EQ(scene->image(), held);
  EXPECT_FALSE(scene->isRunning());
}

TEST(ComposeTexture, PictureWrappedGroupTakesOneFollowUpRenderThenSleeps) {
  const auto scene = TextureScene::make({64, 48}, fonts());
  scene->setAutoTexturePromotion(PromotionPolicy::Off);
  const Element tree = box()
                           .width(64)
                           .height(48)
                           .cache(Cache::Picture)
                           .children({box()
                                          .width(40)
                                          .height(40)
                                          .cache(Cache::Group)
                                          .fill(SkColors::kGreen)});
  scene->render(tree);
  EXPECT_EQ(scene->revision(), 1u);
  EXPECT_EQ(scene->composer().stats().texturesBaked, 0u);
  ASSERT_TRUE(scene->isRunning());
  scene->render(tree, 1.0 / 60.0);
  EXPECT_EQ(scene->revision(), 2u);
  EXPECT_EQ(scene->composer().stats().texturesBaked, 1u);
  EXPECT_FALSE(scene->isRunning());
  const sk_sp<SkImage> held = scene->image();
  for (int frame = 2; frame < 8; ++frame)
    scene->render(tree, (double)frame / 60.0);
  EXPECT_EQ(scene->revision(), 2u);
  EXPECT_EQ(scene->image(), held);
  EXPECT_FALSE(scene->isRunning());
}

TEST(ComposeTexture, EarlierReadingsDoNotBankExtraSceneTime) {
  const auto scene = TextureScene::make({8, 8}, fonts());
  double seconds = -1.0;
  const Element clock =
      custom("clock", [&](sigil::draw::Pen&, const PaintContext& context) {
        seconds = context.elapsedSeconds;
      }).cache(Cache::None);
  scene->render(clock, 1.0);
  EXPECT_DOUBLE_EQ(seconds, 1.0);
  scene->render(clock, 0.5);
  EXPECT_DOUBLE_EQ(seconds, 1.0);
  scene->render(clock, 1.25);
  EXPECT_DOUBLE_EQ(seconds, 1.25);
}

TEST(ComposeTexture, NonFiniteReadingsDoNotPoisonTheSceneClock) {
  const auto scene = TextureScene::make({8, 8}, fonts());
  double seconds = -1.0;
  const Element clock =
      custom("clock", [&](sigil::draw::Pen&, const PaintContext& context) {
        seconds = context.elapsedSeconds;
      }).cache(Cache::None);
  scene->render(clock, 1.0);
  scene->render(clock, std::numeric_limits<double>::infinity());
  EXPECT_DOUBLE_EQ(seconds, 1.0);
  scene->render(clock, std::numeric_limits<double>::quiet_NaN());
  EXPECT_DOUBLE_EQ(seconds, 1.0);
  scene->render(clock, 1.5);
  EXPECT_DOUBLE_EQ(seconds, 1.5);
}

TEST(ComposeTexture, ADescriptionThatChangedPaintsAgain) {
  const std::shared_ptr<TextureScene> scene =
      TextureScene::make({32, 32}, fonts());
  scene->render(plate(SkColors::kBlue));
  const uint64_t painted = scene->revision();
  scene->render(plate(SkColors::kGreen));
  EXPECT_EQ(scene->revision(), painted + 1);
}

TEST(ComposeTexture, ARetainedBindingPaintsWhenItsOutputMoves) {
  sigil::motion::Animatable<float> alpha = sigil::motion::animatable(1.0f);
  const std::shared_ptr<TextureScene> scene =
      TextureScene::make({32, 32}, fonts());
  const Element retained = plate(SkColors::kRed).opacity(alpha);

  scene->render(retained);
  const uint64_t painted = scene->revision();
  ASSERT_TRUE(scene->isRunning());

  alpha = 0.0f;
  scene->render(retained, 1.0 / 60.0);
  EXPECT_EQ(scene->revision(), painted + 1)
      << "the unchanged description hid its moved binding";

  SkBitmap read;
  read.allocPixels(SkImageInfo::MakeN32Premul(32, 32));
  ASSERT_TRUE(scene->image()->readPixels(nullptr, read.pixmap(), 0, 0));
  EXPECT_EQ(read.getColor(16, 16), SK_ColorTRANSPARENT);
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
  tiled.tile(sigil::material::Repeat::Repeat)
      .sampling(sigil::material::Sampling::Nearest);
  EXPECT_EQ(tiled.tileX(), sigil::material::Repeat::Repeat);
  EXPECT_EQ(tiled.sampling(), sigil::material::Sampling::Nearest);
  // A dial is part of the value, so a texture that differs only by one
  // is a different texture.
  EXPECT_FALSE(tiled == scene->texture());

  sigil::material::Texture cut = scene->texture();
  cut.region({0, 0, 8, 8});
  EXPECT_EQ(cut.size(), glm::ivec2(8, 8));
  EXPECT_NE(sigil::material::skia::shader(cut), nullptr);
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
  EXPECT_EQ(value.size(), glm::ivec2(24, 24));
  // The scene stands behind the value, so a copy of the value is the
  // same texture and compares equal to it.
  EXPECT_TRUE(sigil::material::Texture(value) == value);
  // …and a second call is a second scene, which is a different texture
  // however alike the two pictures are.
  EXPECT_FALSE(value == texture(plate(SkColors::kCyan), {24, 24}, fonts()));
}

TEST(ComposeTexture,
     ExplicitFormatsRejectInvalidRequestsAndASizeAloneClampsToAnN32Pixel) {
  EXPECT_EQ(TextureScene::make(SkImageInfo{}, fonts()), nullptr);
  EXPECT_EQ(TextureScene::make(SkImageInfo::MakeN32Premul(0, 8), fonts()),
            nullptr);
  EXPECT_EQ(TextureScene::make(SkImageInfo::Make(8, 8, kRGBA_F16_SkColorType,
                                                 kUnpremul_SkAlphaType),
                               fonts()),
            nullptr);
  EXPECT_EQ(TextureScene::make(SkImageInfo::MakeA8(8, 8), fonts()), nullptr);
  EXPECT_EQ(
      TextureScene::make(SkImageInfo::Make(8, 8, kRGBA_F16Norm_SkColorType,
                                           kPremul_SkAlphaType),
                         fonts()),
      nullptr);
  EXPECT_FALSE(texture(plate(SkColors::kRed), SkImageInfo{}, fonts()).valid());
  const auto sceneFromSizeAlone =
      TextureScene::make(SkISize::Make(-4, 0), fonts());
  ASSERT_TRUE(sceneFromSizeAlone);
  EXPECT_EQ(sceneFromSizeAlone->size(), SkISize::Make(1, 1));
  sceneFromSizeAlone->render(plate(SkColors::kRed));
  ASSERT_TRUE(sceneFromSizeAlone->image());
  EXPECT_EQ(sceneFromSizeAlone->image()->colorType(), kN32_SkColorType);
  EXPECT_EQ(sceneFromSizeAlone->image()->colorSpace(), nullptr);
}

TEST(ComposeTexture, FloatBackgroundsKeepHdrAndPremultipliedAlpha) {
  for (const SkColorType type :
       {kRGBA_F16_SkColorType, kRGBA_F32_SkColorType}) {
    SCOPED_TRACE(static_cast<int>(type));
    const auto scene =
        TextureScene::make(SkImageInfo::Make(8, 8, type, kPremul_SkAlphaType),
                           fonts(), {4, 2, 1, .5f});
    ASSERT_TRUE(scene);
    scene->render(box().width(8).height(8));
    ASSERT_TRUE(scene->image());
    EXPECT_EQ(scene->image()->colorType(), type);
    EXPECT_EQ(scene->image()->alphaType(), kPremul_SkAlphaType);
    const auto pixels = readFloat(scene->image());
    ASSERT_FALSE(pixels.isNull());
    expectPremul(pixels, 4, 4, {2, 1, .5f, .5f});
  }
}

TEST(ComposeTexture, ByteFormatsPreserveChannelOrderAndProfiles) {
  for (const SkColorType type :
       {kRGBA_8888_SkColorType, kBGRA_8888_SkColorType}) {
    SCOPED_TRACE(static_cast<int>(type));
    const auto profile = SkColorSpace::MakeSRGB();
    const auto scene = TextureScene::make(
        SkImageInfo::Make(8, 8, type, kPremul_SkAlphaType, profile), fonts());
    ASSERT_TRUE(scene);
    scene->render(plate({.25f, .5f, .75f, .5f}));
    ASSERT_TRUE(scene->image());
    EXPECT_EQ(scene->image()->colorType(), type);
    EXPECT_TRUE(
        SkColorSpace::Equals(scene->image()->colorSpace(), profile.get()));
    const auto pixels = readFloat(scene->image());
    ASSERT_FALSE(pixels.isNull());
    expectPremul(pixels, 4, 4, {.125f, .25f, .375f, .5f}, 1.f / 255.f);
  }
}

TEST(ComposeTexture, FloatLayersAndTextureSamplingKeepTheirRangeWhileHeld) {
  for (const SkColorType type :
       {kRGBA_F16_SkColorType, kRGBA_F32_SkColorType}) {
    SCOPED_TRACE(static_cast<int>(type));
    const auto scene = TextureScene::make(
        SkImageInfo::Make(32, 24, type, kPremul_SkAlphaType), fonts());
    ASSERT_TRUE(scene);
    scene->setAutoTexturePromotion(PromotionPolicy::Off);
    const Element tree = layeredHdr();
    scene->render(tree);
    scene->render(tree, 1.0 / 60.0);
    ASSERT_FALSE(scene->isRunning());
    const auto heldImage = scene->image();
    const auto heldRevision = scene->revision();
    const auto pixels = readFloat(heldImage);
    ASSERT_FALSE(pixels.isNull());
    expectPremul(pixels, 16, 12, {1.8125f, .9375f, .46875f, .5625f});
    auto sampled = scene->texture();
    const auto original = sampled;
    sampled.region({4, 2, 16, 16})
        .tile(sigil::material::Repeat::Repeat)
        .sampling(sigil::material::Sampling::Nearest);
    const auto sampledPixels = sampledRaster(sampled);
    ASSERT_FALSE(sampledPixels.isNull());
    expectPremul(sampledPixels, 8, 8, {1.8125f, .9375f, .46875f, .5625f});
    EXPECT_NE(original, sampled);
    EXPECT_EQ(original.region(), std::nullopt);
    EXPECT_EQ(original.size(), glm::ivec2(32, 24));
    EXPECT_EQ(sampled.size(), glm::ivec2(16, 16));
    scene->render(tree, 2.0 / 60.0);
    EXPECT_EQ(scene->revision(), heldRevision);
    EXPECT_EQ(scene->image(), heldImage);
    EXPECT_EQ(scene->texture(), original);
  }
}

TEST(ComposeTexture, ExplicitProfilesMatchAnIndependentCanvasAndOneShotCopies) {
  for (const auto profile :
       {SkColorSpace::MakeSRGB(), SkColorSpace::MakeSRGBLinear()}) {
    const SkImageInfo info = SkImageInfo::Make(16, 16, kRGBA_F32_SkColorType,
                                               kPremul_SkAlphaType, profile);
    const sigil::material::Color background{4, 2, 1, .5f};
    const auto scene = TextureScene::make(info, fonts(), background);
    ASSERT_TRUE(scene);
    scene->render(box().width(16).height(16));
    ASSERT_TRUE(scene->image());
    EXPECT_TRUE(
        SkColorSpace::Equals(scene->image()->colorSpace(), profile.get()));
    const auto expected = SkSurfaces::Raster(info);
    ASSERT_TRUE(expected);
    expected->getCanvas()->clear(SkColor4f{4, 2, 1, .5f});
    const auto reference = readFloat(expected->makeImageSnapshot());
    const auto actual = readFloat(scene->image());
    ASSERT_FALSE(reference.isNull());
    ASSERT_FALSE(actual.isNull());
    expectPremul(actual, 8, 8, premul(reference, 8, 8));
    const auto held =
        texture(box().width(16).height(16), info, fonts(), background);
    const auto copy = held;
    EXPECT_EQ(copy, held);
    const auto sampled = sampledRaster(copy, profile);
    ASSERT_FALSE(sampled.isNull());
    expectPremul(sampled, 8, 8, premul(reference, 8, 8));
  }
}

TEST(ComposeTexture, ASourceReadsLatestPixelsAndKeepsItsCapturedRevision) {
  const auto scene = TextureScene::make(
      SkImageInfo::Make(8, 8, kRGBA_F16_SkColorType, kPremul_SkAlphaType),
      fonts());
  ASSERT_TRUE(scene);
  scene->render(plate(SkColors::kRed));
  const uint64_t revision = scene->revision();
  const SceneSource source(scene, revision);
  const auto held = scene->texture();
  const auto oldImage = scene->image();
  scene->render(plate(SkColors::kBlue), 1.0 / 60.0);
  EXPECT_NE(scene->image(), oldImage);
  EXPECT_EQ(source.revision(), revision);
  EXPECT_EQ(sigil::media::toSk(source.frameAt({}).image), scene->image());
  EXPECT_NE(held, scene->texture());
}

TEST(ComposeTexture, NoDeviceMeansNoDeviceImage) {
  const std::shared_ptr<TextureScene> scene =
      TextureScene::make({8, 8}, fonts());
  scene->render(plate(SkColors::kWhite));
  EXPECT_FALSE((bool)scene->texture().deviceImage());
}

void expectDevicePrecision(sigil::core::hardware::GpuDevice& device,
                           sigil::skia::GraphiteContext& context) {
  for (const auto profile :
       {sk_sp<SkColorSpace>{}, SkColorSpace::MakeSRGBLinear()}) {
    const SkImageInfo info = SkImageInfo::Make(32, 24, kRGBA_F16_SkColorType,
                                               kPremul_SkAlphaType, profile);
    const auto scene = TextureScene::make(info, fonts(), {4, 2, 1, .5f});
    ASSERT_TRUE(scene);
    scene->setAutoTexturePromotion(PromotionPolicy::Off);
    const auto tree = layeredHdr();
    scene->render(tree);
    scene->render(tree, 1.0 / 60.0);
    const auto rasterImage = scene->image();
    const auto reference = readFloat(rasterImage);
    ASSERT_FALSE(reference.isNull());
    const auto revision = scene->revision();
    ASSERT_TRUE(scene->useDevice(device, context));
    EXPECT_EQ(scene->revision(), revision);
    EXPECT_EQ(scene->image(), nullptr);
    EXPECT_FALSE(scene->deviceImage());
    scene->render(tree, 2.0 / 60.0);
    scene->render(tree, 3.0 / 60.0);
    ASSERT_TRUE(scene->image());
    EXPECT_TRUE(scene->image()->isTextureBacked());
    EXPECT_EQ(scene->image()->colorType(), kRGBA_F16_SkColorType);
    EXPECT_EQ(scene->image()->alphaType(), kPremul_SkAlphaType);
    EXPECT_TRUE(
        SkColorSpace::Equals(scene->image()->colorSpace(), profile.get()));
    EXPECT_EQ(scene->deviceImage().device, &device);
    const auto target = SkSurfaces::RenderTarget(context.recorder(), info);
    ASSERT_TRUE(target);
    target->getCanvas()->clear(SK_ColorTRANSPARENT);
    const sigil::material::FrameData frame{.resolution = {32, 24},
                                           .recorder = context.recorder()};
    SkPaint paint;
    paint.setShader(sigil::material::skia::shader(scene->texture(), frame));
    ASSERT_TRUE(paint.getShader());
    target->getCanvas()->drawPaint(paint);
    SkBitmap pixels;
    ASSERT_TRUE(
        pixels.tryAllocPixels(info.makeColorType(kRGBA_F32_SkColorType)));
    ASSERT_TRUE(sigil::skia::readbackPixels(context, *target, pixels.pixmap()));
    const auto expected = premul(reference, 16, 12);
    const auto actual = premul(pixels, 16, 12);
    for (size_t channel = 0; channel < 3; ++channel)
      EXPECT_NEAR(actual[channel], expected[channel],
                  std::max(.002f, std::abs(expected[channel]) * .002f));
    EXPECT_FLOAT_EQ(actual[3], expected[3]);
    EXPECT_GT(premul(pixels, 16, 12)[0], 1.f);
    const auto heldImage = scene->image();
    const auto heldRevision = scene->revision();
    scene->render(tree, 4.0 / 60.0);
    EXPECT_FALSE(scene->isRunning());
    EXPECT_EQ(scene->image(), heldImage);
    EXPECT_EQ(scene->revision(), heldRevision);
    const auto oldRaster = readFloat(rasterImage);
    ASSERT_FALSE(oldRaster.isNull());
    expectPremul(oldRaster, 16, 12, expected);
  }

  const auto scene = TextureScene::make(
      SkImageInfo::Make(8, 8, kRGBA_F32_SkColorType, kPremul_SkAlphaType),
      fonts());
  ASSERT_TRUE(scene);
  const auto tree = plate({4, 2, 1, .5f});
  scene->render(tree);
  const auto image = scene->image();
  const auto revision = scene->revision();
  const auto value = scene->texture();
  const auto textures = device.textureCount();
  const auto retired = device.pendingDestroys();
  EXPECT_FALSE(scene->useDevice(device, context));
  EXPECT_EQ(device.textureCount(), textures);
  EXPECT_EQ(device.pendingDestroys(), retired);
  EXPECT_EQ(scene->image(), image);
  EXPECT_EQ(scene->revision(), revision);
  EXPECT_EQ(scene->texture(), value);
  EXPECT_FALSE(scene->deviceImage());
  scene->render(tree, 1.0 / 60.0);
  EXPECT_EQ(scene->image(), image);
  EXPECT_EQ(scene->revision(), revision);
  scene->render(plate({2, 4, 1, .5f}), 2.0 / 60.0);
  EXPECT_EQ(scene->revision(), revision + 1);
  const auto changed = readFloat(scene->image());
  ASSERT_FALSE(changed.isNull());
  expectPremul(changed, 4, 4, {1, 2, .5f, .5f});
}

TEST(ComposeTexture,
     HalfFloatMetalKeepsHdrProfilesAndRefusesF32WithoutStateLoss) {
  std::string error;
  const auto device = sigil::core::hardware::GpuDevice::createOwned(&error);
  if (!device) GTEST_SKIP() << error;
  const auto context = sigil::skia::GraphiteContext::create(*device);
  if (!context) GTEST_SKIP() << "no Graphite context on this device";
  expectDevicePrecision(*device, *context);
}

TEST(ComposeTexture,
     HalfFloatVulkanKeepsHdrProfilesAndRefusesF32WithoutStateLoss) {
  SIGIL_ON_DEVICE_OR_SKIP(device);
  if (!device->gpu() || !device->graphite())
    GTEST_SKIP() << "no adopted Graphite context on this Vulkan device";
  const sigil::geometry::device::Device::QueueLock lock(*device);
  expectDevicePrecision(*device->gpu(), *device->graphite());
}

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
  EXPECT_FALSE((bool)scene->deviceImage());
  EXPECT_FALSE((bool)scene->texture().deviceImage());
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
  const uint64_t painted = scene->revision();
  scene->render(plate(SkColors::kRed), 1.0 / 60.0);
  EXPECT_EQ(scene->revision(), painted);
}

}  // namespace
