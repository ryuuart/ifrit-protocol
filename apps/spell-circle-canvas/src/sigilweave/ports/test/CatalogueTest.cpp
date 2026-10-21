/** @file
 * The type catalogue: the machine's families read as styles and axes —
 * the `fonts` label's cases — and a context's adopted faces among them,
 * read from the committed instrument faces.
 */

#include <gtest/gtest.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/Catalogue.h>
#include <sigilweave/ports/Resource.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "support/Faces.h"

namespace sigil::weave {
namespace {

bool lists(const std::vector<std::string>& names, const std::string& name) {
  return std::ranges::find(names, name) != names.end();
}

TEST(TypeCatalogue, AnInstalledFamilyListsItsStylesByName) {
  EXPECT_TRUE(lists(fonts::families(), "Georgia"));
  const std::optional<fonts::Family> georgia = fonts::family("Georgia");
  ASSERT_TRUE(georgia);
  EXPECT_EQ(georgia->name, "Georgia");
  EXPECT_FALSE(georgia->bundled);
  const auto boldItalic =
      std::ranges::find_if(georgia->styles, [](const fonts::Style& style) {
        return style.weight == 700 && style.slant == FaceSlant::Italic;
      });
  ASSERT_NE(boldItalic, georgia->styles.end());
  EXPECT_EQ(boldItalic->weightName, "Bold");
  EXPECT_FALSE(boldItalic->name.empty());
  EXPECT_TRUE(boldItalic->uri.empty());
  const Face bold = fonts::find("Georgia", {.weight = 700});
  ASSERT_TRUE(bold);
  EXPECT_EQ(bold.familyName(), "Georgia");
  EXPECT_GE(bold.style().weight, 600);
}

TEST(TypeCatalogue, AStyleIsFoundByTheFontsOwnNameForIt) {
  const std::optional<fonts::Family> georgia = fonts::family("Georgia");
  ASSERT_TRUE(georgia);
  const auto boldItalic =
      std::ranges::find_if(georgia->styles, [](const fonts::Style& style) {
        return style.weight == 700 && style.slant == FaceSlant::Italic;
      });
  ASSERT_NE(boldItalic, georgia->styles.end());
  const std::string named = boldItalic->name;
  // The family's own lookup, with case and spaces aside.
  const fonts::Style* listed = georgia->style(named);
  ASSERT_NE(listed, nullptr);
  EXPECT_EQ(*listed, *boldItalic);
  std::string loose;
  for (const char character : named)
    if (character != ' ')
      loose.push_back(static_cast<char>(
          std::tolower(static_cast<unsigned char>(character))));
  EXPECT_EQ(georgia->style(loose), listed);
  EXPECT_EQ(georgia->style("No Style Is Named This"), nullptr);
  // The face under that name is the one the family lists under it.
  const Face face = fonts::find("Georgia", named);
  ASSERT_TRUE(face);
  EXPECT_EQ(face.familyName(), "Georgia");
  EXPECT_EQ(face.style().weight, 700);
  EXPECT_EQ(face.style().slant, FaceSlant::Italic);
  EXPECT_EQ(fonts::find("georgia", loose), face);
  EXPECT_FALSE(fonts::find("Georgia", "No Style Is Named This"));
  EXPECT_FALSE(fonts::find("No Family Is Named This", named));
}

TEST(TypeCatalogue, AFamilyNobodyInstalledIsNothing) {
  EXPECT_FALSE(fonts::family("No Family Is Named This"));
  EXPECT_FALSE(fonts::find("No Family Is Named This"));
}

TEST(TypeCatalogueNames, AWeightIsNamedAsCssNamesIt) {
  EXPECT_EQ(fonts::weightName(100), "Thin");
  EXPECT_EQ(fonts::weightName(400), "Normal");
  EXPECT_EQ(fonts::weightName(600), "Semi Bold");
  EXPECT_EQ(fonts::weightName(680), "Bold");
  EXPECT_EQ(fonts::weightName(1000), "Black");
}

TEST(BundledCatalogue, AnAdoptedFaceStandsInTheCatalogueAsBundled) {
  FontContext context(ports::systemFontManager());
  const Face face = ports::faceFromBytes(test::instrumentBytes("Sans.ttf"));
  ASSERT_TRUE(face);
  const std::string family = face.familyName();
  EXPECT_FALSE(lists(fonts::families(context), family));
  context.adopt(face, "sketch://type/data/Sans.ttf");
  const std::vector<std::string> names = fonts::families(context);
  ASSERT_FALSE(names.empty());
  EXPECT_EQ(names.front(), family);
  // The process's own catalogue is the manager's alone.
  EXPECT_FALSE(lists(fonts::families(), family));
  const std::optional<fonts::Family> bundled = fonts::family(family, context);
  ASSERT_TRUE(bundled);
  EXPECT_TRUE(bundled->bundled);
  ASSERT_EQ(bundled->styles.size(), 1u);
  EXPECT_EQ(bundled->styles.front().uri, "sketch://type/data/Sans.ttf");
  EXPECT_EQ(bundled->styles.front().weightName, "Normal");
  EXPECT_EQ(bundled->styles.front().name, "Regular");
  EXPECT_EQ(fonts::find(family, FaceStyle{}, context), face);
}

TEST(BundledCatalogue, AnAdoptedFaceIsFoundByItsOwnStyleName) {
  FontContext context(ports::systemFontManager());
  const Face face = ports::faceFromBytes(test::instrumentBytes("Sans.ttf"));
  ASSERT_TRUE(face);
  const std::string family = face.familyName();
  EXPECT_FALSE(fonts::find(family, "Regular", context));
  context.adopt(face, "sketch://type/data/Sans.ttf");
  EXPECT_EQ(fonts::find(family, "Regular", context), face);
  EXPECT_EQ(fonts::find(family, " regular ", context), face);
  // An adopted family answers alone: a style it lacks is no face, never
  // a face of the manager's.
  EXPECT_FALSE(fonts::find(family, "Bold", context));
  // The process's own catalogue is the manager's alone.
  EXPECT_FALSE(fonts::find(family, "Regular"));
  const std::optional<fonts::Family> bundled = fonts::family(family, context);
  ASSERT_TRUE(bundled);
  const fonts::Style* regular = bundled->style("REGULAR");
  ASSERT_NE(regular, nullptr);
  EXPECT_EQ(regular->uri, "sketch://type/data/Sans.ttf");
}

TEST(BundledCatalogue, AVariableFamilyListsItsAxes) {
  FontContext context(ports::systemFontManager());
  const Face face = ports::faceFromBytes(test::instrumentBytes("Variable.ttf"));
  ASSERT_TRUE(face);
  context.adopt(face);
  const std::optional<fonts::Family> family =
      fonts::family(face.familyName(), context);
  ASSERT_TRUE(family);
  EXPECT_TRUE(std::ranges::any_of(
      family->axes, [](const VariationAxis& axis) { return axis.is("wght"); }));
}

}  // namespace
}  // namespace sigil::weave
