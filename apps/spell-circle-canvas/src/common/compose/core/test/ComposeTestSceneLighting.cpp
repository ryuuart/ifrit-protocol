// Sources belong to the nearest scene, independently of declaration order,
// layout participation and cache replay. Receivers retain their own overrides.

#include <include/core/SkBitmap.h>
#include <include/core/SkImage.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/Measure.h>
#include <sigilcompose/core/Operator.h>
#include <sigilmaterial/core/Lighting.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmedia/core/PixelSource.h>
#include <sigilmotion/values/Animatable.h>

#include <chrono>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "support/CoreTestSupport.h"

namespace compose = sigil::compose;

namespace {

class SceneLightColor : public ::testing::TestWithParam<material::LightKind> {};

float brightness(SkColor color) {
  return float(SkColorGetR(color) + SkColorGetG(color) + SkColorGetB(color));
}

sk_sp<SkImage> grey() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(64, 32, true);
  bitmap.eraseColor(SkColorSetRGB(150, 150, 150));
  bitmap.setImmutable();
  return bitmap.asImage();
}

sk_sp<SkImage> twoFacedNormals() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(64, 32, true);
  for (int y = 0; y < 32; ++y)
    for (int x = 0; x < 64; ++x)
      *bitmap.getAddr32(x, y) =
          SkPreMultiplyARGB(255, x < 32 ? 37 : 218, 128, 218);
  bitmap.setImmutable();
  return bitmap.asImage();
}

material::Material relief(std::optional<material::Lighting> ownLighting = {}) {
  return material::from(material::Color{.6f, .6f, .6f, 1})
      .surface({.roughness = .8f,
                .normal = material::image(twoFacedNormals()),
                .lighting = std::move(ownLighting)});
}

material::Material flatSurface() {
  return material::from(material::Color{.6f, .6f, .6f, 1})
      .surface({.roughness = .8f});
}

material::Material ambientAndEmission() {
  return material::from(material::Color{.3f, .3f, .3f, 1})
      .surface({.roughness = .8f,
                .emission = {.2f, .05f, .1f, 1},
                .emissionStrength = .5f});
}

material::Light sun(float bearing, material::Color color = {1, 1, 1, 1}) {
  return material::studio({.direction = bearing,
                           .elevation = 30.f,
                           .color = color,
                           .intensity = 1.f,
                           .ambient = 0.f});
}

material::Light point(glm::vec3 position,
                      material::Color color = {1, 1, 1, 1}) {
  return material::studio({.elevation = 90.f,
                           .color = color,
                           .intensity = .7f,
                           .ambient = 0.f,
                           .kind = material::LightKind::Point,
                           .position = position,
                           .range = 240.f});
}

Element receiver(const material::Material& surface, std::string key = "panel",
                 float x = 0, float y = 0) {
  return box().absolute().rect(x, y, 64, 32).key(std::move(key)).fill(surface);
}

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

struct ObservedArrangement {
  std::shared_ptr<std::vector<int>> arranged;
  bool operator==(const ObservedArrangement&) const = default;
  void arrange(Arrangement& arrangement) const {
    arranged->clear();
    for (const auto& child : arrangement.children)
      arranged->push_back(child.attribute<int>("role").value_or(-1));
  }
};

struct ObservedScope {
  std::shared_ptr<std::vector<int>> scoped;
  bool operator==(const ObservedScope&) const = default;
  void add(Scope& scope) const {
    scoped->clear();
    for (const auto& node : scope.nodes())
      scoped->push_back(node.attribute<int>("role").value_or(-1));
  }
};

void expectSamePanel(const Host& actual, const Host& expected, int x = 0,
                     int y = 0, int width = 64, int height = 32) {
  int different = 0;
  for (int py = y; py < y + height; ++py)
    for (int px = x; px < x + width; ++px) {
      const SkColor a = actual.pixel(px, py), b = expected.pixel(px, py);
      different += std::abs(int(SkColorGetR(a)) - int(SkColorGetR(b))) > 1 ||
                   std::abs(int(SkColorGetG(a)) - int(SkColorGetG(b))) > 1 ||
                   std::abs(int(SkColorGetB(a)) - int(SkColorGetB(b))) > 1;
    }
  EXPECT_EQ(different, 0);
}

}  // namespace

TEST(ComposeSceneLighting, SourcesReachReceiversAcrossReorderAndRemoval) {
  const auto surface = relief();
  const auto panel = receiver(surface);
  const auto source = compose::light(sun(180)).key("key");
  Host host;
  host.composer.render(compose::scene().children({panel, source}));
  host.frame();
  const auto before = grab(host);
  EXPECT_GT(brightness(host.pixel(12, 16)),
            brightness(host.pixel(52, 16)) + 60);

  host.composer.render(compose::scene().children({source, panel}));
  host.frame();
  EXPECT_EQ(grab(host), before);

  host.composer.render(compose::scene().children({panel}));
  host.frame();
  Host withoutSource;
  withoutSource.composer.render(compose::scene().children({panel}));
  withoutSource.frame();
  EXPECT_EQ(grab(host), grab(withoutSource));
  EXPECT_EQ(host.pixel(12, 16), host.pixel(52, 16));

  host.composer.render(
      compose::scene().children({panel, box().children({source})}));
  host.frame();
  EXPECT_EQ(grab(host), before);
}

