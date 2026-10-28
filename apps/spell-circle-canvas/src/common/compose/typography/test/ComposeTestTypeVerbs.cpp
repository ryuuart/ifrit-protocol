// The type verbs CSS names beyond the font's family, size and weight: the
// OpenType features and variation axes, the kerning, the word spacing, the
// case, the vertical orientation and the baseline shift on the font's
// side; the last line's alignment, the hanging punctuation, the widows
// and the orphans on the paragraph's. Each writes one longhand of the
// partial it belongs to, and each text form reads as CSS writes it.

#include <sigilcompose/core/StyleSheet.h>

#include <string>
#include <vector>

#include "DressedTypeProbes.h"
#include "support/Rows.h"

namespace {

using sigil::compose::FontKerning;
using sigil::compose::readFontFeatureSettings;
using sigil::compose::readFontVariationSettings;
using sigil::compose::readHangingPunctuation;
using sigil::compose::rule;
using sigil::compose::TextCombineUpright;
using sigil::compose::TextOrientation;
using sigil::weave::FontFeature;
using sigil::weave::FontVariation;
using sigil::weave::TextAlignment;
using sigil::weave::VerticalForm;

/** The width the passage under @p key was laid out at. */
float widthOf(Host& host, std::string_view key) {
  const auto bounds = host.composer.bounds(key);
  return bounds ? bounds->width() : 0.0f;
}

}  // namespace

TEST(ComposeTypeVerbs, EachTextFormReadsAsCssWritesIt) {
  sigil::compose::test::checkRows({
      {"FontFeatureSettings",
       [&] {
         const auto read = readFontFeatureSettings(
             "\"tnum\", 'liga' 0, ss02 2, \"kern\" off");
         ASSERT_TRUE(read.has_value());
         EXPECT_EQ(*read,
                   (std::vector<FontFeature>{
                       {"tnum", 1}, {"liga", 0}, {"ss02", 2}, {"kern", 0}}));
         EXPECT_EQ(readFontFeatureSettings(" normal "),
                   std::optional{std::vector<FontFeature>{}});
         EXPECT_FALSE(readFontFeatureSettings("\"tabular\"").has_value())
             << "a tag is four letters";
         EXPECT_FALSE(readFontFeatureSettings("\"tnum\" -1").has_value());
         EXPECT_FALSE(readFontFeatureSettings("\"tnum\" 1.5").has_value());
       }},
      {"FontVariationSettings",
       [&] {
         const auto read =
             readFontVariationSettings("\"wdth\" 75, 'GRAD' -12.5");
         ASSERT_TRUE(read.has_value());
         EXPECT_EQ(*read, (std::vector<FontVariation>{{"wdth", 75},
                                                      {"GRAD", -12.5f}}));
         EXPECT_FALSE(readFontVariationSettings("\"wdth\"").has_value())
             << "an axis needs a value";
       }},
      {"HangingPunctuation",
       [&] {
         const auto table = readHangingPunctuation("first allow-end last");
         ASSERT_TRUE(table.has_value());
         const auto* opening = table->find(u'“');
         ASSERT_NE(opening, nullptr);
         EXPECT_EQ(opening->atStart, 1.0f);
         const auto* closing = table->find(u'”');
         ASSERT_NE(closing, nullptr);
         EXPECT_EQ(closing->atEnd, 1.0f);
         const auto* stop = table->find(u'.');
         ASSERT_NE(stop, nullptr);
         EXPECT_EQ(stop->atEnd, 1.0f);
         EXPECT_EQ(stop->atStart, 0.0f);
         const auto* quote = table->find(u'"');
         ASSERT_NE(quote, nullptr);
         EXPECT_EQ(quote->atStart, 1.0f) << "a straight quote opens and closes";
         EXPECT_EQ(quote->atEnd, 1.0f);
         EXPECT_TRUE(readHangingPunctuation("none")->empty());
         EXPECT_EQ(readHangingPunctuation("first")->find(u'.'), nullptr);
         EXPECT_FALSE(readHangingPunctuation("first first").has_value());
         EXPECT_FALSE(readHangingPunctuation("none first").has_value());
         EXPECT_FALSE(readHangingPunctuation("hang").has_value());
         EXPECT_FALSE(readHangingPunctuation("").has_value());
       }},
      {"TextThatDoesNotReadLeavesTheValueInForce",
       [&] {
         const auto stated = rule(".x")
                                 .fontFeatureSettings("\"tnum\"")
                                 .fontFeatureSettings("tabular-nums");
         EXPECT_EQ(stated.type().features,
                   (std::vector<FontFeature>{{"tnum", 1}}));
         EXPECT_FALSE(
             rule(".x")
                 .baselineShift(sigil::compose::Dimension(sigil::weave::em(1)))
                 .type()
                 .baselineShift.has_value())
             << "a shift against the font is refused";
       }},
  });
}

