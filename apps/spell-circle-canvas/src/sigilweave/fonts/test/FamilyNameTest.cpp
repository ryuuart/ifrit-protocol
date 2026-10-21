/** @file
 * A face's family name and the comparison that says two names are one
 * family, asked of the committed instrument faces so no installed font is
 * needed.
 */

#include <gtest/gtest.h>
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
