// The lighting a lit surface is shaded under: stated once on a parent and
// inherited, it turns the relief of a normal-mapped fill toward the light;
// a surface with none in force is painted flat; a bound light moves the
// relief frame by frame while the colours beneath are read once; and a
// line of type and a stroke whose material states a surface are lit too.

#include <include/core/SkBitmap.h>
#include <include/core/SkImage.h>
#include <sigilcompose/brush/Relief.h>
#include <sigilcompose/brush/Ribbons.h>
#include <sigilmaterial/core/Lighting.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmedia/core/PixelSource.h>

#include <chrono>
#include <functional>
#include <memory>
#include <optional>

#include "support/CoreTestSupport.h"

namespace {

/** A normal map whose left half faces left and whose right half faces
 *  right, both tilted 45 degrees out of the page. */
sk_sp<SkImage> twoFacedNormals(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height, true);
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x)
      *bitmap.getAddr32(x, y) =
          SkPreMultiplyARGB(255, x < width / 2 ? 37 : 218, 128, 218);
  bitmap.setImmutable();
  return bitmap.asImage();
}

material::Material reliefSurface(
    material::Material colours,
    std::optional<material::Lighting> lighting = {}) {
  return material::from(std::move(colours))
      .surface({.roughness = 0.8f,
                .normal = material::image(twoFacedNormals(64, 32)),
                .lighting = std::move(lighting)});
}

float brightness(SkColor colour) {
  return (float)(SkColorGetR(colour) + SkColorGetG(colour) +
                 SkColorGetB(colour));
}

/** A picture that counts how often it is read: the colours beneath a lit
 *  surface, which a moving light must not read again. */
struct CountedPicture {
  std::shared_ptr<int> reads;
  sk_sp<SkImage> picture;
  sigil::media::Frame frameAt(std::chrono::duration<double>) const {
    ++*reads;
    sigil::media::Frame frame;
    frame.image = picture;
    return frame;
  }
  bool isRunning() const { return false; }
  SkISize size() const { return picture->dimensions(); }
  bool operator==(const CountedPicture& other) const {
    return reads == other.reads;
  }
};

sk_sp<SkImage> grey(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height, true);
  bitmap.eraseColor(SkColorSetRGB(150, 150, 150));
  bitmap.setImmutable();
  return bitmap.asImage();
}

enum class SurfaceMarkSlot { Overlay, SpanStroke, SpanBackground };

void expectEnvironmentRotationThroughSavedCaches(SurfaceMarkSlot slot) {
  SkBitmap panorama;
  panorama.allocN32Pixels(16, 8, true);
  for (int y = 0; y < 8; ++y)
    for (int x = 0; x < 16; ++x)
      *panorama.getAddr32(x, y) = x < 8 ? SK_ColorRED : SK_ColorBLUE;
  panorama.setImmutable();
  const material::Material image = material::image(panorama.asImage());
  const material::Material finish =
      material::from(material::Color{.8f, .8f, .8f, 1})
          .surface({.metallic = 1.0f, .roughness = .5f});
  const auto makeScene = [&](motion::Animatable<float> rotation, Cache cache) {
    Element receiver = box().absolute().rect(20, 20, 64, 64).cache(cache);
    if (slot == SurfaceMarkSlot::Overlay) {
      receiver.overlay(sigil::compose::relief(finish, {.shoulder = 0}));
    } else {
      receiver.shape(skiaShape([] {
        SkPathBuilder path;
        path.moveTo(0, 32).lineTo(64, 32);
        return path.detach();
      }));
      const auto mark = brush::ribbon(geometry::path::Profile(12), finish);
      if (slot == SurfaceMarkSlot::SpanStroke)
        receiver.stroke(spans::every(1), mark);
      else
        receiver.background(spans::every(1), mark);
    }
    return sigil::compose::scene()
        .width(128)
        .height(96)
        .cache(cache)
        .environment(material::environment(
            image, {.rotation = rotation, .size = {16, 8}}))
        .children({receiver});
  };
  for (const Cache cache : {Cache::Picture, Cache::Texture, Cache::Group}) {
    SCOPED_TRACE(static_cast<int>(cache));
    auto rotation = motion::animatable(45.0f);
    Host retained(128, 96), fresh(128, 96);
    retained.composer.render(makeScene(rotation, cache));
    for (int frame = 0; frame < 4; ++frame) retained.frame();
    const SkColor before = retained.pixel(48, 52);
    EXPECT_GT(SkColorGetR(before), SkColorGetB(before) + 60);
    rotation = 225.0f;
    EXPECT_TRUE(retained.composer.isRunning());
    retained.frame();
    fresh.composer.render(makeScene(225.0f, cache));
    for (int frame = 0; frame < 4; ++frame) fresh.frame();
    const SkColor after = retained.pixel(48, 52);
    EXPECT_GT(SkColorGetB(after), SkColorGetR(after) + 60);
    EXPECT_TRUE(identicalPixels(retained, fresh, 128, 96));
  }
}

}  // namespace

