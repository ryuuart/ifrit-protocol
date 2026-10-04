// What a pixel bake keeps of the destination it is drawn for: a float
// destination's precision and colour space through held texture and group
// bakes, promoted and split bakes, a cell sheet and brush art shared by
// composers drawing to different formats, and the switch from one format
// to another dropping what was baked for the last.

#include <include/core/SkColorSpace.h>
#include <sigilcompose/brush/Ribbons.h>
#include <sigilmaterial/core/Lighting.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "support/CoreTestSupport.h"

namespace {

material::Material hdrCacheSurface(material::Color illumination = {4, 2, 1,
                                                                   1}) {
  return material::from(material::Color{.7f, .6f, .5f, 1})
      .surface({.roughness = 1.0f,
                .lighting = material::studio(
                    {.elevation = 90, .color = illumination, .ambient = 0})});
}

Element hdrCacheFigure(Cache cache) {
  const material::Material finish = hdrCacheSurface();
  return box()
      .width(128)
      .height(80)
      .cache(Cache::Picture)
      .children({box().width(128).height(80).cache(cache).children(
          {box()
               .absolute()
               .rect(8.25f, 12.25f, 40, 40)
               .shape(skiaShape([](SkSize size) {
                 return SkPathBuilder()
                     .addOval(SkRect::MakeSize(size))
                     .detach();
               }))
               .fill(finish),
           text(u8"O")
               .absolute()
               .left(64.25f)
               .top(10.75f)
               .font({.face = sigil::test::instrument::sans(), .size = 48})
               .ink(finish)})});
}

float premulDifference(const SkBitmap& actual, const SkBitmap& expected) {
  float worst = 0;
  for (int y = 0; y < actual.height(); ++y)
    for (int x = 0; x < actual.width(); ++x) {
      const SkColor4f a = actual.getColor4f(x, y);
      const SkColor4f e = expected.getColor4f(x, y);
      worst = std::max({worst, std::abs(a.fR * a.fA - e.fR * e.fA),
                        std::abs(a.fG * a.fA - e.fG * e.fA),
                        std::abs(a.fB * a.fA - e.fB * e.fA),
                        std::abs(a.fA - e.fA)});
    }
  return worst;
}

void expectHdrCoverage(const SkBitmap& bitmap) {
  int interior = 0, edges = 0, glyphInterior = 0, glyphEdges = 0;
  for (int y = 0; y < bitmap.height(); ++y)
    for (int x = 0; x < bitmap.width(); ++x) {
      const SkColor4f pixel = bitmap.getColor4f(x, y);
      if (pixel.fA > .99f && pixel.fR > 1.0f) {
        ++interior;
        if (x >= 64) ++glyphInterior;
      }
      if (pixel.fA > .05f && pixel.fA < .95f && pixel.fR > 1.0f) {
        ++edges;
        if (x >= 64) ++glyphEdges;
      }
    }
  EXPECT_GT(interior, 100);
  EXPECT_GT(edges, 10);
  EXPECT_GT(glyphInterior, 10);
  EXPECT_GT(glyphEdges, 5);
}

}  // namespace

TEST(ComposeCaching, FloatDestinationSurvivesHeldPixelBakesAndFormatSwitches) {
  for (const float density : {0.0f, 1.0f}) {
    SCOPED_TRACE(density);
    for (const Cache cache : {Cache::Texture, Cache::Group}) {
      SCOPED_TRACE(static_cast<int>(cache));
      Host retained(128, 80);
      retained.composer.setAutoTexturePromotion(false);
      retained.composer.setBakeDensity(density);
      retained.composer.render(hdrCacheFigure(cache));
      for (const SkColorType type : {kN32_SkColorType, kRGBA_F16_SkColorType,
                                     kRGBA_F32_SkColorType, kN32_SkColorType}) {
        SCOPED_TRACE(static_cast<int>(type));
        const SkImageInfo info =
            SkImageInfo::Make(128, 80, type, kPremul_SkAlphaType);
        retained.surface = SkSurfaces::Raster(info);
        ASSERT_TRUE(retained.surface);
        for (int frame = 0; frame < 4; ++frame)
          retained.frame(0, SK_ColorTRANSPARENT);
        const SkBitmap actual = retained.pixels();
        const test::Raster fresh = test::rasterize(hdrCacheFigure(Cache::None),
                                                   fonts(), {128, 80}, type);
        ASSERT_TRUE(fresh.valid());
        EXPECT_LE(premulDifference(actual, fresh.bitmap),
                  type == kN32_SkColorType ? .016f : .004f);
        EXPECT_EQ(retained.composer.stats().picturesRecorded, 0u);
        EXPECT_EQ(retained.composer.stats().texturesBaked, 0u);
        EXPECT_GE(retained.composer.stats().texturesLive, 1u);
        if (type != kN32_SkColorType) expectHdrCoverage(actual);
      }
    }
  }
}

