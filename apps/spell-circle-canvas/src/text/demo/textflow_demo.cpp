// TextFlow demo — exercises the two acceptance scenarios end-to-end and
// reports per-frame layout times:
//
//   A. A mixed Latin/Japanese/Korean/Chinese paragraph with rich-text spans
//      flowing around animated shapes; the layout reflows every frame while
//      words are occasionally re-styled and re-written (the "Chrome rich
//      text" test).
//   B. A Knuth-Plass justified paragraph whose words update periodically
//      with minimal lag.
//   C. Freeform typography: text along a spiral path and along an arbitrary
//      set of slanted lines (the Pretext-style demos).
//
// Frames render to PNG under ./textflow_demo_out for visual inspection.
//
//   ./build/bin/Release/textflow_demo [frames]

#include <textflow/TextFlow.h>

#include <include/core/SkBlurTypes.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkStream.h>
#include <include/core/SkSurface.h>
#include <include/core/SkTileMode.h>
#include <include/effects/SkGradient.h>
#include <include/encode/SkPngEncoder.h>
#include <include/ports/SkFontMgr_mac_ct.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cstdio>
#include <filesystem>
#include <numbers>
#include <numeric>
#include <random>
#include <string>
#include <vector>

using namespace textflow;
using Clock = std::chrono::steady_clock;

namespace {

constexpr SkColor kInk = 0xFF23252B;
constexpr SkColor kAccent = 0xFFC63D2F;
constexpr SkColor kBlue = 0xFF2B5AA7;
constexpr SkColor kShape = 0x33808A99;
constexpr SkColor kPaper = 0xFFFAF7F0;

struct TimingStats {
  std::vector<double> microseconds;

  /** Records one sample in microseconds. */
  void add(double sampleMicroseconds) {
    microseconds.push_back(sampleMicroseconds);
  }

  /** Prints mean and percentile statistics for all recorded samples. */
  void report(const char *label) const {
    if (microseconds.empty())
      return;
    std::vector<double> sorted = microseconds;
    std::sort(sorted.begin(), sorted.end());
    const double meanMicroseconds =
        std::accumulate(sorted.begin(), sorted.end(), 0.0) / sorted.size();
    const double medianMicroseconds = sorted[sorted.size() / 2];
    const double percentile95Microseconds =
        sorted[static_cast<size_t>(sorted.size() * 0.95)];
    const double maximumMicroseconds = sorted.back();
    std::printf("  %-28s mean %7.1f us   p50 %7.1f us   p95 %7.1f us   max "
                "%7.1f us   (%zu frames)\n",
                label, meanMicroseconds, medianMicroseconds,
                percentile95Microseconds, maximumMicroseconds, sorted.size());
  }
};

void writePng(SkSurface *surface, const std::filesystem::path &path) {
  SkPixmap pixmap;
  if (!surface->peekPixels(&pixmap))
    return;
  SkFILEWStream stream(path.string().c_str());
  if (stream.isValid())
    SkPngEncoder::Encode(&stream, pixmap, {});
}

/** Converts a steady-clock duration to the demo's reporting unit. */
double toMicroseconds(Clock::duration duration) {
  return std::chrono::duration<double, std::micro>(duration).count();
}

/** Creates a single-span style for demo paragraphs. */
TextStyle style(float fontSize, SkColor color = kInk,
                const char *languageTag = "") {
  TextStyle textStyle;
  textStyle.shaping.fontSize = fontSize;
  textStyle.shaping.languageTag = languageTag;
  textStyle.paint.color = color;
  return textStyle;
}

// ── Scene A: moving exclusions through a mixed-script rich paragraph ──────

void sceneExclusions(FontContext &fontContext, int frames,
                     const std::filesystem::path &outputDirectory) {
  std::printf("Scene A — mixed-script paragraph reflowing around moving "
              "shapes\n");

  ParagraphBuilder builder(style(17));
  builder.addText("Typography is the craft of arranging type. ");
  builder.pushStyle(style(22, kAccent)).addText("Glyphs flow").popStyle();
  builder.addText(" around obstacles the way water flows around stones. ");
  builder.pushStyle(style(19, kBlue, "ja"))
      .addText("日本語のテキストも同じ流れに乗って、")
      .popStyle();
  builder.addText("shapes push the lines apart and the words find their way. ");
  builder.pushStyle(style(19, kInk, "ko"))
      .addText("한국어 단어들도 자연스럽게 흐르고, ")
      .popStyle();
  builder.pushStyle(style(19, kInk, "zh"))
      .addText("中文字符同样围绕形状排布。")
      .popStyle();
  builder.addText("Latin and CJK scripts mix freely because every word is "
                  "shaped once, cached, and repositioned with pure "
                  "arithmetic. ");
  builder.addText("The moving circles below force a full line-breaking pass "
                  "every frame, yet the layout stays far under a "
                  "millisecond. ");
  builder.pushStyle(style(17, kAccent))
      .addText("Performance is key. ")
      .popStyle();
  builder.addText(
      "When a shape slides across a line, the line splits into fragments and "
      "each fragment justifies itself independently; when the shape moves "
      "on, the fragments knit back together as if nothing happened. ");
  builder.pushStyle(style(19, kBlue, "ja"))
      .addText("形が動くたびに行は裂け、また元通りに繋がる。")
      .popStyle();
  builder.addText("No word is ever reshaped for any of this — the shape "
                  "cache hands back the same glyph runs and only their "
                  "positions change, frame after frame after frame. ");
  builder.pushStyle(style(19, kInk, "ko"))
      .addText("모든 글자는 한 번만 성형되고 계속 재사용됩니다. ")
      .popStyle();
  builder.addText("That is the whole trick, and it is enough to keep a "
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

  const char *swaps[] = {"water", "sand ", "wind ", "light"};
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

// ── Scene B: Knuth-Plass justified paragraph with live word updates ───────

void sceneKnuthPlass(FontContext &fontContext, int frames,
                     const std::filesystem::path &outputDirectory) {
  std::printf("Scene B — Knuth-Plass justified paragraph, words updating "
              "live\n");

  ParagraphBuilder builder(style(16.5f));
  builder.addText(
      "In olden times, when wishing still helped one, there lived a king "
      "whose daughters were all beautiful; and the youngest was so beautiful "
      "that the sun itself, which has seen so much, was astonished whenever "
      "it shone in her face. Close by the king's castle lay a great dark "
      "forest, and under an old lime-tree in the forest was a well, and when "
      "the day was very warm, the king's child went out into the forest and "
      "sat down by the side of the ");
  builder.pushStyle(style(16.5f, kAccent)).addText("cool").popStyle();
  builder.addText(" fountain; and when she was bored she took a golden ball, "
                  "and threw it up on high and caught it; and this ball was "
                  "her favorite plaything. ");
  builder.pushStyle(style(16.5f, kBlue, "ja"))
      .addText("グリム童話は日本語でも読まれ、")
      .popStyle();
  builder.addText("the tale crosses scripts without breaking its measure.");
  Paragraph paragraph = builder.build();

  BlockFlow flow(SkRect::MakeXYWH(50, 40, 560, 640));
  ParagraphLayoutOptions options;
  options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
  options.alignment = TextAlignment::kJustify;
  options.lineMetrics.height = 27;

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(660, 720));

  const char *synonyms[] = {"cool", "cold", "calm", "deep"};
  const char *ballWords[] = {"golden", "silver", "copper", "crystal"};

  TimingStats layoutTime;
  const uint64_t shapeCallsBefore = fontContext.stats().shapeCalls;

  for (int frame = 0; frame < frames; ++frame) {
    // Word updates while the paragraph stays justified.
    if (frame > 0 && frame % 20 == 0) {
      const std::u16string &text = paragraph.text();
      for (const char *candidate : synonyms) {
        std::u16string needle(candidate, candidate + 4);
        const size_t textOffset = text.find(needle);
        if (textOffset != std::u16string::npos) {
          paragraph.replaceText(static_cast<uint32_t>(textOffset),
                                static_cast<uint32_t>(textOffset + 4),
                                synonyms[(frame / 20) % 4]);
          break;
        }
      }
    }
    if (frame > 0 && frame % 30 == 0) {
      for (const char *candidate : ballWords) {
        std::u16string needle;
        for (const char *character = candidate; *character; ++character)
          needle.push_back(static_cast<char16_t>(*character));
        const size_t textOffset = paragraph.text().find(needle);
        if (textOffset != std::u16string::npos) {
          paragraph.replaceText(
              static_cast<uint32_t>(textOffset),
              static_cast<uint32_t>(textOffset + needle.size()),
              ballWords[(frame / 30) % 4]);
          break;
        }
      }
    }

    const auto layoutStartTime = Clock::now();
    ParagraphLayout layout =
        layoutParagraph(fontContext, paragraph, flow, options);
    const auto layoutEndTime = Clock::now();
    layoutTime.add(toMicroseconds(layoutEndTime - layoutStartTime));

    if (frame % 60 == 0 || frame == frames - 1) {
      surface->getCanvas()->clear(kPaper);
      layout.draw(surface->getCanvas(), paragraph);
      char name[64];
      std::snprintf(name, sizeof name, "knuthplass_f%03d.png", frame);
      writePng(surface.get(), outputDirectory / name);
    }
  }

  layoutTime.report("layout (KP justify per frame)");
  std::printf("  hb_shape calls during animation: %llu (edits only)\n\n",
              static_cast<unsigned long long>(fontContext.stats().shapeCalls -
                                              shapeCallsBefore));
}

// ── Scene C: freeform typography (paths and arbitrary line sets) ──────────

void sceneFreeform(FontContext &fontContext,
                   const std::filesystem::path &outputDirectory) {
  std::printf("Scene C — freeform lines (spiral path + slanted line set)\n");

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(900, 480));
  SkCanvas *canvas = surface->getCanvas();
  canvas->clear(kPaper);

  // Spiral: three turns, as one SkPath, glyphs tangent to the curve.
  {
    SkPathBuilder spiralBuilder;
    const SkPoint center = {240, 240};
    const int steps = 220;
    for (int stepIndex = 0; stepIndex <= steps; ++stepIndex) {
      const float progress = static_cast<float>(stepIndex) / steps;
      const float angle = progress * 6.0f * std::numbers::pi_v<float>;
      const float radius = 40 + 150 * progress;
      const SkPoint point = {center.x() + radius * std::cos(angle),
                             center.y() + radius * std::sin(angle)};
      if (stepIndex == 0)
        spiralBuilder.moveTo(point);
      else
        spiralBuilder.lineTo(point);
    }
    const SkPath spiral = spiralBuilder.detach();
    Paragraph paragraph;
    paragraph.appendText(
        "text can follow any path — 螺旋に沿って文字が流れ、 나선을 따라 "
        "글자가 흐르고, 文字沿着螺旋流动 — and every glyph stays tangent to "
        "the curve while the words keep their cached shapes",
        style(17));
    PathFlow flow(spiral);
    const auto coldStartTime = Clock::now();
    ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);
    const auto coldEndTime = Clock::now();
    ParagraphLayout warm =
        layoutParagraph(fontContext, paragraph, flow); // caches primed
    const auto warmEndTime = Clock::now();
    layout.draw(canvas, paragraph);
    std::printf("  spiral layout: cold %.1f us, warm %.1f us\n",
                toMicroseconds(coldEndTime - coldStartTime),
                toMicroseconds(warmEndTime - coldEndTime));
  }

