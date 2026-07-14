// Scene K — a deterministic complex-script torture wall: Arabic
// presentation forms and mark positioning, Cuneiform in the supplementary
// planes, pathological combining-mark stacks, deep Brahmic/Tibetan clusters,
// UAX#9 bidi mixing, rare-script fallback, emoji ZWJ sequences, and symbols.
#include "DemoScenes.h"
#include "DemoSupport.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkString.h>
#include <include/core/SkSurface.h>

#include <array>
#include <charconv>
#include <cstdio>
#include <set>
#include <string>

using namespace textflow;

namespace {

struct Sample {
  const char8_t *label;
  Paragraph paragraph;
  float lineHeight = 0;
  TextAlignment alignment = TextAlignment::kStart;
};

struct Coverage {
  int glyphs = 0;
  int missing = 0;
  std::set<std::string> families;
};

void includeCoverage(const ParagraphLayout &layout, Coverage &coverage) {
  for (const PositionedRun &run : layout.runs) {
    if (run.shaped->typeface) {
      SkString family;
      run.shaped->typeface->getFamilyName(&family);
      coverage.families.insert(family.c_str());
    }
    for (uint16_t glyph : run.shaped->glyphs) {
      ++coverage.glyphs;
      coverage.missing += glyph == 0;
    }
  }
}

void drawLabel(SkCanvas *canvas, FontContext &fontContext,
               std::u8string_view label, SkPoint origin, float width) {
  Paragraph paragraph;
  paragraph.appendText(label, style(12, 0xFF86D7FF, "en"));
  BlockFlow flow(SkRect::MakeXYWH(origin.x(), origin.y(), width, 24));
  layoutParagraph(fontContext, paragraph, flow).draw(canvas, paragraph);
}

void appendNumber(std::u8string &text, size_t value) {
  char digits[24];
  const auto [end, error] =
      std::to_chars(std::begin(digits), std::end(digits), value);
  if (error == std::errc{})
    text.append(reinterpret_cast<const char8_t *>(digits),
                static_cast<size_t>(end - digits));
}

ParagraphLayout drawSample(SkCanvas *canvas, FontContext &fontContext,
                           Sample &sample, const SkRect &panel,
                           bool highlighted = false) {
  SkPaint fill;
  fill.setAntiAlias(true);
  fill.setColor(highlighted ? 0xFF171123 : 0xFF151923);
  canvas->drawRoundRect(panel, 10, 10, fill);

  SkPaint border;
  border.setAntiAlias(true);
  border.setStyle(SkPaint::kStroke_Style);
  border.setStrokeWidth(highlighted ? 2.0f : 1.0f);
  border.setColor(highlighted ? 0xAAE05497 : 0x553D4960);
  canvas->drawRoundRect(panel, 10, 10, border);

  drawLabel(canvas, fontContext, sample.label,
            {panel.left() + 14, panel.top() + 9}, panel.width() - 28);
  BlockFlow flow(SkRect::MakeXYWH(panel.left() + 14, panel.top() + 36,
                                  panel.width() - 28, panel.height() - 44));
  ParagraphLayoutOptions options;
  options.alignment = sample.alignment;
  options.lineMetrics.height = sample.lineHeight;
  ParagraphLayout layout =
      layoutParagraph(fontContext, sample.paragraph, flow, options);
  layout.draw(canvas, sample.paragraph);
  return layout;
}

} // namespace