TEST(ComposeSceneLighting, SourcesLightTheScenesOwnSurface) {
  Host host;
  host.composer.render(
      compose::scene()
          .fill(flatSurface())
          .children({compose::light(point({50, 40, 35})).key("key")}));
  host.frame();
  EXPECT_GT(brightness(host.pixel(50, 40)),
            brightness(host.pixel(150, 40)) + 60);
}

TEST(ComposeSceneLighting, NestedScenesOwnSourcesAndDropInheritedLighting) {
  const auto surface = relief();
  Host host;
  host.composer.render(compose::scene().children(
      {compose::light(sun(180, {1, 0, 0, 1})), receiver(surface, "outer"),
       compose::scene()
           .absolute()
           .rect(0, 50, 64, 32)
           .children({receiver(surface, "inner"),
                      compose::light(sun(0, {0, 0, 1, 1}))}),
       compose::scene()
           .absolute()
           .rect(0, 100, 64, 32)
           .children({receiver(surface, "empty")})}));
  host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(12, 16)),
            SkColorGetR(host.pixel(52, 16)) + 20);
  EXPECT_EQ(SkColorGetB(host.pixel(12, 16)), 0);
  EXPECT_GT(SkColorGetB(host.pixel(52, 66)),
            SkColorGetB(host.pixel(12, 66)) + 20);
  EXPECT_EQ(SkColorGetR(host.pixel(52, 66)), 0);
  EXPECT_EQ(host.pixel(12, 116), host.pixel(52, 116));
  EXPECT_EQ(SkColorGetR(host.pixel(12, 116)), SkColorGetB(host.pixel(12, 116)));
}

TEST(ComposeSceneLighting, NestedScenesDropTheOuterEnvironment) {
  const auto around =
      material::environment(material::from(material::Color{1, 0, 0, 1}),
                            {.intensity = .4f, .size = {1, 1}});
  Host host;
  host.composer.render(compose::scene().environment(around).children(
      {receiver(flatSurface(), "outer"),
       compose::scene()
           .absolute()
           .rect(0, 50, 64, 32)
           .children({receiver(flatSurface(), "inner")})}));
  host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(32, 16)),
            SkColorGetG(host.pixel(32, 16)) + 20);
  EXPECT_EQ(SkColorGetR(host.pixel(32, 66)), SkColorGetG(host.pixel(32, 66)));
  EXPECT_EQ(SkColorGetG(host.pixel(32, 66)), SkColorGetB(host.pixel(32, 66)));
}

TEST(ComposeSceneLighting, SourcesAndEnvironmentsOutsideScenesHaveNoEffect) {
  const auto panel = receiver(relief());
  Host actual, expected;
  actual.composer.render(
      box()
          .environment(material::environment(
              material::from(material::Color{1, 0, 0, 1}), {.size = {1, 1}}))
          .children({compose::light(sun(180)), panel}));
  expected.composer.render(box().children({panel}));
  actual.frame();
  expected.frame();
  EXPECT_EQ(grab(actual), grab(expected));
}

TEST(ComposeSceneLighting, TwoColoredSourcesAccumulateInEitherOrder) {
  const auto redSource =
      compose::light(point({32, 16, 35}, {1, 0, 0, 1})).key("red");
  const auto blueSource =
      compose::light(point({132, 16, 35}, {0, 0, 1, 1})).key("blue");
  const auto first = receiver(flatSurface(), "first");
  const auto second = receiver(flatSurface(), "second", 100);
  Host host;
  host.composer.render(
      compose::scene().children({redSource, first, second, blueSource}));
  host.frame();
  const auto together = grab(host);
  const SkColor left = host.pixel(32, 16), right = host.pixel(132, 16);
  EXPECT_GT(SkColorGetR(left), SkColorGetB(left) + 20);
  EXPECT_GT(SkColorGetB(right), SkColorGetR(right) + 20);
  host.composer.render(
      compose::scene().children({blueSource, first, second, redSource}));
  host.frame();
  EXPECT_EQ(grab(host), together);

  host.composer.render(compose::scene().children({redSource, first, second}));
  host.frame();
  EXPECT_NEAR(SkColorGetR(host.pixel(32, 16)), SkColorGetR(left), 1);
  EXPECT_LT(SkColorGetB(host.pixel(132, 16)), SkColorGetB(right) - 20);
}