TEST(ComposeLighting,
     AStaticSurfaceOverlayFollowsLiveEnvironmentThroughSavedCaches) {
  expectEnvironmentRotationThroughSavedCaches(SurfaceMarkSlot::Overlay);
}

TEST(ComposeLighting,
     StaticSurfaceSpanPassesFollowLiveEnvironmentThroughSavedCaches) {
  for (const SurfaceMarkSlot slot :
       {SurfaceMarkSlot::SpanStroke, SurfaceMarkSlot::SpanBackground}) {
    SCOPED_TRACE(static_cast<int>(slot));
    expectEnvironmentRotationThroughSavedCaches(slot);
  }
}

TEST(ComposeLighting, AnInheritedLightTurnsTheReliefOfANormalMappedFill) {
  const material::Material surface =
      reliefSurface(material::Color{0.6f, 0.6f, 0.6f, 1});
  const auto scene = [&](float direction) {
    return stack()
        .width(200)
        .height(200)
        .lighting(
            material::studio({.direction = direction, .elevation = 30.0f}))
        .children({box().width(64).height(32).fill(surface)});
  };
  Host host;
  host.composer.render(scene(180.0f));
  host.frame();
  const float leftLitFromLeft = brightness(host.pixel(12, 16));
  const float rightLitFromLeft = brightness(host.pixel(52, 16));
  host.composer.render(scene(0.0f));
  host.frame();
  const float leftLitFromRight = brightness(host.pixel(12, 16));
  const float rightLitFromRight = brightness(host.pixel(52, 16));
  EXPECT_GT(leftLitFromLeft, rightLitFromLeft + 60)
      << "lit from the left, the half whose normal faces left is brighter";
  EXPECT_GT(rightLitFromRight, leftLitFromRight + 60)
      << "lit from the right, the half whose normal faces right is brighter";
}

TEST(ComposeLighting, ASurfaceWithNoLightingInForceIsPaintedFlat) {
  Host host;
  host.composer.render(
      stack().width(200).height(200).children({box().width(64).height(32).fill(
          reliefSurface(material::Color{0.6f, 0.6f, 0.6f, 1}))}));
  host.frame();
  EXPECT_EQ(host.pixel(12, 16), host.pixel(52, 16));
  // `initial` ends an inherited lighting below where it was stated.
  host.composer.render(stack()
                           .width(200)
                           .height(200)
                           .lighting(material::studio())
                           .children({box()
                                          .width(64)
                                          .height(32)
                                          .initial(Property::Lighting)
                                          .fill(reliefSurface(material::Color{
                                              0.6f, 0.6f, 0.6f, 1}))}));
  host.frame();
  EXPECT_EQ(host.pixel(12, 16), host.pixel(52, 16));
}

