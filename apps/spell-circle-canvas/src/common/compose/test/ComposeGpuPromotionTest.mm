// Graphite cases for automatic promotion: a bake taken under a picture, a
// scratch surface reused across backends and a bake nested in a local
// texture each paint for the destination their pixels are blitted into.

#include "support/GpuTestSupport.h"

using namespace sigil::compose::graphiteTesting;

namespace {

struct PaintBackends {
  int graphite = 0;
  int raster = 0;
  int recording = 0;
};

struct BackendMark {
  std::shared_ptr<PaintBackends> painted;

  bool operator==(const BackendMark &) const = default;

  void paint(sigil::draw::Pen &pen, const PaintContext &context) const {
    SkCanvas &canvas = *pen.canvas();
    SkPixmap pixels;
    if (canvas.recorder())
      ++painted->graphite;
    else if (canvas.peekPixels(&pixels))
      ++painted->raster;
    else
      ++painted->recording;
    SkPaint paint;
    paint.setColor(SK_ColorWHITE);
    canvas.drawRect(SkRect::MakeWH(context.size.x, context.size.y), paint);
  }
};

Element markUnderPicture(const std::shared_ptr<PaintBackends> &painted, const char *key) {
  return box()
      .cache(Cache::None)
      .children({box()
                     .absolute()
                     .left(12)
                     .top(12)
                     .width(96)
                     .height(80)
                     .overflow(Overflow::Clip)
                     .cache(Cache::Picture)
                     .children({box()
                                    .key(key)
                                    .absolute()
                                    .left(8)
                                    .top(8)
                                    .width(108)
                                    .height(60)
                                    .cache(Cache::Auto)
                                    .foreground(BackendMark{painted})})});
}

}  // namespace

TEST(ComposeGpu, PromotionUnderAPictureKeepsTheGraphiteDestination) {
  REQUIRE_GPU();
  constexpr int width = 128, height = 112;
  sigil::motion::Engine offEngine;
  Composer off(offEngine, fonts());
  off.setSize({width, height});
  off.setAutoTexturePromotion(Composer::PromotionPolicy::Off);
  const auto offPainted = std::make_shared<PaintBackends>();
  off.render(markUnderPicture(offPainted, "mark"));
  ASSERT_FALSE(drawOnGpu(off, width, height).empty());
  EXPECT_EQ(off.stats().texturesBaked, 0u);
  const SkBitmap expected = drawOnGpu(off, width, height);
  ASSERT_FALSE(expected.empty());
  EXPECT_EQ(off.stats().texturesBaked, 0u);
  EXPECT_GT(offPainted->recording, 0);
  EXPECT_EQ(offPainted->raster, 0);
  ASSERT_EQ(expected.getColor(32, 32), SK_ColorWHITE);
  ASSERT_EQ(expected.getColor(112, 32), SK_ColorBLACK);

  sigil::motion::Engine eagerEngine;
  Composer eager(eagerEngine, fonts());
  eager.setSize({width, height});
  eager.setAutoTexturePromotion(Composer::PromotionPolicy::Eager);
  const auto eagerPainted = std::make_shared<PaintBackends>();
  eager.render(markUnderPicture(eagerPainted, "mark"));
  ASSERT_FALSE(drawOnGpu(eager, width, height).empty());
  const size_t firstBakes = eager.stats().texturesBaked;
  const SkBitmap actual = drawOnGpu(eager, width, height);
  ASSERT_FALSE(actual.empty());
  ASSERT_GT(firstBakes + eager.stats().texturesBaked, 0u);
  EXPECT_GT(eagerPainted->graphite, 0);
  EXPECT_EQ(eagerPainted->raster, 0);
  EXPECT_EQ(mismatchedPixels(expected, actual), 0u);
}

