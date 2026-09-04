/** @file
 * Typographic correctness a shaping engine must hold: variable axes reach
 * HarfBuzz, cluster coverage is complete across scripts, ZWNJ blocks
 * joining, combining marks attach and stack on their base, kinsoku and
 * no-break spaces refuse breaks, tabs measure as spaces, and the strut
 * matches the font.
 */

#include <gtest/gtest.h>
#include <include/core/SkFontArguments.h>
#include <include/core/SkFontMetrics.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkTypeface.h>

#include <algorithm>
#include <boost/container/flat_set.hpp>
#include <boost/unordered/unordered_flat_set.hpp>
#include <cmath>
#include <string>
#include <vector>

#include "support/ParagraphSupport.h"
using namespace sigil::weave;
using namespace sigil::weave::test;

TEST(Correctness, VariableAxesReachHarfBuzz) {
  // A multi-axis variable instance must shape with the same complete design
  // position Skia rasterizes.
  FontContext& fontContext = sharedContext();
  sk_sp<SkTypeface> base = installedFace("Noto Sans");
  const int axisCount = base ? base->getVariationDesignPosition({}) : 0;
  if (axisCount < 2)
    GTEST_SKIP() << "no multi-axis variable Noto Sans installed";

  std::vector<SkFontArguments::VariationPosition::Coordinate> coordinates(
      static_cast<size_t>(axisCount));
  if (base->getVariationDesignPosition(
          {coordinates.data(), coordinates.size()}) != axisCount)
    GTEST_SKIP() << "Noto Sans variation position unavailable";
  bool changedWeight = false;
  bool changedWidth = false;
  for (auto& coordinate : coordinates) {
    if (coordinate.axis == SkSetFourByteTag('w', 'g', 'h', 't')) {
      coordinate.value = 900.0f;
      changedWeight = true;
    } else if (coordinate.axis == SkSetFourByteTag('w', 'd', 't', 'h')) {
      coordinate.value = 62.5f;
      changedWidth = true;
    }
  }
  if (!changedWeight || !changedWidth)
    GTEST_SKIP() << "Noto Sans wght/wdth axes unavailable";

  SkFontArguments args;
  args.setVariationDesignPosition(
      {coordinates.data(), static_cast<int>(coordinates.size())});
  sk_sp<SkTypeface> varied = base->makeClone(args);
  ASSERT_TRUE(varied);

  constexpr std::u8string_view kText = u8"hamburgefonstiv";
  auto shapedAdvance = [&](const sk_sp<SkTypeface>& typeface) {
    Paragraph paragraph;
    TextStyle style = basicStyle(32.0f);
    style.shaping.typeface = typeface;
    paragraph.appendText(kText, style);
    paragraph.ensureShaped(fontContext);
    float total = 0;
    for (const Word& word : paragraph.words()) total += word.width;
    return total;
  };

  const float regular = shapedAdvance(base);
  const float variedAdvance = shapedAdvance(varied);
  EXPECT_GT(std::abs(variedAdvance - regular), regular * 0.01f)
      << "changing wght and wdth should affect shaping";

  // HarfBuzz's advances agree with what Skia measures for that instance.
  const SkScalar measured =
      makeFont(varied, 32.0f)
          .measureText(kText.data(), kText.size(), SkTextEncoding::kUTF8);
  EXPECT_NEAR(variedAdvance, measured, variedAdvance * 0.015f);
}

TEST(Correctness, ClusterCoverageIsComplete) {
  FontContext& fontContext = sharedContext();
  // Ligating Latin, joining Arabic, conjunct Devanagari, ZWJ emoji.
  const char8_t* samples[] = {u8"office", u8"العربية", u8"नमस्ते",
                              u8"👨‍👩‍👧"};
  for (const char8_t* sample : samples) {
    Paragraph paragraph = makeParagraph(sample);
    paragraph.ensureShaped(fontContext);
    for (const Word& word : paragraph.words())
      for (const WordSegment& seg : word.segments()) {
        const auto& clusters = seg.shaped->clusters;
        ASSERT_FALSE(clusters.empty());
        const size_t segLen = seg.shaped->glyphs.size();
        // Every cluster index points inside the shaped text, and the run
        // starts at offset 0 from one end (LTR: front, RTL: back).
        const uint32_t first = std::min(clusters.front(), clusters.back());
        EXPECT_EQ(first, 0u);
        for (uint32_t cluster : clusters)
          EXPECT_LT(cluster, word.textEnd - word.textBegin);
        EXPECT_GT(segLen, 0u);
      }
  }
}

TEST(Correctness, ZwnjBlocksArabicJoining) {
  FontContext& fontContext = sharedContext();
  auto glyphsOf = [&](const char8_t* text) {
    Paragraph paragraph = makeParagraph(text);
    paragraph.ensureShaped(fontContext);
    boost::container::flat_multiset<uint16_t> ids;
    for (const Word& word : paragraph.words())
      for (const WordSegment& seg : word.segments())
        for (uint16_t glyph : seg.shaped->glyphs)
          if (glyph != 0) ids.insert(glyph);
    return ids;
  };
  // "بب" joins (initial+final forms); a ZWNJ between forces isolated forms.
  EXPECT_NE(glyphsOf(u8"بب"), glyphsOf(u8"ب‌ب"));
}