TEST(ComposeSceneLighting, ReorderingMemoKeyedSourcesPreservesSettledCaches) {
  const auto source = [](const material::Light& light) {
    return compose::light(light);
  };
  const auto redSource =
      compose::memo(point({32, 16, 35}, {1, 0, 0, 1}), source).key("red");
  const auto blueSource =
      compose::memo(point({132, 16, 35}, {0, 0, 1, 1}), source).key("blue");
  const auto panel = receiver(flatSurface()).cache(Cache::Texture);
  Host host;
  host.composer.setAutoTexturePromotion(Composer::PromotionPolicy::Off);
  // Reordering invalidates a parent's picture. The uncached scene isolates
  // whether an unchanged lighting context preserves its receiver's texture.
  host.composer.render(compose::scene()
                           .cache(Cache::None)
                           .children({redSource, panel, blueSource}));
  for (int i = 0; i < 16; ++i) host.frame(1. / 60.);
  ASSERT_GE(host.composer.stats().texturesLive, 1u);
  const auto before = grab(host);
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  EXPECT_EQ(host.composer.stats().texturesBaked, 0u);

  host.composer.render(compose::scene()
                           .cache(Cache::None)
                           .children({blueSource, panel, redSource}));
  host.frame();
  EXPECT_EQ(grab(host), before);
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  EXPECT_EQ(host.composer.stats().texturesBaked, 0u);
}

TEST(ComposeSceneLighting, ReceiverAndMaterialLightingReplaceSceneSources) {
  const auto replacement = sun(0, {0, 0, 1, 1});
  const auto own = sun(180, {0, 1, 0, 1});
  const auto receivers = [&] {
    return box().children(
        {receiver(relief(), "declared").lighting(replacement),
         receiver(relief(material::Lighting(own)), "material", 0, 50)
             .lighting(replacement)});
  };
  Host actual, expected;
  actual.composer.render(compose::scene().children(
      {compose::light(sun(180, {1, 0, 0, 1})), receivers()}));
  expected.composer.render(receivers());
  actual.frame();
  expected.frame();
  EXPECT_EQ(grab(actual), grab(expected));
  EXPECT_GT(SkColorGetB(actual.pixel(52, 16)), 20);
  EXPECT_EQ(SkColorGetR(actual.pixel(52, 16)), 0);
  EXPECT_GT(SkColorGetG(actual.pixel(12, 66)), 20);
  EXPECT_EQ(SkColorGetB(actual.pixel(12, 66)), 0);
}

TEST(ComposeSceneLighting, UnlitSurfacesIgnoreSourcesAndEnvironment) {
  const auto surface =
      material::from(material::Color{.6f, .6f, .6f, 1})
          .surface(
              {.normal = material::image(twoFacedNormals()), .unlit = true});
  const auto panel = receiver(surface);
  Host actual, expected;
  actual.composer.render(
      compose::scene()
          .environment(material::environment(
              material::from(material::Color{0, 1, 0, 1}), {.size = {1, 1}}))
          .children({compose::light(sun(180, {1, 0, 0, 1})),
                     compose::light(sun(0, {0, 0, 1, 1})), panel}));
  expected.composer.render(box().children({panel}));
  actual.frame();
  expected.frame();
  EXPECT_EQ(grab(actual), grab(expected));
}

TEST(ComposeSceneLighting, SourcesTakeNoFlexGapPaintHitOrStructuralSlot) {
  auto arranged = std::make_shared<std::vector<int>>();
  auto scoped = std::make_shared<std::vector<int>>();
  Host host;
  host.composer.render(
      compose::scene()
          .row()
          .gap(10)
          .height(40)
          .hitTestable(false)
          .operators({ObservedArrangement{arranged}, ObservedScope{scoped}})
          .children(
              {box().key("a").attribute("role", 1).width(40).height(40).fill(
                   green()),
               compose::light(point({0, 0, 35}))
                   .key("source")
                   .attribute("role", 99)
                   .width(90)
                   .height(90)
                   .fill(red()),
               box().key("b").attribute("role", 2).width(40).height(40).fill(
                   green())}));
  host.frame();
  ASSERT_TRUE(host.composer.bounds("a"));
  ASSERT_TRUE(host.composer.bounds("b"));
  EXPECT_NEAR(require(host.composer.bounds("a")).left(), 0, .01f);
  EXPECT_NEAR(require(host.composer.bounds("b")).left(), 50, .01f);
  EXPECT_EQ(*arranged, (std::vector<int>{1, 2}));
  EXPECT_EQ(*scoped, (std::vector<int>{1, 2}));
  EXPECT_EQ(host.pixel(20, 20), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(70, 20), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(45, 20), SK_ColorBLACK);
  EXPECT_EQ(host.composer.hitTest({20, 20}).value_or(""), "a");
  EXPECT_EQ(host.composer.hitTest({70, 20}).value_or(""), "b");
  EXPECT_FALSE(host.composer.hitTest({45, 20}));
  EXPECT_FALSE(host.composer.hitTest({120, 20}));
}

TEST(ComposeSceneLighting, ASourceCannotAcquireLayoutChildren) {
  auto source = compose::light(sun(180));
  EXPECT_THROW(source.children({box().width(100).height(100)}),
               std::invalid_argument);
}