  // Arbitrary slanted line set (Pretext-style freedom).
  {
    LineSetFlow flow;
    for (int lineIndex = 0; lineIndex < 6; ++lineIndex) {
      // Gentle alternating slopes; rise stays well under the line pitch so
      // neighboring lines never collide.
      const float baselineY = 80.0f + static_cast<float>(lineIndex) * 64.0f;
      const float slope = (lineIndex % 2 == 0) ? 0.07f : -0.055f;
      const float inverseLength = 1.0f / std::sqrt(1 + slope * slope);
      flow.lines().push_back({LineInterval{
          {500, baselineY}, {inverseLength, slope * inverseLength}, 350}});
    }
    Paragraph paragraph;
    paragraph.appendText("lines need not be horizontal nor parallel; ",
                         style(18, kInk));
    paragraph.appendText("각 행은 자유로운 방향을 갖고 ",
                         style(18, kBlue, "ko"));
    paragraph.appendText("每一行都有自己的方向 ", style(18, kAccent, "zh"));
    paragraph.appendText("and the paragraph simply flows through whatever pen "
                         "strokes you hand it.",
                         style(18, kInk));
    const auto coldStartTime = Clock::now();
    ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);
    const auto coldEndTime = Clock::now();
    ParagraphLayout warm = layoutParagraph(fontContext, paragraph, flow);
    const auto warmEndTime = Clock::now();
    layout.draw(canvas, paragraph);
    std::printf("  line-set layout: cold %.1f us, warm %.1f us\n",
                toMicroseconds(coldEndTime - coldStartTime),
                toMicroseconds(warmEndTime - coldEndTime));
  }

  writePng(surface.get(), outputDirectory / "freeform.png");
  std::printf("\n");
}

// ── Scene D: extreme geometries ───────────────────────────────────────────

