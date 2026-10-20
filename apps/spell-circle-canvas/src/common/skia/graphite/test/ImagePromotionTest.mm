/** @file
 * Direct draws on Graphite and the images they promote to textures: a
 * draw reuses the texture the recorder's image provider made, a run of
 * sprites narrower than a pixel lays continuous tinted cover, a raster
 * image's texture goes with the image, each recorder owns its own
 * promotions, and a promotion uploads where no provider can supply it.
 */

#import <Metal/Metal.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkRect.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilskia/draw/Direct.h>
#include <sigilskia/graphite/GraphiteContext.h>

#include <algorithm>
#include <array>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "GraphiteReadback.h"
#include "MetalFixtures.h"

using sigil::skia::GraphiteContext;
using namespace sigil::skia::test;

namespace {

sk_sp<SkImage> whiteSheet() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(4, 4);
  bitmap.eraseColor(SK_ColorWHITE);
  bitmap.setImmutable();
  return bitmap.asImage();
}

class MissingImageProvider final : public skgpu::graphite::ImageProvider {
 public:
  sk_sp<SkImage> findOrCreate(skgpu::graphite::Recorder *, const SkImage *,
                              SkImage::RequiredProperties) override {
    ++calls;
    return nullptr;
  }
  int calls = 0;
};

}  // namespace

TEST(SigilSkiaGraphite, DirectDrawsReuseTheImageProvidersTexture) {
  SKIP_WITHOUT_METAL();
  GraphiteContext *ctx = graphite();
  const sk_sp<SkImage> sheet = whiteSheet();
  sk_sp<SkSurface> surface =
      SkSurfaces::RenderTarget(ctx->recorder(), SkImageInfo::MakeN32Premul(8, 8));
  ASSERT_NE(surface, nullptr);
  SkCanvas &canvas = *surface->getCanvas();
  canvas.clear(SK_ColorBLACK);
  canvas.drawImage(sheet, 0, 0);
  const sk_sp<SkImage> ordinary =
      ctx->recorder()->clientImageProvider()->findOrCreate(ctx->recorder(), sheet.get(), {});
  ASSERT_NE(ordinary, nullptr);
  EXPECT_TRUE(ordinary->isTextureBacked());
  EXPECT_EQ(sigil::skia::draw::ready(canvas, sheet), ordinary);
  EXPECT_EQ(sigil::skia::draw::ready(canvas, ordinary), ordinary);

  sigil::skia::draw::drawLattice(canvas, sheet, {1, 3}, {1, 3}, SkRect::MakeXYWH(4, 0, 4, 4),
                                 SkFilterMode::kNearest);
  const SkRSXform transform = SkRSXform::Make(1, 0, 0, 4);
  const SkRect sourceRectangles = SkRect::MakeWH(4, 4);
  sigil::skia::draw::drawSpriteAtlas(
      canvas, sheet, {.transforms = {&transform, 1}, .sourceRectangles = {&sourceRectangles, 1}},
      SkSamplingOptions());
  EXPECT_EQ(sigil::skia::draw::ready(canvas, sheet), ordinary);
  const SkBitmap pixels = readGraphiteSurface(*ctx, surface.get());
  ASSERT_FALSE(pixels.empty());
  EXPECT_EQ(pixels.getColor(1, 1), SK_ColorWHITE);
  EXPECT_EQ(pixels.getColor(5, 1), SK_ColorWHITE);
  EXPECT_EQ(pixels.getColor(1, 5), SK_ColorWHITE);
  EXPECT_EQ(pixels.getColor(5, 5), SK_ColorBLACK);
}

