// Scene: vertical CJK with ruby, kenten, tate-chu-yoko.
#include "SceneRegistry.h"
#include "SceneSupport.h"

#include <textflow/Query.h>

#include <include/core/SkFontMgr.h>
#include <include/core/SkPaint.h>

#include <algorithm>
#include <cmath>

using namespace textflow;

namespace gallery {

namespace {

class VerticalScene final : public Scene {
public:
  FrameStats render(SkCanvas *canvas, SkISize size, double elapsedSeconds,
                    int /*frameNumber*/, const SceneParams & /*params*/,
                    FontContext &fontContext) override {
    const float canvasWidth = size.width();
    const float canvasHeight = size.height();
    const float fontSize =
        std::clamp(std::min(canvasWidth, canvasHeight) / 24.0f, 16.0f, 30.0f);
    if (!m_mincho) {
      m_mincho = fontContext.fontManager()->matchFamilyStyle(
          "Hiragino Mincho ProN", SkFontStyle());
      if (!m_mincho)
        m_mincho = fontContext.defaultTypeface();
    }
    if (fontSize != m_fontSize) {
      m_fontSize = fontSize;
      buildParagraphs(fontSize);
    }

    canvas->clear(kPaper);

    // Vertical-rl block on the right.
    VerticalBlockFlow verticalFlow(SkRect::MakeXYWH(
        canvasWidth * 0.42f, 30, canvasWidth * 0.55f, canvasHeight - 90));
    ParagraphLayoutOptions verticalOptions;
    verticalOptions.lineMetrics.height = fontSize * 1.9f;
    const auto layoutStartTime = Clock::now();
    ParagraphLayout verticalLayout = layoutParagraph(
        fontContext, m_verticalParagraph, verticalFlow, verticalOptions);
    const auto layoutEndTime = Clock::now();
    verticalLayout.draw(canvas, m_verticalParagraph);

    // Ruby + kenten are external utilities over the placed runs.
    rubyVertical(canvas, fontContext, verticalLayout, u"縦組", u8"たてぐみ");
    rubyVertical(canvas, fontContext, verticalLayout, u"文章", u8"ぶんしょう");
    rubyVertical(canvas, fontContext, verticalLayout, u"対応", u8"たいおう");
    kentenVertical(
        canvas, verticalLayout, u"上から下へ",
        0.6f + 0.4f * static_cast<float>(std::sin(elapsedSeconds * 2.0)));

    // Horizontal comparison block on the left.
    BlockFlow horizontalFlow(SkRect::MakeXYWH(
        30, canvasHeight * 0.14f, canvasWidth * 0.34f, canvasHeight * 0.7f));
    ParagraphLayoutOptions horizontalOptions;
    horizontalOptions.lineMetrics.height = fontSize * 1.9f;
    ParagraphLayout horizontalLayout = layoutParagraph(
        fontContext, m_horizontalParagraph, horizontalFlow, horizontalOptions);
    horizontalLayout.draw(canvas, m_horizontalParagraph);
    rubyHorizontal(canvas, fontContext, horizontalLayout, u"漢字", u8"かんじ");
    rubyHorizontal(canvas, fontContext, horizontalLayout, u"圏点",
                   u8"けんてん");

    drawCaption(canvas, fontContext,
                u8"vertical-rl: UTR#50 orientation, 'vert' forms, "
                "tate-chu-yoko digits, ruby + kenten on top",
                {30, canvasHeight - 28}, canvasWidth - 60);

    return {toMicroseconds(layoutEndTime - layoutStartTime),
            static_cast<int>(verticalLayout.runs.size() +
                             horizontalLayout.runs.size()),
            0};
  }

private:
  /// Creates a Japanese style with the requested vertical glyph behavior.
  TextStyle
  japaneseStyle(float fontSize,
                VerticalForm verticalForm = VerticalForm::kAuto) const {
    TextStyle style = makeStyle(fontSize, kInk, "ja", m_mincho);
    style.shaping.verticalForm = verticalForm;
    return style;
  }

  /// Rebuilds both comparison paragraphs for a new display size.
  void buildParagraphs(float fontSize) {
    m_verticalParagraph.clear();
    m_verticalParagraph.appendText(u8"縦組みの文章は、上から下へ、",
                                   japaneseStyle(fontSize));
    m_verticalParagraph.appendText(u8"右から左へと流れる。平成",
                                   japaneseStyle(fontSize));
    m_verticalParagraph.appendText(
        u8"31", japaneseStyle(fontSize, VerticalForm::kTateChuYoko));
    m_verticalParagraph.appendText(u8"年", japaneseStyle(fontSize));
    m_verticalParagraph.appendText(
        u8"12", japaneseStyle(fontSize, VerticalForm::kTateChuYoko));
    m_verticalParagraph.appendText(u8"月、", japaneseStyle(fontSize));
    m_verticalParagraph.appendText(u8"TextFlow", japaneseStyle(fontSize));
    m_verticalParagraph.appendText(u8"は縦書きに対応した。",
                                   japaneseStyle(fontSize));
    m_verticalParagraph.setWritingMode(WritingMode::kVerticalRL);

    m_horizontalParagraph.clear();
    m_horizontalParagraph.appendText(
        u8"漢字にルビを振ると、誰でも読みやすい。強調したい語には圏点を打つ。",
        japaneseStyle(fontSize * 0.8f));
  }