void sceneExtreme(FontContext &fontContext,
                  const std::filesystem::path &outputDirectory) {
  std::printf("Scene D — extreme geometries (zigzag, scribble, bumps, "
              "confetti)\n");

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1000, 860));
  SkCanvas *canvas = surface->getCanvas();
  canvas->clear(kPaper);

  auto measureColdAndWarm =
      [&]<std::invocable Operation>(const char *label, Operation &&operation) {
        const auto coldStartTime = Clock::now();
        operation();
        const auto coldEndTime = Clock::now();
        operation(); // Warm pass.
        const auto warmEndTime = Clock::now();
        std::printf("  %-10s cold %8.1f us, warm %7.1f us\n", label,
                    toMicroseconds(coldEndTime - coldStartTime),
                    toMicroseconds(warmEndTime - coldEndTime));
      };

  // Zigzag: each *line* is a chain of alternating up/down segments; words
  // march through the chain segment by segment.
  {
    Paragraph paragraph;
    paragraph.appendText(
        "zigzag lines carry words up and down and up and down while the "
        "layout treats every slanted segment as just another interval — "
        "ジグザグの線の上でも単語は流れ続ける — 지그재그 선 위에서도",
        style(16));
    LineSetFlow flow;
    for (int lineIndex = 0; lineIndex < 3; ++lineIndex) {
      std::vector<LineInterval> segments;
      float segmentStartX = 30;
      float segmentStartY = 70.0f + static_cast<float>(lineIndex) * 90.0f;
      for (int segmentIndex = 0; segmentIndex < 9; ++segmentIndex) {
        const float slope = (segmentIndex % 2 == 0) ? 0.45f : -0.45f;
        const float inverseLength = 1.0f / std::sqrt(1 + slope * slope);
        segments.push_back({LineInterval{{segmentStartX, segmentStartY},
                                         {inverseLength, slope * inverseLength},
                                         52}});
        segmentStartX += 52 * inverseLength;
        segmentStartY += 52 * slope * inverseLength;
      }
      flow.lines().push_back(std::move(segments));
    }
    ParagraphLayout layout;
    measureColdAndWarm("zigzag", [&] {
      layout = layoutParagraph(fontContext, paragraph, flow);
    });
    layout.draw(canvas, paragraph);
  }

  // Scribble: a wandering quadratic path.
  {
    SkPathBuilder pathBuilder;
    pathBuilder.moveTo(60, 420);
    std::mt19937 randomEngine(11);
    float previousX = 60;
    float previousY = 420;
    for (int curveIndex = 0; curveIndex < 9; ++curveIndex) {
      const float controlX =
          previousX + 40 + static_cast<float>(randomEngine() % 90);
      const float controlY = 340 + static_cast<float>(randomEngine() % 220);
      previousX += 80 + static_cast<float>(randomEngine() % 60);
      previousY = 360 + static_cast<float>(randomEngine() % 200);
      pathBuilder.quadTo(controlX, controlY, previousX, previousY);
    }
    Paragraph paragraph;
    paragraph.appendText(
        "a scribbled stroke is still a line to this engine, the pen just "
        "wanders wherever the curve goes — 落書きの線でも文字は流れる",
        style(15, kBlue));
    PathFlow flow(pathBuilder.detach());
    ParagraphLayout layout;
    measureColdAndWarm("scribble", [&] {
      layout = layoutParagraph(fontContext, paragraph, flow);
    });
    layout.draw(canvas, paragraph);
  }

  // Extremely bumpy: high-frequency sine paths.
  {
    for (int line = 0; line < 2; ++line) {
      SkPathBuilder pathBuilder;
      const float baseY = 640.0f + static_cast<float>(line) * 80.0f;
      const float amplitude = 14.0f + 10.0f * static_cast<float>(line);
      pathBuilder.moveTo(30, baseY);
      for (int sampleX = 30; sampleX <= 960; sampleX += 6)
        pathBuilder.lineTo(static_cast<float>(sampleX),
                           baseY + amplitude *
                                       std::sin(static_cast<float>(sampleX) *
                                                (0.055f - 0.015f * line)));
      Paragraph paragraph;
      paragraph.appendText(
          line == 0
              ? "extremely bumpy baselines shake every glyph yet the words "
                "keep their spacing along the arc length of the wave"
              : "波打つベースラインの上でも字形は接線に沿って進む — 파도치는 "
                "기준선 위에서도 글자는 계속 흐른다 — 波浪基线上的文字",
          style(15, line == 0 ? kInk : kAccent));
      PathFlow flow(pathBuilder.detach());
      ParagraphLayout layout;
      measureColdAndWarm(line == 0 ? "bumpy" : "bumpy2", [&] {
        layout = layoutParagraph(fontContext, paragraph, flow);
      });
      layout.draw(canvas, paragraph);
    }
  }

  writePng(surface.get(), outputDirectory / "extreme.png");

  // Confetti: single letters littered across the whole canvas — every
  // "word" is one letter, every line interval a tiny randomly rotated
  // stroke barely long enough for it.
  {
    sk_sp<SkSurface> confetti =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1000, 620));
    confetti->getCanvas()->clear(kPaper);

    std::string letters;
    const char *cjk[] = {"文", "字", "術", "式", "光", "影",
                         "한", "글", "빛", "円", "陣", "魔"};
    std::mt19937 randomEngine(5);
    for (int letterIndex = 0; letterIndex < 120; ++letterIndex) {
      if (letterIndex % 3 == 2)
        letters += cjk[randomEngine() % 12];
      else
        letters += static_cast<char>('a' + randomEngine() % 26);
      letters += ' ';
    }
    Paragraph paragraph;
    paragraph.appendText(letters, style(26));
    // Color a few random spans so the confetti is multicolored.
    const uint32_t textLength = static_cast<uint32_t>(paragraph.text().size());
    for (uint32_t textOffset = 0; textOffset + 8 < textLength; textOffset += 8)
      paragraph.setPaint(textOffset, textOffset + 4,
                         PaintStyle{(textOffset / 8) % 3 == 0   ? kAccent
                                    : (textOffset / 8) % 3 == 1 ? kBlue
                                                                : kInk});

    LineSetFlow flow;
    for (int letterIndex = 0; letterIndex < 120; ++letterIndex) {
      const float positionX = 20.0f + static_cast<float>(randomEngine() % 940);
      const float positionY = 30.0f + static_cast<float>(randomEngine() % 560);
      const float angle =
          static_cast<float>(randomEngine() % 628) * 0.01f; // 0..2π
      flow.lines().push_back({LineInterval{
          {positionX, positionY}, {std::cos(angle), std::sin(angle)}, 34}});
    }
    ParagraphLayout layout;
    measureColdAndWarm("confetti", [&] {
      layout = layoutParagraph(fontContext, paragraph, flow);
    });
    layout.draw(confetti->getCanvas(), paragraph);
    writePng(confetti.get(), outputDirectory / "confetti.png");
  }
  std::printf("\n");
}

// ── Scene E: typographic options ──────────────────────────────────────────

