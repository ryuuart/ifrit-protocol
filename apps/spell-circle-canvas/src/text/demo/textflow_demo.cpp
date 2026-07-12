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

#include <include/core/SkCanvas.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkStream.h>
#include <include/core/SkSurface.h>
#include <include/core/SkBlurTypes.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkTileMode.h>
#include <include/effects/SkGradient.h>
#include <include/encode/SkPngEncoder.h>
#include <include/ports/SkFontMgr_mac_ct.h>

#include <algorithm>
#include <chrono>
#include <array>
#include <map>
#include <cmath>
#include <cstdio>
#include <filesystem>
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
  std::vector<double> us;
  void add(double v) { us.push_back(v); }
  void report(const char *label) const {
    if (us.empty())
      return;
    std::vector<double> sorted = us;
    std::sort(sorted.begin(), sorted.end());
    const double mean =
        std::accumulate(sorted.begin(), sorted.end(), 0.0) / sorted.size();
    const double p50 = sorted[sorted.size() / 2];
    const double p95 = sorted[static_cast<size_t>(sorted.size() * 0.95)];
    const double max = sorted.back();
    std::printf("  %-28s mean %7.1f us   p50 %7.1f us   p95 %7.1f us   max "
                "%7.1f us   (%zu frames)\n",
                label, mean, p50, p95, max, sorted.size());
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

double toUs(Clock::duration d) {
  return std::chrono::duration<double, std::micro>(d).count();
}

TextStyle style(float size, SkColor color = kInk, const char *lang = "") {
  TextStyle s;
  s.shaping.size = size;
  s.shaping.language = lang;
  s.paint.color = color;
  return s;
}

// ── Scene A: moving exclusions through a mixed-script rich paragraph ──────

void sceneExclusions(FontContext &ctx, int frames,
                     const std::filesystem::path &outDir) {
  std::printf("Scene A — mixed-script paragraph reflowing around moving "
              "shapes\n");

  ParagraphBuilder builder(style(17));
  builder.addText("Typography is the craft of arranging type. ");
  builder.pushStyle(style(22, kAccent)).addText("Glyphs flow").pop();
  builder.addText(" around obstacles the way water flows around stones. ");
  builder.pushStyle(style(19, kBlue, "ja"))
      .addText("日本語のテキストも同じ流れに乗って、")
      .pop();
  builder.addText("shapes push the lines apart and the words find their way. ");
  builder.pushStyle(style(19, kInk, "ko"))
      .addText("한국어 단어들도 자연스럽게 흐르고, ")
      .pop();
  builder.pushStyle(style(19, kInk, "zh"))
      .addText("中文字符同样围绕形状排布。")
      .pop();
  builder.addText("Latin and CJK scripts mix freely because every word is "
                  "shaped once, cached, and repositioned with pure "
                  "arithmetic. ");
  builder.addText("The moving circles below force a full line-breaking pass "
                  "every frame, yet the layout stays far under a "
                  "millisecond. ");
  builder.pushStyle(style(17, kAccent)).addText("Performance is key. ").pop();
  builder.addText(
      "When a shape slides across a line, the line splits into fragments and "
      "each fragment justifies itself independently; when the shape moves "
      "on, the fragments knit back together as if nothing happened. ");
  builder.pushStyle(style(19, kBlue, "ja"))
      .addText("形が動くたびに行は裂け、また元通りに繋がる。")
      .pop();
  builder.addText("No word is ever reshaped for any of this — the shape "
                  "cache hands back the same glyph runs and only their "
                  "positions change, frame after frame after frame. ");
  builder.pushStyle(style(19, kInk, "ko"))
      .addText("모든 글자는 한 번만 성형되고 계속 재사용됩니다. ")
      .pop();
  builder.addText("That is the whole trick, and it is enough to keep a "
                  "richly styled, multi-script paragraph reflowing at well "
                  "over a thousand frames per second.");
  Paragraph para = builder.build();

  ExclusionFlow flow(SkRect::MakeXYWH(40, 40, 820, 660));
  flow.shapes().push_back(
      {ExclusionFlow::Shape::kCircle, SkRect::MakeXYWH(180, 140, 190, 190), 10});
  flow.shapes().push_back(
      {ExclusionFlow::Shape::kCircle, SkRect::MakeXYWH(520, 380, 150, 150), 10});
  flow.shapes().push_back(
      {ExclusionFlow::Shape::kRect, SkRect::MakeXYWH(360, 250, 170, 110), 10});
  flow.setMinIntervalWidth(56);

  LayoutOptions opts;
  opts.alignment = Alignment::kJustify;
  opts.lineHeight = 30;

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(900, 740));
  SkCanvas *canvas = surface->getCanvas();

  const char *swaps[] = {"water", "sand ", "wind ", "light"};
  const SkColor flashes[] = {kAccent, kBlue, kInk};

  TimingStats layoutTime, drawTime;
  const uint64_t shapeCallsBefore = ctx.stats().shapeCalls;

  for (int frame = 0; frame < frames; ++frame) {
    const float t = static_cast<float>(frame) * 0.045f;

    // Animate the shapes through the text.
    flow.shapes()[0].bounds = SkRect::MakeXYWH(
        180 + 260 * std::sin(t), 140 + 200 * std::sin(t * 0.6f), 190, 190);
    flow.shapes()[1].bounds = SkRect::MakeXYWH(
        520 - 220 * std::sin(t * 0.8f), 380 + 160 * std::cos(t * 0.5f), 150,
        150);
    flow.shapes()[2].bounds = SkRect::MakeXYWH(
        360 + 180 * std::cos(t * 0.7f), 250 + 240 * std::sin(t * 0.35f), 170,
        110);

    // Periodic rich-text updates, mid-animation.
    if (frame > 0 && frame % 24 == 0)
      para.setPaint(15, 30, PaintStyle{flashes[(frame / 24) % 3]});
    if (frame > 0 && frame % 48 == 0) {
      // "water" (5 chars at a fixed offset inside the third sentence).
      const size_t at = para.text().find(u"water");
      if (at != std::u16string::npos)
        para.replaceText(static_cast<uint32_t>(at),
                         static_cast<uint32_t>(at + 5),
                         swaps[(frame / 48) % 4]);
    }

    const auto t0 = Clock::now();
    Layout layout = layoutParagraph(ctx, para, flow, opts);
    const auto t1 = Clock::now();

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
    layout.draw(canvas, para);
    const auto t2 = Clock::now();

    layoutTime.add(toUs(t1 - t0));
    drawTime.add(toUs(t2 - t1));

    if (frame % 60 == 0 || frame == frames - 1) {
      char name[64];
      std::snprintf(name, sizeof name, "exclusions_f%03d.png", frame);
      writePng(surface.get(), outDir / name);
    }
  }

  layoutTime.report("layout (reflow per frame)");
  drawTime.report("draw (CPU raster)");
  std::printf("  hb_shape calls during animation: %llu (edits only)\n\n",
              static_cast<unsigned long long>(ctx.stats().shapeCalls -
                                              shapeCallsBefore));
}