TEST(ComposeCaching, SharedCellSheetKeepsHdrForEachComposersDestination) {
  using namespace sigil::compose::instancing;
  for (const Mode mode : {Mode::Data, Mode::Live}) {
    SCOPED_TRACE(static_cast<int>(mode));
    auto atlas = std::make_shared<CellSheet>(1.0f);
    atlas->cell(
        box()
            .width(40)
            .height(40)
            .cache(Cache::Texture)
            .shape(skiaShape([](SkSize size) {
              return SkPathBuilder().addOval(SkRect::MakeSize(size)).detach();
            }))
            .fill(hdrCacheSurface()),
        {40, 40});
    auto pool = std::make_shared<Pool>();
    pool->add({32, 32});
    const Element tree = box()
                             .width(64)
                             .height(64)
                             .cache(Cache::Picture)
                             .children({instances(atlas, pool, mode)});
    Host byteHost(64, 64), floatHost(64, 64);
    for (Host* host : {&byteHost, &floatHost}) {
      host->composer.setAutoTexturePromotion(false);
      host->composer.render(tree);
    }
    const auto draw = [&](Host& host, SkColorType type) {
      host.surface = SkSurfaces::Raster(
          SkImageInfo::Make(64, 64, type, kPremul_SkAlphaType));
      EXPECT_TRUE(host.surface);
      if (!host.surface) return SkBitmap{};
      for (int frame = 0; frame < 4; ++frame)
        host.frame(0, SK_ColorTRANSPARENT);
      return host.pixels();
    };
    const SkBitmap byte = draw(byteHost, kN32_SkColorType);
    ASSERT_FALSE(byte.isNull());
    const SkBitmap firstFloat = draw(floatHost, kRGBA_F16_SkColorType);
    ASSERT_FALSE(firstFloat.isNull());
    EXPECT_EQ(atlas->image()->colorType(), kRGBA_F16_SkColorType);
    EXPECT_GT(firstFloat.getColor4f(32, 32).fR, 1);
    byteHost.frame(0, SK_ColorTRANSPARENT);
    EXPECT_LE(premulDifference(byteHost.pixels(), byte), .001f);
    const SkBitmap switched = draw(byteHost, kRGBA_F32_SkColorType);
    ASSERT_FALSE(switched.isNull());
    EXPECT_EQ(atlas->image()->colorType(), kRGBA_F32_SkColorType);
    EXPECT_GT(switched.getColor4f(32, 32).fR, 1);
    floatHost.frame(0, SK_ColorTRANSPARENT);
    EXPECT_LE(premulDifference(floatHost.pixels(), firstFloat), .004f);
    const SkBitmap bytesAgain = draw(floatHost, kN32_SkColorType);
    ASSERT_FALSE(bytesAgain.isNull());
    EXPECT_LE(premulDifference(bytesAgain, byte), .001f);

    const Element direct =
        box()
            .absolute()
            .rect(12, 12, 40, 40)
            .shape(skiaShape([](SkSize size) {
              return SkPathBuilder().addOval(SkRect::MakeSize(size)).detach();
            }))
            .fill(hdrCacheSurface());
    const test::Raster reference =
        test::rasterize(direct, fonts(), {64, 64}, kRGBA_F32_SkColorType);
    ASSERT_TRUE(reference.valid());
    const SkColor4f interior = reference.at(32, 32);
    EXPECT_NEAR(switched.getColor4f(32, 32).fR, interior.fR, .004f);
    EXPECT_NEAR(switched.getColor4f(32, 32).fG, interior.fG, .004f);
    EXPECT_NEAR(switched.getColor4f(32, 32).fB, interior.fB, .004f);
    auto unitAtlas = std::make_shared<CellSheet>(1.0f);
    unitAtlas->cell(
        box()
            .width(40)
            .height(40)
            .cache(Cache::Texture)
            .shape(skiaShape([](SkSize size) {
              return SkPathBuilder().addOval(SkRect::MakeSize(size)).detach();
            }))
            .fill(hdrCacheSurface({1, 1, 1, 1})),
        {40, 40});
    const test::Raster coverage =
        test::rasterize(box()
                            .width(64)
                            .height(64)
                            .cache(Cache::None)
                            .children({instances(unitAtlas, pool, Mode::Live)}),
                        fonts(), {64, 64}, kRGBA_F32_SkColorType);
    ASSERT_TRUE(coverage.valid());
    // Atlas sampling has its own coverage. Measure it with the same geometry
    // and unit illumination, then require unclipped radiance at every sample.
    float coverageError = 0, energyError = 0;
    int edges = 0;
    for (int y = 0; y < 64; ++y)
      for (int x = 0; x < 64; ++x) {
        const SkColor4f pixel = switched.getColor4f(x, y);
        const float alpha = coverage.at(x, y).fA;
        coverageError = std::max(coverageError, std::abs(pixel.fA - alpha));
        energyError = std::max(
            {energyError, std::abs(pixel.fR * pixel.fA - interior.fR * alpha),
             std::abs(pixel.fG * pixel.fA - interior.fG * alpha),
             std::abs(pixel.fB * pixel.fA - interior.fB * alpha)});
        edges += pixel.fA > .05f && pixel.fA < .95f && pixel.fR > 1;
      }
    EXPECT_LE(coverageError, .00002f);
    EXPECT_LE(energyError, .004f);
    EXPECT_GT(edges, 10);
  }
}

