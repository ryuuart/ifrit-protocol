// A selected material foreground keeps its surface while the lighting in
// force changes. Its text placement and neighboring paint remain retained.

#include <include/core/SkBitmap.h>
#include <include/core/SkImage.h>
#include <sigilmaterial/core/Lighting.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmedia/core/PixelSource.h>
#include <sigilmotion/values/Animatable.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <optional>
#include <vector>

#include "DressedTypeProbes.h"

namespace compose = sigil::compose;

namespace {

constexpr int kWidth = 448, kHeight = 128;
const SkIRect kNeighbor = SkIRect::MakeXYWH(20, 20, 96, 90);
const SkIRect kSelected = SkIRect::MakeXYWH(116, 20, 88, 90);
constexpr material::Color kGrey{.6f, .6f, .6f, 1};

material::Material surface(bool tilted = false,
                           std::optional<material::Lighting> ownLighting = {}) {
  material::SurfaceOptions finish{.roughness = .8f,
                                  .lighting = std::move(ownLighting)};
  if (tilted) finish.normal = material::Color{.15f, .5f, .85f, 1};
  return material::from(kGrey).surface(std::move(finish));
}

material::Light frontal(material::Color color, float strength = .55f) {
  return material::studio({.elevation = 90.f,
                           .color = color,
                           .intensity = strength,
                           .ambient = 0.f});
}

Text phrase(Cache cache = Cache::Auto) {
  return text(u8"HH HH", whiteStyle(64))
      .absolute()
      .rect(20, 20, kWidth - 40, 90)
      .key("letters")
      .cache(cache);
}

Element selected(const material::Material& finish, Cache cache = Cache::Auto) {
  return phrase(cache).span(sigil::weave::selectors::word(1),
                            SpanStyle().ink(finish));
}

Element scoped(Element lettering, std::optional<material::Light> light = {}) {
  Element result = compose::scene().width(kWidth).height(kHeight);
  if (light) result.children({compose::light(*light).key("source")});
  return result.children({std::move(lettering)});
}

void settle(Host& host) {
  for (int frame = 0; frame < 4; ++frame) host.frame();
}

void expectSameRegion(const SkBitmap& before, const Host& after,
                      const SkIRect& region) {
  size_t changed = 0;
  for (int y = region.top(); y < region.bottom(); ++y)
    for (int x = region.left(); x < region.right(); ++x)
      changed += before.getColor(x, y) != after.pixel(x, y);
  EXPECT_EQ(changed, 0u);
}

void drawReference(Host& reference, const material::Material& finish,
                   material::Lighting lighting = {}) {
  reference.composer.render(
      box().width(kWidth).height(kHeight).fill(finish).lighting(
          std::move(lighting)));
  reference.frame();
}

// The fill shades page pixels independently of glyph-domain placement.
// Only full glyph coverage is compared, so text antialiasing is not a
// competing reference for the material's response.
void expectCoveredPixelsMatch(const Host& actual, const Host& coverage,
                              const Host& reference, const SkIRect& region,
                              bool interiorOnly = false) {
  size_t compared = 0, changed = 0, changedInterior = 0;
  int maximum = 0;
  SkIRect changedBounds = SkIRect::MakeEmpty();
  for (int y = region.top(); y < region.bottom(); ++y)
    for (int x = region.left(); x < region.right(); ++x) {
      if (coverage.pixel(x, y) != SK_ColorWHITE) continue;
      if (interiorOnly) {
        bool interior = x > 0 && y > 0 && x + 1 < kWidth && y + 1 < kHeight;
        for (int dy = -1; interior && dy <= 1; ++dy)
          for (int dx = -1; interior && dx <= 1; ++dx)
            interior = coverage.pixel(x + dx, y + dy) == SK_ColorWHITE;
        if (!interior) continue;
      }
      ++compared;
      const SkColor a = actual.pixel(x, y), b = reference.pixel(x, y);
      int difference = 0;
      for (const int shift : {0, 8, 16})
        difference = std::max(difference, std::abs(int((a >> shift) & 255) -
                                                   int((b >> shift) & 255)));
      maximum = std::max(maximum, difference);
      if (difference > 2) {
        ++changed;
        changedBounds.join(SkIRect::MakeXYWH(x, y, 1, 1));
        bool interior = x > 0 && y > 0 && x + 1 < kWidth && y + 1 < kHeight;
        for (int dy = -1; interior && dy <= 1; ++dy)
          for (int dx = -1; interior && dx <= 1; ++dx)
            interior = coverage.pixel(x + dx, y + dy) == SK_ColorWHITE;
        changedInterior += interior;
      }
    }
  EXPECT_GT(compared, 400u);
  EXPECT_EQ(changed, 0u)
      << "largest channel difference: " << maximum
      << "; bounds: " << changedBounds.left() << ',' << changedBounds.top()
      << "–" << changedBounds.right() << ',' << changedBounds.bottom()
      << "; mismatches inside a full-coverage 3×3 neighborhood: "
      << changedInterior;
}

float meanCoveredBrightness(const Host& actual, const Host& coverage,
                            const SkIRect& region) {
  float total = 0;
  size_t count = 0;
  for (int y = region.top(); y < region.bottom(); ++y)
    for (int x = region.left(); x < region.right(); ++x) {
      if (coverage.pixel(x, y) != SK_ColorWHITE) continue;
      const SkColor pixel = actual.pixel(x, y);
      total += SkColorGetR(pixel) + SkColorGetG(pixel) + SkColorGetB(pixel);
      ++count;
    }
  return count ? total / count : 0.f;
}

void expectRetainedRuns(Host& host, const std::vector<const void*>& shapes,
                        const std::vector<glm::vec2>& origins) {
  EXPECT_EQ(runShapes(host, "letters"), shapes);
  EXPECT_EQ(runOrigins(host, "letters"), origins);
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

sk_sp<SkImage> greyImage(SkColor color = SkColorSetRGB(150, 150, 150)) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(8, 8, true);
  bitmap.eraseColor(color);
  bitmap.setImmutable();
  return bitmap.asImage();
}

}  // namespace