  /// Places a ruby annotation alongside the first matching vertical range.
  void rubyVertical(SkCanvas *canvas, FontContext &fontContext,
                    const ParagraphLayout &layout, std::u16string_view baseText,
                    const char8_t *rubyText) {
    const std::vector<CharRange> matches =
        findAllOccurrences(m_verticalParagraph, baseText);
    if (matches.empty())
      return;
    bool valid = false;
    int line = 0;
    SkPoint origin = {0, 0};
    float rangeBegin = 0;
    float rangeEnd = 0;
    for (const PositionedRun &run : layout.runs) {
      const Word &word = m_verticalParagraph.words()[run.wordIndex];
      if (word.textEnd <= matches[0].start ||
          word.textBegin >= matches[0].end || run.transformed)
        continue;
      if (!valid) {
        valid = true;
        line = run.lineIndex;
        origin = run.origin;
        rangeBegin = 0;
        rangeEnd = word.width;
      } else if (run.lineIndex == line) {
        const float offset = run.origin.y() - origin.y();
        rangeBegin = std::min(rangeBegin, offset);
        rangeEnd = std::max(rangeEnd, offset + word.width);
      }
    }
    if (!valid)
      return;
    Paragraph ruby;
    ruby.appendText(rubyText, japaneseStyle(m_fontSize * 0.5f));
    ruby.setWritingMode(WritingMode::kVerticalRL);
    const float length = ruby.naturalWidth(fontContext);
    LineSetFlow flow;
    flow.lines().push_back({LineInterval{
        {origin.x() + m_fontSize * 0.62f,
         origin.y() + (rangeBegin + rangeEnd) * 0.5f - length * 0.5f},
        {0, 1},
        length + 1}});
    layoutParagraph(fontContext, ruby, flow).draw(canvas, ruby);
  }

  /// Draws emphasis dots beside matching glyphs in the vertical paragraph.
  void kentenVertical(SkCanvas *canvas, const ParagraphLayout &layout,
                      std::u16string_view emphasis, float alpha) {
    const std::vector<CharRange> matches =
        findAllOccurrences(m_verticalParagraph, emphasis);
    if (matches.empty())
      return;
    SkPaint dot;
    dot.setAntiAlias(true);
    dot.setColor(kAccent);
    dot.setAlphaf(alpha);
    for (const PositionedRun &run : layout.runs) {
      const Word &word = m_verticalParagraph.words()[run.wordIndex];
      if (word.textEnd <= matches[0].start ||
          word.textBegin >= matches[0].end || run.transformed ||
          !run.shaped->vertical)
        continue;
      float penAdvance = 0;
      const ShapedWord &shapedWord = *run.shaped;
      for (size_t glyphIndex = 0; glyphIndex < shapedWord.advances.size();
           ++glyphIndex) {
        const uint32_t textOffset =
            word.textBegin + shapedWord.clusters[glyphIndex];
        if (textOffset >= matches[0].start && textOffset < matches[0].end)
          canvas->drawCircle(run.origin.x() + m_fontSize * 0.60f,
                             run.origin.y() + penAdvance +
                                 shapedWord.advances[glyphIndex] * 0.5f,
                             m_fontSize * 0.07f, dot);
        penAdvance += shapedWord.advances[glyphIndex];
      }
    }
  }

  /// Places a ruby annotation above the first matching horizontal range.
  void rubyHorizontal(SkCanvas *canvas, FontContext &fontContext,
                      const ParagraphLayout &layout,
                      std::u16string_view baseText, const char8_t *rubyText) {
    const std::vector<CharRange> matches =
        findAllOccurrences(m_horizontalParagraph, baseText);
    if (matches.empty())
      return;
    float rangeLeft = 0;
    float rangeRight = 0;
    float baseline = 0;
    bool valid = false;
    for (const PositionedRun &run : layout.runs) {
      const Word &word = m_horizontalParagraph.words()[run.wordIndex];
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
    ruby.appendText(rubyText, japaneseStyle(m_fontSize * 0.4f));
    const float width = ruby.naturalWidth(fontContext);
    LineSetFlow flow;
    flow.lines().push_back(
        {LineInterval{{(rangeLeft + rangeRight) * 0.5f - width * 0.5f,
                       baseline - m_fontSize * 0.92f},
                      {1, 0},
                      width + 1}});
    layoutParagraph(fontContext, ruby, flow).draw(canvas, ruby);
  }

  Paragraph m_verticalParagraph;
  Paragraph m_horizontalParagraph;
  sk_sp<SkTypeface> m_mincho;
  float m_fontSize = 0;
};

SceneDescriptor makeVerticalDescriptor() {
  SceneDescriptor descriptor;
  descriptor.name = QStringLiteral("Vertical CJK — ruby · kenten · 縦中横");
  descriptor.textEditable = false;
  descriptor.displayOrder = 60;
  descriptor.make = [] { return std::make_unique<VerticalScene>(); };
  return descriptor;
}

} // namespace

REGISTER_GALLERY_SCENE(makeVerticalDescriptor())

} // namespace gallery
