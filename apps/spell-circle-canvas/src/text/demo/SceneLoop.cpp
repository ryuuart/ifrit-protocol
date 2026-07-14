// Scene F(c) — infinite loop: text marches forever around a closed
// figure-eight. The closed contour wraps arc positions, so animating
// contourStart is the whole effect.
#include "DemoScenes.h"
#include "DemoSupport.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkContourMeasure.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkSurface.h>

#include <cmath>
#include <cstdio>
#include <numbers>

using namespace textflow;

void sceneLoop(FontContext &fontContext, int frames,
               const std::filesystem::path &outputDirectory) {
  SkPathBuilder pathBuilder;
  const SkPoint center = {450, 280};
  const int steps = 400;
  for (int stepIndex = 0; stepIndex <= steps; ++stepIndex) {
    const float angle = static_cast<float>(stepIndex) / steps * 2.0f *
                        std::numbers::pi_v<float>;
    const SkPoint point = {center.fX + 340 * std::sin(angle),
                           center.fY + 420 * std::sin(angle) * std::cos(angle)};
    if (stepIndex == 0)
      pathBuilder.moveTo(point);
    else
      pathBuilder.lineTo(point);
  }
  pathBuilder.close();
  const SkPath eight = pathBuilder.detach();

  SkContourMeasureIter contourIterator(eight, false);
  sk_sp<SkContourMeasure> contour = contourIterator.next();
  const float loopLength = contour->length();

  Paragraph paragraph;
  paragraph.appendText(u8"and the words go round ", style(21, kInk));
  paragraph.appendText(u8"終わらない文字の環 ", style(21, kBlue, "ja"));
  paragraph.appendText(u8"끝나지 않는 글의 고리 ", style(21, kAccent, "ko"));
  paragraph.appendText(u8"文字環繞不息 ", style(21, kInk, "zh"));
  paragraph.appendText(u8"round and round again — ", style(21, kInk));

  LineSetFlow flow;
  LineInterval interval;
  interval.contour = contour;
  interval.length = loopLength;
  flow.lines().push_back({interval});

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(900, 560));
  SkCanvas *canvas = surface->getCanvas();

  SkPaint rail;
  rail.setAntiAlias(true);
  rail.setStyle(SkPaint::kStroke_Style);
  rail.setStrokeWidth(1.2f);
  rail.setColor(0x2A23252B);

  TimingStats layoutTime, drawTime;
  for (int frame = 0; frame < frames; ++frame) {
    const auto layoutStartTime = Clock::now();
    flow.lines()[0][0].contourStart =
        std::fmod(static_cast<float>(frame) * 3.1f, loopLength);
    ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);
    const auto layoutEndTime = Clock::now();
    canvas->clear(kPaper);
    canvas->drawPath(eight, rail);
    layout.draw(canvas, paragraph);
    drawTime.add(toMicroseconds(Clock::now() - layoutEndTime));
    layoutTime.add(toMicroseconds(layoutEndTime - layoutStartTime));

    if (frame == frames / 4 || frame == frames - 1) {
      char name[64];
      std::snprintf(name, sizeof name, "loop_f%03d.png", frame);
      writePng(surface.get(), outputDirectory / name);
    }
  }
  layoutTime.report("infinite loop layout");
  drawTime.report("infinite loop draw (raster)");
}
