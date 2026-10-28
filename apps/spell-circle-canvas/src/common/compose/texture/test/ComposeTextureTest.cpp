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
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Stamp.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilcompose/typography/TextFx.h>
#include <sigilcompose/typography/Track.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilgeometry/device/Device.h>
#include <sigilmaterial/advanced/FrameData.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/skia/Texture.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmedia/core/Frame.h>
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
#include "support/Rows.h"

namespace {

/** A tree with something to paint: a filled box the size of the scene. */
Element plate(SkColor4f colour) {
  return box().width(percent(100)).height(percent(100)).fill(colour);
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

TEST(ComposeTexture, AGroupUnderARecordingTakesOneMoreRenderThenSleeps) {
  const auto scene = TextureScene::make({64, 48}, fonts());
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

TEST(ComposeTexture, TheSceneClockAdvancesOnlyToALaterFiniteReading) {
  sigil::compose::test::checkRows({
      {"AnEarlierReadingBanksNoTime",
       [&] {
         const auto scene = TextureScene::make({8, 8}, fonts());
         double seconds = -1.0;
         const Element clock =
             custom("clock", [&](sigil::draw::Pen&,
                                 const PaintContext& context) {
               seconds = context.elapsedSeconds;
             }).cache(Cache::None);
         scene->render(clock, 1.0);
         EXPECT_DOUBLE_EQ(seconds, 1.0);
         scene->render(clock, 0.5);
         EXPECT_DOUBLE_EQ(seconds, 1.0);
         scene->render(clock, 1.25);
         EXPECT_DOUBLE_EQ(seconds, 1.25);
       }},
      {"ANonFiniteReadingLeavesTheClock",
       [&] {
         const auto scene = TextureScene::make({8, 8}, fonts());
         double seconds = -1.0;
         const Element clock =
             custom("clock", [&](sigil::draw::Pen&,
                                 const PaintContext& context) {
               seconds = context.elapsedSeconds;
             }).cache(Cache::None);
         scene->render(clock, 1.0);
         scene->render(clock, std::numeric_limits<double>::infinity());
         EXPECT_DOUBLE_EQ(seconds, 1.0);
         scene->render(clock, std::numeric_limits<double>::quiet_NaN());
         EXPECT_DOUBLE_EQ(seconds, 1.0);
         scene->render(clock, 1.5);
         EXPECT_DOUBLE_EQ(seconds, 1.5);
       }},
  });
}

TEST(ComposeTexture, ASceneCountsAPaintOnlyWhenSomethingItPaintsChanged) {
  sigil::compose::test::checkRows({
      {"TheFirstRenderPaintsAndCountsOne",
       [&] {
         const std::shared_ptr<TextureScene> scene =
             TextureScene::make({64, 48}, fonts());
         EXPECT_EQ(scene->revision(), 0u);
         EXPECT_EQ(scene->image(), nullptr);

         scene->render(plate(SkColors::kRed));
         EXPECT_EQ(scene->revision(), 1u);
         ASSERT_NE(scene->image(), nullptr);
         EXPECT_EQ(scene->image()->width(), 64);
         EXPECT_EQ(scene->image()->height(), 48);
       }},
      {"AStillTreeCountsNothing",
       [&] {
         const std::shared_ptr<TextureScene> scene =
             TextureScene::make({32, 32}, fonts());
         const Element still = plate(SkColors::kBlue);
         scene->render(still);
         const uint64_t painted = scene->revision();
         for (int frame = 0; frame < 8; ++frame)
           scene->render(still, (double)(frame + 1) / 60.0);
         EXPECT_EQ(scene->revision(), painted);
         EXPECT_FALSE(scene->isRunning());
       }},
      {"AMaterialReadingAnUnchangedScaleLetsTheSceneSleep",
       [&] {
         // The scene's scale does not change between renders, so a material
         // that reads it leaves the scene still: no revision, the same image.
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
       }},
      {"AChangedDescriptionPaintsAgain",
       [&] {
         const std::shared_ptr<TextureScene> scene =
             TextureScene::make({32, 32}, fonts());
         scene->render(plate(SkColors::kBlue));
         const uint64_t painted = scene->revision();
         scene->render(plate(SkColors::kGreen));
         EXPECT_EQ(scene->revision(), painted + 1);
       }},
      {"ABindingWhoseOutputMovesPaintsAgain",
       [&] {
         sigil::motion::Animatable<float> alpha =
             sigil::motion::animatable(1.0f);
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
       }},
  });
}

TEST(ComposeTexture, TheTextureIsTheSceneAtItsRevisionWithEveryDial) {
  sigil::compose::test::checkRows({
      {"EqualAcrossAStillFrameAndNotAfterAPaint",
       [&] {
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
       }},
      {"EverySamplingDialIsPartOfTheValue",
       [&] {
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
       }},
  });
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

TEST(ComposeTexture, TexturesPaintOncePerDistinctValue) {
  Textures maps(fonts());
  // Two trees built apart that describe the same thing are one map: the
  // second ask is handed the first texture and paints nothing.
  const sigil::material::Texture red =
      maps.paint(plate(SkColors::kRed), {8, 8});
  const sigil::material::Texture again =
      maps.paint(plate(SkColors::kRed), {8, 8});
  EXPECT_EQ(maps.paints(), 1u);
  EXPECT_TRUE(again == red);
  // A different tree, the same tree at another size and the same tree over
  // another background are each a map of their own.
  const sigil::material::Texture blue =
      maps.paint(plate(SkColors::kBlue), {8, 8});
  const sigil::material::Texture larger =
      maps.paint(plate(SkColors::kRed), {16, 8});
  const sigil::material::Texture grounded =
      maps.paint(plate(SkColors::kRed), {8, 8}, {0, 0, 0, 1});
  EXPECT_EQ(maps.paints(), 4u);
  EXPECT_EQ(maps.kept(), 4u);
  EXPECT_FALSE(blue == red);
  EXPECT_FALSE(larger == red);
  EXPECT_FALSE(grounded == red);
  EXPECT_EQ(larger.size(), glm::ivec2(16, 8));
  // What it hands back is the tree painted, as the one-shot verb paints it.
  const sk_sp<SkImage> pixels = sigil::media::toSk(blue.frameAt({}).image);
  ASSERT_TRUE(pixels);
  SkBitmap read;
  read.allocPixels(SkImageInfo::MakeN32Premul(8, 8));
  ASSERT_TRUE(pixels->readPixels(nullptr, read.pixmap(), 0, 0));
  EXPECT_EQ(read.getColor(4, 4), SK_ColorBLUE);
}

/** The colour at the middle of a texture the door handed out. */
SkColor middleOf(const sigil::material::Texture& painted) {
  const sk_sp<SkImage> pixels = sigil::media::toSk(painted.frameAt({}).image);
  if (!pixels) return SK_ColorTRANSPARENT;
  SkBitmap read;
  read.allocPixels(
      SkImageInfo::MakeN32Premul(pixels->width(), pixels->height()));
  if (!pixels->readPixels(nullptr, read.pixmap(), 0, 0))
    return SK_ColorTRANSPARENT;
  return read.getColor(pixels->width() / 2, pixels->height() / 2);
}

TEST(ComposeTexture,
     TexturesPaintATreeHoldingAProgramAgainUnlessItIsTheSameValue) {
  sigil::compose::test::checkRows({
      {"AnUnkeyedProgram",
       [&] {
         Textures maps(fonts());
         const auto programmed = [] {
           // A paint program is a callable no comparison sees into.
           return box().width(8).height(8).children(
               {custom([](sigil::draw::Pen&, const PaintContext&) {})});
         };
         (void)maps.paint(programmed(), {8, 8});
         (void)maps.paint(programmed(), {8, 8});
         EXPECT_EQ(maps.paints(), 2u);
         // The same value handed over again is the one picture it was, program
         // and all, and is handed back unpainted.
         const Element held = programmed();
         const sigil::material::Texture first = maps.paint(held, {8, 8});
         EXPECT_TRUE(maps.paint(held, {8, 8}) == first);
         EXPECT_EQ(maps.paints(), 3u);
       }},
      {"ACustomNodesKey",
       [&] {
         Textures maps(fonts());
         // Two maps built apart that draw through the same custom key: the
         // reconciler takes the key as the program's name across one place's
         // describes, but these share no place, so the door paints each.
         const auto keyed = [](SkColor color) {
           return box().width(8).height(8).children(
               {custom("ground",
                       [color](sigil::draw::Pen& pen, const PaintContext&) {
                         pen.canvas()->drawColor(color);
                       })
                    .width(8)
                    .height(8)});
         };
         const sigil::material::Texture red =
             maps.paint(keyed(SK_ColorRED), {8, 8});
         const sigil::material::Texture blue =
             maps.paint(keyed(SK_ColorBLUE), {8, 8});
         EXPECT_EQ(maps.paints(), 2u);
         EXPECT_FALSE(blue == red);
         EXPECT_EQ(middleOf(red), SK_ColorRED);
         EXPECT_EQ(middleOf(blue), SK_ColorBLUE);
         // A keyed program shared by two trees built around it is one node, so
         // the trees are one picture and the second is handed the first.
         const Element ground =
             custom("ground",
                    [](sigil::draw::Pen& pen, const PaintContext&) {
                      pen.canvas()->drawColor(SK_ColorGREEN);
                    })
                 .width(8)
                 .height(8);
         const sigil::material::Texture green =
             maps.paint(box().width(8).height(8).children({ground}), {8, 8});
         const sigil::material::Texture greenAgain =
             maps.paint(box().width(8).height(8).children({ground}), {8, 8});
         EXPECT_EQ(maps.paints(), 3u);
         EXPECT_TRUE(greenAgain == green);
         EXPECT_EQ(middleOf(green), SK_ColorGREEN);
       }},
      {"AnOperatorsOrATextEffectsKey",
       [&] {
         // A keyed operator over a program and a keyed text effect over a body
         // compare equal across the describes of one place on their author's
         // word; trees built apart share no place, so the door paints each.
         Textures maps(fonts());
         const auto drawn = [](SkColor color) {
           return box().width(8).height(8).operators({drawWith(
               "ground", [color](sigil::draw::Pen& pen, const Scope&) {
                 pen.canvas()->drawColor(color);
               })});
         };
         const sigil::material::Texture red =
             maps.paint(drawn(SK_ColorRED), {8, 8});
         const sigil::material::Texture blue =
             maps.paint(drawn(SK_ColorBLUE), {8, 8});
         EXPECT_EQ(maps.paints(), 2u)
             << "two drawWith programs under one key were handed one texture";
         EXPECT_EQ(middleOf(red), SK_ColorRED);
         EXPECT_EQ(middleOf(blue), SK_ColorBLUE);

         const auto stamped = [](sigil::material::Color color) {
           return box()
               .width(8)
               .height(8)
               .children(
                   {box().key("cell").width(8).height(8).attribute("cell", 1)})
               .operators({stamp::ByLane{.lane = "cell",
                                         .key = "ground",
                                         .make = [color](const Scope::Node&) {
                                           return box().width(8).height(8).fill(
                                               Fill::color(color));
                                         }}});
         };
         (void)maps.paint(stamped({1, 0, 0, 1}), {8, 8});
         (void)maps.paint(stamped({0, 0, 1, 1}), {8, 8});
         EXPECT_EQ(maps.paints(), 4u)
             << "two stamp makers under one key were handed one texture";

         const auto moved = [](float dx) {
           GlyphModifier shift;
           shift.dx = dx;
           return box().width(8).height(8).children({text(u8"H").textFx(
               {.effect = textFx::effect(
                    "shift", [shift](const GlyphInfo&, float,
                                     sigil::core::noise::Mix64Stream&) {
                      return shift;
                    })})});
         };
         (void)maps.paint(moved(0), {8, 8});
         (void)maps.paint(moved(4), {8, 8});
         EXPECT_EQ(maps.paints(), 6u)
             << "two text effect bodies under one key were handed one texture";
       }},
  });
}

TEST(ComposeTexture, TexturesKeepWhatASweepFoundAskedForAndLetTheRestGo) {
  Textures maps(fonts());
  (void)maps.paint(plate(SkColors::kRed), {8, 8});
  (void)maps.paint(plate(SkColors::kBlue), {8, 8});
  maps.sweep();
  EXPECT_EQ(maps.kept(), 2u);
  // A declaration that asks for the red map again and not the blue one:
  // the red one is handed back unpainted and the blue one goes.
  (void)maps.paint(plate(SkColors::kRed), {8, 8});
  maps.sweep();
  EXPECT_EQ(maps.kept(), 1u);
  EXPECT_EQ(maps.paints(), 2u);
  (void)maps.paint(plate(SkColors::kBlue), {8, 8});
  EXPECT_EQ(maps.paints(), 3u);
}

TEST(ComposeTexture, AFloatSceneKeepsValuesAboveOne) {
  sigil::compose::test::checkRows({
      {"ThroughItsBackground",
       [&] {
         for (const SkColorType type :
              {kRGBA_F16_SkColorType, kRGBA_F32_SkColorType}) {
           SCOPED_TRACE(static_cast<int>(type));
           const auto scene = TextureScene::make(
               SkImageInfo::Make(8, 8, type, kPremul_SkAlphaType), fonts(),
               {4, 2, 1, .5f});
           ASSERT_TRUE(scene);
           scene->render(box().width(8).height(8));
           ASSERT_TRUE(scene->image());
           EXPECT_EQ(scene->image()->colorType(), type);
           EXPECT_EQ(scene->image()->alphaType(), kPremul_SkAlphaType);
           const auto pixels = readFloat(scene->image());
           ASSERT_FALSE(pixels.isNull());
           expectPremul(pixels, 4, 4, {2, 1, .5f, .5f});
         }
       }},
      {"ThroughHeldLayersAndTextureSampling",
       [&] {
         for (const SkColorType type :
              {kRGBA_F16_SkColorType, kRGBA_F32_SkColorType}) {
           SCOPED_TRACE(static_cast<int>(type));
           const auto scene = TextureScene::make(
               SkImageInfo::Make(32, 24, type, kPremul_SkAlphaType), fonts());
           ASSERT_TRUE(scene);
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
           expectPremul(sampledPixels, 8, 8,
                        {1.8125f, .9375f, .46875f, .5625f});
           EXPECT_NE(original, sampled);
           EXPECT_EQ(original.region(), std::nullopt);
           EXPECT_EQ(original.size(), glm::ivec2(32, 24));
           EXPECT_EQ(sampled.size(), glm::ivec2(16, 16));
           scene->render(tree, 2.0 / 60.0);
           EXPECT_EQ(scene->revision(), heldRevision);
           EXPECT_EQ(scene->image(), heldImage);
           EXPECT_EQ(scene->texture(), original);
         }
       }},
  });
}

TEST(ComposeTexture,
     AnExplicitFormatAndProfileAreKeptAndAnUnsupportedOneRefused) {
  sigil::compose::test::checkRows({
      {"AnUnsupportedRequestIsRefusedAndASizeAloneClampsToOneN32Pixel",
       [&] {
         EXPECT_EQ(TextureScene::make(SkImageInfo{}, fonts()), nullptr);
         EXPECT_EQ(
             TextureScene::make(SkImageInfo::MakeN32Premul(0, 8), fonts()),
             nullptr);
         EXPECT_EQ(
             TextureScene::make(SkImageInfo::Make(8, 8, kRGBA_F16_SkColorType,
                                                  kUnpremul_SkAlphaType),
                                fonts()),
             nullptr);
         EXPECT_EQ(TextureScene::make(SkImageInfo::MakeA8(8, 8), fonts()),
                   nullptr);
         EXPECT_EQ(TextureScene::make(
                       SkImageInfo::Make(8, 8, kRGBA_F16Norm_SkColorType,
                                         kPremul_SkAlphaType),
                       fonts()),
                   nullptr);
         EXPECT_FALSE(
             texture(plate(SkColors::kRed), SkImageInfo{}, fonts()).valid());
         const auto sceneFromSizeAlone =
             TextureScene::make(SkISize::Make(-4, 0), fonts());
         ASSERT_TRUE(sceneFromSizeAlone);
         EXPECT_EQ(sceneFromSizeAlone->size(), SkISize::Make(1, 1));
         sceneFromSizeAlone->render(plate(SkColors::kRed));
         ASSERT_TRUE(sceneFromSizeAlone->image());
         EXPECT_EQ(sceneFromSizeAlone->image()->colorType(), kN32_SkColorType);
         EXPECT_EQ(sceneFromSizeAlone->image()->colorSpace(), nullptr);
       }},
      {"ByteFormatsKeepChannelOrderAndProfile",
       [&] {
         for (const SkColorType type :
              {kRGBA_8888_SkColorType, kBGRA_8888_SkColorType}) {
           SCOPED_TRACE(static_cast<int>(type));
           const auto profile = SkColorSpace::MakeSRGB();
           const auto scene = TextureScene::make(
               SkImageInfo::Make(8, 8, type, kPremul_SkAlphaType, profile),
               fonts());
           ASSERT_TRUE(scene);
           scene->render(plate({.25f, .5f, .75f, .5f}));
           ASSERT_TRUE(scene->image());
           EXPECT_EQ(scene->image()->colorType(), type);
           EXPECT_TRUE(SkColorSpace::Equals(scene->image()->colorSpace(),
                                            profile.get()));
           const auto pixels = readFloat(scene->image());
           ASSERT_FALSE(pixels.isNull());
           expectPremul(pixels, 4, 4, {.125f, .25f, .375f, .5f}, 1.f / 255.f);
         }
       }},
      {"AnExplicitProfileMatchesAnIndependentCanvas",
       [&] {
         for (const auto profile :
              {SkColorSpace::MakeSRGB(), SkColorSpace::MakeSRGBLinear()}) {
           const SkImageInfo info = SkImageInfo::Make(
               16, 16, kRGBA_F32_SkColorType, kPremul_SkAlphaType, profile);
           const sigil::material::Color background{4, 2, 1, .5f};
           const auto scene = TextureScene::make(info, fonts(), background);
           ASSERT_TRUE(scene);
           scene->render(box().width(16).height(16));
           ASSERT_TRUE(scene->image());
           EXPECT_TRUE(SkColorSpace::Equals(scene->image()->colorSpace(),
                                            profile.get()));
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
       }},
  });
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
     HalfFloatOnAnOwnedDeviceKeepsHdrProfilesAndRefusesF32WithoutStateLoss) {
  std::string error;
  auto made = sigil::core::hardware::GpuDevice::createOwned();
  if (!made) GTEST_SKIP() << made.error().message;
  const auto device = std::move(*made);
  const auto context = sigil::skia::GraphiteContext::create(*device);
  if (!context) GTEST_SKIP() << "no Graphite context on this device";
  expectDevicePrecision(*device, *context);
}

TEST(ComposeTexture,
     HalfFloatOnTheSharedDeviceKeepsHdrProfilesAndRefusesF32WithoutStateLoss) {
  SIGIL_ON_DEVICE_OR_SKIP(device);
  if (!device->gpu() || !device->graphite())
    GTEST_SKIP() << "no adopted Graphite context on this Vulkan device";
  const sigil::geometry::device::Device::QueueLock lock(*device);
  expectDevicePrecision(*device->gpu(), *device->graphite());
}

// A SCENE ON THE DEVICE 2D AND 3D SHARE stands in a texture that device
// made, with the whole chain its size can hold, and says so: the image
// the device names, the levels below the one painted for its wearer to
// fill, and — where the device paints its 2D through Metal — the Metal
// texture behind that image, which is what was painted.
TEST(ComposeTexture, ASceneOnTheSharedDeviceStandsInATextureWithItsOwnChain) {
  SIGIL_ON_DEVICE_OR_SKIP(device);
  if (!device->gpu() || !device->graphite())
    GTEST_SKIP() << "no adopted Graphite context on this Vulkan device";
  const std::shared_ptr<TextureScene> scene =
      TextureScene::make({48, 20}, fonts());
  ASSERT_TRUE(scene);
  {
    const sigil::geometry::device::Device::QueueLock lock(*device);
    ASSERT_TRUE(scene->useDevice(*device->gpu(), *device->graphite()));
    scene->render(plate({0, 0, 1, 1}));
  }
  const sigil::material::DeviceImage where = scene->deviceImage();
  ASSERT_TRUE(where);
  EXPECT_EQ(where.device, device->gpu());
  EXPECT_NE(where.handle, 0u);
  EXPECT_EQ(where.mipLevels, sigil::core::hardware::mipLevelsFor(48, 20));
  const sigil::media::Frame frame = scene->texture().source().frameAt({});
  EXPECT_EQ(frame.device.mipLevels, where.mipLevels);
  EXPECT_EQ(frame.device.handle, where.handle);
#ifdef __APPLE__
  if (device->graphite()->recorder()->backend() == skgpu::BackendApi::kMetal)
    EXPECT_NE(where.pointer, nullptr);
#endif
  // …and the pixels are the scene's, read back through the context that
  // painted them.
  const sk_sp<SkImage> copy = scene->hostImage();
  ASSERT_TRUE(copy);
  SkBitmap pixels;
  ASSERT_TRUE(pixels.tryAllocPixels(SkImageInfo::MakeN32Premul(48, 20)));
  ASSERT_TRUE(copy->readPixels(nullptr, pixels.pixmap(), 0, 0));
  const SkColor4f centre = pixels.getColor4f(24, 10);
  EXPECT_NEAR(centre.fB, 1.f, .01f);
  EXPECT_NEAR(centre.fR, 0.f, .01f);
}

// A STACK OF LAYERS EACH MASKED OR PARTLY OPAQUE, over a texture painted
// on the CPU and read as data, paints on the shared device what it paints
// on the CPU. Every such layer mixes what is beneath it with what is
// beneath it blended under the layer, and what is beneath is one input
// read once: read twice per layer, the texture would be sampled twice as
// often with each layer, past the samplers one fragment program can bind
// on Metal, and the device would paint no picture at all.
TEST(ComposeTexture, AStackOfMaskedLayersPaintsOnTheSharedDeviceAsOnTheCpu) {
  namespace material = sigil::material;
  using material::ColorChannel;
  SIGIL_ON_DEVICE_OR_SKIP(device);
  if (!device->gpu() || !device->graphite())
    GTEST_SKIP() << "no adopted Graphite context on this Vulkan device";
  const std::shared_ptr<TextureScene> painted =
      TextureScene::make({8, 32}, fonts(), material::Color{0, 0, 0, 1});
  ASSERT_TRUE(painted);
  painted->render(plate({.2f, .6f, .3f, 1}));
  ASSERT_FALSE(painted->image()->isTextureBacked());
  const material::Material depth =
      material::image(painted->texture()
                          .tile(material::Repeat::Repeat)
                          .sampling(material::Sampling::Linear)
                          .colorSpace(sigil::media::ColorSpace::Data));
  const material::Material stack =
      material::from(material::Color{.1f, .2f, .7f, 1})
          .layer(material::Color{.9f, .8f, .1f, 1},
                 {.mask = {depth, ColorChannel::Luminance, .1f, .9f}})
          .layer(material::Color{.6f, .1f, .1f, 1},
                 {.mask = {depth, ColorChannel::Green, .2f, .8f}})
          .layer(material::Color{.5f, .5f, .5f, 1},
                 {.blend = material::BlendMode::Multiply, .opacity = .35f})
          .layer(material::Color{.1f, .7f, .2f, 1},
                 {.opacity = .8f,
                  .mask = {depth, ColorChannel::Red, 0, .5f, true}})
          .layer(material::Color{.3f, .3f, .9f, 1},
                 {.opacity = .6f, .mask = {depth, ColorChannel::Blue, 0, 1}});
  const Element tree =
      box().width(percent(100)).height(percent(100)).fill(stack);

  const auto centreOf = [](const sk_sp<SkImage>& image) {
    SkBitmap pixels;
    if (!image || !pixels.tryAllocPixels(SkImageInfo::MakeN32Premul(48, 20)) ||
        !image->readPixels(nullptr, pixels.pixmap(), 0, 0))
      return SkColor4f{-1, -1, -1, -1};
    return pixels.getColor4f(24, 10);
  };
  const std::shared_ptr<TextureScene> onTheCpu =
      TextureScene::make({48, 20}, fonts());
  ASSERT_TRUE(onTheCpu);
  onTheCpu->render(tree);
  const SkColor4f expected = centreOf(onTheCpu->image());
  ASSERT_GE(expected.fA, 0.f);

  const std::shared_ptr<TextureScene> onTheDevice =
      TextureScene::make({48, 20}, fonts());
  ASSERT_TRUE(onTheDevice);
  {
    const sigil::geometry::device::Device::QueueLock lock(*device);
    ASSERT_TRUE(onTheDevice->useDevice(*device->gpu(), *device->graphite()));
    onTheDevice->render(tree);
  }
  const SkColor4f actual = centreOf(onTheDevice->hostImage());
  EXPECT_NEAR(actual.fR, expected.fR, .02f);
  EXPECT_NEAR(actual.fG, expected.fG, .02f);
  EXPECT_NEAR(actual.fB, expected.fB, .02f);
  EXPECT_NEAR(actual.fA, expected.fA, .02f);
}

TEST(ComposeTexture, ASceneOnADeviceHandsAForeignRecorderAHostCopy) {
  namespace skia = sigil::skia;
  namespace core = sigil::core;
  namespace media = sigil::media;
  SIGIL_ON_DEVICE_OR_SKIP(device);
  if (!device->gpu() || !device->graphite())
    GTEST_SKIP() << "no adopted Graphite context on this Vulkan device";
  std::string error;
  auto made = core::hardware::GpuDevice::createOwned();
  if (!made) GTEST_SKIP() << made.error().message;
  const std::unique_ptr<core::hardware::GpuDevice> other = std::move(*made);
  const std::unique_ptr<skia::GraphiteContext> foreign =
      skia::GraphiteContext::create(*other);
  if (!foreign) GTEST_SKIP() << "no second Graphite context on this machine";

  const std::shared_ptr<TextureScene> scene =
      TextureScene::make({16, 16}, fonts());
  ASSERT_TRUE(scene);
  {
    const sigil::geometry::device::Device::QueueLock lock(*device);
    ASSERT_TRUE(scene->useDevice(*device->gpu(), *device->graphite()));
    scene->render(plate({1, 0, 0, 1}));
  }
  const sigil::material::Texture texture = scene->texture();
  ASSERT_EQ(scene->recorder(), device->graphite()->recorder());

  // The scene's own recorder draws the snapshot where it stands.
  const sk_sp<SkImage> own =
      media::toSk(texture.frameAt({}, device->graphite()->recorder()).image);
  ASSERT_TRUE(own);
  EXPECT_TRUE(own->isTextureBacked());

  // A recorder of another device, and none, are handed a host copy of
  // the same pixels, which any canvas draws.
  for (skgpu::graphite::Recorder* recorder :
       {foreign->recorder(),
        static_cast<skgpu::graphite::Recorder*>(nullptr)}) {
    const sk_sp<SkImage> copy =
        media::toSk(texture.frameAt({}, recorder).image);
    ASSERT_TRUE(copy) << (recorder ? "foreign" : "none");
    EXPECT_FALSE(copy->isTextureBacked());
    SkBitmap pixels;
    ASSERT_TRUE(pixels.tryAllocPixels(SkImageInfo::MakeN32Premul(16, 16)));
    ASSERT_TRUE(copy->readPixels(nullptr, pixels.pixmap(), 0, 0));
    const SkColor4f centre = pixels.getColor4f(8, 8);
    EXPECT_NEAR(centre.fR, 1.f, .01f);
    EXPECT_NEAR(centre.fG, 0.f, .01f);
    EXPECT_NEAR(centre.fA, 1.f, .01f);
  }
}

TEST(ComposeTexture, ADeviceTakesTheSceneAndSaysWhereItStands) {
  namespace skia = sigil::skia;
  namespace core = sigil::core;
  std::string error;
  auto made = core::hardware::GpuDevice::createOwned();
  if (!made) GTEST_SKIP() << made.error().message;
  const std::unique_ptr<core::hardware::GpuDevice> device = std::move(*made);
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
