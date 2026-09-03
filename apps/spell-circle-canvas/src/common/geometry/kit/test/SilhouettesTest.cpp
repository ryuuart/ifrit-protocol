/** @file
 * The 2D shelves: every silhouette generator answers a path inscribed in
 * the box it is given, equal values generate equal paths (the contract a
 * caching consumer prunes on), and the corner wrapper composes over any
 * of them; every shaper answers the deviation seam and actually moves the
 * mark; and a division ladder is ONE multi-contour path at its frame's
 * convention.
 */

#include <gtest/gtest.h>
#include <include/core/SkPathBuilder.h>
#include <src/core/SkPathPriv.h>
#include <sigilgeometry/kit/Divisions.h>
#include <sigilgeometry/kit/Shapers.h>
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

TEST(Silhouettes, ACornerTreatmentOfZeroIsASquareCorner) {
  // A cut of zero is not a cut of no length: the two vertices it would
  // emit stand on top of each other, and every treatment that reads the
  // vertices afterwards sees a degenerate segment there. Rounding is the
  // one that shows it — it rounds the corners it can find, and a
  // duplicated corner is not one of them.
  const auto vertices = [](const SkPath& p) {
    int points = 0;
    for (auto [verb, pts, w] : SkPathPriv::Iterate(p))
      if (verb == SkPathVerb::kLine) ++points;
    return points;
  };
  EXPECT_EQ(chamfered(0)(kBox), parallelogram(0)(kBox));
  EXPECT_EQ(vertices(chamfered(0)(kBox)), 3);  // …and the close is the 4th
  EXPECT_EQ(vertices(notched(0, 6)(kBox)), 3);
  EXPECT_EQ(vertices(notched(10, 0)(kBox)), 3);
  // Rounded, the square-cornered box rounds all four corners rather than
  // the one seam a duplicated vertex leaves roundable.
  EXPECT_EQ(rounded(chamfered(0), 10)(kBox), rounded(parallelogram(0), 10)(kBox));
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
  static_assert(Silhouette<decltype(a)>);
  // A capture-free closure is an empty class, so a compiler-written
  // equality over one is vacuously true — it would claim two different
  // drawings are the same. Wrapping a callable must therefore give
  // something that compares to nothing, exactly as the callable did.
  const auto raw = [](SkSize s) { return circle()(s); };
  static_assert(!Silhouette<decltype(rounded(raw, 6.0f))>);
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

// ---------------------------------------------------------------------------
// The shaper shelf: the stock values over the deviation seam.

TEST(Shapers, EveryStockShaperAnswersTheSeamAndComparesByItsDials) {
  using namespace sigil::geometry;
  static_assert(path::ShaperScheme<shapers::Wave>);
  static_assert(path::ShaperScheme<shapers::Zigzag>);
  static_assert(path::ShaperScheme<shapers::Jitter>);
  static_assert(path::ShaperScheme<shapers::Offset>);
  static_assert(path::ShaperScheme<shapers::Rounded>);
  static_assert(path::ShaperScheme<shapers::Chamfer>);
  static_assert(path::ShaperScheme<shapers::Square>);
  EXPECT_TRUE(shapers::wave(6, 40) == shapers::wave(6, 40));
  EXPECT_FALSE(shapers::wave(6, 40) == shapers::wave(6, 41));
  // Bleed is how far the deviation reaches, so a cull can grow by it.
  EXPECT_FLOAT_EQ(shapers::wave(6, 40).bleed(), 6.0f);
  EXPECT_FLOAT_EQ(shapers::zigzag(4, 24).bleed(), 4.0f);
  EXPECT_FLOAT_EQ(shapers::offset(-9).bleed(), 9.0f);
}

TEST(Shapers, EachOneActuallyMovesTheMarkItIsGiven) {
  SkPathBuilder b;
  b.moveTo(0, 50);
  b.lineTo(200, 50);
  const SkPath run = b.detach();
  using namespace sigil::geometry;
  // A wave and a zigzag swing the run off its own axis.
  EXPECT_GT(shapers::wave(8, 40).shape(run).getBounds().height(), 8.0f);
  EXPECT_GT(shapers::zigzag(8, 40).shape(run).getBounds().height(), 8.0f);
  EXPECT_GT(shapers::square(8, 40).shape(run).getBounds().height(), 8.0f);
  // An offset moves it bodily, LEFT of travel, which on a west-to-east
  // run is upward on screen.
  EXPECT_NEAR(shapers::offset(10).shape(run).getBounds().centerY(), 40.0f, 1.5f);
  // A corner treatment over a straight run has no corner to treat.
  EXPECT_EQ(shapers::rounded(6).shape(run).getBounds(), run.getBounds());
}

TEST(Shapers, TheOscillatingWidthLawIsZeroMeanAndPlugsTheProfileSeam) {
  using namespace sigil::geometry;
  const path::Profile w = path::profile::wave(9, 50);
  EXPECT_NEAR(w.max(), 9.0f, 1e-4f) << "max() is what a cull is sized from";
  EXPECT_TRUE(w == path::profile::wave(9, 50));
  EXPECT_FALSE(w == path::profile::wave(9, 51));
  // Zero-mean: it goes both ways, which is what makes it a centreline and
  // not a band width.
  bool positive = false, negative = false;
  for (int k = 0; k <= 64; ++k) {
    const float v = w.across((float)k / 64.0f);
    positive = positive || v > 0.5f;
    negative = negative || v < -0.5f;
  }
  EXPECT_TRUE(positive && negative);
}

// ---------------------------------------------------------------------------
// The division shelf: a figure's ladders as ONE path.

TEST(Divisions, ATickLadderIsOneMultiContourPathAtTheFramesConvention) {
  using namespace sigil::geometry;
  const path::Frame fig{.centre = {100, 100}, .radius = 100,
                        .zero = path::Zero::North, .sense = path::Sense::CW};
  const SkPath twelve = ticks(fig, {.divisions = 12});
  EXPECT_FALSE(twelve.isEmpty());
  // One contour per mark, and every mark inside the frame's own box.
  EXPECT_TRUE(fig.box().contains(twelve.getBounds()));
  EXPECT_LT(ticks(fig, {.divisions = 6}).countPoints(), twelve.countPoints());
  // The shape form is a comparable silhouette, so a node carrying one
  // prunes like any other.
  EXPECT_TRUE(ticks(Ticks{.divisions = 12}) == ticks(Ticks{.divisions = 12}));
  EXPECT_FALSE(ticks(Ticks{.divisions = 12}) == ticks(Ticks{.divisions = 13}));
  EXPECT_FALSE(ticks(Ticks{.divisions = 12})({200, 200}).isEmpty());
}

TEST(Divisions, AChordFanIsOnePathAndComparesByItsParameters) {
  using namespace sigil::geometry;
  const path::Frame fig{.centre = {100, 100}, .radius = 100};
  EXPECT_FALSE(chords(fig, {.sides = 7}).isEmpty());
  EXPECT_TRUE(chords(Chords{.sides = 7}) == chords(Chords{.sides = 7}));
  EXPECT_FALSE(chords(Chords{.sides = 7}) == chords(Chords{.sides = 8}));
}