// ── Scene B: Knuth-Plass justified paragraph with live word updates ───────

void sceneKnuthPlass(FontContext &ctx, int frames,
                     const std::filesystem::path &outDir) {
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
  builder.pushStyle(style(16.5f, kAccent)).addText("cool").pop();
  builder.addText(" fountain; and when she was bored she took a golden ball, "
                  "and threw it up on high and caught it; and this ball was "
                  "her favorite plaything. ");
  builder.pushStyle(style(16.5f, kBlue, "ja"))
      .addText("グリム童話は日本語でも読まれ、")
      .pop();
  builder.addText("the tale crosses scripts without breaking its measure.");
  Paragraph para = builder.build();

  BlockFlow flow(SkRect::MakeXYWH(50, 40, 560, 640));
  LayoutOptions opts;
  opts.breaker = Breaker::kKnuthPlass;
  opts.alignment = Alignment::kJustify;
  opts.lineHeight = 27;

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(660, 720));

  const char *synonyms[] = {"cool", "cold", "calm", "deep"};
  const char *ballWords[] = {"golden", "silver", "copper", "crystal"};

  TimingStats layoutTime;
  const uint64_t shapeCallsBefore = ctx.stats().shapeCalls;

  for (int frame = 0; frame < frames; ++frame) {
    // Word updates while the paragraph stays justified.
    if (frame > 0 && frame % 20 == 0) {
      const std::u16string &text = para.text();
      for (const char *candidate : synonyms) {
        std::u16string needle(candidate, candidate + 4);
        const size_t at = text.find(needle);
        if (at != std::u16string::npos) {
          para.replaceText(static_cast<uint32_t>(at),
                           static_cast<uint32_t>(at + 4),
                           synonyms[(frame / 20) % 4]);
          break;
        }
      }
    }
    if (frame > 0 && frame % 30 == 0) {
      for (const char *candidate : ballWords) {
        std::u16string needle;
        for (const char *p = candidate; *p; ++p)
          needle.push_back(static_cast<char16_t>(*p));
        const size_t at = para.text().find(needle);
        if (at != std::u16string::npos) {
          para.replaceText(
              static_cast<uint32_t>(at),
              static_cast<uint32_t>(at + needle.size()),
              ballWords[(frame / 30) % 4]);
          break;
        }
      }
    }

    const auto t0 = Clock::now();
    Layout layout = layoutParagraph(ctx, para, flow, opts);
    const auto t1 = Clock::now();
    layoutTime.add(toUs(t1 - t0));

    if (frame % 60 == 0 || frame == frames - 1) {
      surface->getCanvas()->clear(kPaper);
      layout.draw(surface->getCanvas(), para);
      char name[64];
      std::snprintf(name, sizeof name, "knuthplass_f%03d.png", frame);
      writePng(surface.get(), outDir / name);
    }
  }

  layoutTime.report("layout (KP justify per frame)");
  std::printf("  hb_shape calls during animation: %llu (edits only)\n\n",
              static_cast<unsigned long long>(ctx.stats().shapeCalls -
                                              shapeCallsBefore));
}

// ── Scene C: freeform typography (paths and arbitrary line sets) ──────────

void sceneFreeform(FontContext &ctx, const std::filesystem::path &outDir) {
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
    for (int i = 0; i <= steps; ++i) {
      const float u = static_cast<float>(i) / steps;
      const float angle = u * 6.0f * 3.14159265f;
      const float radius = 40 + 150 * u;
      const SkPoint p = {center.x() + radius * std::cos(angle),
                         center.y() + radius * std::sin(angle)};
      if (i == 0)
        spiralBuilder.moveTo(p);
      else
        spiralBuilder.lineTo(p);
    }
    const SkPath spiral = spiralBuilder.detach();
    Paragraph para;
    para.appendText(
        "text can follow any path — 螺旋に沿って文字が流れ、 나선을 따라 "
        "글자가 흐르고, 文字沿着螺旋流动 — and every glyph stays tangent to "
        "the curve while the words keep their cached shapes",
        style(17));
    PathFlow flow(spiral);
    const auto t0 = Clock::now();
    Layout layout = layoutParagraph(ctx, para, flow);
    const auto t1 = Clock::now();
    Layout warm = layoutParagraph(ctx, para, flow); // caches primed
    const auto t2 = Clock::now();
    layout.draw(canvas, para);
    std::printf("  spiral layout: cold %.1f us, warm %.1f us\n",
                toUs(t1 - t0), toUs(t2 - t1));
  }

  // Arbitrary slanted line set (Pretext-style freedom).
  {
    LineSetFlow flow;
    for (int i = 0; i < 6; ++i) {
      // Gentle alternating slopes; rise stays well under the line pitch so
      // neighboring lines never collide.
      const float y = 80.0f + static_cast<float>(i) * 64.0f;
      const float slope = (i % 2 == 0) ? 0.07f : -0.055f;
      const float inv = 1.0f / std::sqrt(1 + slope * slope);
      flow.lines().push_back(
          {LineInterval{{500, y}, {inv, slope * inv}, 350}});
    }
    Paragraph para;
    para.appendText("lines need not be horizontal nor parallel; ",
                    style(18, kInk));
    para.appendText("각 행은 자유로운 방향을 갖고 ", style(18, kBlue, "ko"));
    para.appendText("每一行都有自己的方向 ", style(18, kAccent, "zh"));
    para.appendText("and the paragraph simply flows through whatever pen "
                    "strokes you hand it.",
                    style(18, kInk));
    const auto t0 = Clock::now();
    Layout layout = layoutParagraph(ctx, para, flow);
    const auto t1 = Clock::now();
    Layout warm = layoutParagraph(ctx, para, flow);
    const auto t2 = Clock::now();
    layout.draw(canvas, para);
    std::printf("  line-set layout: cold %.1f us, warm %.1f us\n",
                toUs(t1 - t0), toUs(t2 - t1));
  }

  writePng(surface.get(), outDir / "freeform.png");
  std::printf("\n");
}


// ── Scene D: extreme geometries ───────────────────────────────────────────

