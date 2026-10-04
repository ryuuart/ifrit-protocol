// How a group bake settles: a picture holding a group revisits it until
// its pixels can be held, a moving parent settles on one local bake, and a
// group refused a bake, or held inside another pixel bake, asks for no
// further draw.

#include <vector>

#include "support/CoreTestSupport.h"

TEST(ComposeCaching, PictureRevisitsASettlingGroupThenHoldsItsPixels) {
  for (const float density : {0.0f, 1.0f}) {
    SCOPED_TRACE(density);
    Host host(64, 64);
    host.composer.setAutoTexturePromotion(false);
    host.composer.setBakeDensity(density);
    const auto tree = [](Fill fill) {
      return box()
          .width(64)
          .height(64)
          .cache(Cache::Picture)
          .children({box()
                         .width(64)
                         .height(64)
                         .cache(Cache::Picture)
                         .children({box()
                                        .absolute()
                                        .rect(10, 10, 20, 20)
                                        .cache(Cache::Group)
                                        .fill(fill)})});
    };
    for (const Fill fill : {green(), red()}) {
      host.composer.render(tree(fill));
      host.frame(0, SK_ColorTRANSPARENT);
      EXPECT_EQ(host.composer.stats().texturesBaked, 0u);
      ASSERT_TRUE(host.composer.isRunning());
      host.frame(0, SK_ColorTRANSPARENT);
      EXPECT_EQ(host.composer.stats().texturesBaked, 1u);
      EXPECT_FALSE(host.composer.isRunning());
      const SkColor expected = host.pixel(20, 20);
      EXPECT_EQ(expected, fill == green() ? SK_ColorGREEN : SK_ColorRED);
      for (int frame = 0; frame < 3; ++frame) {
        host.frame(0, SK_ColorTRANSPARENT);
        EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
        EXPECT_EQ(host.composer.stats().texturesBaked, 0u);
        EXPECT_GE(host.composer.stats().texturesLive, 1u);
        EXPECT_EQ(host.pixel(20, 20), expected);
        EXPECT_FALSE(host.composer.isRunning());
      }
    }
  }
}

TEST(ComposeCaching, MovingPictureParentSettlesOneLocalGroupBake) {
  Host host(64, 64);
  host.composer.setAutoTexturePromotion(false);
  motion::Animatable<float> drift = motion::animatable(0.0f);
  host.composer.render(box()
                           .width(64)
                           .height(64)
                           .cache(Cache::Picture)
                           .translateX(drift)
                           .children({box()
                                          .absolute()
                                          .rect(10, 10, 20, 20)
                                          .cache(Cache::Group)
                                          .fill(green())}));
  host.frame(0, SK_ColorTRANSPARENT);
  EXPECT_EQ(host.composer.stats().texturesBaked, 0u);
  drift = .25f;
  host.frame(0, SK_ColorTRANSPARENT);
  ASSERT_EQ(host.composer.stats().texturesBaked, 1u);
  for (const float x : {.5f, 1.25f, 2.5f, 3.75f}) {
    drift = x;
    host.frame(0, SK_ColorTRANSPARENT);
    EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
    EXPECT_EQ(host.composer.stats().texturesBaked, 0u);
    EXPECT_GE(host.composer.stats().texturesLive, 1u);
    EXPECT_EQ(host.pixel(20, 20), SK_ColorGREEN);
    EXPECT_EQ(host.pixel(5, 5), SK_ColorTRANSPARENT);
  }
}

TEST(ComposeCaching, RefusedGroupsDoNotRequestSettlingDraws) {
  const std::vector<Element> refused{
      box().width(0).height(20).cache(Cache::Group).fill(green()),
      box().absolute().rect(0, 0, 5000, 5000).cache(Cache::Group).fill(green()),
      box().absolute().rect(0, 0, 3800, 3800).cache(Cache::Group).fill(green()),
      box()
          .width(40)
          .height(40)
          .cache(Cache::Group)
          .children({box().width(20).height(20).fill(green()).blendMode(
              material::BlendMode::Multiply)})};
  for (size_t i = 0; i < refused.size(); ++i) {
    SCOPED_TRACE(i);
    Host host(64, 64);
    host.surface = SkSurfaces::Raster(
        SkImageInfo::Make(64, 64, kRGBA_F32_SkColorType, kPremul_SkAlphaType));
    ASSERT_TRUE(host.surface);
    host.composer.setAutoTexturePromotion(false);
    host.composer.setBakeDensity(1);
    host.composer.render(box()
                             .width(64)
                             .height(64)
                             .cache(Cache::Picture)
                             .children({refused[i]}));
    for (int frame = 0; frame < 4; ++frame) {
      host.frame(0, SK_ColorTRANSPARENT);
      EXPECT_EQ(host.composer.stats().texturesBaked, 0u);
      EXPECT_FALSE(host.composer.isRunning());
      if (frame > 0) EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
    }
  }
}

TEST(ComposeCaching, PixelBakeDoesNotKeepAnEnclosedGroupPending) {
  Host host(64, 64);
  host.composer.setAutoTexturePromotion(false);
  host.composer.render(
      box()
          .width(64)
          .height(64)
          .cache(Cache::Texture)
          .children(
              {box().width(40).height(40).cache(Cache::Group).fill(green())}));
  host.frame(0, SK_ColorTRANSPARENT);
  ASSERT_EQ(host.composer.stats().texturesBaked, 1u);
  EXPECT_FALSE(host.composer.isRunning());
  host.frame(0, SK_ColorTRANSPARENT);
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  EXPECT_EQ(host.composer.stats().texturesBaked, 0u);
  EXPECT_FALSE(host.composer.isRunning());
  EXPECT_EQ(host.pixel(20, 20), SK_ColorGREEN);
}