TEST(ComposeSpanLighting, AFlatSurfaceFollowsSelectedWordSceneReplacement) {
  const auto finish = surface();
  Host actual(kWidth, kHeight), coverage(kWidth, kHeight),
      reference(kWidth, kHeight);
  coverage.composer.render(scoped(phrase()));
  coverage.frame();
  const auto untouched = coverage.pixels();
  std::vector<const void*> shapes;
  std::vector<glm::vec2> origins;
  for (const auto& source : {std::optional(frontal({1, 0, 0, 1})),
                             std::optional(frontal({0, 0, 1, 1})),
                             std::optional<material::Light>{}}) {
    actual.composer.render(scoped(selected(finish), source));
    actual.frame();
    drawReference(reference, finish,
                  source ? material::Lighting(*source) : material::Lighting{});
    expectCoveredPixelsMatch(actual, coverage, reference, kSelected);
    expectSameRegion(untouched, actual, kNeighbor);
    if (shapes.empty()) {
      shapes = runShapes(actual, "letters");
      origins = runOrigins(actual, "letters");
      ASSERT_FALSE(shapes.empty());
    } else {
      expectRetainedRuns(actual, shapes, origins);
    }
  }
}

TEST(ComposeSpanLighting, AStaticNormalKeepsItsLightingResponseOnAWord) {
  const auto finish = surface(true);
  Host actual(kWidth, kHeight), coverage(kWidth, kHeight),
      reference(kWidth, kHeight);
  coverage.composer.render(scoped(phrase()));
  coverage.frame();
  float left = 0;
  for (const float bearing : {180.f, 0.f}) {
    const auto source = material::studio({.direction = bearing,
                                          .elevation = 30.f,
                                          .intensity = .75f,
                                          .ambient = 0.f});
    actual.composer.render(scoped(selected(finish), source));
    actual.frame();
    drawReference(reference, finish, source);
    expectCoveredPixelsMatch(actual, coverage, reference, kSelected);
    const float brightness = meanCoveredBrightness(actual, coverage, kSelected);
    if (bearing == 180.f)
      left = brightness;
    else
      EXPECT_GT(left, brightness + 60.f);
  }
}

