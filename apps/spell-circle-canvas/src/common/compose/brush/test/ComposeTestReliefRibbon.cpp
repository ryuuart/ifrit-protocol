/** @file
 * Lit material strokes and ribbons: a stroke's lighting snapshot, live
 * surface maps, own environment and unlit colours stay live under the
 * composer's caching without reading their pictures again, and a held
 * stroke, relief or ribbon follows a point light when only an ancestor
 * moves.
 */

#include <sigilcompose/brush/Relief.h>
#include <sigilcompose/brush/Ribbons.h>
#include <sigilmaterial/core/Lighting.h>
#include <sigilmaterial/texture/Image.h>

#include <cmath>
#include <memory>
#include <utility>

#include "ReliefTestSupport.h"

TEST(ComposeRelief, LightingSnapshotsReuseMaterialStrokeInputs) {
  auto colourReads = std::make_shared<int>(0);
  auto normalReads = std::make_shared<int>(0);
  auto roughnessReads = std::make_shared<int>(0);
  const auto sampled = [](const std::shared_ptr<int>& reads, SkColor colour) {
    return material::image(
        sigil::media::PixelSource(CountedPicture{reads, greyPicture(colour)}));
  };
  const material::Material finish =
      sampled(colourReads, SkColorSetRGB(150, 150, 150))
          .surface(
              {.roughness =
                   sampled(roughnessReads, SkColorSetRGB(204, 204, 204)),
               .normal = sampled(normalReads, SkColorSetRGB(204, 128, 230))});
  const Element contour =
      box()
          .absolute()
          .rect(20, 20, 80, 60)
          .cache(Cache::None)
          .stroke(finish,
                  {.width = 8, .position = material::StrokePosition::Inside});
  Host host(128, 100);
  const auto paintAt = [&](float direction) {
    host.composer.render(
        box()
            .cache(Cache::None)
            .lighting(material::studio(
                {.direction = direction, .elevation = 25.0f, .ambient = 0.1f}))
            .children({contour}));
    host.frame();
    return brightness(host.pixel(60, 23));
  };
  const float right = paintAt(0.0f);
  const int colours = *colourReads, normals = *normalReads,
            roughness = *roughnessReads;
  ASSERT_GT(colours, 0);
  ASSERT_GT(normals, 0);
  ASSERT_GT(roughness, 0);
  const float left = paintAt(180.0f);
  EXPECT_GT(right, left + 30);
  EXPECT_FLOAT_EQ(paintAt(0.0f), right);
  EXPECT_EQ(*colourReads, colours);
  EXPECT_EQ(*normalReads, normals);
  EXPECT_EQ(*roughnessReads, roughness);
  EXPECT_EQ(host.pixel(60, 50), SK_ColorBLACK);
}

TEST(ComposeRelief, AStrokeKeepsLiveSurfaceMapsUnderAutomaticCaching) {
  ASSERT_TRUE(scalarMap());
  auto colourReads = std::make_shared<int>(0);
  auto normalReads = std::make_shared<int>(0);
  auto roughness = motion::animatable(0.05f);
  material::Paint roughMap = material::skia::sksl(scalarMap());
  roughMap.bind("value", roughness);
  const material::Material finish =
      material::image(
          sigil::media::PixelSource(CountedPicture{colourReads, greyPicture()}))
          .surface({.metallic = 1.0f,
                    .roughness = material::skia::base(roughMap),
                    .normal = material::image(sigil::media::PixelSource(
                        CountedPicture{normalReads, greyPicture(SkColorSetRGB(
                                                        128, 128, 255))})),
                    .lighting = material::studio({.elevation = 90.0f})});
  Host held(128, 100);
  held.composer.render(box().children(
      {box()
           .absolute()
           .rect(20, 20, 80, 60)
           .stroke(finish, {.width = 8,
                            .position = material::StrokePosition::Inside})}));
  for (int frame = 0; frame < 16; ++frame) held.frame(1.0 / 60.0);
  const SkColor polished = held.pixel(60, 23);
  const int colours = *colourReads, normals = *normalReads;
  ASSERT_GT(colours, 0);
  ASSERT_GT(normals, 0);
  roughness = 1.0f;
  EXPECT_TRUE(held.composer.isRunning());
  held.frame();
  const SkColor matte = held.pixel(60, 23);
  EXPECT_GT(brightness(polished), brightness(matte) + 30);
  EXPECT_EQ(*colourReads, colours);
  EXPECT_EQ(*normalReads, normals);
  EXPECT_EQ(held.pixel(60, 50), SK_ColorBLACK);
}

