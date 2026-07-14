// Scene A — mixed-script rich paragraph reflowing around moving exclusion
// shapes. The "Chrome rich text" acceptance test: full relayout every frame
// while words are occasionally re-styled and re-written.
#include "DemoScenes.h"
#include "DemoSupport.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkSurface.h>

#include <cmath>
#include <cstdio>
#include <string>

using namespace textflow;

void sceneExclusions(FontContext &fontContext, int frames,
                     const std::filesystem::path &outputDirectory) {
  std::printf("Scene A — mixed-script paragraph reflowing around moving "
              "shapes\n");

  ParagraphBuilder builder(style(17));
  builder.addText(u8"Typography is the craft of arranging type. ");
  builder.pushStyle(style(22, kAccent)).addText(u8"Glyphs flow").popStyle();
  builder.addText(u8" around obstacles the way water flows around stones. ");
  builder.pushStyle(style(19, kBlue, "ja"))
      .addText(u8"日本語のテキストも同じ流れに乗って、")
      .popStyle();
  builder.addText(
      u8"shapes push the lines apart and the words find their way. ");
  builder.pushStyle(style(19, kInk, "ko"))
      .addText(u8"한국어 단어들도 자연스럽게 흐르고, ")
      .popStyle();
  builder.pushStyle(style(19, kInk, "zh"))
      .addText(u8"中文字符同样围绕形状排布。")
      .popStyle();
  builder.addText(u8"Latin and CJK scripts mix freely because every word is "
                  "shaped once, cached, and repositioned with pure "
                  "arithmetic. ");
  builder.addText(u8"The moving circles below force a full line-breaking pass "
                  "every frame, yet the layout stays far under a "
                  "millisecond. ");
  builder.pushStyle(style(17, kAccent))
      .addText(u8"Performance is key. ")
      .popStyle();
  builder.addText(
      u8"When a shape slides across a line, the line splits into fragments and "
      "each fragment justifies itself independently; when the shape moves "
      "on, the fragments knit back together as if nothing happened. ");
  builder.pushStyle(style(19, kBlue, "ja"))
      .addText(u8"形が動くたびに行は裂け、また元通りに繋がる。")
      .popStyle();
  builder.addText(u8"No word is ever reshaped for any of this — the shape "
                  "cache hands back the same glyph runs and only their "
                  "positions change, frame after frame after frame. ");
  builder.pushStyle(style(19, kInk, "ko"))
      .addText(u8"모든 글자는 한 번만 성형되고 계속 재사용됩니다. ")
      .popStyle();
  builder.addText(u8"That is the whole trick, and it is enough to keep a "
                  "richly styled, multi-script paragraph reflowing at well "
                  "over a thousand frames per second.");
  Paragraph paragraph = builder.build();

  ExclusionFlow flow(SkRect::MakeXYWH(40, 40, 820, 660));
  flow.shapes().push_back({ExclusionFlow::Shape::kCircle,
                           SkRect::MakeXYWH(180, 140, 190, 190), 10});
  flow.shapes().push_back({ExclusionFlow::Shape::kCircle,
                           SkRect::MakeXYWH(520, 380, 150, 150), 10});
  flow.shapes().push_back(
      {ExclusionFlow::Shape::kRect, SkRect::MakeXYWH(360, 250, 170, 110), 10});
  flow.setMinIntervalWidth(56);

  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kJustify;
  options.lineMetrics.height = 30;

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(900, 740));
  SkCanvas *canvas = surface->getCanvas();

  const char8_t *swaps[] = {u8"water", u8"sand ", u8"wind ", u8"light"};
  const SkColor flashes[] = {kAccent, kBlue, kInk};

  TimingStats layoutTime, drawTime;
  const uint64_t shapeCallsBefore = fontContext.stats().shapeCalls;

  for (int frame = 0; frame < frames; ++frame) {
    const float animationTime = static_cast<float>(frame) * 0.045f;

    // Animate the shapes through the text.
    flow.shapes()[0].bounds =
        SkRect::MakeXYWH(180 + 260 * std::sin(animationTime),
                         140 + 200 * std::sin(animationTime * 0.6f), 190, 190);
    flow.shapes()[1].bounds =
        SkRect::MakeXYWH(520 - 220 * std::sin(animationTime * 0.8f),
                         380 + 160 * std::cos(animationTime * 0.5f), 150, 150);
    flow.shapes()[2].bounds =
        SkRect::MakeXYWH(360 + 180 * std::cos(animationTime * 0.7f),
                         250 + 240 * std::sin(animationTime * 0.35f), 170, 110);

    // Periodic rich-text updates, mid-animation.
    if (frame > 0 && frame % 24 == 0)
      paragraph.setPaint(15, 30, PaintStyle{flashes[(frame / 24) % 3]});
    if (frame > 0 && frame % 48 == 0) {
      // "water" (5 chars at a fixed offset inside the third sentence).
      const size_t textOffset = paragraph.text().find(u"water");
      if (textOffset != std::u16string::npos)
        paragraph.replaceText(static_cast<uint32_t>(textOffset),
                              static_cast<uint32_t>(textOffset + 5),
                              swaps[(frame / 48) % 4]);
    }

    const auto layoutStartTime = Clock::now();
    ParagraphLayout layout =
        layoutParagraph(fontContext, paragraph, flow, options);
    const auto layoutEndTime = Clock::now();

    canvas->clear(kPaper);
    SkPaint shapePaint;
    shapePaint.setAntiAlias(true);
    shapePaint.setColor(kShape);
    for (const auto &shape : flow.shapes()) {
      if (shape.kind == ExclusionFlow::Shape::kCircle)
        canvas->drawCircle(shape.bounds.centerX(), shape.bounds.centerY(),
                           shape.bounds.width() * 0.5f, shapePaint);
      else
        canvas->drawRect(shape.bounds, shapePaint);
    }
    layout.draw(canvas, paragraph);
    const auto drawEndTime = Clock::now();

    layoutTime.add(toMicroseconds(layoutEndTime - layoutStartTime));
    drawTime.add(toMicroseconds(drawEndTime - layoutEndTime));

    if (frame % 60 == 0 || frame == frames - 1) {
      char name[64];
      std::snprintf(name, sizeof name, "exclusions_f%03d.png", frame);
      writePng(surface.get(), outputDirectory / name);
    }
  }

  layoutTime.report("layout (reflow per frame)");
  drawTime.report("draw (CPU raster)");
  std::printf("  hb_shape calls during animation: %llu (edits only)\n\n",
              static_cast<unsigned long long>(fontContext.stats().shapeCalls -
                                              shapeCallsBefore));
}
