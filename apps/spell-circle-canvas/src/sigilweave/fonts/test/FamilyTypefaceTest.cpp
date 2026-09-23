/** @file
 * The font service asked for a family by name: the machine's own families,
 * so every case here carries the `fonts` label.
 */

#include <gtest/gtest.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkString.h>
#include <include/core/SkTypeface.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

using namespace sigil::weave;

TEST(FamilyTypeface, OneFamilyAndStyleAnswerWithOneFace) {
  // The machine's own families, so the case carries the `fonts` label: a
  // family asked for twice is the same object, which is what lets its id
  // key the shape cache, and a family nobody installed is no face at all.
  FontContext fontContext(ports::systemFontManager());
  const sk_sp<SkTypeface> first = fontContext.familyTypeface("Georgia");
  const sk_sp<SkTypeface> again = fontContext.familyTypeface("Georgia");
  ASSERT_TRUE(first);
  EXPECT_EQ(first.get(), again.get());
  SkString name;
  first->getFamilyName(&name);
  EXPECT_STREQ(name.c_str(), "Georgia");
  EXPECT_FALSE(fontContext.familyTypeface("No Family Is Named This"));
  EXPECT_FALSE(fontContext.familyTypeface(""));
}

TEST(FamilyTypeface, AStyleChoosesAmongTheFamilysFaces) {
  // Georgia carries an italic face and Impact does not: the manager hands
  // back the italic where there is one and the nearest upright where there
  // is none, and never a face of another family.
  FontContext fontContext(ports::systemFontManager());
  const sk_sp<SkTypeface> italic =
      fontContext.familyTypeface("Georgia", SkFontStyle::Italic());
  ASSERT_TRUE(italic);
  EXPECT_NE(italic->fontStyle().slant(), SkFontStyle::kUpright_Slant);
  const sk_sp<SkTypeface> onlyUpright =
      fontContext.familyTypeface("Impact", SkFontStyle::Italic());
  ASSERT_TRUE(onlyUpright);
  EXPECT_EQ(onlyUpright->fontStyle().slant(), SkFontStyle::kUpright_Slant);
}