TEST(ComposeRelief, AStrokeKeepsItsOwnEnvironmentLiveAndRetainsThePicture) {
  SkBitmap panorama;
  panorama.allocN32Pixels(16, 8, true);
  for (int y = 0; y < 8; ++y)
    for (int x = 0; x < 16; ++x)
      *panorama.getAddr32(x, y) = x < 8 ? SK_ColorRED : SK_ColorBLUE;
  panorama.setImmutable();
  auto reads = std::make_shared<int>(0);
  auto rotation = motion::animatable(45.0f);
  const material::Material image = material::image(
      sigil::media::PixelSource(CountedPicture{reads, panorama.asImage()}));
  const material::Material finish =
      material::from(material::Color{0.8f, 0.8f, 0.8f, 1})
          .surface({.metallic = 1.0f,
                    .roughness = 0.5f,
                    .lighting = material::environment(
                        image, {.rotation = rotation, .size = {16, 8}})});
  Host held(128, 100);
  held.composer.render(box().children(
      {box()
           .absolute()
           .rect(20, 20, 80, 60)
           .stroke(finish, {.width = 8,
                            .position = material::StrokePosition::Inside})}));
  for (int frame = 0; frame < 16; ++frame) held.frame(1.0 / 60.0);
  const SkColor before = held.pixel(60, 23);
  EXPECT_GT(SkColorGetR(before), SkColorGetB(before) + 60);
  const int firstReads = *reads;
  ASSERT_GT(firstReads, 0);
  for (const float angle : {225.0f, 45.0f}) {
    rotation = angle;
    EXPECT_TRUE(held.composer.isRunning());
    held.frame();
    const SkColor after = held.pixel(60, 23);
    if (angle == 225.0f)
      EXPECT_GT(SkColorGetB(after), SkColorGetR(after) + 60);
    else
      EXPECT_EQ(after, before);
    EXPECT_EQ(*reads, firstReads);
    EXPECT_EQ(held.pixel(60, 50), SK_ColorBLACK);
  }
}

TEST(ComposeRelief, AnUnlitStrokeResolvesItsColoursAtTheCurrentFrame) {
  ASSERT_TRUE(timeColour());
  const material::Material colour =
      material::skia::base(material::skia::sksl(timeColour()))
          .surface({.unlit = true});
  Host held(128, 100);
  held.composer.render(box().children(
      {box()
           .absolute()
           .rect(20, 20, 80, 60)
           .stroke(colour, {.width = 8,
                            .position = material::StrokePosition::Inside})}));
  held.frame();
  EXPECT_EQ(held.pixel(60, 23), SK_ColorRED);
  EXPECT_TRUE(held.composer.isRunning());
  held.frame(1.0);
  EXPECT_EQ(held.pixel(60, 23), SK_ColorBLUE);
  held.frame();
  EXPECT_EQ(held.pixel(60, 23), SK_ColorBLUE);
  EXPECT_EQ(held.pixel(60, 50), SK_ColorBLACK);
}