TEST(ComposeCaching, PromotedAndSplitBakesKeepTheFloatDestination) {
  for (const bool split : {false, true}) {
    SCOPED_TRACE(split);
    const auto moving = motion::animatable(.5f);
    const auto figure = [&] {
      Element radiant =
          box()
              .key("radiant")
              .absolute()
              .rect(8.25f, 12.25f, 40, 40)
              .shape(skiaShape([](SkSize size) {
                return SkPathBuilder().addOval(SkRect::MakeSize(size)).detach();
              }))
              .fill(hdrCacheSurface());
      if (split)
        radiant.children(
            {box().absolute().rect(14, 14, 8, 8).opacity(moving).fill(blue())});
      return box().width(64).height(64).cache(Cache::None).children({radiant});
    };
    Host retained(64, 64);
    retained.composer.setProfiling(true);
    retained.composer.setAutoTexturePromotion(Composer::PromotionPolicy::Eager);
    retained.composer.render(figure());
    for (const SkColorType type :
         {kN32_SkColorType, kRGBA_F16_SkColorType, kRGBA_F32_SkColorType}) {
      retained.surface = SkSurfaces::Raster(
          SkImageInfo::Make(64, 64, type, kPremul_SkAlphaType));
      ASSERT_TRUE(retained.surface);
      for (int frame = 0; frame < 4; ++frame)
        retained.frame(0, SK_ColorTRANSPARENT);
      const Composer::NodeCost* row = requireRow(retained.composer, "radiant");
      ASSERT_NE(row, nullptr);
      EXPECT_EQ(row->cacheState, split ? Composer::CacheState::SplitOwn
                                       : Composer::CacheState::Promoted);
      const test::Raster fresh =
          test::rasterize(figure(), fonts(), {64, 64}, type);
      ASSERT_TRUE(fresh.valid());
      const SkBitmap actual = retained.pixels();
      EXPECT_LE(premulDifference(actual, fresh.bitmap),
                type == kN32_SkColorType ? .016f : .004f);
      if (type != kN32_SkColorType) EXPECT_GT(actual.getColor4f(28, 22).fR, 1);
    }
  }
}