TEST(ComposeLighting, AFillUsesItsOwnLightingWithOrWithoutAnInheritedLight) {
  const material::Material surface = reliefSurface(
      material::Color{0.6f, 0.6f, 0.6f, 1},
      material::studio({.direction = 180.0f, .elevation = 30.0f}));
  Host host;
  const auto scene = [&] {
    return stack().width(200).height(200).children(
        {box().width(64).height(32).fill(surface)});
  };
  host.composer.render(scene());
  host.frame();
  const SkColor left = host.pixel(12, 16);
  const SkColor right = host.pixel(52, 16);
  EXPECT_GT(brightness(left), brightness(right) + 60);

  host.composer.render(scene().lighting(
      material::studio({.direction = 0.0f, .elevation = 30.0f})));
  host.frame();
  EXPECT_EQ(left, host.pixel(12, 16));
  EXPECT_EQ(right, host.pixel(52, 16));
}

TEST(ComposeLighting, AFillTracksItsOwnBoundLightWithoutRereadingItsColours) {
  auto reads = std::make_shared<int>(0);
  sigil::motion::Animatable<float> sun = sigil::motion::animatable(180.0f);
  const material::Material surface = reliefSurface(
      material::image(
          sigil::media::PixelSource(CountedPicture{reads, grey(64, 32)})),
      material::studio({.direction = sun, .elevation = 30.0f}));
  Host host;
  host.composer.render(stack().width(200).height(200).children(
      {box().width(64).height(32).fill(surface)}));
  host.frame();
  const int readsOnce = *reads;
  EXPECT_GT(readsOnce, 0);
  EXPECT_GT(brightness(host.pixel(12, 16)),
            brightness(host.pixel(52, 16)) + 60);

  sun = 0.0f;
  host.frame();
  EXPECT_GT(brightness(host.pixel(52, 16)),
            brightness(host.pixel(12, 16)) + 60);
  EXPECT_EQ(readsOnce, *reads);
}

TEST(ComposeLighting,
     PreparedFillInputsSurviveLightChangesAndReleaseWhenTheSurfaceIsRemoved) {
  for (const bool noFill : {false, true}) {
    SCOPED_TRACE(noFill ? "no fill" : "flat fill");
    std::weak_ptr<int> colourLifetime, normalLifetime;
    Element receiver = [&] {
      auto colourReads = std::make_shared<int>(0);
      auto normalReads = std::make_shared<int>(0);
      colourLifetime = colourReads;
      normalLifetime = normalReads;
      const material::Material finish =
          material::image(sigil::media::PixelSource(
                              CountedPicture{colourReads, grey(64, 32)}))
              .surface(
                  {.roughness = 0.8f,
                   .normal = material::image(sigil::media::PixelSource(
                       CountedPicture{normalReads, twoFacedNormals(64, 32)}))});
      return box().absolute().rect(20, 20, 64, 32).key("receiver").fill(finish);
    }();
    const auto makeScene = [&](material::Lighting lighting) {
      return box().lighting(std::move(lighting)).children({receiver});
    };
    Host host(128, 100);
    host.composer.render(
        makeScene(material::studio({.direction = 180.0f, .elevation = 30.0f})));
    host.frame();
    ASSERT_FALSE(colourLifetime.expired());
    ASSERT_FALSE(normalLifetime.expired());
    const int colours = *colourLifetime.lock(),
              normals = *normalLifetime.lock();
    ASSERT_GT(colours, 0);
    ASSERT_GT(normals, 0);
    EXPECT_GT(brightness(host.pixel(32, 36)),
              brightness(host.pixel(72, 36)) + 60);

    host.composer.render(makeScene(material::Lighting{}));
    host.frame();
    ASSERT_FALSE(colourLifetime.expired());
    ASSERT_FALSE(normalLifetime.expired());
    EXPECT_EQ(host.pixel(32, 36), host.pixel(72, 36));
    EXPECT_EQ(*colourLifetime.lock(), colours);
    EXPECT_EQ(*normalLifetime.lock(), normals);

    host.composer.render(
        makeScene(material::studio({.direction = 0.0f, .elevation = 30.0f})));
    host.frame();
    EXPECT_GT(brightness(host.pixel(72, 36)),
              brightness(host.pixel(32, 36)) + 60);
    EXPECT_EQ(*colourLifetime.lock(), colours);
    EXPECT_EQ(*normalLifetime.lock(), normals);

    receiver = box().absolute().rect(20, 20, 64, 32).key("receiver");
    if (!noFill) receiver.fill(material::Color{0, 1, 0, 1});
    host.composer.render(
        makeScene(material::studio({.direction = 0.0f, .elevation = 30.0f})));
    for (int frame = 0; frame < 16; ++frame) host.frame(1.0 / 60.0);
    EXPECT_TRUE(colourLifetime.expired());
    EXPECT_TRUE(normalLifetime.expired());
    EXPECT_EQ(host.pixel(32, 36), noFill ? SK_ColorBLACK : SK_ColorGREEN);
  }
}