TEST(ComposeSpanLighting, EmptySceneLeavesSurfaceMapsUnreadUntilLighting) {
  auto normalReads = std::make_shared<int>(0);
  auto roughnessReads = std::make_shared<int>(0);
  const auto normal =
      material::image(sigil::media::PixelSource(CountedPicture{
                          normalReads, greyImage(SkColorSetRGB(37, 128, 218))}),
                      {.repeat = material::Repeat::Pad});
  const auto roughness = material::image(
      sigil::media::PixelSource(CountedPicture{roughnessReads, greyImage()}),
      {.repeat = material::Repeat::Pad});
  const auto finish =
      material::from(kGrey).surface({.roughness = roughness, .normal = normal});
  const auto referenceFinish = material::from(kGrey).surface(
      {.roughness = 150.f / 255,
       .normal = material::Color{37.f / 255, 128.f / 255, 218.f / 255, 1}});
  Host actual(kWidth, kHeight), coverage(kWidth, kHeight),
      reference(kWidth, kHeight);
  coverage.composer.render(scoped(phrase()));
  coverage.frame();
  actual.composer.render(scoped(selected(finish)));
  settle(actual);
  drawReference(reference, material::from(kGrey));
  expectCoveredPixelsMatch(actual, coverage, reference, kSelected);
  EXPECT_EQ(*normalReads, 0);
  EXPECT_EQ(*roughnessReads, 0);

  const auto source = material::studio({.direction = 180.f,
                                        .elevation = 30.f,
                                        .intensity = .55f,
                                        .ambient = 0.f});
  actual.composer.render(scoped(selected(finish), source));
  actual.frame();
  drawReference(reference, referenceFinish, source);
  expectCoveredPixelsMatch(actual, coverage, reference, kSelected);
  EXPECT_GT(*normalReads, 0);
  EXPECT_GT(*roughnessReads, 0);
  const int normals = *normalReads, roughnesses = *roughnessReads;
  actual.composer.render(scoped(selected(finish)));
  actual.frame();
  drawReference(reference, material::from(kGrey));
  expectCoveredPixelsMatch(actual, coverage, reference, kSelected);
  EXPECT_EQ(*normalReads, normals);
  EXPECT_EQ(*roughnessReads, roughnesses);
}

TEST(ComposeSpanLighting, AReceiverReplacesAndClearsSelectedWordLighting) {
  const auto finish = surface();
  const auto inherited = frontal({1, 0, 0, 1});
  const auto replacement = frontal({0, 1, 0, 1});
  Host actual(kWidth, kHeight), coverage(kWidth, kHeight),
      reference(kWidth, kHeight);
  coverage.composer.render(scoped(phrase()));
  coverage.frame();
  for (const auto& override :
       {material::Lighting(replacement), material::Lighting{}}) {
    actual.composer.render(
        scoped(selected(finish).lighting(override), inherited));
    actual.frame();
    drawReference(reference, finish, override);
    expectCoveredPixelsMatch(actual, coverage, reference, kSelected);
    expectSameRegion(coverage.pixels(), actual, kNeighbor);
  }
}

