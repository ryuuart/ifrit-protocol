#import "SCKEngineInternal.h"

#include <sigilskia/graphite/OffscreenSurface.h>
#include <sigilskia/graphite/TextureImage.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRect.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkTileMode.h>
#include <include/effects/SkGradient.h>
#include <include/effects/SkImageFilters.h>
#include <include/gpu/graphite/Image.h>

#include <utility>

namespace {

constexpr BlitPalette kDarkPalette = {
    SkColorSetRGB(0x0d, 0x0d, 0x10),
    SkColorSetRGB(0x1a, 0x1a, 0x20),
    SkColorSetRGB(0x2e, 0x2e, 0x38),
    SkColorSetARGB(0x40, 0xff, 0xff, 0xff),
};

constexpr BlitPalette kLightPalette = {
    SkColorSetRGB(0xd6, 0xd6, 0xda),
    SkColorSetRGB(0xcc, 0xcc, 0xcc),
    SkColorSetRGB(0x99, 0x99, 0x99),
    SkColorSetARGB(0x40, 0x00, 0x00, 0x00),
};

// Checkerboard cell size in drawable pixels, screen-fixed so it doesn't
// scale with zoom.
constexpr int kCheckerCellPixels = 16;

}  // namespace

// The on-screen half: the appearance-matched chrome the scene texture is
// placed on inside a window layer — backdrop, transparency plate, canvas
// border and the progressive top-edge blur — and the present that ends it.
@implementation SCKEngine (Blit)

- (const BlitPalette &)blitPalette {
  return _darkAppearance ? kDarkPalette : kLightPalette;
}

- (sk_sp<SkShader>)checkerShader {
  if (!_checkerShader) {
    const BlitPalette &palette = [self blitPalette];
    SkBitmap bitmap;
    bitmap.allocN32Pixels(kCheckerCellPixels * 2, kCheckerCellPixels * 2,
                          /*isOpaque=*/true);
    SkCanvas cells(bitmap);
    cells.clear(palette.checkerBase);
    SkPaint light;
    light.setColor(palette.checkerAlt);
    cells.drawRect(SkRect::MakeXYWH(0, 0, kCheckerCellPixels, kCheckerCellPixels), light);
    cells.drawRect(SkRect::MakeXYWH(kCheckerCellPixels, kCheckerCellPixels, kCheckerCellPixels,
                                    kCheckerCellPixels),
                   light);
    bitmap.setImmutable();
    // workaround: Graphite draws nothing, and reports nothing, from a
    // shader over a raster-backed image; the tile has to reach the GPU as a
    // texture before it becomes a shader.
    sk_sp<SkImage> tile = bitmap.asImage();
    if (sk_sp<SkImage> uploaded = SkImages::TextureFromImage(_graphite->recorder(), tile.get(), {}))
      tile = std::move(uploaded);
    _checkerShader = tile->makeShader(SkTileMode::kRepeat, SkTileMode::kRepeat,
                                      SkSamplingOptions(SkFilterMode::kNearest));
  }
  return _checkerShader;
}