void sceneTypography(FontContext &fontContext,
                     const std::filesystem::path &outputDirectory) {
  std::printf("Scene E — typographic options (last-line modes, hyphenation, "
              "effects, mixed fonts)\n");

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1060, 820));
  SkCanvas *canvas = surface->getCanvas();
  canvas->clear(kPaper);

  const char *sample =
      "The last line of a justified paragraph reveals the typographer's "
      "intent more than any other line in the whole measure.";

  // Justify with last line start / center / end / full (InDesign-style).
  const TextAlignment lastModes[] = {
      TextAlignment::kStart, TextAlignment::kCenter, TextAlignment::kEnd};
  const char *labels[] = {"last: left", "last: center", "last: right",
                          "last: full"};
  for (int exampleIndex = 0; exampleIndex < 4; ++exampleIndex) {
    const float exampleX =
        30.0f + static_cast<float>(exampleIndex % 2) * 260.0f;
    const float exampleY =
        40.0f + static_cast<float>(exampleIndex / 2) * 190.0f;
    Paragraph caption;
    caption.appendText(labels[exampleIndex], style(12, kAccent));
    BlockFlow capFlow(SkRect::MakeXYWH(exampleX, exampleY - 24, 220, 20));
    layoutParagraph(fontContext, caption, capFlow).draw(canvas, caption);

    Paragraph paragraph;
    paragraph.appendText(sample, style(14.5f));
    BlockFlow flow(SkRect::MakeXYWH(exampleX, exampleY, 220, 160));
    ParagraphLayoutOptions options;
    options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
    options.alignment = TextAlignment::kJustify;
    if (exampleIndex < 3)
      options.justification.lastLineAlignment = lastModes[exampleIndex];
    else
      options.justification.justifyLastLine = true;
    layoutParagraph(fontContext, paragraph, flow, options)
        .draw(canvas, paragraph);
  }

  // Narrow hyphenated Knuth-Plass column (soft hyphens marked with ­).
  {
    Paragraph caption;
    caption.appendText("KP + soft hyphens, 130px", style(12, kAccent));
    BlockFlow capFlow(SkRect::MakeXYWH(570, 16, 220, 20));
    layoutParagraph(fontContext, caption, capFlow).draw(canvas, caption);

    Paragraph paragraph;
    paragraph.appendText("In these as­ton­ish­ing­ly nar­row "
                         "col­umns, dis­cre­tion­ary breaks keep "
                         "jus­ti­fi­ca­tion from tear­ing the "
                         "spac­ing apart, ex­act­ly as a book "
                         "com­pos­i­tor would want.",
                         style(14.5f));
    BlockFlow flow(SkRect::MakeXYWH(570, 40, 130, 400));
    ParagraphLayoutOptions options;
    options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
    options.alignment = TextAlignment::kJustify;
    const auto layoutStartTime = Clock::now();
    ParagraphLayout layout =
        layoutParagraph(fontContext, paragraph, flow, options);
    const auto layoutEndTime = Clock::now();
    layout.draw(canvas, paragraph);
    std::printf("  hyphenated 130px KP column: %.1f us cold\n",
                toMicroseconds(layoutEndTime - layoutStartTime));
  }

  // Effects: drop shadow, gradient shader, glyph blur — all paint-only.
  {
    Paragraph paragraph;
    TextStyle title = style(40, SK_ColorWHITE);
    title.paint.shadows.push_back({0x99000000, {3, 4}, 3.0f});
    paragraph.appendText("Shadowed ", title);

    TextStyle gradient = style(40);
    const SkPoint gradientPoints[2] = {{730, 40}, {1030, 240}};
    const SkColor4f colors[2] = {SkColor4f::FromColor(kAccent),
                                 SkColor4f::FromColor(kBlue)};
    gradient.paint.shader = SkShaders::LinearGradient(
        gradientPoints,
        SkGradient(SkGradient::Colors({colors, 2}, SkTileMode::kClamp),
                   SkGradient::Interpolation()));
    gradient.paint.shadows.push_back({0x44000000, {2, 2}, 2.0f});
    paragraph.appendText("gradient ", gradient);

    TextStyle blurred = style(40, kInk);
    blurred.paint.maskFilter =
        SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, 2.4f);
    paragraph.appendText("blur", blurred);

    BlockFlow flow(SkRect::MakeXYWH(730, 40, 310, 400));
    layoutParagraph(fontContext, paragraph, flow).draw(canvas, paragraph);
  }

  // Mixed families and sizes in one flow (serif / sans / mono / CJK).
  {
    // Noto Sans/Serif (user-installed variable fonts): richer OpenType
    // shaping coverage than the system defaults.
    SkFontMgr *fontManager = fontContext.fontManager();
    TextStyle serif = style(20, kInk);
    serif.shaping.typeface =
        fontManager->matchFamilyStyle("Noto Serif", SkFontStyle());
    TextStyle sans = style(17, kBlue);
    sans.shaping.typeface =
        fontManager->matchFamilyStyle("Noto Sans", SkFontStyle());
    TextStyle mono = style(14, kAccent);
    mono.shaping.typeface =
        fontManager->matchFamilyStyle("Menlo", SkFontStyle());

    Paragraph paragraph;
    paragraph.appendText("Serif voices carry the body, ", serif);
    paragraph.appendText("a grotesque interjects, ", sans);
    paragraph.appendText("code whispers in mono, ", mono);
    paragraph.appendText("そして日本語がフォールバックで加わり、", sans);
    paragraph.appendText("모든 서체가 한 단락 안에서 섞인다 ", serif);
    paragraph.appendText("— one paragraph, many fonts, one shape cache.",
                         serif);

    BlockFlow flow(SkRect::MakeXYWH(30, 470, 660, 320));
    ParagraphLayoutOptions options;
    options.alignment = TextAlignment::kJustify;
    options.lineMetrics.height = 34;
    const auto coldStartTime = Clock::now();
    ParagraphLayout layout =
        layoutParagraph(fontContext, paragraph, flow, options);
    const auto coldEndTime = Clock::now();
    ParagraphLayout warm =
        layoutParagraph(fontContext, paragraph, flow, options);
    const auto warmEndTime = Clock::now();
    layout.draw(canvas, paragraph);
    std::printf("  mixed-font paragraph: cold %.1f us, warm %.1f us\n",
                toMicroseconds(coldEndTime - coldStartTime),
                toMicroseconds(warmEndTime - coldEndTime));
  }

  writePng(surface.get(), outputDirectory / "typography.png");
  std::printf("\n");
}

// ── Scene F: Pretext-style glyph choreography ─────────────────────────────
// Rain and ripple operate on a full ~1000-word paragraph that is re-laid-out
// EVERY frame (breathing measure, live word edits) while its letters
// simultaneously leave their lines as particles/waves. The point: letter-
// level choreography and full-paragraph relayout share one frame budget.

// ~1000 words of mixed Latin/CJK in alternating color/style chunks.
Paragraph makeBigParagraph(int wordCount, float fontSize) {
  const char *latin[] = {"the",     "letters", "fall",   "away",    "from",
                         "their",   "lines",   "and",    "return",  "again",
                         "layout",  "engine",  "words",  "measure", "glyph",
                         "cascade", "gentle",  "steady", "rhythm",  "flowing"};
  const char *cjk[] = {"文字", "雨",   "波紋", "字形", "빗물", "글자",
                       "물결", "여울", "漣漪", "文雨", "字落", "縦横"};
  const TextStyle styles[3] = {style(fontSize, kInk), style(fontSize, kBlue),
                               style(fontSize, kAccent)};
  std::mt19937 randomEngine(23);
  Paragraph paragraph;
  std::string chunk;
  int chunkStyle = 0;
  for (int wordIndex = 0; wordIndex < wordCount; ++wordIndex) {
    if (wordIndex % 5 == 4)
      chunk += cjk[randomEngine() % 12];
    else
      chunk += latin[randomEngine() % 20];
    chunk += ' ';
    if (wordIndex % 120 == 119 || wordIndex + 1 == wordCount) {
      paragraph.appendText(chunk, styles[chunkStyle++ % 3]);
      chunk.clear();
    }
  }
  return paragraph;
}

