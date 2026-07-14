// Scene: deliberately pathological Unicode and complex-script shaping.
// This is a typography stress wall, not a language specimen: it combines
// Arabic joining/calligraphy, Cuneiform, deep combining-mark stacks, Indic
// and Tibetan clusters, bidi reordering, emoji ZWJ sequences, and
// supplementary-plane symbols in one cached layout.
#include "SceneFactories.h"
#include "SceneSupport.h"

#include <include/core/SkPaint.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <set>

using namespace textflow;

namespace gallery {

namespace {

struct Coverage {
  int glyphCount = 0;
  int missingGlyphCount = 0;
  std::set<uint32_t> typefaceIds;
};

void includeCoverage(const ParagraphLayout &layout, Coverage &coverage) {
  for (const PositionedRun &run : layout.runs) {
    if (run.shaped->typeface)
      coverage.typefaceIds.insert(run.shaped->typeface->uniqueID());
    for (uint16_t glyph : run.shaped->glyphs) {
      ++coverage.glyphCount;
      coverage.missingGlyphCount += glyph == 0;
    }
  }
}

void drawSceneLabel(SkCanvas *canvas, FontContext &fontContext,
                    const char *text, SkPoint origin, float width,
                    SkColor color = 0xFF86D7FF) {
  Paragraph paragraph;
  paragraph.appendText(text, makeStyle(12.0f, color, "en"));
  BlockFlow flow(SkRect::MakeXYWH(origin.x(), origin.y(), width, 28));
  layoutParagraph(fontContext, paragraph, flow).draw(canvas, paragraph);
}

class HyperScriptsScene final : public Scene {
public:
  QString name() const override {
    return QStringLiteral("Unicode singularity — ﷽ 𒈙 Z̴̓͝");
  }
  bool supportsTextEdit() const override { return false; }

  FrameStats render(SkCanvas *canvas, SkISize size, double elapsedSeconds,
                    int /*frameNumber*/, const SceneParams &params,
                    FontContext &fontContext) override {
    const float baseSize = std::clamp(params.fontSize, 12.0f, 26.0f);
    if (baseSize != m_builtSize || params.typeface.get() != m_builtTypeface) {
      build(baseSize, params.typeface);
      m_builtSize = baseSize;
      m_builtTypeface = params.typeface.get();
    }

    const float width = static_cast<float>(size.width());
    const float height = static_cast<float>(size.height());
    const float margin = 22.0f;
    const float gap = 11.0f;
    const float headerHeight = 42.0f;
    const float contentHeight = std::max(300.0f, height - headerHeight - 42.0f);
    const float topHeight = contentHeight * 0.22f;
    const float zalgoHeight = contentHeight * 0.24f;
    const float bottomHeight =
        (contentHeight - topHeight - zalgoHeight - gap * 3.0f) * 0.5f;
    const float columnWidth = (width - margin * 2.0f - gap) * 0.5f;
    const float top = headerHeight;
    const float zalgoTop = top + topHeight + gap;
    const float lowerTop = zalgoTop + zalgoHeight + gap;

    const std::array<SkRect, 7> panels = {
        SkRect::MakeXYWH(margin, top, columnWidth, topHeight),
        SkRect::MakeXYWH(margin + columnWidth + gap, top, columnWidth,
                         topHeight),
        SkRect::MakeXYWH(margin, zalgoTop, width - margin * 2.0f,
                         zalgoHeight),
        SkRect::MakeXYWH(margin, lowerTop, columnWidth, bottomHeight),
        SkRect::MakeXYWH(margin + columnWidth + gap, lowerTop, columnWidth,
                         bottomHeight),
        SkRect::MakeXYWH(margin, lowerTop + bottomHeight + gap, columnWidth,
                         bottomHeight),
        SkRect::MakeXYWH(margin + columnWidth + gap,
                         lowerTop + bottomHeight + gap, columnWidth,
                         bottomHeight),
    };

    canvas->clear(0xFF0E1017);
    drawSceneLabel(canvas, fontContext,
                   "UNICODE SINGULARITY  ·  fallback + itemization + "
                   "HarfBuzz + UAX#9 in one frame",
                   {margin, 10}, width - margin * 2.0f);

    const float pulse =
        0.5f + 0.5f * static_cast<float>(std::sin(elapsedSeconds * 1.4));
    double layoutMicroseconds = 0;
    int runCount = 0;
    Coverage coverage;
    for (size_t panelIndex = 0; panelIndex < panels.size(); ++panelIndex) {
      const SkRect panel = panels[panelIndex];
      SkPaint fill;
      fill.setAntiAlias(true);
      fill.setColor(panelIndex == 2 ? 0xFF171123 : 0xFF151923);
      canvas->drawRoundRect(panel, 9, 9, fill);

      SkPaint border;
      border.setAntiAlias(true);
      border.setStyle(SkPaint::kStroke_Style);
      border.setStrokeWidth(panelIndex == 2 ? 1.8f : 1.0f);
      border.setColor(panelIndex == 2
                          ? SkColorSetARGB(static_cast<U8CPU>(90 + 90 * pulse),
                                           224, 84, 151)
                          : 0x443D4960);
      canvas->drawRoundRect(panel, 9, 9, border);

      drawSceneLabel(canvas, fontContext, kLabels[panelIndex],
                     {panel.left() + 12, panel.top() + 8}, panel.width() - 24);

      const SkRect textBox = SkRect::MakeXYWH(
          panel.left() + 12, panel.top() + 31, panel.width() - 24,
          std::max(1.0f, panel.height() - 38));
      BlockFlow flow(textBox);
      ParagraphLayoutOptions options;
      options.alignment = panelIndex <= 2 ? TextAlignment::kCenter
                                          : TextAlignment::kStart;
      options.lineMetrics.height = m_lineHeights[panelIndex];
      const auto layoutStart = Clock::now();
      ParagraphLayout layout =
          layoutParagraph(fontContext, m_paragraphs[panelIndex], flow, options);
      layoutMicroseconds += toMicroseconds(Clock::now() - layoutStart);
      runCount += static_cast<int>(layout.runs.size());
      includeCoverage(layout, coverage);
      layout.draw(canvas, m_paragraphs[panelIndex]);
    }

    const QString status =
        QStringLiteral("%1 glyphs · %2 resolved typefaces · %3 missing")
            .arg(coverage.glyphCount)
            .arg(coverage.typefaceIds.size())
            .arg(coverage.missingGlyphCount);
    drawSceneLabel(canvas, fontContext, status.toUtf8().constData(),
                   {margin, height - 25}, width - margin * 2.0f,
                   coverage.missingGlyphCount == 0 ? 0xFF86D7FF
                                                   : 0xFFFF5B68);

    return {layoutMicroseconds, runCount, coverage.glyphCount};
  }

private:
  TextStyle sampleStyle(float size, SkColor color, const char *language,
                        const sk_sp<SkTypeface> &typeface) const {
    return makeStyle(size, color, language, typeface);
  }

