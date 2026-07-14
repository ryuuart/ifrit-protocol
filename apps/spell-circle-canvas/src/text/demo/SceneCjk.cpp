// Scene H — CJK typography: vertical-rl, ruby, kenten, tate-chu-yoko.
//
// Ruby (furigana) and kenten (emphasis dots) are deliberately *not* library
// features: they are a dozen lines each on top of the layout's placed runs
// — the "build externally on TextFlow" pattern. Tate-chu-yoko needs the
// breaker's cooperation, so it *is* a library feature
// (VerticalForm::kTateChuYoko).
#include "DemoScenes.h"
#include "DemoSupport.h"

#include <textflow/Query.h>

#include <include/core/SkCanvas.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkPaint.h>
#include <include/core/SkSurface.h>

#include <algorithm>
#include <cstdio>

using namespace textflow;

namespace {

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

} // namespace

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
