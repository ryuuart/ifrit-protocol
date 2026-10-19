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
#include <cstddef>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace sigil::weave {
namespace {

std::vector<std::byte> bytesOf(const char* fileName) {
  std::ifstream file(std::string(SIGIL_TEST_INSTRUMENT_DIR "/") + fileName,
                     std::ios::binary);
  const std::vector<char> characters{std::istreambuf_iterator<char>(file),
                                     std::istreambuf_iterator<char>()};
  std::vector<std::byte> bytes(characters.size());
  for (size_t index = 0; index < characters.size(); ++index)
    bytes[index] = std::byte(characters[index]);
  return bytes;
}

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
  const Face face = ports::faceFromBytes(bytesOf("Sans.ttf"));
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
  EXPECT_EQ(fonts::find(family, {}, context), face);
}

TEST(BundledCatalogue, AVariableFamilyListsItsAxes) {
  FontContext context(ports::systemFontManager());
  const Face face = ports::faceFromBytes(bytesOf("Variable.ttf"));
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