// (a) A 1000-word paragraph rains onto an umbrella: the justified measure
// breathes (full relayout, every frame), attached letters track their
// re-laid-out lines, detached letters fall, slide down the dome tangent,
// drain off-screen, and re-attach to the paragraph.
void sceneRain(FontContext &fontContext, int frames,
               const std::filesystem::path &outputDirectory) {
  Paragraph paragraph = makeBigParagraph(1000, 13.0f);

  const float baseWidth = 1080;
  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kJustify;
  options.lineMetrics.height = 22;

  const SkPoint domeCenter = {600, 1560};
  const float domeRadius = 210;
  const float standoffDistance = domeRadius + 10;

  struct Particle {
    enum Mode : uint8_t { kAttached, kFalling, kSliding } mode = kAttached;
    SkPoint position = {0, 0};
    float horizontalVelocity = 0;
    float verticalVelocity = 0;
    float angle = 0;
    float angularVelocity = 0;
  };

  // Stable letter count from an initial layout.
  BlockFlow probe(SkRect::MakeXYWH(60, 40, baseWidth, 1320));
  ParagraphLayout first =
      layoutParagraph(fontContext, paragraph, probe, options);
  size_t glyphCount = 0;
  forEachPlacedGlyph(first, paragraph, [&](auto...) { glyphCount++; });
  std::vector<Particle> particles(glyphCount);
  std::printf("  rain: %zu letters from %zu words\n", glyphCount,
              paragraph.words().size());

  std::mt19937 randomEngine(9);
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1200, 1900));
  SkCanvas *canvas = surface->getCanvas();
  GlyphRSXformBatches glyphBatches;
  TimingStats layoutTime, simulationTime, drawTime;

  for (int frame = 0; frame < frames; ++frame) {
    // The measure breathes: a genuinely different line-breaking problem
    // every frame, solved from the shape cache.
    const float width =
        baseWidth *
        (1.0f + 0.06f * std::sin(static_cast<float>(frame) * 0.02f));
    BlockFlow flow(SkRect::MakeXYWH(60, 40, width, 1320));

    const auto layoutStartTime = Clock::now();
    ParagraphLayout layout =
        layoutParagraph(fontContext, paragraph, flow, options);
    const auto layoutEndTime = Clock::now();

    // Detach a few attached letters per frame (after a settling intro).
    if (frame > 30)
      for (int detachAttempt = 0; detachAttempt < 22; ++detachAttempt) {
        Particle &particle = particles[randomEngine() % particles.size()];
        if (particle.mode == Particle::kAttached)
          particle.mode =
              Particle::kFalling; // Position seeded from layout below.
      }

    glyphBatches.clear();
    size_t particleIndex = 0;
    forEachPlacedGlyph(
        layout, paragraph,
        [&](const ShapedWord *shapedWord, SkGlyphID glyph, float glyphAdvance,
            SkColor color, SkPoint restingOrigin) {
          Particle &particle = particles[particleIndex % particles.size()];
          particleIndex++;
          const float halfAdvance = glyphAdvance * 0.5f;
          // Anchor everything on the glyph's advance center.
          const SkPoint restingCenter =
              restingOrigin + SkVector{halfAdvance, 0};
          float cosine = 1;
          float sine = 0;
          SkPoint drawingCenter = restingCenter;
          switch (particle.mode) {
          case Particle::kAttached:
            break; // rides its (re-laid-out) line
          case Particle::kFalling: {
            if (particle.verticalVelocity == 0 && particle.position.fY == 0) {
              particle.position = restingCenter;
              particle.verticalVelocity =
                  2.5f + static_cast<float>(randomEngine() % 100) * 0.025f;
              particle.horizontalVelocity = 0;
              particle.angularVelocity =
                  (static_cast<float>(randomEngine() % 100) - 50.0f) * 0.0015f;
              particle.angle = 0;
            }
            particle.position.fY += particle.verticalVelocity;
            particle.position.fX += particle.horizontalVelocity;
            particle.angle += particle.angularVelocity;
            const SkVector direction = particle.position - domeCenter;
            if (direction.length() < standoffDistance &&
                particle.position.fY < domeCenter.fY)
              particle.mode = Particle::kSliding;
            if (particle.position.fY > 1940) {
              particle = Particle{}; // Drained: rejoin the paragraph.
            }
            break;
          }
          case Particle::kSliding: {
            SkVector direction = particle.position - domeCenter;
            direction.normalize();
            particle.position = domeCenter + direction * standoffDistance;
            SkVector tangent = {-direction.fY, direction.fX};
            if (tangent.fX * direction.fX < 0)
              tangent = -tangent;
            const float slideDistance = particle.verticalVelocity *
                                        (0.55f + 0.9f * std::abs(direction.fX));
            particle.position += tangent * slideDistance;
            particle.angle = std::atan2(tangent.fY, tangent.fX);
            if (direction.fY > -0.18f) {
              particle.mode = Particle::kFalling;
              particle.horizontalVelocity = tangent.fX * slideDistance;
            }
            break;
          }
          }
          if (particle.mode != Particle::kAttached) {
            drawingCenter = particle.position;
            quantizeAngle(particle.angle, cosine, sine);
          }
          GlyphRSXformBatches::Batch &batch =
              glyphBatches.batchForStyle(shapedWord, color);
          batch.glyphs.push_back(glyph);
          batch.transforms.push_back({cosine, sine,
                                      drawingCenter.fX - cosine * halfAdvance,
                                      drawingCenter.fY - sine * halfAdvance});
        });
    const auto simulationEndTime = Clock::now();

    canvas->clear(kPaper);
    SkPaint umbrella;
    umbrella.setAntiAlias(true);
    umbrella.setColor(0xFFB9552F);
    SkPathBuilder dome;
    dome.addArc(SkRect::MakeLTRB(
                    domeCenter.fX - domeRadius, domeCenter.fY - domeRadius,
                    domeCenter.fX + domeRadius, domeCenter.fY + domeRadius),
                180, 180);
    dome.close();
    canvas->drawPath(dome.detach(), umbrella);
    SkPaint pole;
    pole.setAntiAlias(true);
    pole.setStyle(SkPaint::kStroke_Style);
    pole.setStrokeWidth(8);
    pole.setColor(kInk);
    canvas->drawLine(domeCenter.fX, domeCenter.fY - 8, domeCenter.fX,
                     domeCenter.fY + 300, pole);
    glyphBatches.draw(canvas);
    const auto drawEndTime = Clock::now();

    layoutTime.add(toMicroseconds(layoutEndTime - layoutStartTime));
    simulationTime.add(toMicroseconds(simulationEndTime - layoutEndTime));
    drawTime.add(toMicroseconds(drawEndTime - simulationEndTime));
    if (frame == frames / 2 || frame == frames - 1) {
      char name[64];
      std::snprintf(name, sizeof name, "rain_f%03d.png", frame);
      writePng(surface.get(), outputDirectory / name);
    }
  }
  layoutTime.report("rain relayout (1000 words)");
  simulationTime.report("rain letters (sim + place)");
  drawTime.report("rain draw (CPU raster)");
}

// (b) Pool of text, at scale: drops land on a 1000-word paragraph and the
// expanding rings push each letter outward off its line as the front
// passes. The paragraph is re-laid-out every frame, and every ~150 frames
// a word is edited mid-ripple (cache-hot) to prove layout stays live.
void sceneRipple(FontContext &fontContext, int frames,
                 const std::filesystem::path &outputDirectory) {
  Paragraph paragraph = makeBigParagraph(1000, 13.0f);

  BlockFlow flow(SkRect::MakeXYWH(60, 50, 1080, 860));
  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kJustify;
  options.lineMetrics.height = 21.0f;

  {
    ParagraphLayout probe =
        layoutParagraph(fontContext, paragraph, flow, options);
    size_t glyphCount = 0;
    forEachPlacedGlyph(probe, paragraph, [&](auto...) { glyphCount++; });
    std::printf("  ripple: %zu letters from %zu words\n", glyphCount,
                paragraph.words().size());
  }

  struct Drop {
    SkPoint center;
    int birthFrameNumber;
  };
  std::vector<Drop> drops;
  std::mt19937 randomEngine(31);
  const float ringSpeed = 6.0f;

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1200, 960));
  SkCanvas *canvas = surface->getCanvas();
  GlyphRSXformBatches glyphBatches;
  TimingStats layoutTime, waveTime, drawTime;
  const char *swaps[] = {"letters", "glyphs ", "symbols", "strokes"};

  for (int frame = 0; frame < frames; ++frame) {
    if (frame % 55 == 0)
      drops.push_back({{140.0f + static_cast<float>(randomEngine() % 920),
                        140.0f + static_cast<float>(randomEngine() % 680)},
                       frame});
    std::erase_if(drops, [&](const Drop &drop) {
      return frame - drop.birthFrameNumber > 280;
    });

    // Live edit mid-ripple: same-length swap, everything else cache-hot.
    if (frame > 0 && frame % 150 == 0) {
      const size_t textOffset = paragraph.text().find(u"letters");
      if (textOffset != std::u16string::npos)
        paragraph.replaceText(static_cast<uint32_t>(textOffset),
                              static_cast<uint32_t>(textOffset + 7),
                              swaps[(frame / 150) % 4]);
    }

    const auto layoutStartTime = Clock::now();
    ParagraphLayout layout =
        layoutParagraph(fontContext, paragraph, flow, options);
    const auto layoutEndTime = Clock::now();

    glyphBatches.clear();
    forEachPlacedGlyph(
        layout, paragraph,
        [&](const ShapedWord *shapedWord, SkGlyphID glyph, float glyphAdvance,
            SkColor color, SkPoint restingOrigin) {
          SkVector offset = {0, 0};
          float tilt = 0;
          for (const Drop &drop : drops) {
            const SkVector radialVector = restingOrigin - drop.center;
            const float distance = radialVector.length() + 1.0f;
            const float ringRadius =
                ringSpeed * static_cast<float>(frame - drop.birthFrameNumber);
            const float normalizedRingDistance =
                (distance - ringRadius) / 46.0f;
            if (normalizedRingDistance > 3 || normalizedRingDistance < -3)
              continue; // outside the ring front
            const float amplitude =
                42.0f *
                std::exp(-static_cast<float>(frame - drop.birthFrameNumber) /
                         150.0f);
            const float pulse = amplitude * std::exp(-normalizedRingDistance *
                                                     normalizedRingDistance);
            offset += radialVector * (pulse / distance);
            tilt +=
                pulse * 0.012f * (normalizedRingDistance < 0 ? -1.0f : 1.0f);
          }
          float cosine;
          float sine;
          quantizeAngle(tilt, cosine, sine);
          const SkPoint drawingOrigin = restingOrigin + offset;
          const float halfAdvance = glyphAdvance * 0.5f;
          GlyphRSXformBatches::Batch &batch =
              glyphBatches.batchForStyle(shapedWord, color);
          batch.glyphs.push_back(glyph);
          batch.transforms.push_back(
              {cosine, sine,
               drawingOrigin.fX - cosine * halfAdvance + halfAdvance,
               drawingOrigin.fY - sine * halfAdvance});
        });
    const auto waveEndTime = Clock::now();

    canvas->clear(kPaper);
    glyphBatches.draw(canvas);
    const auto drawEndTime = Clock::now();

    layoutTime.add(toMicroseconds(layoutEndTime - layoutStartTime));
    waveTime.add(toMicroseconds(waveEndTime - layoutEndTime));
    drawTime.add(toMicroseconds(drawEndTime - waveEndTime));

    if (frame == 85 || frame == 200 || frame == frames - 1) {
      char name[64];
      std::snprintf(name, sizeof name, "ripple_f%03d.png", frame);
      writePng(surface.get(), outputDirectory / name);
    }
  }
  layoutTime.report("ripple relayout (1000 words)");
  waveTime.report("ripple wave math (per letter)");
  drawTime.report("ripple draw (CPU raster)");
}

