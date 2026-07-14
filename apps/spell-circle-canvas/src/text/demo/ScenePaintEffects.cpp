// Ordered paint-layer showcase: cheap presets plus arbitrary SkPaint passes.
#include "DemoScenes.h"
#include "DemoSupport.h"

#include <textflow/PaintShaders.h>

#include <include/core/SkBlendMode.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <include/core/SkTileMode.h>
#include <include/effects/SkGradient.h>

#include <array>
#include <cstdio>
#include <string_view>

using namespace textflow;

void scenePaintEffects(FontContext &fontContext,
                       const std::filesystem::path &outputDirectory) {
  std::printf("Scene L — ordered paint layers (presets + arbitrary SkPaint)\n");

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1160, 720));
  SkCanvas *canvas = surface->getCanvas();
  canvas->clear(kPaper);

  const SkPoint gradientPoints[2] = {{310, 0}, {980, 0}};
  const SkColor4f gradientColors[3] = {
      SkColor4f::FromColor(0x001A66FF),
      SkColor4f::FromColor(0xAAFFFFFF),
      SkColor4f::FromColor(0x00FFFFFF)};
  const sk_sp<SkShader> gradient = SkShaders::LinearGradient(
      gradientPoints,
      SkGradient(SkGradient::Colors({gradientColors, 3}, SkTileMode::kMirror),
                 SkGradient::Interpolation()));

  std::array<PaintStyle, 5> paints;

  // Complete SkPaint foreground: solid color is merely its simplest form.
  SkPaint solid;
  solid.setAntiAlias(true);
  solid.setColor(kInk);
  paints[0] = PaintStyle(std::move(solid));

  paints[1] = PaintStyle(kInk);
  paints[1].addUnderlay(PaintLayer::dropShadow(0x77000000, {5, 6}, 3.5f));

  paints[2] = PaintStyle(kBlue);
  paints[2].addUnderlay(PaintLayer::glow(0x8892C7FF, 7.0f));

  paints[3] = PaintStyle(SK_ColorWHITE);
  paints[3].foreground.setShader(
      PaintShaders::water(SkRect::MakeWH(1160, 720), 1.7f));
  paints[3].addUnderlay(PaintLayer::outline(0xFF5A1E17, 7.0f));

  // Presets and caller-authored layers compose in the same ordered lists.
  paints[4] = PaintStyle(SK_ColorWHITE);
  paints[4].foreground.setShader(
      PaintShaders::meshGradient(SkRect::MakeWH(1160, 720), 1.7f));
  paints[4]
      .addUnderlay(PaintLayer::dropShadow(0x77000000, {6, 7}, 4.5f))
      .addUnderlay(PaintLayer::glow(0x6692C7FF, 8.0f))
      .addUnderlay(PaintLayer::outline(kInk, 9.0f));
  SkPaint stars;
  stars.setAntiAlias(true);
  stars.setShader(
      PaintShaders::starField(SkRect::MakeWH(1160, 720), 1.7f));
  stars.setBlendMode(SkBlendMode::kScreen);
  paints[4].addOverlay(PaintLayer(std::move(stars)));
  SkPaint sheen;
  sheen.setAntiAlias(true);
  sheen.setShader(gradient);
  sheen.setBlendMode(SkBlendMode::kScreen);
  paints[4].addOverlay(PaintLayer(std::move(sheen)));

  const std::array<std::u8string_view, 5> labels = {
      u8"foreground SkPaint · 1 draw", u8"dropShadow() underlay · 2 draws",
      u8"glow() underlay · 2 draws", u8"outline() + water shader · 2 draws",
      u8"mesh + stars + gradient composite · 6 draws"};

  int totalRuns = 0;
  for (size_t row = 0; row < paints.size(); ++row) {
    const float baseline = 122.0f + static_cast<float>(row) * 125.0f;
    Paragraph caption;
    caption.appendText(labels[row], style(13, kBlue));
    layoutSingleLine(fontContext, caption, {34, baseline - 34})
        .draw(canvas, caption);

    TextStyle sampleStyle = style(58, kInk);
    sampleStyle.paint = paints[row];
    Paragraph sample;
    sample.appendText(u8"IFRIT · 文字", sampleStyle);
    ParagraphLayout layout =
        layoutSingleLine(fontContext, sample, {310, baseline});
    totalRuns += static_cast<int>(layout.runs.size());
    layout.drawBatched(canvas, sample);
  }

  Paragraph note;
  note.appendText(
      u8"Underlays draw back-to-front, then the foreground, then overlays. "
      u8"Replace either shader every frame with setPaint(): layout and "
      u8"HarfBuzz stay untouched.",
      style(14, kInk));
  BlockFlow noteFlow(SkRect::MakeXYWH(34, 660, 1090, 45));
  layoutParagraph(fontContext, note, noteFlow).draw(canvas, note);

  std::printf("  %d positioned runs; layer count is the predictable draw "
              "multiplier (filters add backend work)\n",
              totalRuns);
  writePng(surface.get(), outputDirectory / "paint_layers.png");

  // The scale test deliberately places the entire paragraph rather than
  // relying on overflow clipping. The same four-pass style covers thousands
  // of words and drawBatched still submits one draw per font/style/layer.
  constexpr int kStressWordCount = 2500;
  sk_sp<SkSurface> stressSurface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1600, 1000));
  SkCanvas *stressCanvas = stressSurface->getCanvas();
  stressCanvas->clear(0xFF050A18);
  const SkRect stressBounds = SkRect::MakeXYWH(34, 56, 1532, 914);

  PaintStyle stressPaint(SK_ColorWHITE);
  stressPaint
      .addUnderlay(PaintLayer::glow(0x882A77FF, 2.4f))
      .addUnderlay(PaintLayer::outline(0xFF061229, 0.9f));
  stressPaint.foreground.setShader(
      PaintShaders::water(stressBounds, 2.4f));
  SkPaint stressStars;
  stressStars.setAntiAlias(true);
  stressStars.setShader(PaintShaders::starField(stressBounds, 2.4f));
  stressStars.setBlendMode(SkBlendMode::kScreen);
  stressPaint.addOverlay(PaintLayer(std::move(stressStars)));

  Paragraph stressParagraph = makeBigParagraph(kStressWordCount, 12.2f);
  stressParagraph.setPaint(
      0, static_cast<uint32_t>(stressParagraph.text().size()), stressPaint);
  BlockFlow stressFlow(stressBounds);
  ParagraphLayoutOptions stressOptions;
  stressOptions.alignment = TextAlignment::kJustify;
  stressOptions.lineMetrics.height = 15.0f;
  ParagraphLayout stressLayout = layoutParagraph(
      fontContext, stressParagraph, stressFlow, stressOptions);
  stressLayout.drawBatched(stressCanvas, stressParagraph);

  Paragraph stressCaption;
  stressCaption.appendText(
      u8"2,500 words · four passes · water runtime shader + tiled vector stars",
      style(13, 0xFFD7E7FF));
  layoutSingleLine(fontContext, stressCaption, {34, 30})
      .draw(stressCanvas, stressCaption);

  size_t stressGlyphCount = 0;
  for (const PositionedRun &run : stressLayout.runs)
    if (run.shaped)
      stressGlyphCount += run.shaped->glyphs.size();
  std::printf("  stress: %d words, %zu glyphs, four passes, %s\n",
              kStressWordCount, stressGlyphCount,
              stressLayout.overflowed() ? "overflowed" : "all placed");
  writePng(stressSurface.get(), outputDirectory / "paint_stress.png");
}