TEST(ComposeSceneLighting, OnlyDisplayNoneDisablesAHiddenSourceSubtree) {
  const auto panel = receiver(relief());
  const auto tree = [&](bool present) {
    return compose::scene().children(
        {panel, box()
                    .absolute()
                    .rect(0, 0, 1, 1)
                    .opacity(0.f)
                    .overflow(Overflow::Clip)
                    .display(present ? Display::Flex : Display::None)
                    .children({compose::light(sun(180))})});
  };
  Host host;
  host.composer.render(tree(true));
  host.frame();
  const auto lit = grab(host);
  EXPECT_GT(brightness(host.pixel(12, 16)),
            brightness(host.pixel(52, 16)) + 60);
  host.composer.render(tree(false));
  host.frame();
  EXPECT_EQ(host.pixel(12, 16), host.pixel(52, 16));
  host.composer.render(tree(true));
  host.frame();
  EXPECT_EQ(grab(host), lit);
}

TEST(ComposeSceneLighting, CachedReceiversFollowASourceInsideACachedWrapper) {
  for (const Cache cache : {Cache::Picture, Cache::Texture}) {
    SCOPED_TRACE(static_cast<int>(cache));
    auto offset = motion::animatable(0.f);
    auto reads = std::make_shared<int>(0);
    const auto surface =
        material::from(material::image(sigil::media::PixelSource(
                           CountedPicture{reads, grey()})))
            .surface({.roughness = .8f});
    Host held;
    held.composer.setAutoTexturePromotion(Composer::PromotionPolicy::Off);
    held.composer.render(compose::scene().children(
        {receiver(surface).cache(cache),
         box()
             .absolute()
             .rect(0, 0, 200, 200)
             .cache(cache)
             .translateX(offset)
             .children(
                 {compose::light(point({32, 16, 35})),
                  box().absolute().rect(180, 180, 2, 2).fill(green())})}));
    for (int i = 0; i < 16; ++i) held.frame(1. / 60.);
    const int readsOnce = *reads;
    int readsAfterReference = readsOnce;
    const float near = brightness(held.pixel(32, 16));
    ASSERT_GT(readsOnce, 0);
    ASSERT_GT(near, 60);
    // The visible wrapper has a live transform and may keep the host drawing.
    // Its source must still update the receiver without rereading its colors.
    if (cache == Cache::Texture)
      ASSERT_GE(held.composer.stats().texturesLive, 1u);
    else
      ASSERT_GE(held.composer.stats().picturesLive, 1u);

    for (const int x : {100, 0}) {
      offset = float(x);
      EXPECT_TRUE(held.composer.isRunning());
      held.frame(1. / 60.);
      EXPECT_EQ(*reads, readsAfterReference);
      Host fresh;
      fresh.composer.render(compose::scene().children(
          {receiver(surface), compose::light(point({32.f + x, 16, 35}))}));
      fresh.frame();
      readsAfterReference = *reads;
      expectSamePanel(held, fresh);
      if (x == 100) EXPECT_LT(brightness(held.pixel(32, 16)), near - 60);
      for (int i = 0; i < 16; ++i) held.frame(1. / 60.);
      expectSamePanel(held, fresh);
      EXPECT_EQ(held.composer.stats().picturesRecorded, 0u);
      EXPECT_EQ(held.composer.stats().texturesBaked, 0u);
      EXPECT_EQ(*reads, readsAfterReference);
    }
  }
}

TEST(ComposeSceneLighting, ABoundLightLeafParksAndResumesWithoutPainting) {
  auto offset = motion::animatable(0.f);
  auto ignoredInk = motion::animatable(.2f);
  struct Ink {
    float value;
  };
  const auto ignoredPaint =
      material::shader("half4 main(float2 p) { return half4(value, 0, 0, 1); }",
                       Ink{.2f})
          .bind("value", ignoredInk);
  auto reads = std::make_shared<int>(0);
  const auto surface = material::from(material::image(sigil::media::PixelSource(
                                          CountedPicture{reads, grey()})))
                           .surface({.roughness = .8f});
  Host host;
  host.composer.setAutoTexturePromotion(Composer::PromotionPolicy::Off);
  host.composer.render(
      compose::scene().children({receiver(surface).cache(Cache::Texture),
                                 compose::light(point({32, 16, 35}))
                                     .key("source")
                                     .translateX(offset)
                                     .cache(Cache::None)
                                     .fill(ignoredPaint)}));
  for (int i = 0; i < 16; ++i) host.frame(1. / 60.);
  const int readsOnce = *reads;
  ASSERT_GT(readsOnce, 0);
  ASSERT_GE(host.composer.stats().texturesLive, 1u);
  EXPECT_FALSE(host.composer.isRunning());
  ignoredInk = .8f;
  EXPECT_FALSE(host.composer.isRunning());

  for (const float x : {100.f, 0.f, 100.f}) {
    offset = x;
    EXPECT_FALSE(host.composer.dirty());
    EXPECT_TRUE(host.composer.isRunning());
    host.frame();
    Host fresh;
    const auto freshSurface =
        material::from(material::image(grey())).surface({.roughness = .8f});
    fresh.composer.render(compose::scene().children(
        {receiver(freshSurface), compose::light(point({32 + x, 16, 35}))}));
    fresh.frame();
    expectSamePanel(host, fresh);
    for (int i = 0; i < 16; ++i) host.frame(1. / 60.);
    expectSamePanel(host, fresh);
    EXPECT_EQ(*reads, readsOnce);
    EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
    EXPECT_EQ(host.composer.stats().texturesBaked, 0u);
    EXPECT_FALSE(host.composer.isRunning());
  }
}