// (c) Infinite loop: text marches forever around a closed figure-eight —
// the closed contour wraps arc positions, so animating contourStart is the
// whole effect.
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
  paragraph.appendText("and the words go round ", style(21, kInk));
  paragraph.appendText("終わらない文字の環 ", style(21, kBlue, "ja"));
  paragraph.appendText("끝나지 않는 글의 고리 ", style(21, kAccent, "ko"));
  paragraph.appendText("文字環繞不息 ", style(21, kInk, "zh"));
  paragraph.appendText("round and round again — ", style(21, kInk));

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

void scenePretext(FontContext &fontContext, int frames,
                  const std::filesystem::path &outputDirectory) {
  std::printf("Scene F — Pretext-style glyph choreography (rain / pool "
              "ripple / infinite loop)\n");
  sceneRain(fontContext, frames, outputDirectory);
  sceneRipple(fontContext, frames, outputDirectory);
  sceneLoop(fontContext, frames, outputDirectory);
  std::printf("\n");
}

// ── Scene G: script coverage + OpenType features ──────────────────────────

// 2000 tokens across a dozen writing systems scattered as rotated confetti:
// Arabic joins, Devanagari conjuncts, emoji ZWJ sequences and all — every
// token resolved through per-codepoint fallback and the one shape cache.
void sceneBabel(FontContext &fontContext,
                const std::filesystem::path &outputDirectory) {
  const char *tokens[] = {"حرف",   "كلمة",   "अक्षर", "शब्द",   "אות",
                          "מילה",  "ตัวอักษร", "字",   "글",    "λόγος",
                          "буква", "🎉",     "👍🏽", "文字",  "ঢাকা",
                          "கடல்",   "ᚱᚢᚾ",    "ainm", "słowo", "λέξη"};
  std::mt19937 randomEngine(77);
  Paragraph paragraph;
  std::string text;
  for (int tokenIndex = 0; tokenIndex < 2000; ++tokenIndex) {
    text += tokens[randomEngine() % 20];
    text += ' ';
  }
  paragraph.appendText(text, style(15));
  const uint32_t textLength = static_cast<uint32_t>(paragraph.text().size());
  for (uint32_t textOffset = 0; textOffset + 40 < textLength; textOffset += 40)
    paragraph.setPaint(textOffset, textOffset + 20,
                       PaintStyle{(textOffset / 40) % 3 == 0   ? kAccent
                                  : (textOffset / 40) % 3 == 1 ? kBlue
                                                               : kInk});

  LineSetFlow flow;
  for (int tokenIndex = 0; tokenIndex < 2000; ++tokenIndex) {
    const float angle = static_cast<float>(randomEngine() % 628) * 0.01f;
    flow.lines().push_back(
        {LineInterval{{20.0f + static_cast<float>(randomEngine() % 1360),
                       20.0f + static_cast<float>(randomEngine() % 860)},
                      {std::cos(angle), std::sin(angle)},
                      60}});
  }

  const auto coldStartTime = Clock::now();
  ParagraphLayout layout = layoutParagraph(fontContext, paragraph, flow);
  const auto coldEndTime = Clock::now();
  ParagraphLayout warm = layoutParagraph(fontContext, paragraph, flow);
  const auto warmEndTime = Clock::now();

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1400, 900));
  surface->getCanvas()->clear(kPaper);
  layout.draw(surface->getCanvas(), paragraph);
  writePng(surface.get(), outputDirectory / "babel.png");
  std::printf("Scene G — babel confetti: %zu tokens, %zu runs, cold %.1f us, "
              "warm %.1f us\n",
              paragraph.words().size(), layout.runs.size(),
              toMicroseconds(coldEndTime - coldStartTime),
              toMicroseconds(warmEndTime - coldEndTime));
}

// OpenType features made visible: ligature control, discretionary
// ligatures, small caps, lining vs oldstyle figures — each row is the same
// engine with a different FontFeature list (part of the shape-cache key).
void sceneFeatures(FontContext &fontContext,
                   const std::filesystem::path &outputDirectory) {
  sk_sp<SkTypeface> hoeflerTypeface =
      fontContext.fontManager()->matchFamilyStyle("Hoefler Text",
                                                  SkFontStyle());
  if (!hoeflerTypeface) {
    std::printf("Scene G — features: Hoefler Text missing, skipped\n\n");
    return;
  }

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(980, 560));
  SkCanvas *canvas = surface->getCanvas();
  canvas->clear(kPaper);

  struct Row {
    const char *label;
    const char *text;
    std::vector<FontFeature> fontFeatures;
  };
  const Row rows[] = {
      {"default (liga on, oldstyle figures)",
       "The office staff filed 1234567890 affidavits.",
       {}},
      {"liga=0 clig=0 (ligatures off)",
       "The office staff filed 1234567890 affidavits.",
       {{"liga", 0}, {"clig", 0}}},
      {"dlig=1 (discretionary ct/st ligatures)",
       "The strict architect stood fast.",
       {{"dlig", 1}}},
      {"smcp=1 (small caps)",
       "The office staff filed affidavits.",
       {{"smcp", 1}}},
      {"lnum=1 (lining figures)",
       "Figures 1234567890 rise to the cap height.",
       {{"lnum", 1}}},
      {"frac=1 (fractions)", "Mix 1/2 cup with 3/4 spoon.", {{"frac", 1}}},
  };

  float rowTop = 30;
  for (const Row &row : rows) {
    Paragraph caption;
    caption.appendText(row.label, style(12, kAccent));
    BlockFlow capFlow(SkRect::MakeXYWH(40, rowTop, 900, 18));
    layoutParagraph(fontContext, caption, capFlow).draw(canvas, caption);

    TextStyle body = style(30, kInk);
    body.shaping.typeface = hoeflerTypeface;
    body.shaping.fontFeatures = row.fontFeatures;
    Paragraph paragraph;
    paragraph.appendText(row.text, body);
    BlockFlow flow(SkRect::MakeXYWH(40, rowTop + 20, 900, 48));
    layoutParagraph(fontContext, paragraph, flow).draw(canvas, paragraph);
    rowTop += 88;
  }

  writePng(surface.get(), outputDirectory / "features.png");
  std::printf("Scene G — OpenType features panel written\n\n");
}