TEST(ComposeSpanLighting, AnOwnEmptyLightingKeepsSelectedWordPaintFlat) {
  const auto finish = surface(true, material::Lighting{});
  Host actual(kWidth, kHeight), coverage(kWidth, kHeight),
      reference(kWidth, kHeight);
  coverage.composer.render(scoped(phrase()));
  coverage.frame();
  drawReference(reference, material::from(kGrey));
  for (const auto& source : {frontal({1, 0, 0, 1}), frontal({0, 0, 1, 1})}) {
    actual.composer.render(scoped(selected(finish), source));
    actual.frame();
    expectCoveredPixelsMatch(actual, coverage, reference, kSelected);
    expectSameRegion(coverage.pixels(), actual, kNeighbor);
  }
}

TEST(ComposeSpanLighting, OwnStaticLightingExcludesInheritedLightBindings) {
  for (const material::Lighting& own :
       {material::Lighting{}, material::Lighting(frontal({0, 0, 1, 1}))}) {
    SCOPED_TRACE(bool(own));
    auto color = motion::animatable(material::Color{1, 0, 0, 1});
    auto intensity = motion::animatable(.55f);
    const auto inherited = material::studio(
        {.color = color, .intensity = intensity, .ambient = 0.f});
    const auto finish = surface(false, own);
    Host actual(kWidth, kHeight), coverage(kWidth, kHeight),
        reference(kWidth, kHeight);
    coverage.composer.render(scoped(phrase()));
    coverage.frame();
    actual.composer.render(box().lighting(inherited).children(
        {selected(finish).cache(Cache::Picture)}));
    settle(actual);
    drawReference(reference, surface(), own);
    expectCoveredPixelsMatch(actual, coverage, reference, kSelected);
    const auto before = actual.pixels();
    EXPECT_FALSE(actual.composer.isRunning());
    color = material::Color{0, 1, 0, 1};
    intensity = .15f;
    EXPECT_FALSE(actual.composer.isRunning());
    actual.frame();
    expectSameRegion(before, actual, SkIRect::MakeWH(kWidth, kHeight));
    EXPECT_EQ(actual.composer.stats().picturesRecorded, 0u);
    EXPECT_EQ(actual.composer.stats().texturesBaked, 0u);
  }
}

TEST(ComposeSpanLighting, OverwrittenSurfaceInkDoesNotObserveInheritedLight) {
  auto color = motion::animatable(material::Color{1, 0, 0, 1});
  const auto inherited = material::studio({.color = color});
  auto reads = std::make_shared<int>(0);
  const auto hidden =
      material::image(
          sigil::media::PixelSource(CountedPicture{reads, greyImage()}),
          {.repeat = material::Repeat::Pad})
          .surface({.roughness = .8f});
  const int initialReads = *reads;
  Host actual(kWidth, kHeight), plain(kWidth, kHeight);
  plain.composer.render(scoped(phrase()));
  plain.frame();
  actual.composer.render(box().lighting(inherited).children(
      {phrase(Cache::Picture)
           .span(sigil::weave::selectors::word(1), SpanStyle().ink(hidden))
           .span(sigil::weave::selectors::word(1),
                 SpanStyle().ink(material::Color{1, 1, 1, 1}))}));
  settle(actual);
  EXPECT_TRUE(identicalPixels(actual, plain, kWidth, kHeight));
  EXPECT_EQ(*reads, initialReads);
  EXPECT_FALSE(actual.composer.isRunning());
  color = material::Color{0, 0, 1, 1};
  EXPECT_FALSE(actual.composer.isRunning());
  actual.frame();
  EXPECT_TRUE(identicalPixels(actual, plain, kWidth, kHeight));
  EXPECT_EQ(*reads, initialReads);
  EXPECT_EQ(actual.composer.stats().picturesRecorded, 0u);
  EXPECT_EQ(actual.composer.stats().texturesBaked, 0u);
}