TEST(ComposeSceneLighting, DirectionalSourcesUseARotatedReceiversPageNormal) {
  const auto panel = receiver(relief(), "panel", 40, 40).rotate(90.f);
  const auto source = sun(90);
  material::Lighting pageLighting(source);
  pageLighting.frame = material::LightingFrame::Scene;
  Host scoped, explicitPage, surfaceFrame;
  scoped.composer.render(
      compose::scene().children({panel, compose::light(source)}));
  explicitPage.composer.render(box().lighting(pageLighting).children({panel}));
  surfaceFrame.composer.render(box().lighting(source).children({panel}));
  scoped.frame();
  explicitPage.frame();
  surfaceFrame.frame();
  EXPECT_GT(brightness(scoped.pixel(72, 36)),
            brightness(scoped.pixel(72, 76)) + 60);
  EXPECT_EQ(surfaceFrame.pixel(72, 36), surfaceFrame.pixel(72, 76));
  expectSamePanel(scoped, explicitPage, 0, 0, 200, 200);
}

TEST(ComposeSceneLighting, UnsupportedSourcePlanesKeepAmbientAndEmission) {
  auto source = point({32, 16, 35});
  source.ambient = .2f;
  auto noDirect = source;
  noDirect.intensity = 0.f;
  const auto surface = ambientAndEmission();
  Host reference;
  reference.composer.render(
      box().lighting(noDirect).children({receiver(surface)}));
  reference.frame();
  for (const auto& placement :
       {box().rotateX(30.f), box().rotateY(30.f), box().perspective(600.f),
        box().scaleX(0.f), box().scaleY(0.f)}) {
    Host actual;
    actual.composer.render(compose::scene().children(
        {receiver(surface),
         Element(placement).children({compose::light(source)})}));
    actual.frame();
    expectSamePanel(actual, reference);
    EXPECT_EQ(SkColorGetA(actual.pixel(32, 16)), 255);
    EXPECT_GT(SkColorGetR(actual.pixel(32, 16)),
              SkColorGetG(actual.pixel(32, 16)));
  }
}

TEST(ComposeSceneLighting,
     InvalidSourcePositionOrRangeKeepsAmbientAndEmission) {
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float infinity = std::numeric_limits<float>::infinity();
  auto source = point({32, 16, 35});
  source.ambient = .2f;
  auto noDirect = source;
  noDirect.intensity = 0.f;
  const auto surface = ambientAndEmission();
  Host reference;
  reference.composer.render(
      box().lighting(noDirect).children({receiver(surface)}));
  reference.frame();
  std::vector<material::Light> invalid;
  for (const auto position :
       {glm::vec3{nan, 16, 35}, glm::vec3{32, infinity, 35},
        glm::vec3{32, 16, -infinity}}) {
    auto light = source;
    light.position = position;
    invalid.push_back(light);
  }
  for (const float range : {0.f, -1.f, nan, infinity}) {
    auto light = source;
    light.range = range;
    invalid.push_back(light);
  }
  for (size_t i = 0; i < invalid.size(); ++i) {
    SCOPED_TRACE(i);
    Host actual;
    actual.composer.render(compose::scene().children(
        {receiver(surface), compose::light(invalid[i])}));
    actual.frame();
    expectSamePanel(actual, reference);
    EXPECT_EQ(SkColorGetA(actual.pixel(32, 16)), 255);
    EXPECT_GT(brightness(actual.pixel(32, 16)), 30);
  }
}

TEST(ComposeSceneLighting, AReflectedSpotAxisAimsToTheOppositeSide) {
  auto source = point({0, 0, 60});
  source.kind = material::LightKind::Spot;
  source.direction = 0.f;
  source.elevation = 45.f;
  source.innerAngle = 5.f;
  source.outerAngle = 10.f;
  const auto panels = [&] {
    return box().children(
        {box().absolute().rect(140, 80, 40, 40).fill(flatSurface()),
         box().absolute().rect(20, 80, 40, 40).fill(flatSurface())});
  };
  Host actual, reference;
  actual.composer.render(compose::scene().children(
      {panels(), box()
                     .absolute()
                     .rect(100, 100, 0, 0)
                     .scaleX(-1.f)
                     .children({compose::light(source)})}));
  auto placed = source;
  placed.position = {100, 100, 60};
  placed.direction = 180.f;
  reference.composer.render(panels().lighting(placed));
  actual.frame();
  reference.frame();
  EXPECT_GT(brightness(actual.pixel(160, 100)), 60);
  EXPECT_EQ(actual.pixel(40, 100), SK_ColorBLACK);
  expectSamePanel(actual, reference, 0, 0, 200, 200);
}