namespace {

Element positionedLightingMark(int slot, Cache cache) {
  Element child = slot == 1 ? text(u8"HO", whiteStyle(64)) : box();
  child.absolute().rect(20, 20, 120, 100).key("panel").cache(cache);
  if (slot == 0)
    child.background(relief(greySurface(), {.shoulder = 4, .depth = 1}));
  else if (slot == 1)
    child.ink(material::Color{0, 0, 0, 0})
        .decorationOutline(Boundary::Glyphs)
        .foreground(relief(greySurface(), {.shoulder = 2, .depth = 1}));
  else
    child.stroke(brush::ribbon(14, greySurface()));
  child.children(
      {box().absolute().rect(0, 0, 4, 4).fill(material::Color{0, 1, 0, 1})});
  return child;
}

int lightingMarkBrightness(const Host& host, int shift) {
  int sum = 0;
  for (int y = 12; y < 128; ++y)
    for (int x = 12 + shift; x < 148 + shift; ++x)
      sum += static_cast<int>(brightness(host.pixel(x, y)));
  return sum;
}

void expectLightingMarkMatches(const Host& actual, const Host& expected,
                               int shift) {
  int changed = 0, painted = 0;
  for (int y = 12; y < 128; ++y)
    for (int x = 12 + shift; x < 148 + shift; ++x) {
      const SkColor a = actual.pixel(x, y), b = expected.pixel(x, y);
      changed += std::abs(static_cast<int>(SkColorGetR(a)) -
                          static_cast<int>(SkColorGetR(b))) > 1 ||
                 std::abs(static_cast<int>(SkColorGetG(a)) -
                          static_cast<int>(SkColorGetG(b))) > 1 ||
                 std::abs(static_cast<int>(SkColorGetB(a)) -
                          static_cast<int>(SkColorGetB(b))) > 1;
      painted += brightness(b) > 0;
    }
  EXPECT_GT(painted, 80);
  EXPECT_EQ(changed, 0);
}

}  // namespace

TEST(ComposeRelief,
     CachedMaterialStrokesFollowOwnPointLightsAndRootSourcesWhenAncestorsMove) {
  material::Light point;
  point.kind = material::LightKind::Point;
  point.position = {60, 50, 35};
  point.range = 240;
  point.ambient = 0;
  const material::Material pointFinish =
      material::from(material::Color{0.6f, 0.6f, 0.6f, 1})
          .surface({.roughness = 0.8f, .lighting = point});
  for (const bool rootSource : {false, true}) {
    SCOPED_TRACE(rootSource ? "root-anchored colours" : "own point light");
    const material::Material finish = rootSource ? rootGradient() : pointFinish;
    for (const Cache cache : {Cache::Picture, Cache::Texture, Cache::Auto}) {
      SCOPED_TRACE(static_cast<int>(cache));
      auto shift = motion::animatable(0.0f);
      const auto makeScene = [&](motion::Animatable<float> offset, Cache tier) {
        Element receiver =
            box().absolute().rect(20, 20, 120, 100).key("panel").cache(tier);
        receiver.stroke(
            finish, {.width = 8, .position = material::StrokePosition::Inside});
        receiver.children({box()
                               .absolute()
                               .rect(0, 0, 4, 4)
                               .fill(material::Color{0, 1, 0, 1})});
        return box()
            .cache(Cache::None)
            .children({box()
                           .absolute()
                           .rect(0, 0, 320, 160)
                           .translateX(offset)
                           .cache(Cache::None)
                           .children({receiver})});
      };
      Host held(320, 160);
      held.composer.setProfiling(true);
      held.composer.setAutoTexturePromotion(
          cache == Cache::Auto ? Composer::PromotionPolicy::Eager
                               : Composer::PromotionPolicy::Off);
      held.composer.render(makeScene(shift, cache));
      for (int frame = 0; frame < 16; ++frame) held.frame(1.0 / 60.0);
      const SkColor before = held.pixel(60, 23);
      ASSERT_GT(brightness(before), 0);
      ASSERT_EQ(panelCacheState(held),
                cache == Cache::Picture   ? Composer::CacheState::Picture
                : cache == Cache::Texture ? Composer::CacheState::Texture
                                          : Composer::CacheState::Promoted);
      for (const int offset : {80, 0}) {
        shift = static_cast<float>(offset);
        held.frame(1.0 / 60.0);
        Host fresh(320, 160);
        fresh.composer.setAutoTexturePromotion(Composer::PromotionPolicy::Off);
        fresh.composer.render(
            makeScene(static_cast<float>(offset), Cache::None));
        fresh.frame();
        expectLightingMarkMatches(held, fresh, offset);
        if (offset == 80) {
          EXPECT_NE(held.pixel(offset + 60, 23), before);
          EXPECT_EQ(held.pixel(30, 23), SK_ColorBLACK);
        } else {
          EXPECT_EQ(held.pixel(60, 23), before);
        }
        for (int frame = 0; frame < 16; ++frame) held.frame(1.0 / 60.0);
        expectLightingMarkMatches(held, fresh, offset);
        EXPECT_EQ(held.composer.stats().picturesRecorded, 0u);
        EXPECT_EQ(held.composer.stats().texturesBaked, 0u);
      }
    }
  }
}