TEST(ComposeSpanLighting, AFinalFontColorClearsTheEarlierSurfaceInk) {
  const auto finish = surface(true);
  const auto source = frontal({0, 0, 1, 1});
  Host actual(kWidth, kHeight), expected(kWidth, kHeight);
  expected.composer.render(
      scoped(phrase().span(sigil::weave::selectors::word(1),
                           SpanStyle().ink(material::Color{1, 0, 0, 1})),
             source));
  expected.frame();
  actual.composer.render(scoped(phrase(), source));
  actual.frame();
  const auto shapes = runShapes(actual, "letters");
  const auto origins = runOrigins(actual, "letters");
  actual.composer.render(scoped(
      phrase().span(sigil::weave::selectors::word(1),
                    SpanStyle().ink(finish).font(
                        sigil::weave::Type{.color = SkColor4f{1, 0, 0, 1}})),
      source));
  actual.frame();
  EXPECT_TRUE(identicalPixels(actual, expected, kWidth, kHeight));
  EXPECT_GT(countColor(actual, kSelected, SK_ColorRED), 400);
  expectRetainedRuns(actual, shapes, origins);
}

TEST(ComposeSpanLighting, LiveSceneLightWakesCachesAndRetainsStaticInputs) {
  for (const Cache cache : {Cache::Picture, Cache::Texture, Cache::Group}) {
    SCOPED_TRACE(static_cast<int>(cache));
    auto color = motion::animatable(material::Color{1, 0, 0, 1});
    auto intensity = motion::animatable(.55f);
    auto reads = std::make_shared<int>(0);
    const auto finish =
        material::image(
            sigil::media::PixelSource(CountedPicture{reads, greyImage()}),
            {.repeat = material::Repeat::Pad})
            .surface({.roughness = .8f});
    const auto referenceFinish =
        material::from(
            material::Color{150.f / 255, 150.f / 255, 150.f / 255, 1})
            .surface({.roughness = .8f});
    Host actual(kWidth, kHeight), coverage(kWidth, kHeight),
        reference(kWidth, kHeight);
    coverage.composer.render(scoped(phrase(cache)));
    settle(coverage);
    actual.composer.render(scoped(selected(finish, cache),
                                  material::studio({.elevation = 90.f,
                                                    .color = color,
                                                    .intensity = intensity,
                                                    .ambient = 0.f})));
    settle(actual);
    const int initialReads = *reads;
    ASSERT_GT(initialReads, 0);
    const auto shapes = runShapes(actual, "letters");
    const auto origins = runOrigins(actual, "letters");
    ASSERT_FALSE(shapes.empty());
    EXPECT_FALSE(actual.composer.isRunning());
    const auto neighbor = actual.pixels();

    color = material::Color{0, 0, 1, 1};
    EXPECT_TRUE(actual.composer.isRunning());
    settle(actual);
    drawReference(reference, referenceFinish, frontal({0, 0, 1, 1}));
    expectCoveredPixelsMatch(actual, coverage, reference, kSelected);
    expectSameRegion(neighbor, actual, kNeighbor);
    expectRetainedRuns(actual, shapes, origins);
    EXPECT_EQ(*reads, initialReads);
    const float bright = meanCoveredBrightness(actual, coverage, kSelected);

    intensity = .15f;
    EXPECT_TRUE(actual.composer.isRunning());
    settle(actual);
    drawReference(reference, referenceFinish, frontal({0, 0, 1, 1}, .15f));
    expectCoveredPixelsMatch(actual, coverage, reference, kSelected);
    EXPECT_GT(bright,
              meanCoveredBrightness(actual, coverage, kSelected) + 40.f);
    expectSameRegion(neighbor, actual, kNeighbor);
    expectRetainedRuns(actual, shapes, origins);
    EXPECT_EQ(*reads, initialReads);
    EXPECT_FALSE(actual.composer.isRunning());
  }
}