TEST(ComposeSceneLighting, AScaledSpotAxisChangesItsElevation) {
  auto source = point({0, 0, 60});
  source.kind = material::LightKind::Spot;
  source.direction = 0.f;
  source.elevation = 45.f;
  source.intensity = 1.f;
  source.innerAngle = 5.f;
  source.outerAngle = 10.f;
  const auto panels = [&] {
    return box().children(
        {box().absolute().rect(10, 80, 40, 40).fill(flatSurface()),
         box().absolute().rect(130, 20, 40, 40).fill(flatSurface())});
  };
  Host actual, reference;
  actual.composer.render(compose::scene().children(
      {panels(), box()
                     .absolute()
                     .rect(150, 100, 0, 0)
                     .scaleX(2.f)
                     .children({compose::light(source)})}));
  auto placed = source;
  placed.position = {150, 100, 60};
  placed.elevation = std::atan(.5f) * 180.f / 3.14159265358979323846f;
  reference.composer.render(panels().lighting(placed));
  actual.frame();
  reference.frame();
  EXPECT_GT(brightness(actual.pixel(30, 100)), 60);
  EXPECT_EQ(actual.pixel(150, 40), SK_ColorBLACK);
  expectSamePanel(actual, reference, 0, 0, 200, 200);
}

TEST(ComposeSceneLighting, SourceAncestorDepthAddsToEmitterHeight) {
  const auto surface = flatSurface();
  Host actual, expected;
  actual.composer.render(compose::scene().children(
      {receiver(surface), box().translateZ(50.f).children(
                              {compose::light(point({32, 16, 35}))})}));
  expected.composer.render(compose::scene().children(
      {receiver(surface), compose::light(point({32, 16, 85}))}));
  actual.frame();
  expected.frame();
  ASSERT_GT(brightness(actual.pixel(32, 16)), 60);
  expectSamePanel(actual, expected);
}

TEST(ComposeSceneLighting, ASpotAxisFollowsItsSourceAncestorsRotation) {
  auto source = point({0, 0, 60});
  source.kind = material::LightKind::Spot;
  source.direction = 0.f;
  source.elevation = 45.f;
  source.innerAngle = 10.f;
  source.outerAngle = 20.f;
  const auto panels = [&] {
    return box().children(
        {box().absolute().rect(80, 20, 40, 40).fill(flatSurface()),
         box().absolute().rect(20, 80, 40, 40).fill(flatSurface())});
  };
  Host actual, expected;
  actual.composer.render(compose::scene().children(
      {panels(), box()
                     .absolute()
                     .rect(100, 100, 0, 0)
                     .rotate(90.f)
                     .children({compose::light(source)})}));
  auto placed = source;
  placed.position = {100, 100, 60};
  placed.direction = -90.f;
  expected.composer.render(panels().lighting(placed));
  actual.frame();
  expected.frame();
  EXPECT_GT(brightness(actual.pixel(100, 40)), 60);
  EXPECT_EQ(actual.pixel(40, 100), SK_ColorBLACK);
  expectSamePanel(actual, expected, 0, 0, 200, 200);
}

TEST(ComposeSceneLighting, BoundSourceAnglesAndStrengthWakeWithoutDescription) {
  auto direction = motion::animatable(180.f);
  auto intensity = motion::animatable(1.f);
  auto key = sun(180);
  key.direction = direction;
  key.intensity = intensity;
  auto reads = std::make_shared<int>(0);
  const auto surface =
      material::from(material::image(sigil::media::PixelSource(
                         CountedPicture{reads, grey()})))
          .surface(
              {.roughness = .8f, .normal = material::image(twoFacedNormals())});
  Host host;
  host.composer.render(compose::scene().children(
      {receiver(surface), compose::light(key).key("key")}));
  for (int i = 0; i < 16; ++i) host.frame(1. / 60.);
  const int readsOnce = *reads;
  ASSERT_GT(readsOnce, 0);
  EXPECT_FALSE(host.composer.isRunning());
  EXPECT_GT(brightness(host.pixel(12, 16)),
            brightness(host.pixel(52, 16)) + 60);
  direction = 0.f;
  EXPECT_FALSE(host.composer.dirty());
  EXPECT_TRUE(host.composer.isRunning());
  host.frame();
  EXPECT_GT(brightness(host.pixel(52, 16)),
            brightness(host.pixel(12, 16)) + 60);
  intensity = 0.f;
  EXPECT_TRUE(host.composer.isRunning());
  host.frame();
  EXPECT_EQ(host.pixel(12, 16), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(52, 16), SK_ColorBLACK);
  EXPECT_EQ(*reads, readsOnce);
}