TEST(ComposeGpu, PromotionScratchFollowsTheDestinationBackend) {
  REQUIRE_GPU();
  constexpr int width = 128, height = 112;
  sigil::motion::Engine engine;
  Composer composer(engine, fonts());
  composer.setSize({width, height});
  composer.setAutoTexturePromotion(Composer::PromotionPolicy::Eager);

  const auto firstRasterPaint = std::make_shared<PaintBackends>();
  composer.render(markUnderPicture(firstRasterPaint, "raster-first"));
  ASSERT_FALSE(drawOnRaster(composer, width, height).empty());
  const size_t firstRasterBakes = composer.stats().texturesBaked;
  const SkBitmap expected = drawOnRaster(composer, width, height);
  ASSERT_FALSE(expected.empty());
  ASSERT_GT(firstRasterBakes + composer.stats().texturesBaked, 0u);
  EXPECT_GT(firstRasterPaint->raster, 0);
  EXPECT_EQ(firstRasterPaint->graphite, 0);
  ASSERT_EQ(expected.getColor(32, 32), SK_ColorWHITE);
  ASSERT_EQ(expected.getColor(112, 32), SK_ColorBLACK);

  // A new child forces a fresh bake while the scratch allocation is held.
  const auto graphitePaint = std::make_shared<PaintBackends>();
  composer.render(markUnderPicture(graphitePaint, "graphite"));
  ASSERT_FALSE(drawOnGpu(composer, width, height).empty());
  const size_t firstGraphiteBakes = composer.stats().texturesBaked;
  const SkBitmap gpu = drawOnGpu(composer, width, height);
  ASSERT_FALSE(gpu.empty());
  ASSERT_GT(firstGraphiteBakes + composer.stats().texturesBaked, 0u);
  EXPECT_GT(graphitePaint->graphite, 0);
  EXPECT_EQ(graphitePaint->raster, 0);
  EXPECT_EQ(mismatchedPixels(expected, gpu), 0u);

  const auto lastRasterPaint = std::make_shared<PaintBackends>();
  composer.render(markUnderPicture(lastRasterPaint, "raster-last"));
  ASSERT_FALSE(drawOnRaster(composer, width, height).empty());
  const size_t lastRasterBakes = composer.stats().texturesBaked;
  const SkBitmap raster = drawOnRaster(composer, width, height);
  ASSERT_FALSE(raster.empty());
  ASSERT_GT(lastRasterBakes + composer.stats().texturesBaked, 0u);
  EXPECT_GT(lastRasterPaint->raster, 0);
  EXPECT_EQ(lastRasterPaint->graphite, 0);
  EXPECT_EQ(mismatchedPixels(expected, raster), 0u);
}

TEST(ComposeGpu, PromotionInsideALocalTextureKeepsTheRasterDestination) {
  REQUIRE_GPU();
  constexpr int width = 128, height = 112;
  const auto tree = [](Cache childCache, const std::shared_ptr<PaintBackends> &painted) {
    return box()
        .cache(Cache::None)
        .children({box()
                       .absolute()
                       .left(8)
                       .top(8)
                       .width(112)
                       .height(96)
                       .cache(Cache::Picture)
                       .children({box()
                                      .absolute()
                                      .left(8)
                                      .top(8)
                                      .width(96)
                                      .height(80)
                                      .overflow(Overflow::Clip)
                                      .cache(Cache::Texture)
                                      .cacheScale(0.5f)
                                      .children({box()
                                                     .key("mark")
                                                     .absolute()
                                                     .left(8)
                                                     .top(8)
                                                     .width(108)
                                                     .height(60)
                                                     .cache(childCache)
                                                     .foreground(BackendMark{painted})})})});
  };

  sigil::motion::Engine referenceEngine;
  Composer reference(referenceEngine, fonts());
  reference.setSize({width, height});
  reference.setAutoTexturePromotion(Composer::PromotionPolicy::Off);
  const auto referencePaint = std::make_shared<PaintBackends>();
  // Both paths share the half-density outer bake; only the child takes
  // pixels instead of recording its draw commands.
  reference.render(tree(Cache::Picture, referencePaint));
  const SkBitmap firstExpected = drawOnGpu(reference, width, height);
  ASSERT_FALSE(firstExpected.empty());
  EXPECT_EQ(reference.stats().texturesBaked, 1u);
  const SkBitmap expected = drawOnGpu(reference, width, height);
  ASSERT_FALSE(expected.empty());
  EXPECT_EQ(reference.stats().texturesBaked, 0u);
  EXPECT_GT(referencePaint->recording, 0);
  EXPECT_EQ(referencePaint->raster, 0);
  EXPECT_EQ(referencePaint->graphite, 0);
  ASSERT_EQ(expected.getColor(40, 40), SK_ColorWHITE);
  ASSERT_EQ(expected.getColor(116, 40), SK_ColorBLACK);
  EXPECT_EQ(mismatchedPixels(firstExpected, expected), 0u);

  sigil::motion::Engine nestedEngine;
  Composer nested(nestedEngine, fonts());
  nested.setSize({width, height});
  nested.setAutoTexturePromotion(Composer::PromotionPolicy::Off);
  const auto nestedPaint = std::make_shared<PaintBackends>();
  nested.render(tree(Cache::Texture, nestedPaint));
  const SkBitmap firstActual = drawOnGpu(nested, width, height);
  ASSERT_FALSE(firstActual.empty());
  EXPECT_GE(nested.stats().texturesBaked, 2u);
  EXPECT_GT(nestedPaint->raster, 0);
  EXPECT_EQ(nestedPaint->graphite, 0);
  EXPECT_EQ(mismatchedPixels(expected, firstActual), 0u);
  const int paintsAfterBake = nestedPaint->raster;
  const SkBitmap retained = drawOnGpu(nested, width, height);
  ASSERT_FALSE(retained.empty());
  EXPECT_EQ(nested.stats().texturesBaked, 0u);
  EXPECT_EQ(nestedPaint->raster, paintsAfterBake);
  EXPECT_EQ(mismatchedPixels(expected, retained), 0u);
}