TEST(ComposeSpanLighting, PointLightReadsPlacedGlyphAndWordDomains) {
  for (const PaintBox domain : {PaintBox::Glyph, PaintBox::Word}) {
    SCOPED_TRACE(static_cast<int>(domain));
    for (const bool tilted : {false, true}) {
      SCOPED_TRACE(tilted);
      auto offset = motion::animatable(40.f);
      const auto finish = surface(tilted);
      const auto textOnly = [&] {
        auto result = text(u8"HH    HH", whiteStyle(64))
                          .absolute()
                          .rect(20, 20, kWidth - 40, 90)
                          .translateX(17)
                          .translateY(11)
                          .key("letters");
        if (tilted && domain == PaintBox::Word)
          result.span(sigil::weave::selectors::range({7, 8}),
                      SpanStyle().fontSize(96));
        return result;
      };
      material::Light source{.intensity = .85f,
                             .ambient = 0.f,
                             .kind = material::LightKind::Point,
                             .position = {32, 48, 42},
                             .range = 400};
      Host actual(kWidth, kHeight), coverage(kWidth, kHeight),
          reference(kWidth, kHeight);
      coverage.composer.render(scoped(textOnly()));
      coverage.frame();
      actual.composer.render(
          compose::scene().width(kWidth).height(kHeight).children(
              {compose::light(source).key("source").translateX(offset),
               textOnly().span(sigil::weave::Selector{},
                               SpanStyle().ink(finish, domain))}));
      std::vector<const void*> shapes;
      std::vector<glm::vec2> origins;
      const SkIRect left = SkIRect::MakeXYWH(37, 31, 90, 90);
      const SkIRect right = SkIRect::MakeXYWH(171, 31, 128, 90);
      for (const float position : {40.f, 168.f}) {
        offset = position;
        actual.frame();
        material::Light placed = source;
        placed.position.x += position;
        material::Lighting lighting(placed);
        lighting.frame = material::LightingFrame::Scene;
        drawReference(reference, finish, std::move(lighting));
        expectCoveredPixelsMatch(actual, coverage, reference,
                                 SkIRect::MakeWH(kWidth, kHeight));
        const float near = meanCoveredBrightness(actual, coverage, left);
        const float far = meanCoveredBrightness(actual, coverage, right);
        if (!tilted) {
          if (position == 40.f)
            EXPECT_GT(near, far + 60.f);
          else
            EXPECT_GT(far, near + 60.f);
        }
        if (shapes.empty()) {
          shapes = runShapes(actual, "letters");
          origins = runOrigins(actual, "letters");
        } else {
          expectRetainedRuns(actual, shapes, origins);
        }
      }
    }
  }
}

TEST(ComposeSpanLighting, WholeLeafSurfaceInkReadsPlacedPaintDomains) {
  const auto finish = surface(true);
  for (const PaintBox domain :
       {PaintBox::Element, PaintBox::Glyph, PaintBox::Word}) {
    SCOPED_TRACE(static_cast<int>(domain));
    for (const auto kind :
         {material::LightKind::Point, material::LightKind::Spot}) {
      SCOPED_TRACE(static_cast<int>(kind));
      auto offset = motion::animatable(40.f);
      const auto textOnly = [] {
        return text(u8"HH    HH", whiteStyle(64))
            .absolute()
            .rect(20, 20, kWidth - 40, 90)
            .translateX(17)
            .translateY(11)
            .key("letters")
            .span(sigil::weave::selectors::range({7, 8}),
                  SpanStyle().fontSize(96));
      };
      material::Light source{.elevation = 90.f,
                             .intensity = .8f,
                             .ambient = 0.f,
                             .kind = kind,
                             .position = {32, 48, 96},
                             .range = 440,
                             .innerAngle = 25,
                             .outerAngle = 80};
      Host actual(kWidth, kHeight), coverage(kWidth, kHeight),
          reference(kWidth, kHeight);
      coverage.composer.render(scoped(textOnly()));
      coverage.frame();
      actual.composer.render(
          compose::scene().width(kWidth).height(kHeight).children(
              {compose::light(source).key("source").translateX(offset),
               textOnly().ink(finish, domain)}));
      std::vector<const void*> shapes;
      std::vector<glm::vec2> origins;
      for (const float position : {40.f, 168.f}) {
        offset = position;
        actual.frame();
        material::Light placed = source;
        placed.position.x += position;
        material::Lighting lighting(placed);
        lighting.frame = material::LightingFrame::Scene;
        drawReference(reference, finish, std::move(lighting));
        const SkIRect page = SkIRect::MakeWH(kWidth, kHeight);
        expectCoveredPixelsMatch(actual, coverage, reference, page);
        EXPECT_GT(meanCoveredBrightness(actual, coverage, page), 30.f);
        if (shapes.empty()) {
          shapes = runShapes(actual, "letters");
          origins = runOrigins(actual, "letters");
          ASSERT_FALSE(shapes.empty());
        } else {
          expectRetainedRuns(actual, shapes, origins);
        }
      }
    }
  }
}

