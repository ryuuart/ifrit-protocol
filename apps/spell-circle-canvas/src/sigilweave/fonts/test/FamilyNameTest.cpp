/** @file
 * A face's family name and the font's own name for its style, and the
 * comparisons that say two names are one, asked of the committed instrument
 * faces so no installed font is needed.
 */

#include <gtest/gtest.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkTypeface.h>
#include <sigilweave/fonts/FamilyName.h>

#include <Fonts.h>

using namespace sigil::weave;

TEST(FamilyName, ATypefaceStatesTheFamilyItsFontNames) {
  const sk_sp<SkTypeface> sans = sigil::test::instrument::sans();
  ASSERT_TRUE(sans);
  EXPECT_EQ(fonts::familyName(*sans), "Sigil Instrument Sans");
}

TEST(FamilyName, TwoNamesAreOneFamilyWithCaseAsideAsCssComparesThem) {
  EXPECT_TRUE(fonts::sameFamily("Avenir Next", "avenir next"));
  EXPECT_TRUE(fonts::sameFamily("GEORGIA", "Georgia"));
  EXPECT_TRUE(fonts::sameFamily("", ""));
  // Spaces are part of a family name, and a prefix is another family.
  EXPECT_FALSE(fonts::sameFamily("Avenir Next", "AvenirNext"));
  EXPECT_FALSE(fonts::sameFamily("Avenir", "Avenir Next"));
  EXPECT_FALSE(fonts::sameFamily("Georgia", ""));
}

TEST(FamilyName, AFaceStatesTheNameItsFontGivesItsStyle) {
  const sk_sp<SkTypeface> sans = sigil::test::instrument::sans();
  ASSERT_TRUE(sans);
  EXPECT_EQ(fonts::styleName(*sans), "Regular");
}

TEST(FamilyName, AStyleNamedNowhereIsTheNameItsWeightAndSlantCompose) {
  EXPECT_EQ(fonts::weightName(400), "Normal");
  EXPECT_EQ(fonts::weightName(640), "Semi Bold");
  EXPECT_EQ(fonts::weightName(1000), "Black");
  EXPECT_EQ(fonts::composedStyleName(SkFontStyle::Bold()), "Bold");
  EXPECT_EQ(fonts::composedStyleName(SkFontStyle(
                600, SkFontStyle::kNormal_Width, SkFontStyle::kItalic_Slant)),
            "Semi Bold Italic");
  EXPECT_EQ(fonts::composedStyleName(SkFontStyle(
                300, SkFontStyle::kNormal_Width, SkFontStyle::kOblique_Slant)),
            "Light Oblique");
}

TEST(FamilyName, TwoStyleNamesAreOneWithCaseAndSpacesAside) {
  EXPECT_TRUE(fonts::sameStyleName("Demi Bold Italic", "demibold italic"));
  EXPECT_TRUE(fonts::sameStyleName(" Book ", "book"));
  EXPECT_FALSE(fonts::sameStyleName("Book", "Book Oblique"));
  EXPECT_FALSE(fonts::sameStyleName("Roman", "Regular"));
}