void sceneExtreme(FontContext &ctx, const std::filesystem::path &outDir) {
  std::printf("Scene D — extreme geometries (zigzag, scribble, bumps, "
              "confetti)\n");

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1000, 860));
  SkCanvas *canvas = surface->getCanvas();
  canvas->clear(kPaper);

  auto timeIt = [&](const char *label, auto &&fn) {
    const auto t0 = Clock::now();
    fn();
    const auto t1 = Clock::now();
    fn(); // warm
    const auto t2 = Clock::now();
    std::printf("  %-10s cold %8.1f us, warm %7.1f us\n", label,
                toUs(t1 - t0), toUs(t2 - t1));
  };

  // Zigzag: each *line* is a chain of alternating up/down segments; words
  // march through the chain segment by segment.
  {
    Paragraph para;
    para.appendText(
        "zigzag lines carry words up and down and up and down while the "
        "layout treats every slanted segment as just another interval — "
        "ジグザグの線の上でも単語は流れ続ける — 지그재그 선 위에서도",
        style(16));
    LineSetFlow flow;
    for (int line = 0; line < 3; ++line) {
      std::vector<LineInterval> segs;
      float x = 30, y = 70.0f + static_cast<float>(line) * 90.0f;
      for (int k = 0; k < 9; ++k) {
        const float slope = (k % 2 == 0) ? 0.45f : -0.45f;
        const float inv = 1.0f / std::sqrt(1 + slope * slope);
        segs.push_back({LineInterval{{x, y}, {inv, slope * inv}, 52}});
        x += 52 * inv;
        y += 52 * slope * inv;
      }
      flow.lines().push_back(std::move(segs));
    }
    Layout layout;
    timeIt("zigzag", [&] { layout = layoutParagraph(ctx, para, flow); });
    layout.draw(canvas, para);
  }

  // Scribble: a wandering quadratic path.
  {
    SkPathBuilder pb;
    pb.moveTo(60, 420);
    std::mt19937 rng(11);
    float px = 60, py = 420;
    for (int k = 0; k < 9; ++k) {
      const float cx = px + 40 + static_cast<float>(rng() % 90);
      const float cy = 340 + static_cast<float>(rng() % 220);
      px = px + 80 + static_cast<float>(rng() % 60);
      py = 360 + static_cast<float>(rng() % 200);
      pb.quadTo(cx, cy, px, py);
    }
    Paragraph para;
    para.appendText(
        "a scribbled stroke is still a line to this engine, the pen just "
        "wanders wherever the curve goes — 落書きの線でも文字は流れる",
        style(15, kBlue));
    PathFlow flow(pb.detach());
    Layout layout;
    timeIt("scribble", [&] { layout = layoutParagraph(ctx, para, flow); });
    layout.draw(canvas, para);
  }

  // Extremely bumpy: high-frequency sine paths.
  {
    for (int line = 0; line < 2; ++line) {
      SkPathBuilder pb;
      const float baseY = 640.0f + static_cast<float>(line) * 80.0f;
      const float amp = 14.0f + 10.0f * static_cast<float>(line);
      pb.moveTo(30, baseY);
      for (int x = 30; x <= 960; x += 6)
        pb.lineTo(static_cast<float>(x),
                  baseY + amp * std::sin(static_cast<float>(x) *
                                         (0.055f - 0.015f * line)));
      Paragraph para;
      para.appendText(
          line == 0
              ? "extremely bumpy baselines shake every glyph yet the words "
                "keep their spacing along the arc length of the wave"
              : "波打つベースラインの上でも字形は接線に沿って進む — 파도치는 "
                "기준선 위에서도 글자는 계속 흐른다 — 波浪基线上的文字",
          style(15, line == 0 ? kInk : kAccent));
      PathFlow flow(pb.detach());
      Layout layout;
      timeIt(line == 0 ? "bumpy" : "bumpy2",
             [&] { layout = layoutParagraph(ctx, para, flow); });
      layout.draw(canvas, para);
    }
  }

  writePng(surface.get(), outDir / "extreme.png");

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
    std::mt19937 rng(5);
    for (int i = 0; i < 120; ++i) {
      if (i % 3 == 2)
        letters += cjk[rng() % 12];
      else
        letters += static_cast<char>('a' + rng() % 26);
      letters += ' ';
    }
    Paragraph para;
    para.appendText(letters, style(26));
    // Color a few random spans so the confetti is multicolored.
    const uint32_t n = static_cast<uint32_t>(para.text().size());
    for (uint32_t at = 0; at + 8 < n; at += 8)
      para.setPaint(at, at + 4,
                    PaintStyle{(at / 8) % 3 == 0   ? kAccent
                               : (at / 8) % 3 == 1 ? kBlue
                                                   : kInk});

    LineSetFlow flow;
    for (int i = 0; i < 120; ++i) {
      const float x = 20.0f + static_cast<float>(rng() % 940);
      const float y = 30.0f + static_cast<float>(rng() % 560);
      const float angle =
          static_cast<float>(rng() % 628) * 0.01f; // 0..2π
      flow.lines().push_back({LineInterval{
          {x, y}, {std::cos(angle), std::sin(angle)}, 34}});
    }
    Layout layout;
    timeIt("confetti", [&] { layout = layoutParagraph(ctx, para, flow); });
    layout.draw(confetti->getCanvas(), para);
    writePng(confetti.get(), outDir / "confetti.png");
  }
  std::printf("\n");
}

// ── Scene E: typographic options ──────────────────────────────────────────

