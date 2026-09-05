/** @file
 * The dial door: any operator's numeric field by its own name, vector
 * components dotted, enums and bools as numbers — and a name the operator
 * lacks refused rather than silently accepted.
 */

#include <gtest/gtest.h>

#include <optional>

#include "sigilgeometry/mesh/pop/Pop.h"

using namespace sigil::geometry;
using namespace sigil::geometry::mesh;


TEST(Pop, FieldsAreAddressableByName) {
  // The dial door: any operator's numeric field by its own name, vector
  // components dotted, enums and bools as numbers; a name the operator
  // lacks is refused and leaves it untouched.
  pop::Op twist = pop::Deform{};
  EXPECT_TRUE(pop::setField(twist, "amount", 45.0f));
  EXPECT_TRUE(pop::setField(twist, "origin.x", 12.0f));
  EXPECT_TRUE(pop::setField(twist, "kind", (float)pop::Deform::Kind::Bend));
  EXPECT_FALSE(pop::setField(twist, "wibble", 1.0f));
  const auto& d = std::get<pop::Deform>(twist);
  EXPECT_FLOAT_EQ(d.amount, 45.0f);
  EXPECT_FLOAT_EQ(d.origin.x, 12.0f);
  EXPECT_EQ(d.kind, pop::Deform::Kind::Bend);
  const std::optional<float> amount = pop::getField(twist, "amount");
  ASSERT_TRUE(amount.has_value());
  EXPECT_FLOAT_EQ(*amount, 45.0f);
  const std::optional<float> kind = pop::getField(twist, "kind");
  ASSERT_TRUE(kind.has_value());
  EXPECT_FLOAT_EQ(*kind, 2.0f);
  EXPECT_FALSE(pop::getField(twist, "mask"));  // a string, not a dial

  pop::Op group = pop::Select{};
  EXPECT_TRUE(pop::setField(group, "center.y", 80.0f));
  EXPECT_TRUE(pop::setField(group, "invert", 1.0f));
  EXPECT_TRUE(
      pop::setField(group, "combine", (float)pop::Select::Combine::Union));
  const auto& g = std::get<pop::Select>(group);
  EXPECT_FLOAT_EQ(g.center.y, 80.0f);
  EXPECT_TRUE(g.invert);
  EXPECT_EQ(g.combine, pop::Select::Combine::Union);

  pop::Op ramp = pop::Ramp{};
  EXPECT_TRUE(pop::setField(ramp, "to.g", 0.25f));  // colour spelling
  EXPECT_FLOAT_EQ(std::get<pop::Ramp>(ramp).to.y, 0.25f);
  const std::optional<float> toY = pop::getField(ramp, "to.y");
  ASSERT_TRUE(toY.has_value());
  EXPECT_FLOAT_EQ(*toY, 0.25f);

  pop::Op scatter = pop::SplineScatter{};
  EXPECT_TRUE(pop::setField(scatter, "count", 250.7f));  // int truncates
  EXPECT_EQ(std::get<pop::SplineScatter>(scatter).count, 250);
  EXPECT_TRUE(pop::setField(scatter, "seed", 9.0f));
  EXPECT_EQ(std::get<pop::SplineScatter>(scatter).seed, 9u);

  // Operators without dials say no to everything.
  pop::Op promote = pop::Promote{};
  EXPECT_FALSE(pop::setField(promote, "to", 1.0f));
  pop::Op given = pop::PointSet{};
  EXPECT_FALSE(pop::getField(given, "count"));
}