  void build(float baseSize, const sk_sp<SkTypeface> &typeface) {
    for (Paragraph &paragraph : m_paragraphs)
      paragraph.clear();

    m_paragraphs[0].appendText(
        "﷽  السَّلَامُ عَلَيْكُمْ",
        sampleStyle(baseSize * 2.45f, 0xFFFFD37A, "ar", typeface));
    m_lineHeights[0] = baseSize * 3.25f;

    m_paragraphs[1].appendText(
        "𒀭𒂗𒆠  𒈙  𒀀𒁀𒄿𒅗",
        sampleStyle(baseSize * 2.05f, 0xFF86D7FF, "akk", typeface));
    m_lineHeights[1] = baseSize * 2.65f;

    m_paragraphs[2].appendText(
        "Z̴̢̨̛̲̦̹̰̓̈́͊͘A̵̛̪̯̜̩͆̈́͝L̷̨̡̲̤̬̝̑̓͑̕G̵̢̺̙͎̺̤̓͛̾Ơ̶̢͙̟̲̦̿̽͋̚  "
        "T̷̨̗̰͉̼̯͛̋E̴̡̨̩̱͕̪͗̎X̷̢̳̮̱̪̿̈́͘T̴̛̬̠̦̞͙̋̄͝",
        sampleStyle(baseSize * 1.75f, 0xFFFF66B3, "und", typeface));
    m_lineHeights[2] = baseSize * 3.2f;

    m_paragraphs[3].appendText(
        "ٱلْعَرَبِيَّةُ  ﷽  لَا إِلٰهَ إِلَّا ٱللَّٰهُ",
        sampleStyle(baseSize * 1.35f, 0xFFFFD37A, "ar", typeface));
    m_lineHeights[3] = baseSize * 2.0f;

    m_paragraphs[4].appendText(
        "क्ष्ण्य  स्त्रै  བསྒྲུབས་  မြန်မာ  ខ្មែរ  ශ්‍රී",
        sampleStyle(baseSize * 1.28f, 0xFFA8E6A3, "und", typeface));
    m_lineHeights[4] = baseSize * 2.05f;

    m_paragraphs[5].appendText(
        "RTL ← ", sampleStyle(baseSize * 1.12f, 0xFFAAB6CC, "en", typeface));
    m_paragraphs[5].appendText(
        "﷽ السَّلَامُ ١٢٣",
        sampleStyle(baseSize * 1.35f, 0xFFFFD37A, "ar", typeface));
    m_paragraphs[5].appendText(
        " ⇄ EN 123 ⇄ ",
        sampleStyle(baseSize * 1.12f, 0xFFAAB6CC, "en", typeface));
    m_paragraphs[5].appendText(
        "𒈙 𒀭", sampleStyle(baseSize * 1.25f, 0xFF86D7FF, "akk", typeface));
    m_lineHeights[5] = baseSize * 1.95f;

    m_paragraphs[6].appendText(
        "👩🏽‍🚀  🧑🏿‍💻  👨‍👩‍👧‍👦  🏳️‍🌈  𝄞  𝕳𝖆𝖗𝖋  a̪̺̬̍̎̄͛",
        sampleStyle(baseSize * 1.25f, 0xFFD7B5FF, "und", typeface));
    m_lineHeights[6] = baseSize * 2.05f;
  }

  static constexpr std::array<const char *, 7> kLabels = {
      "ARABIC PRESENTATION FORM + JOINING + VOWEL MARKS",
      "CUNEIFORM · SUPPLEMENTARY PLANE · AKKADIAN",
      "COMBINING-MARK STORM · ONE BASE, MANY ATTACHMENTS",
      "FULLY VOCALIZED RTL · LIGATURES · MARK POSITIONING",
      "DEEP CLUSTERS · DEVANAGARI · TIBETAN · MYANMAR · KHMER · SINHALA",
      "UAX#9 BIDI COLLISION · RTL + LTR + NUMBERS + CUNEIFORM",
      "ZWJ + MODIFIERS + FLAGS + MUSIC + FRAKTUR + STACKED MARKS",
  };

  std::array<Paragraph, 7> m_paragraphs;
  std::array<float, 7> m_lineHeights{};
  const SkTypeface *m_builtTypeface = nullptr;
  float m_builtSize = 0;
};

} // namespace

std::unique_ptr<Scene> makeHyperScriptsScene() {
  return std::make_unique<HyperScriptsScene>();
}

} // namespace gallery
