/** @file
 * The silhouette shelf: every generator answers a path inscribed in the
 * box it is given, equal values generate equal paths (the contract a
 * caching consumer prunes on), and the corner wrapper composes over any
 * of them.
 */

#include <gtest/gtest.h>
#include <sigilgeometry/kit/Silhouettes.h>

using namespace sigil::geometry::shapes;

namespace {
constexpr SkSize kBox{200, 120};
}

TEST(Silhouettes, EveryGeneratorStaysInsideTheBoxItIsGiven) {
  const SkRect box = SkRect::MakeWH(kBox.width(), kBox.height());
  const auto within = [&](const SkPath& p) {
    const SkRect b = p.getBounds();
    return b.left() >= box.left() - 0.5f && b.top() >= box.top() - 0.5f &&
           b.right() <= box.right() + 0.5f && b.bottom() <= box.bottom() + 0.5f;
  };
  EXPECT_TRUE(within(polygon(6)(kBox)));
  EXPECT_TRUE(within(star(5)(kBox)));
  EXPECT_TRUE(within(circle()(kBox)));
  EXPECT_TRUE(within(annulus()(kBox)));
  EXPECT_TRUE(within(squircle()(kBox)));
  EXPECT_TRUE(within(blob(7)(kBox)));
  EXPECT_TRUE(within(sector(0, 90)(kBox)));
  EXPECT_TRUE(within(parallelogram(12)(kBox)));
  EXPECT_TRUE(within(arrow()(kBox)));
  EXPECT_TRUE(within(chamfered(8)(kBox)));
  EXPECT_TRUE(within(notched(10, 6)(kBox)));
}

TEST(Silhouettes, EqualValuesGenerateEqualPaths) {
  // The contract a caching consumer prunes on: two separately built
  // values that compare equal must draw the same path at every size.
  EXPECT_EQ(star(5, 0.4f), star(5, 0.4f));
  EXPECT_NE(star(5, 0.4f), star(5, 0.5f));
  EXPECT_EQ(star(5, 0.4f)(kBox), star(5, 0.4f)(kBox));
  EXPECT_EQ(blob(3)(kBox), blob(3)(kBox));  // seeded, so it is reproducible
  EXPECT_NE(blob(3)(kBox), blob(4)(kBox));
}

TEST(Silhouettes, CircleInsetStandsConcentricallyInsideTheBox) {
  const SkRect inscribed = circle()(kBox).getBounds();
  const SkRect drawn = circle(12.0f)(kBox).getBounds();
  EXPECT_FLOAT_EQ(drawn.left(), inscribed.left() + 12.0f);
  EXPECT_FLOAT_EQ(drawn.right(), inscribed.right() - 12.0f);
  EXPECT_EQ(circle()(kBox), circle(0.0f)(kBox));
}

TEST(Silhouettes, TheCornerWrapperComposesOverAnyGeneratorAndKeepsComparing) {
  const auto a = rounded(star(5), 6.0f);
  EXPECT_EQ(a, rounded(star(5), 6.0f));
  EXPECT_NE(a, rounded(star(5), 7.0f));
  // Rounding a star cannot be said with a box-corner radius, which is
  // why the wrapper exists: the result is a different path.
  EXPECT_NE(a(kBox), star(5)(kBox));
  EXPECT_EQ(rounded(star(5), 0.0f)(kBox), star(5)(kBox));  // no radius, no-op
}

TEST(Silhouettes, AKeyedParametricComparesByItsKeyAndAnUnkeyedOneNever) {
  const auto f = [](float t) { return SkPoint{std::cos(t), std::sin(t)}; };
  EXPECT_EQ(parametric("ring", f, 0.0f, 6.28f), parametric("ring", f, 0.0f, 6.28f));
  EXPECT_NE(parametric("ring", f, 0.0f, 6.28f), parametric("arc", f, 0.0f, 6.28f));
  EXPECT_FALSE(parametric(f, 0.0f, 6.28f)(kBox).isEmpty());
}

TEST(Silhouettes, TheCurveFamiliesSampleIntoNonEmptyOpenPaths) {
  EXPECT_FALSE(lissajous(3, 2)(kBox).isEmpty());
  EXPECT_FALSE(harmonograph(3, 2)(kBox).isEmpty());
  EXPECT_FALSE(rose(5)(kBox).isEmpty());
  EXPECT_FALSE(spiral(4)(kBox).isEmpty());
  EXPECT_FALSE(trochoid(5, 3, 2)(kBox).isEmpty());
}