TEST(ComposeSceneLighting, APointIgnoresUnusedAnglesAndTheirLiveBindings) {
  auto direction = motion::animatable(0.f);
  auto elevation = motion::animatable(90.f);
  auto source = point({32, 16, 35});
  source.direction = direction;
  source.elevation = elevation;
  const auto surface = flatSurface();
  Host actual, reference;
  actual.composer.render(compose::scene().children(
      {receiver(surface).cache(Cache::Texture), compose::light(source)}));
  reference.composer.render(compose::scene().children(
      {receiver(surface), compose::light(point({32, 16, 35}))}));
  reference.frame();
  for (int i = 0; i < 16; ++i) actual.frame(1. / 60.);
  ASSERT_GT(brightness(reference.pixel(32, 16)), 60);
  ASSERT_FALSE(actual.composer.isRunning());
  for (const float angle : {180.f, std::numeric_limits<float>::quiet_NaN(),
                            std::numeric_limits<float>::infinity()}) {
    direction = angle;
    elevation = angle;
    EXPECT_FALSE(actual.composer.isRunning());
    actual.frame();
    expectSamePanel(actual, reference);
    EXPECT_FALSE(actual.composer.isRunning());
  }
}

TEST_P(SceneLightColor, LiveColorWakesCachedScopesAndRetainsSurfaceInputs) {
  const auto kind = GetParam();
  auto color = motion::animatable(material::Color{1, 0, 0, 1});
  auto source = point({32, 16, 35});
  source.kind = kind;
  source.elevation = 90.f;
  source.intensity = .25f;
  source.outerAngle = 80.f;
  source.color = color;
  auto baseReads = std::make_shared<int>(0);
  auto normalReads = std::make_shared<int>(0);
  const auto surface =
      material::from(material::image(sigil::media::PixelSource(
                         CountedPicture{baseReads, grey()})))
          .surface({.roughness = .8f,
                    .normal = material::image(sigil::media::PixelSource(
                        CountedPicture{normalReads, twoFacedNormals()}))});
  const auto referenceSurface =
      material::from(material::image(grey()))
          .surface(
              {.roughness = .8f, .normal = material::image(twoFacedNormals())});
  const auto held = [&](const material::Material& paint,
                        const material::Light& light) {
    return compose::scene().children(
        {box()
             .absolute()
             .rect(0, 0, 64, 32)
             .cache(Cache::Texture)
             .children(
                 {receiver(paint), box()
                                       .cache(Cache::Picture)
                                       .children({compose::light(light)})}),
         compose::scene()
             .absolute()
             .rect(0, 50, 64, 32)
             .children({receiver(flatSurface()),
                        compose::light(point({32, 16, 35}, {0, 0, 1, 1}))})});
  };
  Host actual;
  actual.composer.render(held(surface, source));
  for (int i = 0; i < 16; ++i) actual.frame(1. / 60.);
  ASSERT_FALSE(actual.composer.isRunning());
  ASSERT_GT(*baseReads, 0);
  ASSERT_GT(*normalReads, 0);
  const int retainedBaseReads = *baseReads;
  const int retainedNormalReads = *normalReads;
  const SkColor isolated = actual.pixel(32, 66);
  const void* authoredCell = color.identity();
  const auto before = grab(actual);
  for (const material::Color next :
       {material::Color{0, 1, 0, 1}, material::Color{0, 0, 1, 1},
        material::Color{4, .5f, .25f, 1}, material::Color{0, 0, 0, 1}}) {
    color = next;
    EXPECT_FALSE(actual.composer.dirty());
    EXPECT_TRUE(actual.composer.isRunning());
    actual.frame();
    auto constant = source;
    constant.color = motion::Animatable<material::Color>(next);
    Host reference;
    reference.composer.render(held(referenceSurface, constant));
    reference.frame();
    expectSamePanel(actual, reference);
    EXPECT_EQ(actual.pixel(32, 66), isolated);
    EXPECT_EQ(*baseReads, retainedBaseReads);
    EXPECT_EQ(*normalReads, retainedNormalReads);
    EXPECT_EQ(color.identity(), authoredCell);
    EXPECT_EQ(color.value(), next);
    EXPECT_EQ(source.color.value(), next);
    for (int i = 0; i < 16; ++i) actual.frame(1. / 60.);
    EXPECT_FALSE(actual.composer.isRunning());
  }
  EXPECT_NE(grab(actual), before);
  EXPECT_EQ(actual.pixel(32, 16), SK_ColorBLACK);
}

INSTANTIATE_TEST_SUITE_P(ComposeSceneLighting, SceneLightColor,
                         ::testing::Values(material::LightKind::Directional,
                                           material::LightKind::Point,
                                           material::LightKind::Spot),
                         [](const auto& param) {
                           switch (param.param) {
                             case material::LightKind::Directional:
                               return "Directional";
                             case material::LightKind::Point:
                               return "Point";
                             case material::LightKind::Spot:
                               return "Spot";
                           }
                           return "Unknown";
                         });

