// Scene I — arbitrary SkPath exclusions (star / heart / donut hole). Any
// SkPath — concave stars, compound paths, cubic hearts — carves its exact
// silhouette out of the line bands, and even-odd holes stay open to text.
#include <include/core/SkCanvas.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkSurface.h>
#include <sigilmeasure/time/Stopwatch.h>

#include <cmath>
#include <cstdio>
#include <numbers>

#include "DemoScenes.h"
#include "DemoSupport.h"

using namespace sigil::weave;

void sceneShapes(FontContext& fontContext,
                 const std::filesystem::path& outputDirectory) {
  SkFontMgr* fontManager = fontContext.fontManager();
  TextStyle body = style(16.5f, kInk);
  body.shaping.typeface =
      fontManager->matchFamilyStyle("Noto Serif", SkFontStyle());

  Paragraph paragraph;
  for (int repetitionIndex = 0; repetitionIndex < 7; ++repetitionIndex)
    paragraph.appendText(
        u8"Text no longer flows around circles and boxes alone: any SkPath — "
        "concave stars, compound paths, cubic hearts — carves its exact "
        "silhouette out of the line bands, and even-odd holes stay open, so "
        "the paragraph pours right through the middle of the donut. ",
        body);

  // Star (concave, winding fill).
  SkPathBuilder star;
  for (int pointIndex = 0; pointIndex < 5; ++pointIndex) {
    const float angle = -std::numbers::pi_v<float> / 2.0f +
                        static_cast<float>(pointIndex) * 4.0f *
                            std::numbers::pi_v<float> / 5.0f;
    const SkPoint point = {215 + 135 * std::cos(angle),
                           230 + 135 * std::sin(angle)};
    if (pointIndex == 0)
      star.moveTo(point);
    else
      star.lineTo(point);
  }
  star.close();

  // Heart (two cubics).
  SkPathBuilder heart;
  heart.moveTo(700, 620);
  heart.cubicTo(540, 470, 590, 330, 700, 420);
  heart.cubicTo(810, 330, 860, 470, 700, 620);
  heart.close();

  // Donut (even-odd: the hole is open to text).
  SkPathBuilder donut;
  donut.addCircle(330, 660, 150);
  donut.addCircle(330, 660, 82);
  donut.setFillType(SkPathFillType::kEvenOdd);

  ExclusionFlow flow(SkRect::MakeXYWH(40, 40, 920, 800));
  const SkPath starPath = star.detach();
  const SkPath heartPath = heart.detach();
  const SkPath donutPath = donut.detach();
  flow.exclusions().push_back({silhouette::path(starPath), 10});
  flow.exclusions().push_back({silhouette::path(heartPath), 10});
  flow.exclusions().push_back({silhouette::path(donutPath), 8});
  flow.setMinimumIntervalWidth(46);

  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kJustify;
  options.lineMetrics.height = 26;

  const auto layoutStartTime = Clock::now();
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  const auto layoutEndTime = Clock::now();

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1000, 880));
  SkCanvas* canvas = surface->getCanvas();
  canvas->clear(kPaper);
  SkPaint shapePaint;
  shapePaint.setAntiAlias(true);
  shapePaint.setColor(kShape);
  for (const SkPath& shape : {starPath, heartPath, donutPath})
    canvas->drawPath(shape, shapePaint);
  layout.draw(canvas, paragraph);

  writePng(surface.get(), outputDirectory / "shapes.png");
  std::printf(
      "Scene I — SkPath exclusions written (layout %.1f us, %d "
      "lines)\n\n",
      toMicroseconds(layoutEndTime - layoutStartTime), layout.lineCount);
}