TEST(ComposeLighting, ABoundLightMovesTheReliefAndReadsTheColoursOnce) {
  auto reads = std::make_shared<int>(0);
  const material::Material surface = reliefSurface(material::image(
      sigil::media::PixelSource(CountedPicture{reads, grey(64, 32)})));
  sigil::motion::Animatable<float> sun = sigil::motion::animatable(180.0f);
  Host host;
  host.composer.render(
      stack()
          .width(200)
          .height(200)
          .lighting(material::studio({.direction = sun, .elevation = 30.0f}))
          .children({box().width(64).height(32).fill(surface)}));
  host.frame();
  const int readsOnce = *reads;
  EXPECT_GT(readsOnce, 0);
  const float leftBefore = brightness(host.pixel(12, 16));
  const float rightBefore = brightness(host.pixel(52, 16));
  sun = 0.0f;  // no render: the bound light alone moves
  host.frame();
  const float leftAfter = brightness(host.pixel(12, 16));
  const float rightAfter = brightness(host.pixel(52, 16));
  EXPECT_GT(leftBefore, rightBefore + 60);
  EXPECT_GT(rightAfter, leftAfter + 60) << "the relief turned with the light";
  sun = 90.0f;
  host.frame();
  EXPECT_EQ(readsOnce, *reads)
      << "only the lighting pass re-ran; the colours beneath were not read "
         "again";
}

namespace {

/** How many sampled pixels differ between @p element lit from the left
 *  and lit from the right. */
int turnedBy(const std::function<Element()>& element) {
  Host host;
  const auto scene = [&](float direction) {
    return stack()
        .width(200)
        .height(200)
        .lighting(
            material::studio({.direction = direction, .elevation = 30.0f}))
        .children({element()});
  };
  host.composer.render(scene(180.0f));
  host.frame();
  SkBitmap fromLeft;
  fromLeft.allocN32Pixels(200, 200);
  host.surface->readPixels(fromLeft.pixmap(), 0, 0);
  host.composer.render(scene(0.0f));
  host.frame();
  SkBitmap fromRight;
  fromRight.allocN32Pixels(200, 200);
  host.surface->readPixels(fromRight.pixmap(), 0, 0);
  int moved = 0;
  for (int y = 0; y < 200; y += 2)
    for (int x = 0; x < 200; x += 2)
      moved += fromLeft.getColor(x, y) != fromRight.getColor(x, y);
  return moved;
}

}  // namespace

TEST(ComposeLighting, AStrokeAndALineOfTypeAreLitToo) {
  const material::Material surface =
      reliefSurface(material::Color{0.6f, 0.6f, 0.6f, 1});
  EXPECT_GT(turnedBy([&] {
              return box().width(64).height(32).stroke(surface, {.width = 8});
            }),
            20)
      << "the stroke turned with the light";
  EXPECT_GT(turnedBy([&] {
              return stack().children(
                  {text(u8"MMMM").font({.size = 48}).ink(surface)});
            }),
            20)
      << "the glyphs turned with the light";
}