TEST(SigilSkiaGraphite, ARunOfSpritesNarrowerThanAPixelLaysContinuousTintedCover) {
  // The raster case's run, on the device: square sprites narrower than a
  // pixel, tinted, laid densely along a row at subpixel offsets. Every
  // column the run crosses takes some of its ink, about as much as its
  // neighbours, and the ink is the tint's.
  SKIP_WITHOUT_METAL();
  GraphiteContext *ctx = graphite();
  const sk_sp<SkImage> sheet = whiteSheet();
  const SkColor tint = SkColorSetARGB(255, 0x33, 0x66, 0xCC);
  for (const float width : {0.35f, 0.6f, 1.0f, 2.0f})
    for (const float offset : {0.0f, 0.5f}) {
      std::vector<SkRSXform> transforms;
      for (float x = 20.0f; x < 180.0f; x += width * 0.5f)
        transforms.push_back(
            SkRSXform::MakeFromRadians(width / 4.0f, 0.0f, x + offset, 20.0f + offset, 2.0f, 2.0f));
      const std::vector<SkRect> cells(transforms.size(), SkRect::MakeWH(4, 4));
      const std::vector<SkColor> colors(transforms.size(), tint);
      sk_sp<SkSurface> surface =
          SkSurfaces::RenderTarget(ctx->recorder(), SkImageInfo::MakeN32Premul(200, 40));
      ASSERT_NE(surface, nullptr);
      SkCanvas &canvas = *surface->getCanvas();
      canvas.clear(SK_ColorBLACK);
      sigil::skia::draw::drawSpriteAtlas(
          canvas, sheet, {.transforms = transforms, .sourceRectangles = cells, .colors = colors},
          SkSamplingOptions(SkFilterMode::kLinear));
      const SkBitmap pixels = readGraphiteSurface(*ctx, surface.get());
      ASSERT_FALSE(pixels.empty());
      int least = 1 << 30, most = 0;
      for (int x = 25; x < 175; ++x) {
        int column = 0;
        for (int y = 15; y <= 26; ++y) {
          const SkColor pixel = pixels.getColor(x, y);
          column += (int)SkColorGetB(pixel);
          EXPECT_GE(SkColorGetB(pixel), SkColorGetR(pixel)) << "the tint at " << x << "," << y;
        }
        least = std::min(least, column);
        most = std::max(most, column);
      }
      EXPECT_GT(least, 0) << "every column is inked: width " << width << " offset " << offset;
      EXPECT_GE(least * 2, most) << "and evenly: width " << width << " offset " << offset;
    }
}

TEST(SigilSkiaGraphite, ARasterImagesTextureGoesWithTheImage) {
  // A host that reads a picture back from another device draws a fresh
  // raster image every frame. The texture made for each one must go when
  // the image does, or the provider holds one picture-sized texture per
  // frame ever drawn.
  SKIP_WITHOUT_METAL();
  GraphiteContext *ctx = graphite();
  skgpu::graphite::ImageProvider *provider = ctx->recorder()->clientImageProvider();
  ASSERT_NE(provider, nullptr);
  sk_sp<SkImage> frame = whiteSheet();
  const sk_sp<SkImage> texture = provider->findOrCreate(ctx->recorder(), frame.get(), {});
  ASSERT_NE(texture, nullptr);
  EXPECT_FALSE(texture->unique()) << "the provider keeps what it made while the image lives";
  frame.reset();
  // The next request, for any image, is where the provider lets go.
  const sk_sp<SkImage> next = whiteSheet();
  ASSERT_NE(provider->findOrCreate(ctx->recorder(), next.get(), {}), nullptr);
  EXPECT_TRUE(texture->unique());
}

TEST(SigilSkiaGraphite, EachRecorderOwnsItsDirectImagePromotions) {
  SKIP_WITHOUT_METAL();
  GraphiteContext *ctx = graphite();
  const sk_sp<SkImage> sheet = whiteSheet();
  std::array<std::unique_ptr<skgpu::graphite::Recorder>, 2> recorders{ctx->makeRecorder(),
                                                                      ctx->makeRecorder()};
  std::array<sk_sp<SkImage>, 2> promoted;
  for (size_t i = 0; i < recorders.size(); ++i) {
    ASSERT_NE(recorders[i], nullptr);
    sk_sp<SkSurface> surface =
        SkSurfaces::RenderTarget(recorders[i].get(), SkImageInfo::MakeN32Premul(8, 8));
    ASSERT_NE(surface, nullptr);
    SkCanvas &canvas = *surface->getCanvas();
    promoted[i] = sigil::skia::draw::ready(canvas, sheet);
    ASSERT_NE(promoted[i], nullptr);
    EXPECT_TRUE(promoted[i]->isTextureBacked());
    EXPECT_EQ(sigil::skia::draw::ready(canvas, sheet), promoted[i]);
    canvas.drawImage(promoted[i], 0, 0);
    ASSERT_TRUE(submitRecorder(*ctx, *recorders[i]));
  }
  EXPECT_NE(promoted[0], promoted[1]);
  recorders[0].reset();
  sk_sp<SkSurface> surface =
      SkSurfaces::RenderTarget(recorders[1].get(), SkImageInfo::MakeN32Premul(8, 8));
  ASSERT_NE(surface, nullptr);
  EXPECT_EQ(sigil::skia::draw::ready(*surface->getCanvas(), sheet), promoted[1]);
  surface->getCanvas()->drawImage(promoted[1], 0, 0);
  EXPECT_TRUE(submitRecorder(*ctx, *recorders[1]));
}