- (void)drawInLayer:(CAMetalLayer *)layer
              scale:(double)scale
            centerX:(double)centerX
            centerY:(double)centerY
           topInset:(double)topInsetPixels {
  if (!_graphite || !layer || scale <= 0) return;

  id<CAMetalDrawable> drawable = [layer nextDrawable];
  if (!drawable) return;

  id<MTLTexture> target = drawable.texture;
  const int width = static_cast<int>(target.width);
  const int height = static_cast<int>(target.height);

  sigil::skia::OffscreenSurface surface(*_graphite, (__bridge void *)target, width, height);
  SkCanvas *canvas = surface.canvas();
  if (!canvas) return;

  canvas->clear([self blitPalette].backdrop);

  // The view owns the whole transform (fit region, zoom, pan): this method
  // just places the canvas rectangle where it was told to.
  const double drawWidth = _canvasWidth * scale;
  const double drawHeight = _canvasHeight * scale;
  const SkRect destination = SkRect::MakeXYWH(
      static_cast<float>(centerX - drawWidth / 2.0), static_cast<float>(centerY - drawHeight / 2.0),
      static_cast<float>(drawWidth), static_cast<float>(drawHeight));

  // Transparency checkerboard as the canvas plate: shows the publishable
  // extent and reads transparent scene regions as alpha, like the Qt
  // viewport. Tiled in device space, so cells stay screen-sized while
  // zooming.
  SkPaint platePaint;
  platePaint.setShader([self checkerShader]);
  canvas->drawRect(destination, platePaint);

  if (_sceneTexture && _graphite->recorder()) {
    sk_sp<SkImage> sceneImage = sigil::skia::wrapImage(
        *_graphite->recorder(), (__bridge void *)_sceneTexture,
        static_cast<int>(_sceneTexture.width), static_cast<int>(_sceneTexture.height));
    if (sceneImage) {
      SkPaint imagePaint;
      imagePaint.setAntiAlias(true);
      canvas->drawImageRect(sceneImage, destination,
                            SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kNone),
                            &imagePaint);
    }
  }

  SkPaint borderPaint;
  borderPaint.setAntiAlias(true);
  borderPaint.setStyle(SkPaint::kStroke_Style);
  borderPaint.setStrokeWidth(1.0f);
  borderPaint.setColor([self blitPalette].canvasBorder);
  canvas->drawRect(destination, borderPaint);

  // Progressive top-edge blur emulating the system toolbar scroll-edge
  // effect: macOS 26 only composes that effect over chrome-managed
  // scrollable content, and this canvas is deliberately a window backdrop
  // outside the chrome — so the blit draws its own. Backdrop-blur bands of
  // decreasing sigma feathering out below the bar, plus a faint backdrop
  // tint for toolbar-item legibility.
  if (topInsetPixels > 1.0) {
    const float blurBottom = static_cast<float>(topInsetPixels) * 1.5f;
    constexpr int kBandCount = 5;
    constexpr float kMaxSigma = 13.0f;
    for (int band = 0; band < kBandCount; ++band) {
      const float y0 = blurBottom * band / kBandCount;
      const float y1 = blurBottom * (band + 1) / kBandCount;
      const float remaining = 1.0f - static_cast<float>(band) / kBandCount;
      const float sigma = kMaxSigma * remaining * remaining;
      if (sigma < 0.4f) continue;
      sk_sp<SkImageFilter> bandBlur = SkImageFilters::Blur(sigma, sigma, nullptr);
      canvas->save();
      canvas->clipRect(SkRect::MakeXYWH(0, y0, static_cast<float>(width), y1 - y0));
      SkCanvas::SaveLayerRec blurLayer(nullptr, nullptr, bandBlur.get(), 0);
      canvas->saveLayer(blurLayer);
      canvas->restore();  // composite the backdrop-blurred layer
      canvas->restore();  // drop the band clip
    }

    const SkColor backdropColor = [self blitPalette].backdrop;
    const SkPoint tintSpan[2] = {{0.0f, 0.0f}, {0.0f, blurBottom}};
    const SkColor4f tintColors[2] = {SkColor4f::FromColor(SkColorSetA(backdropColor, 0x55)),
                                     SkColor4f::FromColor(SkColorSetA(backdropColor, 0x00))};
    SkPaint tint;
    tint.setShader(SkShaders::LinearGradient(
        tintSpan, SkGradient(SkGradient::Colors(SkSpan(tintColors, 2), SkTileMode::kClamp),
                             SkGradient::Interpolation())));
    canvas->drawRect(SkRect::MakeXYWH(0, 0, static_cast<float>(width), blurBottom), tint);
  }

  surface.submit();

  // Present after Graphite's queued work: command buffers on one queue
  // execute in commit order. During transaction-bound presentation (the
  // view sets presentsWithTransaction for animated resizes) the present
  // must happen on this thread after the work is scheduled, so the
  // in-flight CoreAnimation commit picks the drawable up.
  id<MTLCommandBuffer> commandBuffer = [_queue commandBuffer];
  if (layer.presentsWithTransaction) {
    [commandBuffer commit];
    [commandBuffer waitUntilScheduled];
    [drawable present];
  } else {
    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];
  }
}

@end