TEST(ComposeLighting, AStyleRulePreservesAnInksOwnLighting) {
  const material::Material surface = reliefSurface(
      material::Color{0.6f, 0.6f, 0.6f, 1},
      material::studio({.direction = 180.0f, .elevation = 30.0f}));
  Host host;
  host.composer.render(stack().width(200).height(200).children(
      {text(u8"MMMM").font({.size = 48}).ink(surface)}));
  host.frame();
  SkBitmap direct;
  direct.allocN32Pixels(200, 200);
  host.surface->readPixels(direct.pixmap(), 0, 0);

  host.composer.render(
      stack()
          .width(200)
          .height(200)
          .applyStyleSheet(StyleSheet{rule(".metal").ink(surface)})
          .children({text(u8"MMMM").styleClass("metal").font({.size = 48})}));
  host.frame();
  int changed = 0;
  int visible = 0;
  for (int y = 0; y < 200; y += 2)
    for (int x = 0; x < 200; x += 2) {
      changed += host.pixel(x, y) != direct.getColor(x, y);
      visible += SkColorGetA(direct.getColor(x, y)) > 0;
    }
  EXPECT_GT(visible, 20);
  EXPECT_EQ(changed, 0);
}

namespace {

material::Light pageLight(material::LightKind kind, glm::vec3 position) {
  material::Light light;
  light.kind = kind;
  light.position = position;
  light.elevation = 90.0f;
  light.intensity = 1.0f;
  light.ambient = 0.0f;
  light.range = 240.0f;
  light.innerAngle = 10.0f;
  light.outerAngle = 25.0f;
  return light;
}

material::Material flatSurface(
    std::optional<material::Lighting> lighting = {}) {
  return material::from(material::Color{0.6f, 0.6f, 0.6f, 1})
      .surface({.roughness = 0.8f, .lighting = std::move(lighting)});
}

Element spatialSubject(const material::Material& surface, Cache cache,
                       bool ink) {
  Element subject =
      box().absolute().rect(20, 20, 100, 80).key("subject").cache(cache);
  if (ink)
    subject.ink(surface)
        .font({.face = sigil::test::instrument::sans(), .size = 36})
        .children({text(u8"MMMM").absolute().rect(0, 0, 100, 80)});
  else
    subject.fill(surface).children(
        {box().absolute().rect(96, 76, 4, 4).fill(green())});
  return subject;
}

int subjectBrightness(const Host& host, int offset) {
  int total = 0;
  for (int y = 20; y < 100; ++y)
    for (int x = 20 + offset; x < 120 + offset; ++x)
      total += static_cast<int>(brightness(host.pixel(x, y)));
  return total;
}

void expectSameSubject(const Host& actual, const Host& expected, int offset) {
  int differences = 0;
  for (int y = 20; y < 100; ++y)
    for (int x = 20 + offset; x < 120 + offset; ++x) {
      const SkColor a = actual.pixel(x, y), b = expected.pixel(x, y);
      differences += std::abs(static_cast<int>(SkColorGetR(a)) -
                              static_cast<int>(SkColorGetR(b))) > 1 ||
                     std::abs(static_cast<int>(SkColorGetG(a)) -
                              static_cast<int>(SkColorGetG(b))) > 1 ||
                     std::abs(static_cast<int>(SkColorGetB(a)) -
                              static_cast<int>(SkColorGetB(b))) > 1;
    }
  EXPECT_EQ(differences, 0);
}

}  // namespace

TEST(ComposeLighting, AnInheritedPointLightUsesOnePagePositionAcrossSiblings) {
  const auto surface = flatSurface();
  const auto scene = [&](float x) {
    return box()
        .lighting(pageLight(material::LightKind::Point, {x, 40, 35}))
        .children({box().absolute().rect(20, 20, 60, 40).fill(surface),
                   box().absolute().rect(120, 20, 60, 40).fill(surface)});
  };
  Host host;
  host.composer.render(scene(50));
  host.frame();
  EXPECT_GT(brightness(host.pixel(50, 40)),
            brightness(host.pixel(150, 40)) + 60);
  host.composer.render(scene(150));
  host.frame();
  EXPECT_GT(brightness(host.pixel(150, 40)),
            brightness(host.pixel(50, 40)) + 60);
}