TEST(ComposeSpanLighting, InheritedPointLightFollowsCachedAncestorPlacement) {
  const auto finish = surface();
  const auto point = material::studio({.intensity = .7f,
                                       .ambient = 0.f,
                                       .kind = material::LightKind::Point,
                                       .position = {170, 70, 75},
                                       .range = 500});
  for (const Cache cache : {Cache::Picture, Cache::Texture}) {
    SCOPED_TRACE(static_cast<int>(cache));
    auto offset = motion::animatable(24.f);
    Host actual(kWidth, kHeight), coverage(kWidth, kHeight),
        reference(kWidth, kHeight);
    const auto receiver = [&](motion::Animatable<float> translation) {
      return stack().width(kWidth).height(kHeight).lighting(point).children(
          {stack()
               .width(kWidth)
               .height(kHeight)
               .translateX(translation)
               .children({selected(finish, cache).rotate(13)})});
    };
    actual.composer.render(receiver(offset));
    drawReference(reference, finish, point);
    std::vector<const void*> shapes;
    std::vector<glm::vec2> origins;
    for (const float position : {24.f, 64.f}) {
      offset = position;
      actual.frame();
      // Black outside the selected word makes this an independent glyph
      // coverage mask, while preserving its real shaped placement.
      coverage.composer.render(stack().width(kWidth).height(kHeight).children(
          {stack().width(kWidth).height(kHeight).translateX(position).children(
              {phrase(cache)
                   .span(sigil::weave::selectors::word(0),
                         SpanStyle().ink(material::Color{0, 0, 0, 1}))
                   .rotate(13)})}));
      coverage.frame();
      // A moving Texture receiver linearly resamples its local bake. Its
      // glyph fringes cannot match an unmasked page fill's full coverage.
      expectCoveredPixelsMatch(actual, coverage, reference,
                               SkIRect::MakeWH(kWidth, kHeight), true);
      // A fresh live transform takes the same local cache path, including
      // every glyph edge, without retaining the earlier placement.
      auto freshOffset = motion::animatable(position);
      Host fresh(kWidth, kHeight);
      fresh.composer.render(receiver(freshOffset));
      fresh.frame();
      EXPECT_TRUE(identicalPixels(actual, fresh, kWidth, kHeight));
      if (shapes.empty()) {
        shapes = runShapes(actual, "letters");
        origins = runOrigins(actual, "letters");
        ASSERT_FALSE(shapes.empty());
      } else {
        expectRetainedRuns(actual, shapes, origins);
      }
    }
  }
}