TEST(ComposeSceneLighting, ColorSanitizingDoesNotWriteTheAuthoredLiveCell) {
  auto color = motion::animatable(material::Color{1, 0, 0, 1});
  auto source = point({32, 16, 35});
  source.color = color;
  source.intensity = .1f;
  Host actual;
  actual.composer.render(compose::scene().children(
      {receiver(flatSurface()), compose::light(source)}));
  for (int i = 0; i < 16; ++i) actual.frame(1. / 60.);
  ASSERT_FALSE(actual.composer.isRunning());
  const void* identity = color.identity();
  color = material::Color{2, std::numeric_limits<float>::quiet_NaN(),
                          std::numeric_limits<float>::infinity(), 1};
  EXPECT_TRUE(actual.composer.isRunning());
  actual.frame();
  auto finiteSource = source;
  finiteSource.color =
      motion::Animatable<material::Color>(material::Color{2, 0, 0, 1});
  Host reference;
  reference.composer.render(compose::scene().children(
      {receiver(flatSurface()), compose::light(finiteSource)}));
  reference.frame();
  expectSamePanel(actual, reference);
  EXPECT_EQ(color.identity(), identity);
  EXPECT_EQ(color.value().r, 2);
  EXPECT_TRUE(std::isnan(color.value().g));
  EXPECT_TRUE(std::isinf(color.value().b));
  EXPECT_TRUE(std::isnan(source.color.value().g));
  EXPECT_TRUE(std::isinf(source.color.value().b));
  for (int i = 0; i < 16; ++i) actual.frame(1. / 60.);
  EXPECT_FALSE(actual.composer.isRunning());
}

TEST(ComposeSceneLighting,
     ColorSnapshotsStayFrozenWhileNewPicturesSampleLiveColor) {
  auto color = motion::animatable(material::Color{1, 0, 0, 1});
  auto source = point({32, 16, 35});
  source.color = color;
  const auto tree =
      box().children({compose::scene().width(200).height(200).children(
          {receiver(flatSurface()).cache(Cache::Texture),
           compose::light(source)})});
  Host live;
  live.composer.render(tree);
  live.frame();
  const auto before = grab(live);
  const auto oldPicture =
      compose::snapshot(tree, fonts(), SkSize::Make(200, 200));
  ASSERT_TRUE(oldPicture);
  color = material::Color{0, 0, 1, 1};
  live.frame();
  ASSERT_NE(grab(live), before);
  const auto newPicture =
      compose::snapshot(tree, fonts(), SkSize::Make(200, 200));
  ASSERT_TRUE(newPicture);
  Host oldBake, newBake;
  oldBake.surface->getCanvas()->drawPicture(oldPicture);
  newBake.surface->getCanvas()->drawPicture(newPicture);
  EXPECT_EQ(grab(oldBake), before);
  expectSamePanel(newBake, live);
  EXPECT_EQ(color.value(), (material::Color{0, 0, 1, 1}));
}

TEST(ComposeSceneLighting, EmptyScopesLeaveSurfaceMapsUnlowered) {
  auto normalReads = std::make_shared<int>(0);
  auto roughnessReads = std::make_shared<int>(0);
  const auto surface =
      material::from(material::Color{.6f, .6f, .6f, 1})
          .surface({.roughness = material::image(sigil::media::PixelSource(
                        CountedPicture{roughnessReads, grey()})),
                    .normal = material::image(sigil::media::PixelSource(
                        CountedPicture{normalReads, twoFacedNormals()}))});
  const auto panel = receiver(surface);
  const auto label =
      compose::text("A").absolute().rect(0, 40, 64, 32).ink(surface);
  const auto tree = [&](bool lit) {
    std::vector<Element> children{panel, label};
    if (lit) children.push_back(compose::light(sun(180)));
    return compose::scene().children(std::move(children));
  };
  const int normalBefore = *normalReads, roughnessBefore = *roughnessReads;
  Host host;
  host.composer.render(tree(false));
  host.frame();
  EXPECT_EQ(*normalReads, normalBefore);
  EXPECT_EQ(*roughnessReads, roughnessBefore);
  host.composer.render(tree(true));
  host.frame();
  EXPECT_GT(*normalReads, normalBefore);
  EXPECT_GT(*roughnessReads, roughnessBefore);
  const int normalLit = *normalReads, roughnessLit = *roughnessReads;
  host.composer.render(tree(false));
  host.frame();
  host.composer.render(tree(true));
  host.frame();
  EXPECT_EQ(*normalReads, normalLit);
  EXPECT_EQ(*roughnessReads, roughnessLit);
}

TEST(ComposeSceneLighting, SnapshotsResolveSourcesBeforeRecordingReceivers) {
  auto offset = motion::animatable(0.f);
  const auto tree =
      box().children({compose::scene().width(200).height(200).children(
          {receiver(flatSurface()),
           box().translateX(offset).children(
               {compose::light(point({32, 16, 35}))})})});
  Host live;
  live.composer.render(tree);
  for (const float x : {0.f, 100.f}) {
    offset = x;
    live.frame();
    const auto picture =
        compose::snapshot(tree, fonts(), SkSize::Make(200, 200));
    ASSERT_TRUE(picture);
    Host baked;
    baked.surface->getCanvas()->drawPicture(picture);
    EXPECT_EQ(grab(baked), grab(live));
  }
}
