/** @file
 * Typographic options that reach shaping: OpenType feature toggles and
 * presets, locale-aware text transform, and word spacing added after
 * measurement.
 */

#include <gtest/gtest.h>
#include <sigilweave/kit/Features.h>

#include <string>
#include <utility>
#include <vector>

#include "support/ParagraphSupport.h"
#include "support/Readings.h"
using namespace sigil::weave;
using namespace sigil::weave::test;

// ── OpenType features ─────────────────────────────────────────────────────

TEST(Features, LigatureToggleChangesGlyphCount) {
  // The instrument ligates f f i under `liga` and does nothing else with
  // it, so the glyph count is the feature and not the face's own taste.
  FontContext& fontContext = sigil::test::fonts();
  sk_sp<SkTypeface> instrument = sigil::test::instrument::sans();
  ASSERT_TRUE(instrument);

  TextStyle ligaturesEnabledStyle = basicStyle();
  ligaturesEnabledStyle.shaping.typeface = instrument;
  TextStyle ligaturesDisabledStyle = ligaturesEnabledStyle;
  ligaturesDisabledStyle.shaping.fontFeatures.emplace_back("liga", 0);
  ligaturesDisabledStyle.shaping.fontFeatures.emplace_back("clig", 0);

  Paragraph ligaturesEnabledParagraph;
  Paragraph ligaturesDisabledParagraph;
  ligaturesEnabledParagraph.appendText(u8"official", ligaturesEnabledStyle);
  ligaturesDisabledParagraph.appendText(u8"official", ligaturesDisabledStyle);
  ligaturesEnabledParagraph.ensureShaped(fontContext);
  ligaturesDisabledParagraph.ensureShaped(fontContext);
  const size_t enabledGlyphCount =
      ligaturesEnabledParagraph.words()[0].segments()[0].shaped->glyphs.size();
  const size_t disabledGlyphCount =
      ligaturesDisabledParagraph.words()[0].segments()[0].shaped->glyphs.size();
  EXPECT_LT(enabledGlyphCount, disabledGlyphCount)
      << "'ffi' must ligate when liga is on";
  // Features are part of the shape-cache key: both variants coexist.
  EXPECT_NE(ligaturesEnabledParagraph.words()[0].segments()[0].shaped.get(),
            ligaturesDisabledParagraph.words()[0].segments()[0].shaped.get());
}

// ── Text transform (ShapingStyle::textTransform) ─────────────────────────

namespace {

Paragraph transformedParagraph(std::u8string_view text, TextTransform transform,
                               std::string languageTag = {}) {
  TextStyle style = basicStyle();
  style.shaping.textTransform = transform;
  style.shaping.languageTag = std::move(languageTag);
  Paragraph paragraph;
  paragraph.appendText(text, style);
  return paragraph;
}

float paragraphWidth(Paragraph& paragraph) {
  return paragraph.naturalWidth(sigil::test::fonts());
}

}  // namespace

TEST(TextTransform, UppercaseShapesUppercaseGlyphs) {
  Paragraph transformed =
      transformedParagraph(u8"hello", TextTransform::kUppercase);
  Paragraph reference = makeParagraph(u8"HELLO");
  // Identical shaped output — and, per the documented contract, the same
  // shape-cache entry, since the transformed text is itself the key text.
  transformed.ensureShaped(sigil::test::fonts());
  reference.ensureShaped(sigil::test::fonts());
  EXPECT_EQ(transformed.words()[0].segments()[0].shaped.get(),
            reference.words()[0].segments()[0].shaped.get());
  // The stored document text stays untransformed.
  EXPECT_EQ(transformed.text(), u"hello");
}

TEST(TextTransform, GermanSharpSExpandsUnderUppercase) {
  Paragraph transformed =
      transformedParagraph(u8"straße", TextTransform::kUppercase);
  Paragraph reference = makeParagraph(u8"STRASSE");
  EXPECT_NEAR(paragraphWidth(transformed), paragraphWidth(reference), 0.5f)
      << "ß must full-map to SS, not simple-map";
}

