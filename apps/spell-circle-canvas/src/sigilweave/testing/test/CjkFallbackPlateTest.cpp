/** @file
 * CJK fallback coverage: Noto Sans and Noto Serif as primary families
 * whose Latin faces send the Japanese, Korean, Simplified and Traditional
 * Chinese clauses through the font context's fallback. The panel claims
 * that each clause fell back and resolved every glyph, and nothing about
 * WHICH family it fell back to — that policy belongs to the font manager
 * or the fallback resolver a host supplies.
 */

#include <gtest/gtest.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkString.h>
#include <sigilweave/layout/Flow.h>
#include <sigilweave/testing/Passage.h>

#include <iterator>
#include <utility>

#include "support/Plates.h"

using namespace sigil::weave;
using namespace sigil::weave::test;
namespace weave = sigil::weave;

namespace {

struct Row {
  const char* languageTag;
  const char8_t* text;  ///< a Latin lead-in and a clause that falls back
};

const Row kRows[] = {
    {"ja", u8"Japanese falls back to 日本語のテキスト for this sentence."},
    {"ko", u8"Korean falls back to 한국어 텍스트 for this sentence."},
    {"zh-Hans", u8"Simplified Chinese falls back to 简体中文文本 here."},
    {"zh-Hant", u8"Traditional Chinese falls back to 繁體中文文本 here."},
};

/// Whether any segment of @p paragraph shaped in a face other than
/// @p primary.
bool fellBack(const Paragraph& paragraph, const SkTypeface& primary) {
  for (const Word& word : paragraph.words())
    for (const WordSegment& segment : word.segments())
      if (segment.shaped->typeface.get() != &primary) return true;
  return false;
}

/// Whether every glyph of @p paragraph is a real glyph rather than the
/// missing-glyph box.
bool everyGlyphResolved(const Paragraph& paragraph) {
  for (const Word& word : paragraph.words())
    for (const WordSegment& segment : word.segments())
      for (const uint16_t glyph : segment.shaped->glyphs)
        if (glyph == 0) return false;
  return true;
}

}  // namespace

TEST(WeavePlates, CjkFallbackDrawsItsBaseline) {
  FontContext& fonts = sigil::test::fonts();
  SkFontMgr* fontManager = fonts.fontManager();
  const sk_sp<SkTypeface> sans =
      fontManager->matchFamilyStyle("Noto Sans", SkFontStyle());
  const sk_sp<SkTypeface> serif =
      fontManager->matchFamilyStyle("Noto Serif", SkFontStyle());
  if (!sans || !serif)
    GTEST_SKIP() << "the panel's primary families are Noto Sans and Serif";

  constexpr int kRowCount = static_cast<int>(std::size(kRows));
  constexpr float kColumnWidth = 460;
  constexpr float kRowHeight = 130;
  const weave::testing::Plate plate(
      {static_cast<int>(80 + 2 * kColumnWidth),
       static_cast<int>(60 + kRowCount * kRowHeight)},
      kPaper);

  auto drawCaption = [&](const char8_t* text, float left, float top) {
    kit::drawLabel(plate.canvas(), fonts, text, {left, top},
                   {.fontSize = 13,
                    .color = kAccent,
                    .width = kColumnWidth,
                    .height = 18});
  };
  drawCaption(u8"Noto Sans primary", 40, 16);
  drawCaption(u8"Noto Serif primary", 40 + kColumnWidth, 16);

  for (int rowIndex = 0; rowIndex < kRowCount; ++rowIndex) {
    const Row& row = kRows[rowIndex];
    const float top = 40 + static_cast<float>(rowIndex) * kRowHeight;
    const std::pair<const sk_sp<SkTypeface>*, float> columns[] = {
        {&sans, 40.0f}, {&serif, 40.0f + kColumnWidth}};
    for (const auto& [primary, left] : columns) {
      TextStyle style = plateStyle(17, kInk, row.languageTag);
      style.shaping.typeface = *primary;
      Paragraph paragraph;
      paragraph.appendText(row.text, style);
      BlockFlow flow(
          sigil::geometry::path::Rect::of({left, top}, {kColumnWidth, kRowHeight - 12}));
      const weave::testing::Passage passage =
          weave::testing::lay(fonts, std::move(paragraph), flow);
      EXPECT_TRUE(fellBack(passage.paragraph, **primary)) << row.languageTag;
      EXPECT_TRUE(everyGlyphResolved(passage.paragraph)) << row.languageTag;
      plate.draw(passage);
    }
  }

  expectPlate(plate, "fallback_cjk");
}
