/** @file
 * The 2D silhouette shelf: every generator answers a path inscribed in the
 * box it is given, equal values generate equal paths — the contract a
 * caching consumer prunes on — and the corner wrapper composes over any of
 * them without losing that comparison.
 */

#include <gtest/gtest.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <sigilgeometry/kit/Silhouettes.h>

#include <algorithm>
#include <cmath>
#include <string>

using namespace sigil::geometry::shapes;

namespace {

constexpr SkSize kBox{200, 120};

/** How many line segments an outline was AUTHORED with, read through
 *  Skia's public iterator. The line the iterator synthesizes to close a
 *  contour is not one of them. A treatment that emits a zero-length cut
 *  leaves two vertices on top of each other, which shows up here as an
 *  extra segment. */
int lineSegments(const SkPath& p) {
  int lines = 0;
  SkPath::Iter iter(p, false);
  SkPoint pts[4];
  for (SkPath::Verb verb = iter.next(pts); verb != SkPath::kDone_Verb;
       verb = iter.next(pts))
    lines += verb == SkPath::kLine_Verb && !iter.isCloseLine();
  return lines;
}

// ---------------------------------------------------------------------------
// Every generator, held to the one promise they all make: whatever the
// value, the path it draws is inscribed in the box it was handed. A
// generator that sized itself from a radius, or centred on the origin
// rather than on the box, escapes it.

struct Generator {
  const char* name;
  OutlineFn make;
};

class SilhouetteGenerator : public ::testing::TestWithParam<Generator> {};

TEST_P(SilhouetteGenerator, StaysInsideTheBoxItIsGiven) {
  const SkRect box = SkRect::MakeWH(kBox.width(), kBox.height());
  const SkPath p = GetParam().make(kBox);
  ASSERT_FALSE(p.isEmpty());
  const SkRect b = p.getBounds();
  EXPECT_GE(b.left(), box.left() - 0.5f);
  EXPECT_GE(b.top(), box.top() - 0.5f);
  EXPECT_LE(b.right(), box.right() + 0.5f);
  EXPECT_LE(b.bottom(), box.bottom() + 0.5f);
}

INSTANTIATE_TEST_SUITE_P(
    Silhouettes, SilhouetteGenerator,
    ::testing::Values(Generator{"Polygon", polygon(6)},
                      Generator{"Star", star(5)}, Generator{"Circle", circle()},
                      Generator{"Annulus", annulus()},
                      Generator{"Squircle", squircle()},
                      Generator{"Blob", blob(7)},
                      Generator{"Sector", sector(0, 90)},
                      Generator{"Parallelogram", parallelogram(12)},
                      Generator{"Arrow", arrow()},
                      Generator{"Chamfered", chamfered(8)},
                      Generator{"Notched", notched(10, 6)}),
    [](const ::testing::TestParamInfo<Generator>& info) {
      return std::string(info.param.name);
    });

// ---------------------------------------------------------------------------
// The curve families sample a closed form into an open path. Each one's
// figure is its own; what the shelf promises of all five is that they
// sample into a path with ink in it, inscribed in the box they were handed.

class CurveFamily : public ::testing::TestWithParam<Generator> {};

TEST_P(CurveFamily, SamplesIntoAPathInscribedInItsBox) {
  const SkPath p = GetParam().make(kBox);
  ASSERT_FALSE(p.isEmpty());
  const SkRect b = p.getBounds();
  EXPECT_GE(b.left(), -1.0f);
  EXPECT_GE(b.top(), -1.0f);
  EXPECT_LE(b.right(), kBox.width() + 1.0f);
  EXPECT_LE(b.bottom(), kBox.height() + 1.0f);
}

INSTANTIATE_TEST_SUITE_P(
    Silhouettes, CurveFamily,
    ::testing::Values(Generator{"Lissajous", lissajous(3, 2)},
                      Generator{"Harmonograph", harmonograph(3, 2)},
                      Generator{"Rose", rose(5)}, Generator{"Spiral", spiral(4)},
                      Generator{"Trochoid", trochoid(5, 3, 2)}),
    [](const ::testing::TestParamInfo<Generator>& info) {
      return std::string(info.param.name);
    });

// ---------------------------------------------------------------------------

TEST(Silhouettes, ACornerTreatmentOfZeroIsASquareCorner) {
  // A cut of zero is not a cut of no length: the two vertices it would
  // emit stand on top of each other, and every treatment that reads the
  // vertices afterwards sees a degenerate segment there. Rounding is the
  // one that shows it — it rounds the corners it can find, and a
  // duplicated corner is not one of them.
  EXPECT_EQ(chamfered(0)(kBox), parallelogram(0)(kBox));
  EXPECT_EQ(lineSegments(chamfered(0)(kBox)), 3);  // …and the close is the 4th
  EXPECT_EQ(lineSegments(notched(0, 6)(kBox)), 3);
  EXPECT_EQ(lineSegments(notched(10, 0)(kBox)), 3);
  // Rounded, the square-cornered box rounds all four corners rather than
  // the one seam a duplicated vertex leaves roundable.
  EXPECT_EQ(rounded(chamfered(0), 10)(kBox),
            rounded(parallelogram(0), 10)(kBox));
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

TEST(Silhouettes, ABlobIsSeededChaosThatStaysInsideItsBox) {
  // A blob's lobes are placed by the seeded mixer one library down, which
  // is what makes the same blob the same blob in a stored render and in a
  // resource key — chaos a caching consumer can key on. Its outline is not
  // a circle, the lobes reach in and out, and it still covers the centre
  // and escapes nothing.
  const SkSize box{120, 120};
  const SkPath organic = blob(7, 0.3f, 9)(box);
  ASSERT_FALSE(organic.isEmpty());
  EXPECT_EQ(organic, blob(7, 0.3f, 9)(box));
  EXPECT_NE(organic, blob(8, 0.3f, 9)(box));
  EXPECT_NE(organic, circle()(box));
  EXPECT_TRUE(organic.contains(60, 60)) << "the centre is always covered";
}

TEST(Silhouettes, StarArmsCanBeWaisted) {
  // Engraved stars are almost never straight-chorded: the arms narrow fast
  // off the hub and then run as needles, which is a waist rather than a
  // chord. The waist lives in the QUAD CONTROL POINTS, so the covered area
  // is sampled rather than read off the endpoints — a polygon area sees no
  // difference at all and would pass on a no-op.
  const SkSize box{200, 200};
  const auto covered = [](const SkPath& p) {
    int inside = 0;
    for (int y = 0; y < 128; ++y)
      for (int x = 0; x < 128; ++x)
        inside += p.contains((float)x * 200.0f / 128.0f + 0.5f,
                             (float)y * 200.0f / 128.0f + 0.5f);
    return inside;
  };
  const SkPath straight = star(6, 0.35f, 0.0f)(box);
  const SkPath waisted = star(6, 0.35f, 0.22f)(box);
  const SkPath bulged = star(6, 0.35f, -0.22f)(box);
  // The tips are unmoved — the waist pinches the EDGES, not the points.
  EXPECT_NEAR(straight.getBounds().height(), waisted.getBounds().height(), 1.0f);
  // The figure loses ink, because every edge bows toward the centre…
  EXPECT_LT(covered(waisted), covered(straight));
  // …and a negative waist bulges instead, which is the compass-rose
  // direction.
  EXPECT_GT(covered(bulged), covered(straight));
}

TEST(Silhouettes, TheCurveFamiliesEvaluateInTheUnitFrame) {
  // A curve DEFINED by a parameter needs a generator of its own, or every
  // caller writes the same sampling loop inside its outline lambda. The
  // frame is the box's own half-extents, so a non-square box is where the
  // convention shows.
  const SkSize box{200, 100};

  // A 1:1 Lissajous with a quarter-turn phase IS the inscribed ellipse.
  const SkPath ellipse = lissajous(1, 1, 90.0f)(box);
  const SkRect bounds = ellipse.getBounds();
  EXPECT_NEAR(bounds.width(), 200.0f, 1.5f);
  EXPECT_NEAR(bounds.height(), 100.0f, 1.5f);
  EXPECT_NEAR(bounds.centerX(), 100.0f, 0.5f);
  EXPECT_NEAR(bounds.centerY(), 50.0f, 0.5f);

  // Damping shrinks the figure AS IT DRAWS — the whole visual difference
  // between a harmonograph and a Lissajous, and why a pen-and-pendulum
  // figure spirals inward instead of retracing one rosette. Both ends sit
  // AT the centre, so the honest measurement is the reach of each half.
  const SkPath damped = harmonograph(3, 2, 0, 0.25f, 0, 6.0f)(box);
  const SkPoint centre = SkPoint{100, 50};
  const int pts = damped.countPoints();
  ASSERT_GT(pts, 100);
  const auto reach = [&](int from, int to) {
    float most = 0;
    for (int i = from; i < to; ++i)
      most = std::max(most, SkPoint::Distance(damped.getPoint(i), centre));
    return most;
  };
  EXPECT_GT(reach(0, pts / 2), reach(pts / 2, pts) * 1.5f);

  // A rose with odd k has k petals, each reaching the rim. It is NOT
  // centred on the box — r = cos(5θ) puts tips at θ = 0, 2π/5, … — so the
  // bounds sit off to one side, and asserting otherwise would be asserting
  // a defect into existence.
  const SkPath five = rose(5)(box);
  EXPECT_GT(five.countPoints(), 100);
  int tips = 0;
  for (int i = 0; i < five.countPoints(); ++i)
    if (SkPoint::Distance(five.getPoint(i), centre) > 49.0f) ++tips;
  EXPECT_GT(tips, 5);

  // Spirals start at the centre and end at the rim.
  const SkPath coil = spiral(3)(box);
  EXPECT_NEAR(SkPoint::Distance(coil.getPoint(0), centre), 0.0f, 1.0f);
  EXPECT_GT(SkPoint::Distance(coil.getPoint(coil.countPoints() - 1), centre),
            40.0f);
}

TEST(Silhouettes, CircleInsetStandsConcentricallyInsideTheBox) {
  const SkSize size{200, 200};
  const SkRect inscribed = circle()(size).getBounds();
  const SkRect drawn = circle(24.0f)(size).getBounds();
  EXPECT_FLOAT_EQ(drawn.left(), inscribed.left() + 24.0f);
  EXPECT_FLOAT_EQ(drawn.top(), inscribed.top() + 24.0f);
  EXPECT_FLOAT_EQ(drawn.right(), inscribed.right() - 24.0f);
  EXPECT_FLOAT_EQ(drawn.bottom(), inscribed.bottom() - 24.0f);
  // Zero inset IS the inscribed circle, and the value form compares by its
  // parameters — the prune contract every generator keeps.
  EXPECT_EQ(circle()(size), circle(0.0f)(size));
  EXPECT_TRUE(circle() == circle(0.0f));
  EXPECT_FALSE(circle() == circle(24.0f));
  // The oriented overload carries the same trailing inset.
  EXPECT_EQ(circle(SkPathDirection::kCCW, 1, 24.0f)(size).getBounds(), drawn);
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
  EXPECT_EQ(parametric("ring", f, 0.0f, 6.28f),
            parametric("ring", f, 0.0f, 6.28f));
  EXPECT_NE(parametric("ring", f, 0.0f, 6.28f),
            parametric("arc", f, 0.0f, 6.28f));
  EXPECT_FALSE(parametric(f, 0.0f, 6.28f)(kBox).isEmpty());
}

}  // namespace