// ── Scene I: arbitrary SkPath exclusions (star / heart / donut hole) ──────

void sceneShapes(FontContext &fontContext,
                 const std::filesystem::path &outputDirectory) {
  SkFontMgr *fontManager = fontContext.fontManager();
  TextStyle body = style(16.5f, kInk);
  body.shaping.typeface =
      fontManager->matchFamilyStyle("Noto Serif", SkFontStyle());

  Paragraph paragraph;
  for (int repetitionIndex = 0; repetitionIndex < 7; ++repetitionIndex)
    paragraph.appendText(
        "Text no longer flows around circles and boxes alone: any SkPath — "
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
  flow.shapes().push_back(ExclusionFlow::Shape::fromPath(star.detach(), 10));
  flow.shapes().push_back(ExclusionFlow::Shape::fromPath(heart.detach(), 10));
  flow.shapes().push_back(ExclusionFlow::Shape::fromPath(donut.detach(), 8));
  flow.setMinIntervalWidth(46);

  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kJustify;
  options.lineMetrics.height = 26;

  const auto layoutStartTime = Clock::now();
  ParagraphLayout layout =
      layoutParagraph(fontContext, paragraph, flow, options);
  const auto layoutEndTime = Clock::now();

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1000, 880));
  SkCanvas *canvas = surface->getCanvas();
  canvas->clear(kPaper);
  SkPaint shapePaint;
  shapePaint.setAntiAlias(true);
  shapePaint.setColor(kShape);
  for (const auto &shape : flow.shapes())
    canvas->drawPath(shape.path, shapePaint);
  layout.draw(canvas, paragraph);

  writePng(surface.get(), outputDirectory / "shapes.png");
  std::printf("Scene I — SkPath exclusions written (layout %.1f us, %d "
              "lines)\n\n",
              toMicroseconds(layoutEndTime - layoutStartTime),
              layout.lineCount);
}

// ── Scene H: CJK typography — vertical-rl, ruby, kenten, tate-chu-yoko ────
//
// Ruby (furigana) and kenten (emphasis dots) are deliberately *not* library
// features: they are a dozen lines each on top of the layout's placed runs
// — the "build externally on TextFlow" pattern. Tate-chu-yoko needs the
// breaker's cooperation, so it *is* a library feature
// (VerticalForm::kTateChuYoko).

// Placed extent of a UTF-16 range along its column/line: every run whose
// word overlaps the range contributes [pen, pen + width) from its origin.
struct RangeExtent {
  bool valid = false;
  int lineIndex = 0;
  SkPoint origin = {0, 0}; // first covering run's origin (on the axis)
  float flowBegin = 0;
  float flowEnd = 0;
};

/** Measures a text range along the flow direction of its placed line. */
RangeExtent placedExtent(const Paragraph &paragraph,
                         const ParagraphLayout &layout, uint32_t rangeStart,
                         uint32_t rangeEnd) {
  RangeExtent extent;
  for (const PositionedRun &run : layout.runs) {
    const Word &word = paragraph.words()[run.wordIndex];
    if (word.textEnd <= rangeStart || word.textBegin >= rangeEnd ||
        run.transformed)
      continue;
    if (!extent.valid) {
      extent.valid = true;
      extent.lineIndex = run.lineIndex;
      extent.origin = run.origin;
      extent.flowBegin = 0;
      extent.flowEnd = word.width;
    } else if (run.lineIndex == extent.lineIndex) {
      // Same column/line: extend along the flow direction (vertical here).
      const float offset = run.origin.y() - extent.origin.y();
      extent.flowBegin = std::min(extent.flowBegin, offset);
      extent.flowEnd = std::max(extent.flowEnd, offset + word.width);
    }
  }
  return extent;
}