TEST(ComposeRelief,
     CachedReliefAndRibbonFollowAnInheritedPointLightWhenAnAncestorMoves) {
  material::Light light;
  light.kind = material::LightKind::Point;
  light.position = {60, 50, 35};
  light.range = 240;
  light.ambient = 0;
  for (const Cache cache : {Cache::Picture, Cache::Texture}) {
    for (const int slot : {0, 1, 2}) {
      SCOPED_TRACE(cache == Cache::Picture ? "Picture" : "Texture");
      SCOPED_TRACE(slot == 0   ? "background relief"
                   : slot == 1 ? "glyph relief"
                               : "ribbon stroke");
      auto shift = motion::animatable(0.0f);
      Host held(320, 160);
      held.composer.setProfiling(true);
      held.composer.setAutoTexturePromotion(Composer::PromotionPolicy::Off);
      held.composer.render(box()
                               .cache(Cache::None)
                               .lighting(light)
                               .children({box()
                                              .absolute()
                                              .rect(0, 0, 320, 160)
                                              .translateX(shift)
                                              .cache(Cache::None)
                                              .children({positionedLightingMark(
                                                  slot, cache)})}));
      for (int frame = 0; frame < 16; ++frame) held.frame(1.0 / 60.0);
      const int initial = lightingMarkBrightness(held, 0);
      ASSERT_GT(initial, 10000);
      ASSERT_EQ(panelCacheState(held), cache == Cache::Picture
                                           ? Composer::CacheState::Picture
                                           : Composer::CacheState::Texture);
      EXPECT_EQ(held.composer.stats().picturesRecorded, 0u);
      for (const int offset : {80, 0}) {
        // The light and decorated child stay held. Only the ancestor moves.
        shift = static_cast<float>(offset);
        held.frame(1.0 / 60.0);
        Host fresh(320, 160);
        fresh.composer.setAutoTexturePromotion(Composer::PromotionPolicy::Off);
        fresh.composer.render(
            box()
                .cache(Cache::None)
                .lighting(light)
                .children({box()
                               .absolute()
                               .rect(offset, 0, 320, 160)
                               .cache(Cache::None)
                               .children({positionedLightingMark(
                                   slot, Cache::None)})}));
        fresh.frame();
        expectLightingMarkMatches(held, fresh, offset);
        if (offset == 80) {
          EXPECT_LT(lightingMarkBrightness(held, offset), initial - 300);
          EXPECT_EQ(held.pixel(30, 50), SK_ColorBLACK);
        }
        for (int frame = 0; frame < 16; ++frame) held.frame(1.0 / 60.0);
        expectLightingMarkMatches(held, fresh, offset);
        EXPECT_EQ(held.composer.stats().picturesRecorded, 0u);
        EXPECT_EQ(held.composer.stats().texturesBaked, 0u);
      }
    }
  }
}