TEST(ComposeLighting, AnInheritedSpotLightKeepsItsConeInPageCoordinates) {
  auto light = pageLight(material::LightKind::Spot, {50, 40, 60});
  const auto surface = flatSurface();
  const auto scene = [&] {
    return box().lighting(light).children(
        {box().absolute().rect(20, 20, 60, 40).fill(surface),
         box().absolute().rect(120, 20, 60, 40).fill(surface)});
  };
  Host host;
  host.composer.render(scene());
  host.frame();
  EXPECT_GT(brightness(host.pixel(50, 40)), 60);
  EXPECT_EQ(host.pixel(150, 40), SK_ColorBLACK);
  light.outerAngle = 85;
  host.composer.render(scene());
  host.frame();
  EXPECT_GT(brightness(host.pixel(150, 40)), 20);
}

TEST(ComposeLighting, CachedSpatialFillAndInkFollowOnlyAnAncestorTranslation) {
  const auto light = pageLight(material::LightKind::Point, {60, 50, 35});
  const auto surface = flatSurface();
  for (const Cache cache : {Cache::Picture, Cache::Texture}) {
    for (const bool ink : {false, true}) {
      SCOPED_TRACE(static_cast<int>(cache));
      SCOPED_TRACE(ink ? "ink" : "fill");
      auto offset = motion::animatable(0.0f);
      Host held(320, 160);
      held.composer.setAutoTexturePromotion(Composer::PromotionPolicy::Off);
      held.composer.render(box()
                               .cache(Cache::None)
                               .lighting(light)
                               .children({box()
                                              .absolute()
                                              .rect(0, 0, 320, 160)
                                              .translateX(offset)
                                              .cache(Cache::None)
                                              .children({spatialSubject(
                                                  surface, cache, ink)})}));
      for (int frame = 0; frame < 16; ++frame) held.frame(1.0 / 60.0);
      const int initial = subjectBrightness(held, 0);
      ASSERT_GT(initial, 1000);
      EXPECT_EQ(held.composer.stats().picturesRecorded, 0u);
      if (cache == Cache::Texture)
        ASSERT_GE(held.composer.stats().texturesLive, 1u);
      else
        ASSERT_GE(held.composer.stats().picturesLive, 1u);

      for (const int x : {100, 0}) {
        // The held tree is never described again. Only its ancestor moves.
        offset = static_cast<float>(x);
        held.frame(1.0 / 60.0);
        Host fresh(320, 160);
        fresh.composer.setAutoTexturePromotion(Composer::PromotionPolicy::Off);
        fresh.composer.render(
            box()
                .cache(Cache::None)
                .lighting(light)
                .children({box()
                               .absolute()
                               .rect(x, 0, 320, 160)
                               .cache(Cache::None)
                               .children({spatialSubject(surface, Cache::None,
                                                         ink)})}));
        fresh.frame();
        expectSameSubject(held, fresh, x);
        if (x == 100) {
          EXPECT_LT(subjectBrightness(held, x), initial - 300);
          EXPECT_EQ(held.pixel(40, 40), SK_ColorBLACK);
        }
        for (int frame = 0; frame < 16; ++frame) held.frame(1.0 / 60.0);
        expectSameSubject(held, fresh, x);
        EXPECT_EQ(held.composer.stats().picturesRecorded, 0u);
        EXPECT_EQ(held.composer.stats().texturesBaked, 0u);
      }
    }
  }
}

TEST(ComposeLighting, AFillAndInkKeepTheirOwnPointLightOverInheritedLighting) {
  const auto own = pageLight(material::LightKind::Point, {60, 50, 35});
  const auto surface = flatSurface(material::Lighting(own));
  for (const bool ink : {false, true}) {
    SCOPED_TRACE(ink ? "ink" : "fill");
    Host alone, inherited;
    alone.composer.render(
        box().children({spatialSubject(surface, Cache::None, ink)}));
    inherited.composer.render(
        box()
            .lighting(pageLight(material::LightKind::Spot, {260, 50, 35}))
            .children({spatialSubject(surface, Cache::None, ink)}));
    alone.frame();
    inherited.frame();
    ASSERT_GT(subjectBrightness(alone, 0), 1000);
    expectSameSubject(inherited, alone, 0);
  }
}