TEST(ComposeSpanLighting, AuthoredLiveSurfaceInputsRemainOutsideSpanContract) {
  struct Roughness {
    float value = .8f;
  };
  auto roughness = motion::animatable(.8f);
  const auto channel = material::shader(
                           "half4 main(float2 p) { return half4(value, value, "
                           "value, 1); }",
                           Roughness{})
                           .bind("value", roughness);
  auto ownColor = motion::animatable(material::Color{0, 1, 0, 1});
  const auto ownLight = material::studio({.color = ownColor});
  const std::vector<material::Material> liveInks{
      material::from(kGrey).surface({.roughness = channel}),
      surface(false, material::Lighting(ownLight))};
  Host coverage(kWidth, kHeight);
  coverage.composer.render(scoped(phrase()));
  coverage.frame();
  for (const auto& ink : liveInks) {
    ASSERT_TRUE(ink.isRunning());
    Host actual(kWidth, kHeight);
    actual.composer.render(scoped(selected(ink), frontal({1, 0, 0, 1})));
    actual.frame();
    EXPECT_TRUE(identicalPixels(actual, coverage, kWidth, kHeight));
  }
}

TEST(ComposeSpanLighting, HeightInkUsesUnitPixelWidthsAcrossFontSplits) {
  const auto height = material::shader(R"(
half4 main(float2 p) { return half4(half3(.25 + .5 * p.x), 1); }
)");
  for (const PaintBox domain : {PaintBox::Glyph, PaintBox::Word})
    for (const Cache cache : {Cache::Picture, Cache::Texture})
      for (float depth : {-8.0f, 8.0f}) {
        SCOPED_TRACE(static_cast<int>(domain));
        SCOPED_TRACE(static_cast<int>(cache));
        SCOPED_TRACE(depth);
        const auto finish = [](material::Material normal) {
          return material::from(kGrey).surface(
              {.roughness = .8f, .normal = std::move(normal)});
        };
        const auto encoded = [&](float width) {
          const float slope = -depth * .5f / width;
          const float length = std::sqrt(slope * slope + 1);
          return finish(material::Color{slope / length * .5f + .5f, .5f,
                                        .5f / length + .5f, 1});
        };
        const auto letters = [&] {
          return phrase(cache).span(sigil::weave::selectors::range({4, 5}),
                                    SpanStyle().fontSize(96));
        };
        const auto normal = material::surface::normalFromHeight(
            height, {.depth = depth, .step = .5f});
        auto actualText = letters().span(
            sigil::weave::selectors::word(1),
            SpanStyle().ink(finish(material::surface::blendNormals(
                                normal, material::Color{.5f, .5f, 1, 1})),
                            domain));
        // The instrument advances 0.6 em: 38.4 and 57.6 pixels for these
        // two sizes, or one 96-pixel word across their font boundary.
        auto expectedText =
            letters()
                .span(sigil::weave::selectors::range({3, 4}),
                      SpanStyle().ink(
                          encoded(domain == PaintBox::Word ? 96 : 38.4f)))
                .span(sigil::weave::selectors::range({4, 5}),
                      SpanStyle().ink(
                          encoded(domain == PaintBox::Word ? 96 : 57.6f)));
        auto position = motion::animatable(100.f);
        const material::Light source{.intensity = .6f,
                                     .ambient = .05f,
                                     .kind = material::LightKind::Point,
                                     .position = {0, 60, 80},
                                     .range = 500};
        const auto scene = [&](Element body) {
          return compose::scene().width(kWidth).height(kHeight).children(
              {compose::light(source).translateX(position), std::move(body)});
        };
        Host actual(kWidth, kHeight), expected(kWidth, kHeight),
            coverage(kWidth, kHeight);
        coverage.composer.render(scoped(letters()));
        coverage.frame();
        actual.composer.render(scene(std::move(actualText)));
        expected.composer.render(scene(std::move(expectedText)));
        settle(actual);
        const auto shapes = runShapes(actual, "letters");
        const auto origins = runOrigins(actual, "letters");
        ASSERT_FALSE(shapes.empty());
        for (float x : {100.f, 280.f, 100.f}) {
          position = x;
          settle(actual);
          settle(expected);
          expectCoveredPixelsMatch(actual, coverage, expected,
                                   SkIRect::MakeWH(kWidth, kHeight));
          expectRetainedRuns(actual, shapes, origins);
        }
      }
}