void sceneTypography(FontContext &ctx, const std::filesystem::path &outDir) {
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
  const Alignment lastModes[] = {Alignment::kStart, Alignment::kCenter,
                                 Alignment::kEnd};
  const char *labels[] = {"last: left", "last: center", "last: right",
                          "last: full"};
  for (int i = 0; i < 4; ++i) {
    const float x = 30.0f + static_cast<float>(i % 2) * 260.0f;
    const float y = 40.0f + static_cast<float>(i / 2) * 190.0f;
    Paragraph caption;
    caption.appendText(labels[i], style(12, kAccent));
    BlockFlow capFlow(SkRect::MakeXYWH(x, y - 24, 220, 20));
    layoutParagraph(ctx, caption, capFlow).draw(canvas, caption);

    Paragraph para;
    para.appendText(sample, style(14.5f));
    BlockFlow flow(SkRect::MakeXYWH(x, y, 220, 160));
    LayoutOptions opts;
    opts.breaker = Breaker::kKnuthPlass;
    opts.alignment = Alignment::kJustify;
    if (i < 3)
      opts.lastLineAlignment = lastModes[i];
    else
      opts.justifyLastLine = true;
    layoutParagraph(ctx, para, flow, opts).draw(canvas, para);
  }

  // Narrow hyphenated Knuth-Plass column (soft hyphens marked with ­).
  {
    Paragraph caption;
    caption.appendText("KP + soft hyphens, 130px", style(12, kAccent));
    BlockFlow capFlow(SkRect::MakeXYWH(570, 16, 220, 20));
    layoutParagraph(ctx, caption, capFlow).draw(canvas, caption);

    Paragraph para;
    para.appendText(
        "In these as­ton­ish­ing­ly nar­row "
        "col­umns, dis­cre­tion­ary breaks keep "
        "jus­ti­fi­ca­tion from tear­ing the "
        "spac­ing apart, ex­act­ly as a book "
        "com­pos­i­tor would want.",
        style(14.5f));
    BlockFlow flow(SkRect::MakeXYWH(570, 40, 130, 400));
    LayoutOptions opts;
    opts.breaker = Breaker::kKnuthPlass;
    opts.alignment = Alignment::kJustify;
    const auto t0 = Clock::now();
    Layout layout = layoutParagraph(ctx, para, flow, opts);
    const auto t1 = Clock::now();
    layout.draw(canvas, para);
    std::printf("  hyphenated 130px KP column: %.1f us cold\n", toUs(t1 - t0));
  }

  // Effects: drop shadow, gradient shader, glyph blur — all paint-only.
  {
    Paragraph para;
    TextStyle title = style(40, SK_ColorWHITE);
    title.paint.shadows.push_back({0x99000000, {3, 4}, 3.0f});
    para.appendText("Shadowed ", title);

    TextStyle gradient = style(40);
    const SkPoint pts[2] = {{730, 40}, {1030, 240}};
    const SkColor4f colors[2] = {SkColor4f::FromColor(kAccent),
                                 SkColor4f::FromColor(kBlue)};
    gradient.paint.shader = SkShaders::LinearGradient(
        pts, SkGradient(SkGradient::Colors({colors, 2}, SkTileMode::kClamp),
                        SkGradient::Interpolation()));
    gradient.paint.shadows.push_back({0x44000000, {2, 2}, 2.0f});
    para.appendText("gradient ", gradient);

    TextStyle blurred = style(40, kInk);
    blurred.paint.maskFilter =
        SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, 2.4f);
    para.appendText("blur", blurred);

    BlockFlow flow(SkRect::MakeXYWH(730, 40, 310, 400));
    layoutParagraph(ctx, para, flow).draw(canvas, para);
  }

  // Mixed families and sizes in one flow (serif / sans / mono / CJK).
  {
    // Noto Sans/Serif (user-installed variable fonts): richer OpenType
    // shaping coverage than the system defaults.
    SkFontMgr *mgr = ctx.fontMgr();
    TextStyle serif = style(20, kInk);
    serif.shaping.typeface =
        mgr->matchFamilyStyle("Noto Serif", SkFontStyle());
    TextStyle sans = style(17, kBlue);
    sans.shaping.typeface = mgr->matchFamilyStyle("Noto Sans", SkFontStyle());
    TextStyle mono = style(14, kAccent);
    mono.shaping.typeface = mgr->matchFamilyStyle("Menlo", SkFontStyle());

    Paragraph para;
    para.appendText("Serif voices carry the body, ", serif);
    para.appendText("a grotesque interjects, ", sans);
    para.appendText("code whispers in mono, ", mono);
    para.appendText("そして日本語がフォールバックで加わり、", sans);
    para.appendText("모든 서체가 한 단락 안에서 섞인다 ", serif);
    para.appendText("— one paragraph, many fonts, one shape cache.", serif);

    BlockFlow flow(SkRect::MakeXYWH(30, 470, 660, 320));
    LayoutOptions opts;
    opts.alignment = Alignment::kJustify;
    opts.lineHeight = 34;
    const auto t0 = Clock::now();
    Layout layout = layoutParagraph(ctx, para, flow, opts);
    const auto t1 = Clock::now();
    Layout warm = layoutParagraph(ctx, para, flow, opts);
    const auto t2 = Clock::now();
    layout.draw(canvas, para);
    std::printf("  mixed-font paragraph: cold %.1f us, warm %.1f us\n",
                toUs(t1 - t0), toUs(t2 - t1));
  }

  writePng(surface.get(), outDir / "typography.png");
  std::printf("\n");
}


// ── Scene F: Pretext-style glyph choreography ─────────────────────────────
// Rain and ripple operate on a full ~1000-word paragraph that is re-laid-out
// EVERY frame (breathing measure, live word edits) while its letters
// simultaneously leave their lines as particles/waves. The point: letter-
// level choreography and full-paragraph relayout share one frame budget.

// ~1000 words of mixed Latin/CJK in alternating color/style chunks.
Paragraph makeBigParagraph(int wordCount, float size) {
  const char *latin[] = {"the",     "letters", "fall",   "away",    "from",
                         "their",   "lines",   "and",    "return",  "again",
                         "layout",  "engine",  "words",  "measure", "glyph",
                         "cascade", "gentle",  "steady", "rhythm",  "flowing"};
  const char *cjk[] = {"文字", "雨", "波紋", "字形", "빗물", "글자",
                       "물결", "여울", "漣漪", "文雨", "字落", "縦横"};
  const TextStyle styles[3] = {style(size, kInk), style(size, kBlue),
                               style(size, kAccent)};
  std::mt19937 rng(23);
  Paragraph para;
  std::string chunk;
  int chunkStyle = 0;
  for (int i = 0; i < wordCount; ++i) {
    if (i % 5 == 4)
      chunk += cjk[rng() % 12];
    else
      chunk += latin[rng() % 20];
    chunk += ' ';
    if (i % 120 == 119 || i + 1 == wordCount) { // new style chunk
      para.appendText(chunk, styles[chunkStyle++ % 3]);
      chunk.clear();
    }
  }
  return para;
}