void sceneCjk(FontContext &fontContext,
              const std::filesystem::path &outputDirectory) {
  SkFontMgr *fontManager = fontContext.fontManager();
  sk_sp<SkTypeface> minchoTypeface =
      fontManager->matchFamilyStyle("Hiragino Mincho ProN", SkFontStyle());
  sk_sp<SkTypeface> notoSansTypeface =
      fontManager->matchFamilyStyle("Noto Sans", SkFontStyle());
  if (!minchoTypeface) {
    std::printf("Scene H — cjk: Hiragino Mincho ProN missing, skipped\n\n");
    return;
  }

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(980, 700));
  SkCanvas *canvas = surface->getCanvas();
  canvas->clear(kPaper);

  const float fontSize = 26.0f;
  auto japaneseStyle = [&](float requestedFontSize,
                           VerticalForm verticalForm = VerticalForm::kAuto) {
    TextStyle textStyle = style(requestedFontSize, kInk, "ja");
    textStyle.shaping.typeface = minchoTypeface;
    textStyle.shaping.verticalForm = verticalForm;
    return textStyle;
  };

  // ── Vertical-rl block (right side of the page) ─────────────────────────
  Paragraph verticalParagraph;
  verticalParagraph.appendText("縦組みの文章は、上から下へ、",
                               japaneseStyle(fontSize));
  verticalParagraph.appendText("右から左へと流れる。平成",
                               japaneseStyle(fontSize));
  verticalParagraph.appendText(
      "31", japaneseStyle(fontSize, VerticalForm::kTateChuYoko));
  verticalParagraph.appendText("年", japaneseStyle(fontSize));
  verticalParagraph.appendText(
      "12", japaneseStyle(fontSize, VerticalForm::kTateChuYoko));
  verticalParagraph.appendText("月、", japaneseStyle(fontSize));
  verticalParagraph.appendText("TextFlow", japaneseStyle(fontSize));
  verticalParagraph.appendText("は縦書きに対応した。", japaneseStyle(fontSize));
  verticalParagraph.setWritingMode(WritingMode::kVerticalRL);

  const SkRect verticalBounds = SkRect::MakeXYWH(430, 80, 500, 540);
  VerticalBlockFlow verticalFlow(verticalBounds);
  ParagraphLayoutOptions verticalOptions;
  verticalOptions.lineMetrics.height = fontSize * 1.9f;
  ParagraphLayout verticalLayout = layoutParagraph(
      fontContext, verticalParagraph, verticalFlow, verticalOptions);
  verticalLayout.draw(canvas, verticalParagraph);

  // Ruby (furigana): a small vertical paragraph laid along the base's
  // column, offset into the inter-column gap.
  auto drawVerticalRuby = [&](std::u16string_view baseText,
                              const char *rubyText) {
    const std::vector<CharRange> matches =
        findAllOccurrences(verticalParagraph, baseText);
    if (matches.empty())
      return;
    const RangeExtent extent = placedExtent(verticalParagraph, verticalLayout,
                                            matches[0].start, matches[0].end);
    if (!extent.valid)
      return;
    Paragraph ruby;
    ruby.appendText(rubyText, japaneseStyle(fontSize * 0.5f));
    ruby.setWritingMode(WritingMode::kVerticalRL);
    const float length = ruby.naturalWidth(fontContext);
    const float mid =
        extent.origin.y() + (extent.flowBegin + extent.flowEnd) * 0.5f;
    LineSetFlow flow;
    flow.lines().push_back({LineInterval{
        {extent.origin.x() + fontSize * 0.62f, mid - length * 0.5f},
        {0, 1},
        length + 1}});
    layoutParagraph(fontContext, ruby, flow).draw(canvas, ruby);
  };
  drawVerticalRuby(u"縦組", "たてぐみ");
  drawVerticalRuby(u"文章", "ぶんしょう");
  drawVerticalRuby(u"対応", "たいおう");

  // Kenten (emphasis dots): one sesame dot beside each emphasized glyph.
  {
    const std::vector<CharRange> matches =
        findAllOccurrences(verticalParagraph, u"上から下へ");
    SkPaint dot;
    dot.setAntiAlias(true);
    dot.setColor(kAccent);
    if (!matches.empty()) {
      for (const PositionedRun &run : verticalLayout.runs) {
        const Word &word = verticalParagraph.words()[run.wordIndex];
        if (word.textEnd <= matches[0].start ||
            word.textBegin >= matches[0].end || run.transformed ||
            !run.shaped->vertical)
          continue;
        float penAdvance = 0;
        const ShapedWord &shapedWord = *run.shaped;
        for (size_t glyphIndex = 0; glyphIndex < shapedWord.advances.size();
             ++glyphIndex) {
          // Clusters are offsets into the shaped segment, which starts at
          // the word's first character — dot only the emphasized glyphs.
          const uint32_t textOffset =
              word.textBegin + shapedWord.clusters[glyphIndex];
          if (textOffset >= matches[0].start && textOffset < matches[0].end)
            canvas->drawCircle(run.origin.x() + fontSize * 0.60f,
                               run.origin.y() + penAdvance +
                                   shapedWord.advances[glyphIndex] * 0.5f,
                               fontSize * 0.07f, dot);
          penAdvance += shapedWord.advances[glyphIndex];
        }
      }
    }
  }

  // ── Horizontal block with ruby + kenten (left side) ────────────────────
  Paragraph horizontalParagraph;
  horizontalParagraph.appendText(
      "漢字にルビを振ると、誰でも読みやすい。強調したい語には圏点を打つ。",
      japaneseStyle(fontSize * 0.85f));
  const SkRect horizontalBounds = SkRect::MakeXYWH(50, 120, 330, 400);
  BlockFlow horizontalFlow(horizontalBounds);
  ParagraphLayoutOptions horizontalOptions;
  horizontalOptions.lineMetrics.height = fontSize * 2.0f;
  ParagraphLayout horizontalLayout = layoutParagraph(
      fontContext, horizontalParagraph, horizontalFlow, horizontalOptions);
  horizontalLayout.draw(canvas, horizontalParagraph);

  auto drawHorizontalRuby = [&](std::u16string_view baseText,
                                const char *rubyText) {
    const std::vector<CharRange> matches =
        findAllOccurrences(horizontalParagraph, baseText);
    if (matches.empty())
      return;
    // Horizontal extent along the line this time.
    float rangeLeft = 0;
    float rangeRight = 0;
    float baseline = 0;
    bool valid = false;
    for (const PositionedRun &run : horizontalLayout.runs) {
      const Word &word = horizontalParagraph.words()[run.wordIndex];
      if (word.textEnd <= matches[0].start || word.textBegin >= matches[0].end)
        continue;
      if (!valid) {
        valid = true;
        rangeLeft = run.origin.x();
        rangeRight = run.origin.x() + word.width;
        baseline = run.origin.y();
      } else if (run.origin.y() == baseline) {
        rangeRight = std::max(rangeRight, run.origin.x() + word.width);
      }
    }
    if (!valid)
      return;
    Paragraph ruby;
    ruby.appendText(rubyText, japaneseStyle(fontSize * 0.42f));
    const float width = ruby.naturalWidth(fontContext);
    LineSetFlow flow;
    flow.lines().push_back(
        {LineInterval{{(rangeLeft + rangeRight) * 0.5f - width * 0.5f,
                       baseline - fontSize * 0.98f},
                      {1, 0},
                      width + 1}});
    layoutParagraph(fontContext, ruby, flow).draw(canvas, ruby);
  };
  drawHorizontalRuby(u"漢字", "かんじ");
  drawHorizontalRuby(u"圏点", "けんてん");

  {
    const std::vector<CharRange> matches =
        findAllOccurrences(horizontalParagraph, u"強調");
    SkPaint dot;
    dot.setAntiAlias(true);
    dot.setColor(kAccent);
    if (!matches.empty()) {
      for (const PositionedRun &run : horizontalLayout.runs) {
        const Word &word = horizontalParagraph.words()[run.wordIndex];
        if (word.textEnd <= matches[0].start ||
            word.textBegin >= matches[0].end)
          continue;
        float penAdvance = 0;
        const ShapedWord &shapedWord = *run.shaped;
        for (size_t glyphIndex = 0; glyphIndex < shapedWord.advances.size();
             ++glyphIndex) {
          const uint32_t textOffset =
              word.textBegin + shapedWord.clusters[glyphIndex];
          if (textOffset >= matches[0].start && textOffset < matches[0].end)
            canvas->drawCircle(run.origin.x() + penAdvance +
                                   shapedWord.advances[glyphIndex] * 0.5f,
                               run.origin.y() - fontSize * 0.92f,
                               fontSize * 0.06f, dot);
          penAdvance += shapedWord.advances[glyphIndex];
        }
      }
    }
  }

  // Captions.
  auto drawCaption = [&](const char *text, float positionX, float positionY) {
    TextStyle captionStyle = style(13, kBlue);
    captionStyle.shaping.typeface = notoSansTypeface;
    Paragraph paragraph;
    paragraph.appendText(text, captionStyle);
    BlockFlow flow(SkRect::MakeXYWH(positionX, positionY, 400, 18));
    layoutParagraph(fontContext, paragraph, flow).draw(canvas, paragraph);
  };
  drawCaption("horizontal: ruby + kenten (external utilities)", 50, 90);
  drawCaption("vertical-rl: UTR#50 mixed orientation, 'vert' forms,", 430, 630);
  drawCaption("tate-chu-yoko digits, ruby + kenten in the column gap", 430,
              648);

  writePng(surface.get(), outputDirectory / "cjk.png");
  std::printf("Scene H — CJK vertical/ruby/kenten/tate-chu-yoko written\n\n");
}

} // namespace

int main(int argc, char **argv) {
  const int frames = argc > 1 ? std::max(1, std::atoi(argv[1])) : 240;

  const std::filesystem::path outputDirectory = "textflow_demo_out";
  std::filesystem::create_directories(outputDirectory);

  FontContext fontContext(SkFontMgr_New_CoreText(nullptr));

  sceneExclusions(fontContext, frames, outputDirectory);
  sceneKnuthPlass(fontContext, frames, outputDirectory);
  sceneFreeform(fontContext, outputDirectory);
  sceneExtreme(fontContext, outputDirectory);
  sceneTypography(fontContext, outputDirectory);
  sceneBabel(fontContext, outputDirectory);
  sceneFeatures(fontContext, outputDirectory);
  sceneCjk(fontContext, outputDirectory);
  sceneShapes(fontContext, outputDirectory);
  scenePretext(fontContext, frames, outputDirectory);

  std::printf("PNGs written to %s\n",
              std::filesystem::absolute(outputDirectory).string().c_str());
  return 0;
}