void sceneHyperScripts(FontContext &fontContext,
                       const std::filesystem::path &outputDirectory) {
  std::printf("Scene K — Unicode singularity (complex-script torture wall)\n");
  constexpr float kBase = 24.0f;
  std::array<Sample, 7> samples;

  samples[0].label = u8"ARABIC PRESENTATION FORM + JOINING + VOWEL MARKS";
  samples[0].paragraph.appendText(u8"﷽  السَّلَامُ عَلَيْكُمْ",
                                  style(kBase * 2.45f, 0xFFFFD37A, "ar"));
  samples[0].lineHeight = kBase * 3.25f;
  samples[0].alignment = TextAlignment::kCenter;

  samples[1].label = u8"CUNEIFORM · SUPPLEMENTARY PLANE · AKKADIAN";
  samples[1].paragraph.appendText(u8"𒀱  𒀭𒂗𒆠  𒈙  𒀀𒁀𒄿𒅗",
                                  style(kBase * 2.05f, 0xFF86D7FF, "akk"));
  samples[1].lineHeight = kBase * 2.65f;
  samples[1].alignment = TextAlignment::kCenter;

  samples[2].label = u8"COMBINING-MARK STORM · ONE BASE, MANY ATTACHMENTS";
  samples[2].paragraph.appendText(u8"Z̴̢̨̛̲̦̹̰̓̈́͊͘A̵̛̪̯̜̩͆̈́͝L̷̨̡̲̤̬̝̑̓͑̕G̵̢̺̙͎̺̤̓͛̾Ơ̶̢͙̟̲̦̿̽͋̚  "
                                  "T̷̨̗̰͉̼̯͛̋E̴̡̨̩̱͕̪͗̎X̷̢̳̮̱̪̿̈́͘T̴̛̬̠̦̞͙̋̄͝",
                                  style(kBase * 1.75f, 0xFFFF66B3, "und"));
  samples[2].lineHeight = kBase * 3.2f;
  samples[2].alignment = TextAlignment::kCenter;

  samples[3].label = u8"FULLY VOCALIZED RTL · LIGATURES · MARK POSITIONING";
  samples[3].paragraph.appendText(u8"ٱلْعَرَبِيَّةُ  ﷽  لَا إِلٰهَ إِلَّا ٱللَّٰهُ",
                                  style(kBase * 1.35f, 0xFFFFD37A, "ar"));
  samples[3].lineHeight = kBase * 2.0f;

  samples[4].label =
      u8"DEEP CLUSTERS · DEVANAGARI · TIBETAN · MYANMAR · KHMER · SINHALA";
  samples[4].paragraph.appendText(u8"क्ष्ण्य  स्त्रै  བསྒྲུབས་  မြန်မာ  ខ្មែរ  "
                                  "ශ්‍රී",
                                  style(kBase * 1.28f, 0xFFA8E6A3, "und"));
  samples[4].lineHeight = kBase * 2.05f;

  samples[5].label = u8"UAX#9 BIDI COLLISION · RTL + LTR + NUMBERS + CUNEIFORM";
  samples[5].paragraph.appendText(u8"RTL ← ",
                                  style(kBase * 1.12f, 0xFFAAB6CC, "en"));
  samples[5].paragraph.appendText(u8"﷽ السَّلَامُ ١٢٣",
                                  style(kBase * 1.35f, 0xFFFFD37A, "ar"));
  samples[5].paragraph.appendText(u8" ⇄ EN 123 ⇄ ",
                                  style(kBase * 1.12f, 0xFFAAB6CC, "en"));
  samples[5].paragraph.appendText(u8"𒈙 𒀭",
                                  style(kBase * 1.25f, 0xFF86D7FF, "akk"));
  samples[5].lineHeight = kBase * 1.95f;

  samples[6].label = u8"RARE GLYPHS + DEEP CLUSTER · ZWJ + MODIFIERS + SYMBOLS";
  samples[6].paragraph.appendText(u8"𒀱   𰻞   ཧཱུྃ   ꧅   ᎙\n",
                                  style(kBase * 1.70f, 0xFFFFD37A, "und"));
  samples[6].paragraph.appendText(
      u8"👩🏽‍🚀  🧑🏿‍💻  👨‍👩‍👧‍👦  "
      "🏳️‍🌈  𝄞  𝕳𝖆𝖗𝖋  "
      "a̪̺̬̍̎̄͛",
      style(kBase * 1.08f, 0xFFD7B5FF, "und"));
  samples[6].lineHeight = kBase * 2.05f;

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1400, 1050));
  SkCanvas *canvas = surface->getCanvas();
  canvas->clear(0xFF0E1017);
  drawLabel(canvas, fontContext,
            u8"UNICODE SINGULARITY · FALLBACK + ITEMIZATION + HARFBUZZ + "
            "UAX#9 IN ONE FRAME",
            {36, 18}, 1328);

  constexpr float kMargin = 36;
  constexpr float kGap = 16;
  constexpr float kColumnWidth = (1400 - kMargin * 2 - kGap) * 0.5f;
  const std::array<SkRect, 7> panels = {
      SkRect::MakeXYWH(kMargin, 58, kColumnWidth, 190),
      SkRect::MakeXYWH(kMargin + kColumnWidth + kGap, 58, kColumnWidth, 190),
      SkRect::MakeXYWH(kMargin, 264, 1400 - kMargin * 2, 225),
      SkRect::MakeXYWH(kMargin, 505, kColumnWidth, 225),
      SkRect::MakeXYWH(kMargin + kColumnWidth + kGap, 505, kColumnWidth, 225),
      SkRect::MakeXYWH(kMargin, 746, kColumnWidth, 225),
      SkRect::MakeXYWH(kMargin + kColumnWidth + kGap, 746, kColumnWidth, 225),
  };

  Coverage coverage;
  const auto coldStart = Clock::now();
  for (size_t index = 0; index < samples.size(); ++index) {
    ParagraphLayout layout = drawSample(canvas, fontContext, samples[index],
                                        panels[index], index == 2);
    includeCoverage(layout, coverage);
  }
  const double coldMicroseconds = toMicroseconds(Clock::now() - coldStart);

  std::u8string status;
  appendNumber(status, static_cast<size_t>(coverage.glyphs));
  status += u8" glyphs · ";
  appendNumber(status, coverage.families.size());
  status += u8" resolved typefaces · ";
  appendNumber(status, static_cast<size_t>(coverage.missing));
  status += u8" missing";
  drawLabel(canvas, fontContext, status, {36, 1002}, 1328);
  writePng(surface.get(), outputDirectory / "hyper_scripts.png");

  std::printf("  %d glyphs, %zu typefaces, %d missing, layout+draw %.1f us\n",
              coverage.glyphs, coverage.families.size(), coverage.missing,
              coldMicroseconds);
  std::printf("  fallback families:");
  for (const std::string &family : coverage.families)
    std::printf(" %s;", family.c_str());
  std::printf("\n\n");
}