// (a) A 1000-word paragraph rains onto an umbrella: the justified measure
// breathes (full relayout, every frame), attached letters track their
// re-laid-out lines, detached letters fall, slide down the dome tangent,
// drain off-screen, and re-attach to the paragraph.
void sceneRain(FontContext &ctx, int frames,
               const std::filesystem::path &outDir) {
  Paragraph para = makeBigParagraph(1000, 13.0f);

  const float baseWidth = 1080;
  LayoutOptions opts;
  opts.alignment = Alignment::kJustify;
  opts.lineHeight = 22;

  const SkPoint domeC = {600, 1560};
  const float domeR = 210;
  const float standoff = domeR + 10;

  struct Particle {
    enum Mode : uint8_t { kAttached, kFalling, kSliding } mode = kAttached;
    SkPoint pos = {0, 0};
    float vx = 0, vy = 0, angle = 0, spin = 0;
  };

  // Stable letter count from an initial layout.
  BlockFlow probe(SkRect::MakeXYWH(60, 40, baseWidth, 1320));
  Layout first = layoutParagraph(ctx, para, probe, opts);
  size_t glyphCount = 0;
  forEachPlacedGlyph(first, para, [&](auto...) { glyphCount++; });
  std::vector<Particle> parts(glyphCount);
  std::printf("  rain: %zu letters from %zu words\n", glyphCount,
              para.words().size());

  std::mt19937 rng(9);
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1200, 1900));
  SkCanvas *canvas = surface->getCanvas();
  GlyphRSXformBatches batchset;
  TimingStats layoutTime, simTime, drawTime;

  for (int frame = 0; frame < frames; ++frame) {
    // The measure breathes: a genuinely different line-breaking problem
    // every frame, solved from the shape cache.
    const float width =
        baseWidth * (1.0f + 0.06f * std::sin(static_cast<float>(frame) * 0.02f));
    BlockFlow flow(SkRect::MakeXYWH(60, 40, width, 1320));

    const auto t0 = Clock::now();
    Layout layout = layoutParagraph(ctx, para, flow, opts);
    const auto t1 = Clock::now();

    // Detach a few attached letters per frame (after a settling intro).
    if (frame > 30)
      for (int d = 0; d < 22; ++d) {
        Particle &p = parts[rng() % parts.size()];
        if (p.mode == Particle::kAttached)
          p.mode = Particle::kFalling; // position seeded from layout below
      }

    batchset.clear();
    size_t idx = 0;
    forEachPlacedGlyph(
        layout, para,
        [&](const ShapedWord *sw, SkGlyphID glyph, float adv, SkColor color,
            SkPoint rest) {
          Particle &p = parts[idx % parts.size()];
          idx++;
          const float half = adv * 0.5f;
          // Anchor everything on the glyph's advance center.
          const SkPoint restCenter = rest + SkVector{half, 0};
          float c = 1, sn = 0;
          SkPoint at = restCenter;
          switch (p.mode) {
          case Particle::kAttached:
            break; // rides its (re-laid-out) line
          case Particle::kFalling: {
            if (p.vy == 0 && p.pos.fY == 0) { // just detached: leave the line
              p.pos = restCenter;
              p.vy = 2.5f + static_cast<float>(rng() % 100) * 0.025f;
              p.vx = 0;
              p.spin = (static_cast<float>(rng() % 100) - 50.0f) * 0.0015f;
              p.angle = 0;
            }
            p.pos.fY += p.vy;
            p.pos.fX += p.vx;
            p.angle += p.spin;
            SkVector dir = p.pos - domeC;
            if (dir.length() < standoff && p.pos.fY < domeC.fY)
              p.mode = Particle::kSliding;
            if (p.pos.fY > 1940) { // drained: rejoin the paragraph
              p = Particle{};
            }
            break;
          }
          case Particle::kSliding: {
            SkVector dir = p.pos - domeC;
            dir.normalize();
            p.pos = domeC + dir * standoff;
            SkVector tangent = {-dir.fY, dir.fX};
            if (tangent.fX * dir.fX < 0)
              tangent = -tangent;
            const float slide = p.vy * (0.55f + 0.9f * std::abs(dir.fX));
            p.pos += tangent * slide;
            p.angle = std::atan2(tangent.fY, tangent.fX);
            if (dir.fY > -0.18f) {
              p.mode = Particle::kFalling;
              p.vx = tangent.fX * slide;
            }
            break;
          }
          }
          if (p.mode != Particle::kAttached) {
            at = p.pos;
            quantAngle(p.angle, c, sn);
          }
          GlyphRSXformBatches::Batch &b = batchset.batchFor(sw, color);
          b.glyphs.push_back(glyph);
          b.xforms.push_back(
              {c, sn, at.fX - c * half, at.fY - sn * half});
        });
    const auto t2 = Clock::now();

    canvas->clear(kPaper);
    SkPaint umbrella;
    umbrella.setAntiAlias(true);
    umbrella.setColor(0xFFB9552F);
    SkPathBuilder dome;
    dome.addArc(SkRect::MakeLTRB(domeC.fX - domeR, domeC.fY - domeR,
                                 domeC.fX + domeR, domeC.fY + domeR),
                180, 180);
    dome.close();
    canvas->drawPath(dome.detach(), umbrella);
    SkPaint pole;
    pole.setAntiAlias(true);
    pole.setStyle(SkPaint::kStroke_Style);
    pole.setStrokeWidth(8);
    pole.setColor(kInk);
    canvas->drawLine(domeC.fX, domeC.fY - 8, domeC.fX, domeC.fY + 300, pole);
    batchset.draw(canvas);
    const auto t3 = Clock::now();

    layoutTime.add(toUs(t1 - t0));
    simTime.add(toUs(t2 - t1));
    drawTime.add(toUs(t3 - t2));
    if (frame == frames / 2 || frame == frames - 1) {
      char name[64];
      std::snprintf(name, sizeof name, "rain_f%03d.png", frame);
      writePng(surface.get(), outDir / name);
    }
  }
  layoutTime.report("rain relayout (1000 words)");
  simTime.report("rain letters (sim + place)");
  drawTime.report("rain draw (CPU raster)");
}

// (b) Pool of text, at scale: drops land on a 1000-word paragraph and the
// expanding rings push each letter outward off its line as the front
// passes. The paragraph is re-laid-out every frame, and every ~150 frames
// a word is edited mid-ripple (cache-hot) to prove layout stays live.
void sceneRipple(FontContext &ctx, int frames,
                 const std::filesystem::path &outDir) {
  Paragraph para = makeBigParagraph(1000, 13.0f);

  BlockFlow flow(SkRect::MakeXYWH(60, 50, 1080, 860));
  LayoutOptions opts;
  opts.alignment = Alignment::kJustify;
  opts.lineHeight = 21.0f;

  {
    Layout probe = layoutParagraph(ctx, para, flow, opts);
    size_t n = 0;
    forEachPlacedGlyph(probe, para, [&](auto...) { n++; });
    std::printf("  ripple: %zu letters from %zu words\n", n,
                para.words().size());
  }

  struct Drop {
    SkPoint center;
    int born;
  };
  std::vector<Drop> drops;
  std::mt19937 rng(31);
  const float ringSpeed = 6.0f;

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1200, 960));
  SkCanvas *canvas = surface->getCanvas();
  GlyphRSXformBatches batchset;
  TimingStats layoutTime, waveTime, drawTime;
  const char *swaps[] = {"letters", "glyphs ", "symbols", "strokes"};

  for (int frame = 0; frame < frames; ++frame) {
    if (frame % 55 == 0)
      drops.push_back({{140.0f + static_cast<float>(rng() % 920),
                        140.0f + static_cast<float>(rng() % 680)},
                       frame});
    std::erase_if(drops, [&](const Drop &d) { return frame - d.born > 280; });

    // Live edit mid-ripple: same-length swap, everything else cache-hot.
    if (frame > 0 && frame % 150 == 0) {
      const size_t at = para.text().find(u"letters");
      if (at != std::u16string::npos)
        para.replaceText(static_cast<uint32_t>(at),
                         static_cast<uint32_t>(at + 7),
                         swaps[(frame / 150) % 4]);
    }

    const auto t0 = Clock::now();
    Layout layout = layoutParagraph(ctx, para, flow, opts);
    const auto t1 = Clock::now();

    batchset.clear();
    forEachPlacedGlyph(
        layout, para,
        [&](const ShapedWord *sw, SkGlyphID glyph, float adv, SkColor color,
            SkPoint rest) {
          SkVector offset = {0, 0};
          float tilt = 0;
          for (const Drop &drop : drops) {
            SkVector rv = rest - drop.center;
            const float r = rv.length() + 1.0f;
            const float ringR =
                ringSpeed * static_cast<float>(frame - drop.born);
            const float dr = (r - ringR) / 46.0f;
            if (dr > 3 || dr < -3)
              continue; // outside the ring front
            const float amp =
                42.0f * std::exp(-static_cast<float>(frame - drop.born) / 150.0f);
            const float pulse = amp * std::exp(-dr * dr);
            offset += rv * (pulse / r);
            tilt += pulse * 0.012f * (dr < 0 ? -1.0f : 1.0f);
          }
          float c, sn;
          quantAngle(tilt, c, sn);
          const SkPoint at = rest + offset;
          const float half = adv * 0.5f;
          GlyphRSXformBatches::Batch &b = batchset.batchFor(sw, color);
          b.glyphs.push_back(glyph);
          b.xforms.push_back(
              {c, sn, at.fX - c * half + half, at.fY - sn * half});
        });
    const auto t2 = Clock::now();

    canvas->clear(kPaper);
    batchset.draw(canvas);
    const auto t3 = Clock::now();

    layoutTime.add(toUs(t1 - t0));
    waveTime.add(toUs(t2 - t1));
    drawTime.add(toUs(t3 - t2));

    if (frame == 85 || frame == 200 || frame == frames - 1) {
      char name[64];
      std::snprintf(name, sizeof name, "ripple_f%03d.png", frame);
      writePng(surface.get(), outDir / name);
    }
  }
  layoutTime.report("ripple relayout (1000 words)");
  waveTime.report("ripple wave math (per letter)");
  drawTime.report("ripple draw (CPU raster)");
}