TEST(ComposeCaching, SharedBrushArtKeepsHdrThroughSpanAndPictureCaches) {
  for (const bool span : {false, true}) {
    SCOPED_TRACE(span);
    const auto makeArt = [] {
      return brush::artAlong(
          box()
              .width(40)
              .height(16)
              .cache(Cache::Texture)
              .shape(skiaShape([](SkSize size) {
                return SkPathBuilder().addOval(SkRect::MakeSize(size)).detach();
              }))
              .fill(hdrCacheSurface()),
          16);
    };
    const brush::Art shared = makeArt();
    const auto tree = [&](brush::Art art, Cache cache) {
      Element run = box()
                        .absolute()
                        .rect(12, 20.25f, 104, 40)
                        .shape(skiaShape([](SkSize size) {
                          return SkPathBuilder()
                              .moveTo(0, size.height() / 2)
                              .lineTo(size.width(), size.height() / 2)
                              .detach();
                        }));
      if (span)
        run.stroke(spans::every(1), std::move(art));
      else
        run.foreground(std::move(art));
      return box().width(128).height(80).cache(cache).children({run});
    };
    Host byteHost(128, 80), floatHost(128, 80);
    for (Host* host : {&byteHost, &floatHost}) {
      host->composer.setAutoTexturePromotion(false);
      host->composer.render(tree(shared, Cache::Picture));
    }
    const auto draw = [&](Host& host, SkColorType type) {
      host.surface = SkSurfaces::Raster(
          SkImageInfo::Make(128, 80, type, kPremul_SkAlphaType));
      EXPECT_TRUE(host.surface);
      if (!host.surface) return SkBitmap{};
      for (int frame = 0; frame < 4; ++frame)
        host.frame(0, SK_ColorTRANSPARENT);
      return host.pixels();
    };
    const SkBitmap byte = draw(byteHost, kN32_SkColorType);
    ASSERT_FALSE(byte.isNull());
    const SkBitmap firstFloat = draw(floatHost, kRGBA_F16_SkColorType);
    ASSERT_FALSE(firstFloat.isNull());
    EXPECT_EQ(shared.cache->image->colorType(), kRGBA_F16_SkColorType);
    EXPECT_GT(firstFloat.getColor4f(64, 40).fR, 1);
    byteHost.frame(0, SK_ColorTRANSPARENT);
    EXPECT_LE(premulDifference(byteHost.pixels(), byte), .001f);
    const SkBitmap switched = draw(byteHost, kRGBA_F32_SkColorType);
    ASSERT_FALSE(switched.isNull());
    EXPECT_EQ(shared.cache->image->colorType(), kRGBA_F32_SkColorType);
    floatHost.frame(0, SK_ColorTRANSPARENT);
    EXPECT_LE(premulDifference(floatHost.pixels(), firstFloat), .004f);
    const SkBitmap bytesAgain = draw(floatHost, kN32_SkColorType);
    ASSERT_FALSE(bytesAgain.isNull());
    EXPECT_LE(premulDifference(bytesAgain, byte), .001f);
    const test::Raster fresh =
        test::rasterize(tree(makeArt(), Cache::None), fonts(), {128, 80},
                        kRGBA_F32_SkColorType);
    ASSERT_TRUE(fresh.valid());
    EXPECT_LE(premulDifference(switched, fresh.bitmap), .004f);
    EXPECT_GT(switched.getColor4f(64, 40).fR, 1);
    int edges = 0;
    for (int y = 0; y < 80; ++y)
      for (int x = 0; x < 128; ++x) {
        const SkColor4f pixel = switched.getColor4f(x, y);
        edges += pixel.fA > .05f && pixel.fA < .95f && pixel.fR > 1;
      }
    EXPECT_GT(edges, 10);
  }
}

TEST(ComposeCaching,
     PixelBakesFollowDestinationProfileAndOpaqueFloatPrecision) {
  Host retained(128, 80);
  retained.composer.setAutoTexturePromotion(false);
  retained.composer.render(hdrCacheFigure(Cache::Texture));
  const auto draw = [&](const SkImageInfo& info) {
    retained.surface = SkSurfaces::Raster(info);
    EXPECT_TRUE(retained.surface);
    if (!retained.surface) return SkBitmap{};
    for (int frame = 0; frame < 4; ++frame)
      retained.frame(0, SK_ColorTRANSPARENT);
    return retained.pixels();
  };
  for (const auto& profile :
       {SkColorSpace::MakeSRGB(), SkColorSpace::MakeSRGBLinear()}) {
    const SkImageInfo info = SkImageInfo::Make(128, 80, kRGBA_F16_SkColorType,
                                               kPremul_SkAlphaType, profile);
    const SkBitmap actual = draw(info);
    ASSERT_FALSE(actual.isNull());
    Host fresh(128, 80);
    fresh.surface = SkSurfaces::Raster(info);
    ASSERT_TRUE(fresh.surface);
    fresh.composer.setAutoTexturePromotion(false);
    fresh.composer.render(hdrCacheFigure(Cache::None));
    fresh.frame(0, SK_ColorTRANSPARENT);
    EXPECT_LE(premulDifference(actual, fresh.pixels()), .004f);
  }
  const SkImageInfo sameProfile =
      SkImageInfo::Make(128, 80, kRGBA_F16_SkColorType, kPremul_SkAlphaType,
                        SkColorSpace::MakeSRGBLinear());
  retained.surface = SkSurfaces::Raster(sameProfile);
  ASSERT_TRUE(retained.surface);
  retained.frame(0, SK_ColorTRANSPARENT);
  EXPECT_EQ(retained.composer.stats().picturesRecorded, 0u);
  EXPECT_EQ(retained.composer.stats().texturesBaked, 0u);

  const SkImageInfo opaque = SkImageInfo::Make(
      128, 80, kRGB_F16F16F16x_SkColorType, kOpaque_SkAlphaType);
  const SkBitmap actual = draw(opaque);
  ASSERT_FALSE(actual.isNull());
  Host fresh(128, 80);
  fresh.surface = SkSurfaces::Raster(opaque);
  ASSERT_TRUE(fresh.surface);
  fresh.composer.setAutoTexturePromotion(false);
  fresh.composer.render(hdrCacheFigure(Cache::None));
  fresh.frame(0, SK_ColorTRANSPARENT);
  EXPECT_LE(premulDifference(actual, fresh.pixels()), .004f);
  EXPECT_GT(actual.getColor4f(28, 32).fR, 1);
  EXPECT_FLOAT_EQ(actual.getColor4f(2, 2).fR, 0);
}