TEST(SigilSkiaGraphite, DirectDrawsUploadOnADefaultRecorder) {
  SKIP_WITHOUT_METAL();
  GraphiteContext *ctx = graphite();
  auto recorder = ctx->context()->makeRecorder({});
  ASSERT_NE(recorder, nullptr);
  ASSERT_NE(recorder->clientImageProvider(), nullptr);
  sk_sp<SkSurface> surface =
      SkSurfaces::RenderTarget(recorder.get(), SkImageInfo::MakeN32Premul(8, 8));
  ASSERT_NE(surface, nullptr);
  const sk_sp<SkImage> sheet = whiteSheet();
  SkCanvas &canvas = *surface->getCanvas();
  const sk_sp<SkImage> promoted = sigil::skia::draw::ready(canvas, sheet);
  ASSERT_NE(promoted, nullptr);
  EXPECT_TRUE(promoted->isTextureBacked());
  canvas.clear(SK_ColorBLACK);
  sigil::skia::draw::drawLattice(canvas, sheet, {1, 3}, {1, 3}, SkRect::MakeXYWH(4, 0, 4, 4),
                                 SkFilterMode::kNearest);
  const SkRSXform transform = SkRSXform::Make(1, 0, 0, 4);
  const SkRect sourceRectangles = SkRect::MakeWH(4, 4);
  sigil::skia::draw::drawSpriteAtlas(
      canvas, sheet, {.transforms = {&transform, 1}, .sourceRectangles = {&sourceRectangles, 1}},
      SkSamplingOptions());
  ASSERT_TRUE(submitRecorder(*ctx, *recorder));
  const SkBitmap pixels = readGraphiteSurface(*ctx, surface.get());
  ASSERT_FALSE(pixels.empty());
  EXPECT_EQ(pixels.getColor(5, 1), SK_ColorWHITE);
  EXPECT_EQ(pixels.getColor(1, 5), SK_ColorWHITE);
  EXPECT_EQ(pixels.getColor(5, 5), SK_ColorBLACK);
}

TEST(SigilSkiaGraphite, DirectPromotionUploadsWhenTheImageProviderCannotSupplyIt) {
  SKIP_WITHOUT_METAL();
  GraphiteContext *ctx = graphite();
  const auto provider = sk_make_sp<MissingImageProvider>();
  skgpu::graphite::RecorderOptions options;
  options.fImageProvider = provider;
  auto recorder = ctx->context()->makeRecorder(options);
  ASSERT_NE(recorder, nullptr);
  sk_sp<SkSurface> surface =
      SkSurfaces::RenderTarget(recorder.get(), SkImageInfo::MakeN32Premul(8, 8));
  ASSERT_NE(surface, nullptr);
  const sk_sp<SkImage> sheet = whiteSheet();
  const sk_sp<SkImage> promoted = sigil::skia::draw::ready(*surface->getCanvas(), sheet);
  ASSERT_NE(promoted, nullptr);
  EXPECT_TRUE(promoted->isTextureBacked());
  EXPECT_EQ(provider->calls, 1);
  surface->getCanvas()->clear(SK_ColorBLACK);
  surface->getCanvas()->drawImage(promoted, 0, 0);
  ASSERT_TRUE(submitRecorder(*ctx, *recorder));
  const SkBitmap pixels = readGraphiteSurface(*ctx, surface.get());
  ASSERT_FALSE(pixels.empty());
  EXPECT_EQ(pixels.getColor(1, 1), SK_ColorWHITE);
  EXPECT_EQ(pixels.getColor(5, 5), SK_ColorBLACK);
}