// (c) Infinite loop: text marches forever around a closed figure-eight —
// the closed contour wraps arc positions, so animating contourStart is the
// whole effect.
void sceneLoop(FontContext &ctx, int frames,
               const std::filesystem::path &outDir) {
  SkPathBuilder pb;
  const SkPoint c = {450, 280};
  const int steps = 400;
  for (int i = 0; i <= steps; ++i) {
    const float t = static_cast<float>(i) / steps * 6.2831853f;
    const SkPoint p = {c.fX + 340 * std::sin(t),
                       c.fY + 420 * std::sin(t) * std::cos(t)};
    if (i == 0)
      pb.moveTo(p);
    else
      pb.lineTo(p);
  }
  pb.close();
  const SkPath eight = pb.detach();

  SkContourMeasureIter iter(eight, false);
  sk_sp<SkContourMeasure> contour = iter.next();
  const float loopLen = contour->length();

  Paragraph para;
  para.appendText("and the words go round ", style(21, kInk));
  para.appendText("終わらない文字の環 ", style(21, kBlue, "ja"));
  para.appendText("끝나지 않는 글의 고리 ", style(21, kAccent, "ko"));
  para.appendText("文字環繞不息 ", style(21, kInk, "zh"));
  para.appendText("round and round again — ", style(21, kInk));

  LineSetFlow flow;
  LineInterval interval;
  interval.contour = contour;
  interval.length = loopLen;
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
    const auto t0 = Clock::now();
    flow.lines()[0][0].contourStart =
        std::fmod(static_cast<float>(frame) * 3.1f, loopLen);
    Layout layout = layoutParagraph(ctx, para, flow);
    const auto t1 = Clock::now();
    canvas->clear(kPaper);
    canvas->drawPath(eight, rail);
    layout.draw(canvas, para);
    drawTime.add(toUs(Clock::now() - t1));
    layoutTime.add(toUs(t1 - t0));

    if (frame == frames / 4 || frame == frames - 1) {
      char name[64];
      std::snprintf(name, sizeof name, "loop_f%03d.png", frame);
      writePng(surface.get(), outDir / name);
    }
  }
  layoutTime.report("infinite loop layout");
  drawTime.report("infinite loop draw (raster)");
}

void scenePretext(FontContext &ctx, int frames,
                  const std::filesystem::path &outDir) {
  std::printf("Scene F — Pretext-style glyph choreography (rain / pool "
              "ripple / infinite loop)\n");
  sceneRain(ctx, frames, outDir);
  sceneRipple(ctx, frames, outDir);
  sceneLoop(ctx, frames, outDir);
  std::printf("\n");
}


// ── Scene G: script coverage + OpenType features ──────────────────────────

// 2000 tokens across a dozen writing systems scattered as rotated confetti:
// Arabic joins, Devanagari conjuncts, emoji ZWJ sequences and all — every
// token resolved through per-codepoint fallback and the one shape cache.
void sceneBabel(FontContext &ctx, const std::filesystem::path &outDir) {
  const char *tokens[] = {
      "حرف", "كلمة",  "अक्षर", "शब्द", "אות",  "מילה", "ตัวอักษร",
      "字",   "글",     "λόγος", "буква", "🎉",   "👍🏽",  "文字",
      "ঢাকা", "கடல்",  "ᚱᚢᚾ",   "ainm",  "słowo", "λέξη"};
  std::mt19937 rng(77);
  Paragraph para;
  std::string text;
  for (int i = 0; i < 2000; ++i) {
    text += tokens[rng() % 20];
    text += ' ';
  }
  para.appendText(text, style(15));
  const uint32_t len = static_cast<uint32_t>(para.text().size());
  for (uint32_t at = 0; at + 40 < len; at += 40)
    para.setPaint(at, at + 20,
                  PaintStyle{(at / 40) % 3 == 0   ? kAccent
                             : (at / 40) % 3 == 1 ? kBlue
                                                  : kInk});

  LineSetFlow flow;
  for (int i = 0; i < 2000; ++i) {
    const float angle = static_cast<float>(rng() % 628) * 0.01f;
    flow.lines().push_back({LineInterval{
        {20.0f + static_cast<float>(rng() % 1360),
         20.0f + static_cast<float>(rng() % 860)},
        {std::cos(angle), std::sin(angle)},
        60}});
  }

  const auto t0 = Clock::now();
  Layout layout = layoutParagraph(ctx, para, flow);
  const auto t1 = Clock::now();
  Layout warm = layoutParagraph(ctx, para, flow);
  const auto t2 = Clock::now();

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1400, 900));
  surface->getCanvas()->clear(kPaper);
  layout.draw(surface->getCanvas(), para);
  writePng(surface.get(), outDir / "babel.png");
  std::printf("Scene G — babel confetti: %zu tokens, %zu runs, cold %.1f us, "
              "warm %.1f us\n",
              para.words().size(), layout.runs.size(), toUs(t1 - t0),
              toUs(t2 - t1));
}

