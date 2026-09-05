/** @file The stock catalogue holds every feature's recipes, and warms them. */

#include <gtest/gtest.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/kit/Recipes.h>
#include <sigilmaterial/sdf/Sdf.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/stock/Stock.h>

#include <string>

namespace sigil::material {
namespace {

TEST(MaterialStock, HoldsEveryFeatureCatalogue) {
  const size_t parts = field::everyRecipe().size() + sdf::everyRecipe().size() +
                       kit::everyRecipe().size();
  const std::vector<Material> stocked = stock::everyRecipe();
  EXPECT_EQ(stocked.size(), parts);
  for (const Material& item : stocked) EXPECT_FALSE(item.recipe().name().empty());
}

TEST(MaterialStock, WarmsEveryProgramItGathered) {
  skia::install();
  const WarmupResult result = stock::warmup(Target::SkSL);
  EXPECT_GT(result.requested, 0u);
  EXPECT_EQ(result.ready, result.unique);
}

}  // namespace
}  // namespace sigil::material
