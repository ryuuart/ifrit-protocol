// The paint binary's share of ComposeTestGrammar.cpp: the suites whose subjects
// are paint-tier values, cut from that file so each test binary links only the
// target it exercises.

#include "support/PaintTestSupport.h"

// ---- one namespace, one name per brush kind -------------------------------

TEST(ComposeR3Brush, TheFoldIsOneNamespaceAndOneNamePerKind) {
  // Every brush kind answers to exactly one name, under `brush::`, with no
  // suffix and no second namespace.
  const brush::Ribbon taught = brush::taper(10, 2, red());
  EXPECT_FLOAT_EQ(taught.widthStart, 10.0f);
  // The taught constructor is the PROFILE one.
  const brush::Ribbon profiled =
      brush::ribbon(geometry::path::profile::offset(9.0f), red());
  EXPECT_TRUE(profiled.hasProfile());
  EXPECT_FLOAT_EQ(profiled.bleed(), 9.0f);
  // The kinds are values under the taught spelling, nothing else.
  static_assert(std::is_default_constructible_v<brush::Pattern>);
  static_assert(std::is_default_constructible_v<brush::Scatter>);
  static_assert(std::is_default_constructible_v<brush::Art>);
}

// ---- 8. cornerAlign is a required argument --------------------------------

TEST(ComposeR1Corner, AlignmentCannotBeOmitted) {
  // Corner alignment has no defensible default — bisector and outgoing are
  // both right for different marks — so it is required by the type system
  // rather than defaulted and warned about. There is no way to describe
  // corner art without stating it.
  static_assert(!std::is_default_constructible_v<brush::CornerArt>);
  static_assert(!std::is_constructible_v<brush::CornerArt, Element>);
  static_assert(
      std::is_constructible_v<brush::CornerArt, Element, brush::CornerAlign>);
  // And the alignment participates in equality, so two brushes that differ
  // only in how their corners face do not prune into each other.
  const Element art = box().width(10).height(10).fill(red());
  brush::Pattern a, b;
  a.side = box().width(10).height(2).fill(red());
  b.side = a.side;
  a.corner = brush::CornerArt{art, brush::CornerAlign::Bisector};
  b.corner = brush::CornerArt{art, brush::CornerAlign::Outgoing};
  EXPECT_FALSE(a == b);
  b.corner = brush::CornerArt{art, brush::CornerAlign::Bisector};
  EXPECT_TRUE(a == b);
}