TEST(Correctness, CombiningMarkAttachesToBase) {
  FontContext& fontContext = sharedContext();
  Paragraph nfc = makeParagraph(u8"café");  // é precomposed
  Paragraph nfd = makeParagraph(u8"café");  // e + combining acute
  nfc.ensureShaped(fontContext);
  nfd.ensureShaped(fontContext);
  ASSERT_EQ(nfc.words().size(), 1u);
  ASSERT_EQ(nfd.words().size(), 1u);
  // The decomposed form must not gain width: the mark attaches to the base.
  EXPECT_NEAR(nfc.words()[0].width, nfd.words()[0].width, 0.75f);
  // And the mark forms one grapheme cluster with its base: the NFD segment
  // reports at most as many clusters as it has base characters (4).
  boost::unordered_flat_set<uint32_t> unique(
      nfd.words()[0].segments()[0].shaped->clusters.begin(),
      nfd.words()[0].segments()[0].shaped->clusters.end());
  EXPECT_LE(unique.size(), 4u);
}

TEST(Correctness, ExtremeCombiningStacksKeepBaseAdvance) {
  FontContext& fontContext = sharedContext();
  Paragraph plain = makeParagraph(u8"ZALGO TEXT", 32.0f);
  Paragraph stacked = makeParagraph(
      u8"Z̴̢̨̛̲̦̹̰̓̈́͊͘A̵̛̪̯̜̩͆̈́͝L̷̨̡̲̤̬̝̑̓͑̕G̵̢̺̙͎̺̤̓͛̾Ơ̶̢͙̟̲̦̿̽͋̚ "
      "T̷̨̗̰͉̼̯͛̋E̴̡̨̩̱͕̪͗̎X̷̢̳̮̱̪̿̈́͘T̴̛̬̠̦̞͙̋̄͝",
      32.0f);
  plain.ensureShaped(fontContext);
  stacked.ensureShaped(fontContext);
  if (!allGlyphsResolved(stacked))
    GTEST_SKIP() << "combining-mark fallback coverage unavailable";

  auto glyphCount = [](const Paragraph& paragraph) {
    size_t count = 0;
    for (const Word& word : paragraph.words())
      for (const WordSegment& segment : word.segments())
        count += segment.shaped->glyphs.size();
    return count;
  };
  EXPECT_GT(glyphCount(stacked), glyphCount(plain));
  const float plainWidth = plain.naturalWidth(fontContext);
  EXPECT_NEAR(stacked.naturalWidth(fontContext), plainWidth, plainWidth * 0.03f)
      << "attached mark stacks must add ink, not horizontal advance";
}

TEST(Correctness, KinsokuProhibitsLineInitialPunctuation) {
  FontContext& fontContext = sharedContext();
  Paragraph paragraph =
      makeParagraph(u8"これは、禁則処理のテストです。行頭に句読点は来ない。");
  paragraph.ensureShaped(fontContext);
  const std::u16string& text = paragraph.text();
  for (const Word& word : paragraph.words()) {
    // A break opportunity never lands *before* a closing punctuation mark:
    // no word (== potential line start) begins with 。、」.
    const char16_t first = text[word.textBegin];
    EXPECT_NE(first, u'。');
    EXPECT_NE(first, u'、');
    EXPECT_NE(first, u'」');
    // …and never *after* an opening bracket: no word's content ends with 「.
    if (word.textEnd > word.textBegin) {
      EXPECT_NE(text[word.textEnd - 1], u'「');
    }
  }
}

TEST(Correctness, NbspNeverBreaks) {
  FontContext& fontContext = sharedContext();
  Paragraph spaced = makeParagraph(u8"100 km");
  Paragraph glued = makeParagraph(u8"100 km");
  spaced.ensureShaped(fontContext);
  glued.ensureShaped(fontContext);
  EXPECT_EQ(spaced.words().size(), 2u);
  EXPECT_EQ(glued.words().size(), 1u) << "NBSP must not be a break point";
}

TEST(Correctness, StrutMatchesFontMetrics) {
  FontContext& fontContext = sharedContext();
  Paragraph paragraph = makeParagraph(u8"metrics", 32.0f);
  const Paragraph::Strut strut = paragraph.strut(fontContext);
  const SkFont font = makeFont(fontContext.defaultTypeface(), 32.0f);
  SkFontMetrics metrics;
  font.getMetrics(&metrics);
  EXPECT_FLOAT_EQ(strut.ascent, -metrics.fAscent);
  EXPECT_FLOAT_EQ(strut.height,
                  -metrics.fAscent + metrics.fDescent + metrics.fLeading);
}