TEST(TextTransform, TurkishDotlessIRespectsLocale) {
  Paragraph turkish =
      transformedParagraph(u8"istanbul", TextTransform::kUppercase, "tr");
  Paragraph plain =
      transformedParagraph(u8"istanbul", TextTransform::kUppercase);
  turkish.ensureShaped(sigil::test::fonts());
  plain.ensureShaped(sigil::test::fonts());
  // tr maps i → İ (dotted capital); the root locale maps i → I. Different
  // glyph streams must come back.
  EXPECT_NE(turkish.words()[0].segments()[0].shaped->glyphs,
            plain.words()[0].segments()[0].shaped->glyphs);
}

TEST(TextTransform, CapitalizeTitlecasesFirstLetterOnly) {
  Paragraph transformed = transformedParagraph(u8"mixedCase words here",
                                               TextTransform::kCapitalize);
  Paragraph reference = makeParagraph(u8"MixedCase Words Here");
  transformed.ensureShaped(sigil::test::fonts());
  reference.ensureShaped(sigil::test::fonts());
  ASSERT_EQ(transformed.words().size(), reference.words().size());
  for (size_t wordIndex = 0; wordIndex < reference.words().size(); ++wordIndex)
    EXPECT_EQ(transformed.words()[wordIndex].segments()[0].shaped.get(),
              reference.words()[wordIndex].segments()[0].shaped.get())
        << "word " << wordIndex
        << ": capitalize must uppercase the first letter and leave the rest";
}

// ── Word spacing (ShapingStyle::wordSpacing) ─────────────────────────────

TEST(WordSpacing, WidensGlueWithoutReshaping) {
  FontContext& fontContext = sigil::test::fonts();
  Paragraph paragraph = makeParagraph(u8"alpha beta gamma");
  paragraph.ensureShaped(fontContext);
  const float baseGlue = paragraph.words()[0].spaceWidth;
  ASSERT_GT(baseGlue, 0.0f);

  fontContext.resetStats();
  TextStyle spaced = basicStyle();
  spaced.shaping.wordSpacing = 12.0f;
  paragraph.setStyle(0, static_cast<uint32_t>(paragraph.text().size()), spaced);
  paragraph.ensureShaped(fontContext);
  EXPECT_FLOAT_EQ(paragraph.words()[0].spaceWidth, baseGlue + 12.0f);
  EXPECT_EQ(fontContext.stats().shapeCalls, 0u)
      << "word spacing must not enter the shape-cache key";

  // Negative spacing shrinks but never below zero.
  TextStyle crushed = basicStyle();
  crushed.shaping.wordSpacing = -1000.0f;
  paragraph.setStyle(0, static_cast<uint32_t>(paragraph.text().size()),
                     crushed);
  paragraph.ensureShaped(fontContext);
  EXPECT_FLOAT_EQ(paragraph.words()[0].spaceWidth, 0.0f);
}

// ── features:: presets (style/Features.h)
// ──────────────────────────────────────

TEST(FeaturePresets, TabularNumbersEqualizeDigitAdvances) {
  FontContext& fontContext = sigil::test::fonts();
  // The instrument gives every figure a different advance and carries a
  // tnum that equalises them, so both halves of the claim are measurable
  // here — on a face whose figures are already even there is nothing to
  // prove.
  auto digitWidths = [&](std::vector<FontFeature> features) {
    TextStyle style = basicStyle(32.0f);
    style.shaping.typeface = sigil::test::instrument::sans();
    style.shaping.fontFeatures = std::move(features);
    std::vector<float> widths;
    for (const char8_t* digit : {u8"1", u8"0", u8"7", u8"9"}) {
      Paragraph paragraph;
      paragraph.appendText(digit, style);
      paragraph.ensureShaped(fontContext);
      widths.push_back(paragraph.words()[0].width);
    }
    return widths;
  };

  const std::vector<float> proportional = digitWidths({});
  const std::vector<float> tabular = digitWidths({features::tabularNumbers});

  EXPECT_GT(spread(proportional), 0.01f)
      << "the instrument's figures must be proportional, or the feature has "
         "nothing to equalise";
  EXPECT_LT(spread(tabular), 0.01f) << "tabular figures must share one advance";
}
