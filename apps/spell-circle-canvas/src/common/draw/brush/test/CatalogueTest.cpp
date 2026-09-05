/** @file
 * The catalogue: the stock names, lookup by view, and scaling.
 */

#include <gtest/gtest.h>
#include <sigildraw/brush/Catalogue.h>

#include <string_view>

namespace {

namespace brush = sigil::draw::brush;

TEST(Catalogue, OwnsTheFullStockNameSetAndScalesSpatialValues) {
  brush::Catalogue catalogue = brush::Catalogue::stock();
  for (const char* name :
       {"2B", "HB", "2H", "cpencil", "pen", "rotring", "spray", "marker",
        "marker2", "charcoal", "hatch_brush", "pastel", "crayon"})
    EXPECT_TRUE(catalogue.contains(name)) << name;
  EXPECT_EQ(catalogue.names().size(), 13u);
  EXPECT_EQ(catalogue.find("HB")->tip, brush::Tip::Grain);
  EXPECT_EQ(catalogue.find("marker")->tip, brush::Tip::Nib);
  EXPECT_FLOAT_EQ(catalogue.find("marker")->width, 2.0f);
  EXPECT_FLOAT_EQ(catalogue.find("marker")->spacing, 0.03f);
  EXPECT_FLOAT_EQ(catalogue.find("charcoal")->width, 0.35f);
  EXPECT_FLOAT_EQ(catalogue.find("charcoal")->spacing, 0.03f);
  const float before = catalogue.find("HB")->width;
  const float scatter = catalogue.find("HB")->scatter;
  const float spacing = catalogue.find("HB")->spacing;
  catalogue.scale(2.0f);
  EXPECT_FLOAT_EQ(catalogue.find("HB")->width, before * 2.0f);
  EXPECT_FLOAT_EQ(catalogue.find("HB")->scatter, scatter * 2.0f);
  EXPECT_FLOAT_EQ(catalogue.find("HB")->spacing, spacing * 2.0f);
}

TEST(Catalogue, AddAnswersTheToolAndRejectsAnEmptyName) {
  brush::Catalogue catalogue;
  EXPECT_EQ(catalogue.add("", brush::pencil(SkColors::kBlack)), nullptr);
  const brush::Tool* added = catalogue.add("lead", brush::pencil(SkColors::kBlack, 3.0f));
  ASSERT_NE(added, nullptr);
  EXPECT_FLOAT_EQ(added->width, 3.0f);
  const std::string_view name = "lead";
  EXPECT_EQ(catalogue.find(name), added);
  EXPECT_EQ(catalogue.find("missing"), nullptr);
}

}  // namespace