TEST(ComposeTypeVerbs, EachTypeVerbWritesItsLonghand) {
  sigil::compose::test::checkRows({
      {"EachFontVerb",
       [&] {
         const auto stated =
             rule(".x")
                 .fontFeatureSettings("\"tnum\"")
                 .fontVariationSettings("\"wdth\" 80")
                 .fontKerning(FontKerning::Optical)
                 .wordSpacing(3)
                 .textTransform(sigil::weave::TextTransform::kUppercase)
                 .textOrientation(TextOrientation::Upright)
                 .baselineShift(sigil::compose::Dimension(sigil::weave::pt(3)));
         const sigil::weave::Type& type = stated.type();
         EXPECT_EQ(type.features, (std::vector<FontFeature>{{"tnum", 1}}));
         EXPECT_EQ(type.variations, (std::vector<FontVariation>{{"wdth", 80}}));
         EXPECT_EQ(type.opticalKerning, true);
         EXPECT_EQ(type.wordSpacing, sigil::weave::Length(3.0f));
         EXPECT_EQ(type.textTransform, sigil::weave::TextTransform::kUppercase);
         EXPECT_EQ(type.verticalForm, VerticalForm::kUpright);
         EXPECT_EQ(type.baselineShift, 4.0f)
             << "a point is four thirds of a pixel";
         EXPECT_EQ(rule(".x")
                       .textCombineUpright(TextCombineUpright::All)
                       .type()
                       .verticalForm,
                   VerticalForm::kTateChuYoko);
         EXPECT_EQ(rule(".x")
                       .textOrientation(TextOrientation::Sideways)
                       .type()
                       .verticalForm,
                   VerticalForm::kRotated);
         EXPECT_EQ(
             rule(".x").fontKerning(FontKerning::Normal).type().opticalKerning,
             false);
       }},
      {"EachParagraphVerb",
       [&] {
         const auto stated = rule(".x")
                                 .textAlignLast(TextAlignment::kJustify)
                                 .hangingPunctuation("first")
                                 .widows(2)
                                 .orphans(3);
         const sigil::weave::ParagraphBlock& block = stated.paragraph();
         EXPECT_EQ(block.lastLineAlignment, TextAlignment::kJustify);
         EXPECT_EQ(block.justifyLastLine, true);
         ASSERT_TRUE(block.hanging.has_value());
         EXPECT_NE(block.hanging->find(u'('), nullptr);
         EXPECT_EQ(block.widowLines, 2);
         EXPECT_EQ(block.orphanLines, 3);
         const auto centred = rule(".x").textAlignLast(TextAlignment::kCenter);
         EXPECT_EQ(centred.paragraph().justifyLastLine, false);
         const sigil::weave::HangingTable optical{
             {{.character = u'-', .atEnd = 0.5f}}};
         EXPECT_EQ(rule(".x").hangingPunctuation(optical).paragraph().hanging,
                   optical)
             << "a table of fractions stands as it was given";
       }},
  });
}

TEST(ComposeTypeVerbs, WordSpacingWidensTheGapsItInherits) {
  Host host(600, 200);
  const auto tree = [](float spacing) {
    return box().wordSpacing(spacing).children(
        {text(u8"one two three four")
             .fontFamily(sigil::test::instrument::sans())
             .fontSize(20)
             .key("words")});
  };
  host.composer.render(tree(0));
  host.frame();
  const float plain = widthOf(host, "words");
  host.composer.render(tree(10));
  host.frame();
  EXPECT_NEAR(widthOf(host, "words") - plain, 30.0f, 0.5f)
      << "three gaps, each ten pixels wider";
}