// OpenType features made visible: ligature control, discretionary
// ligatures, small caps, lining vs oldstyle figures — each row is the same
// engine with a different FontFeature list (part of the shape-cache key).
void sceneFeatures(FontContext &ctx, const std::filesystem::path &outDir) {
  sk_sp<SkTypeface> hoefler =
      ctx.fontMgr()->matchFamilyStyle("Hoefler Text", SkFontStyle());
  if (!hoefler) {
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
    std::vector<FontFeature> features;
  };
  const Row rows[] = {
      {"default (liga on, oldstyle figures)",
       "The office staff filed 1234567890 affidavits.", {}},
      {"liga=0 clig=0 (ligatures off)",
       "The office staff filed 1234567890 affidavits.",
       {{"liga", 0}, {"clig", 0}}},
      {"dlig=1 (discretionary ct/st ligatures)",
       "The strict architect stood fast.", {{"dlig", 1}}},
      {"smcp=1 (small caps)",
       "The office staff filed affidavits.", {{"smcp", 1}}},
      {"lnum=1 (lining figures)",
       "Figures 1234567890 rise to the cap height.", {{"lnum", 1}}},
      {"frac=1 (fractions)",
       "Mix 1/2 cup with 3/4 spoon.", {{"frac", 1}}},
  };

  float y = 30;
  for (const Row &row : rows) {
    Paragraph caption;
    caption.appendText(row.label, style(12, kAccent));
    BlockFlow capFlow(SkRect::MakeXYWH(40, y, 900, 18));
    layoutParagraph(ctx, caption, capFlow).draw(canvas, caption);

    TextStyle body = style(30, kInk);
    body.shaping.typeface = hoefler;
    body.shaping.features = row.features;
    Paragraph para;
    para.appendText(row.text, body);
    BlockFlow flow(SkRect::MakeXYWH(40, y + 20, 900, 48));
    layoutParagraph(ctx, para, flow).draw(canvas, para);
    y += 88;
  }

  writePng(surface.get(), outDir / "features.png");
  std::printf("Scene G — OpenType features panel written\n\n");
}

// ── Scene I: arbitrary SkPath exclusions (star / heart / donut hole) ──────

void sceneShapes(FontContext &ctx, const std::filesystem::path &outDir) {
  SkFontMgr *mgr = ctx.fontMgr();
  TextStyle body = style(16.5f, kInk);
  body.shaping.typeface = mgr->matchFamilyStyle("Noto Serif", SkFontStyle());

  Paragraph para;
  for (int i = 0; i < 7; ++i)
    para.appendText(
        "Text no longer flows around circles and boxes alone: any SkPath — "
        "concave stars, compound paths, cubic hearts — carves its exact "
        "silhouette out of the line bands, and even-odd holes stay open, so "
        "the paragraph pours right through the middle of the donut. ",
        body);

  // Star (concave, winding fill).
  SkPathBuilder star;
  for (int i = 0; i < 5; ++i) {
    const float a = -1.5707963f + static_cast<float>(i) * 2 * 6.2831853f / 5;
    const SkPoint p = {215 + 135 * std::cos(a), 230 + 135 * std::sin(a)};
    if (i == 0)
      star.moveTo(p);
    else
      star.lineTo(p);
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
  flow.shapes().push_back(ExclusionFlow::Shape::Path(star.detach(), 10));
  flow.shapes().push_back(ExclusionFlow::Shape::Path(heart.detach(), 10));
  flow.shapes().push_back(ExclusionFlow::Shape::Path(donut.detach(), 8));
  flow.setMinIntervalWidth(46);

  LayoutOptions opts;
  opts.alignment = Alignment::kJustify;
  opts.lineHeight = 26;

  const auto t0 = Clock::now();
  Layout layout = layoutParagraph(ctx, para, flow, opts);
  const auto t1 = Clock::now();

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1000, 880));
  SkCanvas *canvas = surface->getCanvas();
  canvas->clear(kPaper);
  SkPaint shapePaint;
  shapePaint.setAntiAlias(true);
  shapePaint.setColor(kShape);
  for (const auto &shape : flow.shapes())
    canvas->drawPath(shape.path, shapePaint);
  layout.draw(canvas, para);

  writePng(surface.get(), outDir / "shapes.png");
  std::printf("Scene I — SkPath exclusions written (layout %.1f us, %d "
              "lines)\n\n",
              toUs(t1 - t0), layout.lineCount);
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
  int line = 0;
  SkPoint origin = {0, 0}; // first covering run's origin (on the axis)
  float begin = 0, end = 0; // flow-direction extent relative to origin
};

RangeExtent placedExtent(const Paragraph &para, const Layout &layout,
                         uint32_t start, uint32_t end) {
  RangeExtent extent;
  for (const PlacedRun &run : layout.runs) {
    const Word &word = para.words()[run.wordIndex];
    if (word.textEnd <= start || word.textBegin >= end || run.transformed)
      continue;
    if (!extent.valid) {
      extent.valid = true;
      extent.line = run.line;
      extent.origin = run.origin;
      extent.begin = 0;
      extent.end = word.width;
    } else if (run.line == extent.line) {
      // Same column/line: extend along the flow direction (vertical here).
      const float offset = run.origin.y() - extent.origin.y();
      extent.begin = std::min(extent.begin, offset);
      extent.end = std::max(extent.end, offset + word.width);
    }
  }
  return extent;
}

void sceneCjk(FontContext &ctx, const std::filesystem::path &outDir) {
  SkFontMgr *mgr = ctx.fontMgr();
  sk_sp<SkTypeface> mincho =
      mgr->matchFamilyStyle("Hiragino Mincho ProN", SkFontStyle());
  sk_sp<SkTypeface> notoSans =
      mgr->matchFamilyStyle("Noto Sans", SkFontStyle());
  if (!mincho) {
    std::printf("Scene H — cjk: Hiragino Mincho ProN missing, skipped\n\n");
    return;
  }

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(980, 700));
  SkCanvas *canvas = surface->getCanvas();
  canvas->clear(kPaper);

  const float em = 26.0f;
  auto jp = [&](float size, VerticalForm form = VerticalForm::kAuto) {
    TextStyle s = style(size, kInk, "ja");
    s.shaping.typeface = mincho;
    s.shaping.verticalForm = form;
    return s;
  };

  // ── Vertical-rl block (right side of the page) ─────────────────────────
  Paragraph vert;
  vert.appendText("縦組みの文章は、上から下へ、", jp(em));
  vert.appendText("右から左へと流れる。平成", jp(em));
  vert.appendText("31", jp(em, VerticalForm::kTateChuYoko));
  vert.appendText("年", jp(em));
  vert.appendText("12", jp(em, VerticalForm::kTateChuYoko));
  vert.appendText("月、", jp(em));
  vert.appendText("TextFlow", jp(em)); // UTR#50 rotates Latin automatically
  vert.appendText("は縦書きに対応した。", jp(em));
  vert.setWritingMode(WritingMode::kVerticalRL);

  const SkRect vertRect = SkRect::MakeXYWH(430, 80, 500, 540);
  VerticalBlockFlow vertFlow(vertRect);
  LayoutOptions vertOpts;
  vertOpts.lineHeight = em * 1.9f; // column pitch, ruby lives in the gap
  Layout vertLayout = layoutParagraph(ctx, vert, vertFlow, vertOpts);
  vertLayout.draw(canvas, vert);

  // Ruby (furigana): a small vertical paragraph laid along the base's
  // column, offset into the inter-column gap.
  auto vertRuby = [&](std::u16string_view base, const char *rubyText) {
    const std::vector<CharRange> found = findAll(vert, base);
    if (found.empty())
      return;
    const RangeExtent extent =
        placedExtent(vert, vertLayout, found[0].start, found[0].end);
    if (!extent.valid)
      return;
    Paragraph ruby;
    ruby.appendText(rubyText, jp(em * 0.5f));
    ruby.setWritingMode(WritingMode::kVerticalRL);
    const float length = ruby.naturalWidth(ctx);
    const float mid =
        extent.origin.y() + (extent.begin + extent.end) * 0.5f;
    LineSetFlow flow;
    flow.lines().push_back({LineInterval{
        {extent.origin.x() + em * 0.62f, mid - length * 0.5f},
        {0, 1},
        length + 1}});
    layoutParagraph(ctx, ruby, flow).draw(canvas, ruby);
  };
  vertRuby(u"縦組", "たてぐみ");
  vertRuby(u"文章", "ぶんしょう");
  vertRuby(u"対応", "たいおう");

  // Kenten (emphasis dots): one sesame dot beside each emphasized glyph.
  {
    const std::vector<CharRange> found = findAll(vert, u"上から下へ");
    SkPaint dot;
    dot.setAntiAlias(true);
    dot.setColor(kAccent);
    if (!found.empty()) {
      for (const PlacedRun &run : vertLayout.runs) {
        const Word &word = vert.words()[run.wordIndex];
        if (word.textEnd <= found[0].start || word.textBegin >= found[0].end ||
            run.transformed || !run.shaped->vertical)
          continue;
        float pen = 0;
        const ShapedWord &sw = *run.shaped;
        for (size_t g = 0; g < sw.advances.size(); ++g) {
          // Clusters are offsets into the shaped segment, which starts at
          // the word's first character — dot only the emphasized glyphs.
          const uint32_t at = word.textBegin + sw.clusters[g];
          if (at >= found[0].start && at < found[0].end)
            canvas->drawCircle(run.origin.x() + em * 0.60f,
                               run.origin.y() + pen + sw.advances[g] * 0.5f,
                               em * 0.07f, dot);
          pen += sw.advances[g];
        }
      }
    }
  }

  // ── Horizontal block with ruby + kenten (left side) ────────────────────
  Paragraph horiz;
  horiz.appendText("漢字にルビを振ると、誰でも読みやすい。強調したい語には圏点を打つ。",
                   jp(em * 0.85f));
  const SkRect horizRect = SkRect::MakeXYWH(50, 120, 330, 400);
  BlockFlow horizFlow(horizRect);
  LayoutOptions horizOpts;
  horizOpts.lineHeight = em * 2.0f; // ruby lives above each line
  Layout horizLayout = layoutParagraph(ctx, horiz, horizFlow, horizOpts);
  horizLayout.draw(canvas, horiz);

  auto horizRuby = [&](std::u16string_view base, const char *rubyText) {
    const std::vector<CharRange> found = findAll(horiz, base);
    if (found.empty())
      return;
    // Horizontal extent along the line this time.
    float x0 = 0, x1 = 0, baseline = 0;
    bool valid = false;
    for (const PlacedRun &run : horizLayout.runs) {
      const Word &word = horiz.words()[run.wordIndex];
      if (word.textEnd <= found[0].start || word.textBegin >= found[0].end)
        continue;
      if (!valid) {
        valid = true;
        x0 = run.origin.x();
        x1 = run.origin.x() + word.width;
        baseline = run.origin.y();
      } else if (run.origin.y() == baseline) {
        x1 = std::max(x1, run.origin.x() + word.width);
      }
    }
    if (!valid)
      return;
    Paragraph ruby;
    ruby.appendText(rubyText, jp(em * 0.42f));
    const float width = ruby.naturalWidth(ctx);
    LineSetFlow flow;
    flow.lines().push_back({LineInterval{
        {(x0 + x1) * 0.5f - width * 0.5f, baseline - em * 0.98f},
        {1, 0},
        width + 1}});
    layoutParagraph(ctx, ruby, flow).draw(canvas, ruby);
  };
  horizRuby(u"漢字", "かんじ");
  horizRuby(u"圏点", "けんてん");

  {
    const std::vector<CharRange> found = findAll(horiz, u"強調");
    SkPaint dot;
    dot.setAntiAlias(true);
    dot.setColor(kAccent);
    if (!found.empty()) {
      for (const PlacedRun &run : horizLayout.runs) {
        const Word &word = horiz.words()[run.wordIndex];
        if (word.textEnd <= found[0].start || word.textBegin >= found[0].end)
          continue;
        float pen = 0;
        const ShapedWord &sw = *run.shaped;
        for (size_t g = 0; g < sw.advances.size(); ++g) {
          const uint32_t at = word.textBegin + sw.clusters[g];
          if (at >= found[0].start && at < found[0].end)
            canvas->drawCircle(run.origin.x() + pen + sw.advances[g] * 0.5f,
                               run.origin.y() - em * 0.92f, em * 0.06f, dot);
          pen += sw.advances[g];
        }
      }
    }
  }

  // Captions.
  auto caption = [&](const char *text, float x, float y) {
    TextStyle s = style(13, kBlue);
    s.shaping.typeface = notoSans;
    Paragraph para;
    para.appendText(text, s);
    BlockFlow flow(SkRect::MakeXYWH(x, y, 400, 18));
    layoutParagraph(ctx, para, flow).draw(canvas, para);
  };
  caption("horizontal: ruby + kenten (external utilities)", 50, 90);
  caption("vertical-rl: UTR#50 mixed orientation, 'vert' forms,", 430, 630);
  caption("tate-chu-yoko digits, ruby + kenten in the column gap", 430, 648);

  writePng(surface.get(), outDir / "cjk.png");
  std::printf("Scene H — CJK vertical/ruby/kenten/tate-chu-yoko written\n\n");
}

} // namespace

int main(int argc, char **argv) {
  const int frames = argc > 1 ? std::max(1, std::atoi(argv[1])) : 240;

  const std::filesystem::path outDir = "textflow_demo_out";
  std::filesystem::create_directories(outDir);

  FontContext ctx(SkFontMgr_New_CoreText(nullptr));

  sceneExclusions(ctx, frames, outDir);
  sceneKnuthPlass(ctx, frames, outDir);
  sceneFreeform(ctx, outDir);
  sceneExtreme(ctx, outDir);
  sceneTypography(ctx, outDir);
  sceneBabel(ctx, outDir);
  sceneFeatures(ctx, outDir);
  sceneCjk(ctx, outDir);
  sceneShapes(ctx, outDir);
  scenePretext(ctx, frames, outDir);

  std::printf("PNGs written to %s\n",
              std::filesystem::absolute(outDir).string().c_str());
  return 0;
}
