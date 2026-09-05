/** @file
 * The division shelf: a figure's tick ladder and chord fan as ONE path of
 * many contours, laid out at the frame's own convention — which is what
 * lets a run of type walk every side of a polygon as one arc-length
 * coordinate — and the agreement between a frame's arc-length fraction and
 * the contour the silhouette shelf actually builds.
 */

#include <gtest/gtest.h>
#include <include/core/SkContourMeasure.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <sigilgeometry/kit/Divisions.h>
#include <sigilgeometry/kit/Silhouettes.h>

#include <cmath>
#include <vector>

using namespace sigil::geometry;

namespace {

::testing::AssertionResult near(SkPoint a, SkPoint b, float tol) {
  const float d = std::hypot(a.fX - b.fX, a.fY - b.fY);
  if (d <= tol) return ::testing::AssertionSuccess();
  return ::testing::AssertionFailure()
         << "(" << a.fX << ", " << a.fY << ") vs (" << b.fX << ", " << b.fY
         << ") — " << d << " px apart, tolerance " << tol;
}

/** Every contour of @p path, split out, with each one's length. */
struct Contours {
  std::vector<SkPath> pieces;
  std::vector<float> lengths;
  float total = 0;
};

Contours walk(const SkPath& path) {
  Contours out;
  SkContourMeasureIter it(path, false);
  while (sk_sp<SkContourMeasure> c = it.next()) {
    SkPathBuilder piece;
    (void)c->getSegment(0, c->length(), &piece, true);
    out.pieces.push_back(piece.detach());
    out.lengths.push_back(c->length());
    out.total += c->length();
  }
  return out;
}

/** The point at arc-length fraction @p f of the WHOLE path, walking every
 *  contour in order — the coordinate a run of marks laid along a path
 *  travels, so a multi-contour path is one continuous run to it. */
SkPoint atFraction(const SkPath& path, float f) {
  float want = f * walk(path).total;
  SkContourMeasureIter it(path, false);
  while (sk_sp<SkContourMeasure> m = it.next()) {
    if (want <= m->length()) {
      SkPoint p{0, 0};
      (void)m->getPosTan(want, &p, nullptr);
      return p;
    }
    want -= m->length();
  }
  return {0, 0};
}

// ---------------------------------------------------------------------------
// The frame against the contours the shelf above it builds. A frame's
// arc-length conversions are only right if they know where a generated
// contour starts and which way it runs, so they are asserted against the
// paths the generators actually make rather than against the arithmetic
// the conversion is written with.

TEST(FrameOnAShape, FractionAgreesWithTheCircleContourTheShelfBuilds) {
  // circle()'s contour starts due EAST, not due north, so the conversion
  // carries a −90° term. A change to where circle() starts fails here
  // rather than silently rotating every label on a ring.
  const SkSize size{200, 200};
  const SkPath circle = shapes::circle()(size);
  const path::Frame f{.centre = {100, 100}, .radius = 100};
  for (float th : {0.0f, 45.0f, 90.0f, 137.0f, 180.0f, 300.0f})
    EXPECT_TRUE(near(atFraction(circle, f.fraction(th)), f.at(th, 1.0f), 0.25f))
        << "th=" << th;
}

TEST(FrameOnAShape, TheBaselinesDirectionIsNotTheFramesSense) {
  // A frame's `sense` and the winding of the path it labels are
  // independent facts, which is why fraction() takes the path direction as
  // a separate argument rather than reading it off the frame. A CCW circle
  // still STARTS due east — only the travel direction flips.
  const SkSize size{200, 200};
  const SkPath cw = shapes::circle(SkPathDirection::kCW)(size);
  const SkPath ccw = shapes::circle(SkPathDirection::kCCW)(size);
  EXPECT_TRUE(near(atFraction(cw, 0.0f), atFraction(ccw, 0.0f), 0.25f))
      << "both contours start due east";
  EXPECT_FALSE(near(atFraction(cw, 0.25f), atFraction(ccw, 0.25f), 1.0f))
      << "…and then run opposite ways";

  // Every combination of frame sense and baseline direction must land on
  // the same point the frame names.
  for (path::Sense sense : {path::Sense::CW, path::Sense::CCW}) {
    const path::Frame f{.centre = {100, 100},
                        .radius = 100,
                        .zero = path::Zero::North,
                        .sense = sense};
    for (auto dir : {SkPathDirection::kCW, SkPathDirection::kCCW}) {
      const SkPath& path = dir == SkPathDirection::kCW ? cw : ccw;
      for (float th : {0.0f, 60.0f, 210.0f})
        EXPECT_TRUE(
            near(atFraction(path, f.fraction(th, dir)), f.at(th, 1.0f), 0.25f))
            << "sense=" << (sense == path::Sense::CW ? "CW" : "CCW")
            << " baseline=" << (dir == SkPathDirection::kCW ? "CW" : "CCW")
            << " th=" << th;
      for (float frac : {0.1f, 0.6f})
        EXPECT_NEAR(
            std::fmod(f.fraction(f.degOf(frac, dir), dir) - frac + 2.0f, 1.0f),
            0.0f, 1e-3f);
    }
  }
}

TEST(FrameOnAShape, TheBoxIsWhereACircleInscribesItselfOnTheFrame) {
  // The circle inscribed in `box(k)` passes through `at(θ, k)`, which is
  // what makes a rect from the frame and a silhouette in that rect name
  // the same geometry.
  const path::Frame f{.centre = {50, 60}, .radius = 20};
  const SkRect b = f.box(0.5f);
  const SkPath c = shapes::circle()(SkSize{b.width(), b.height()});
  SkPoint p = atFraction(c, f.fraction(0.0f));
  p.offset(b.fLeft, b.fTop);
  EXPECT_TRUE(near(p, f.at(0.0f, 0.5f), 0.2f));
}

// ---------------------------------------------------------------------------
// ticks — a division ladder as ONE path with N contours.

TEST(Divisions, ATickLadderEmitsOneContourPerDivisionOnTheFrame) {
  const path::Frame f{.centre = {0, 0}, .radius = 100};
  const SkPath p = shapes::ticks(f, {.divisions = 12, .mark = {0.9f, 1.0f}});
  const Contours c = walk(p);
  EXPECT_EQ(c.pieces.size(), 12u);
  for (float len : c.lengths) EXPECT_NEAR(len, 10.0f, 1e-2f);
  // Mark 0 runs from at(0, .9) to at(0, 1).
  SkPoint start{0, 0};
  ASSERT_TRUE(c.pieces[0].getLastPt(&start));
  EXPECT_TRUE(near(start, f.at(0, 1.0f), 1e-3f));
  // Every mark stands inside the frame's own box, and fewer divisions is
  // less path.
  const path::Frame boxed{.centre = {100, 100}, .radius = 100};
  const SkPath twelve = shapes::ticks(boxed, {.divisions = 12});
  EXPECT_TRUE(boxed.box().contains(twelve.getBounds()));
  EXPECT_LT(shapes::ticks(boxed, {.divisions = 6}).countPoints(),
            twelve.countPoints());
}

TEST(Divisions, LongEveryLengthensEveryNthMark) {
  const path::Frame f{.centre = {0, 0}, .radius = 100};
  const SkPath p = shapes::ticks(f, {.divisions = 72,
                                     .mark = {0.96f, 1.0f},
                                     .longEvery = 6,
                                     .longMark = {0.91f, 1.0f}});
  const Contours c = walk(p);
  ASSERT_EQ(c.pieces.size(), 72u);
  int longs = 0;
  for (size_t i = 0; i < c.lengths.size(); ++i) {
    const bool isLong = c.lengths[i] > 6.5f;
    EXPECT_EQ(isLong, i % 6 == 0) << "mark " << i;
    longs += isLong;
  }
  EXPECT_EQ(longs, 12);
}

TEST(Divisions, ClassifyReachesLengthClassesTheLongShortPairCannot) {
  // A three-way length pattern cannot be expressed by the long/short pair,
  // which is the whole reason `classify` exists: it hands each mark's
  // index to the caller and takes back that mark's span.
  const path::Frame f{.centre = {0, 0}, .radius = 100};
  const SkPath p = shapes::ticks(
      f, {.divisions = 9,
          .mark = {0.5f, 1.0f},
          .classify = [](int i, shapes::Span s) {
            s.outer = (i % 3 == 0) ? 1.0f : (i % 3 == 1 ? 0.86f : 0.93f);
            return s;
          }});
  const Contours c = walk(p);
  ASSERT_EQ(c.lengths.size(), 9u);
  for (size_t i = 0; i < 9; ++i) {
    const float want =
        ((i % 3 == 0) ? 1.0f : (i % 3 == 1 ? 0.86f : 0.93f)) - 0.5f;
    EXPECT_NEAR(c.lengths[i], want * 100.0f, 1e-2f) << "mark " << i;
  }
}

TEST(Divisions, ClosedAddsTheEndMarkAndSweepScopesTheLadder) {
  const path::Frame f{.centre = {0, 0}, .radius = 100};
  const shapes::Ticks quarter{
      .divisions = 9, .from = 0, .sweep = 90, .closed = true};
  const Contours c = walk(shapes::ticks(f, quarter));
  EXPECT_EQ(c.pieces.size(), 10u);  // 9 divisions, 10 rules
  SkPoint last{0, 0};
  ASSERT_TRUE(c.pieces.back().getLastPt(&last));
  EXPECT_TRUE(near(last, f.at(90.0f, 1.0f), 1e-3f));

  const shapes::Ticks open{.divisions = 9, .from = 0, .sweep = 90};
  EXPECT_EQ(walk(shapes::ticks(f, open)).pieces.size(), 9u);
}

TEST(Divisions, TheOutlineFormTakesHalfTheShorterSideOfANonSquareBox) {
  // A non-square box must still produce a CIRCULAR ladder, or the frame's
  // fraction stops matching and every label on it slides.
  const shapes::OutlineFn fn = shapes::ticks({.divisions = 4, .mark = {0, 1}});
  const Contours c = walk(fn(SkSize{400, 100}));
  ASSERT_EQ(c.pieces.size(), 4u);
  for (float len : c.lengths) EXPECT_NEAR(len, 50.0f, 1e-2f);
}

TEST(Divisions, ZeroLengthMarksAreSkippedRatherThanEmittedEmpty) {
  const path::Frame f{.centre = {0, 0}, .radius = 10};
  EXPECT_TRUE(shapes::ticks(f, {.divisions = 6, .mark = {1.0f, 1.0f}}).isEmpty());
}

TEST(Divisions, ATickLadderIsAComparableSilhouette) {
  // The shape form is a comparable silhouette, so a consumer carrying one
  // prunes like any other.
  EXPECT_TRUE(shapes::ticks(shapes::Ticks{.divisions = 12}) ==
              shapes::ticks(shapes::Ticks{.divisions = 12}));
  EXPECT_FALSE(shapes::ticks(shapes::Ticks{.divisions = 12}) ==
               shapes::ticks(shapes::Ticks{.divisions = 13}));
  EXPECT_FALSE(shapes::ticks(shapes::Ticks{.divisions = 12})({200, 200})
                   .isEmpty());
}

// ---------------------------------------------------------------------------
// chords — a polygon's sides as N OPEN contours of one path.

TEST(Divisions, SideKsMidpointIsAtExactlyKPlusHalfOverN) {
  // The whole reason chords() exists: a run of marks walks every contour
  // in order as ONE arc-length coordinate, so a run laid around a heptagon
  // is a single run with each side occupying a known 1/n of it. Measured
  // against the path chords() built, not against the formula it used.
  const path::Frame f{.centre = {0, 0}, .radius = 100};
  for (int n : {5, 7, 12}) {
    const SkPath p = shapes::chords(f, {.sides = n, .radius = 1.0f});
    const Contours c = walk(p);
    ASSERT_EQ((int)c.pieces.size(), n) << "n=" << n;
    for (int k = 0; k < n; ++k) {
      const float pitch = 360.0f / (float)n;
      const SkPoint a = f.at(pitch * (float)k, 1.0f);
      const SkPoint b = f.at(pitch * (float)(k + 1), 1.0f);
      const SkPoint mid{(a.fX + b.fX) * 0.5f, (a.fY + b.fY) * 0.5f};
      const float frac = ((float)k + 0.5f) / (float)n;
      EXPECT_TRUE(near(atFraction(p, frac), mid, 0.05f))
          << "n=" << n << " side " << k;
    }
  }
}

TEST(Divisions, APolygonIsOneContourAndHasNoPerSideCoordinate) {
  // The control for the case above: polygon() emits ONE closed contour, so
  // a per-side coordinate does not exist on it. If this ever starts
  // failing, chords() has become redundant.
  EXPECT_EQ(walk(shapes::polygon(7)(SkSize{200, 200})).pieces.size(), 1u);
}

TEST(Divisions, AChordInsetShortensBothEndsAndDropsDegenerateSides) {
  const path::Frame f{.centre = {0, 0}, .radius = 100};
  const Contours plain = walk(shapes::chords(f, {.sides = 7}));
  const Contours inset = walk(shapes::chords(f, {.sides = 7, .inset = 6.0f}));
  ASSERT_EQ(inset.pieces.size(), 7u);
  EXPECT_NEAR(plain.lengths[0] - inset.lengths[0], 12.0f, 1e-2f);
  // An inset wider than the side leaves nothing to draw.
  EXPECT_TRUE(shapes::chords(f, {.sides = 7, .inset = 500.0f}).isEmpty());
}

TEST(Divisions, StepMakesStarPolygonsAndTheCommonFactorDecidesTheRingCount) {
  const path::Frame f{.centre = {0, 0}, .radius = 100};
  // {7/2}: coprime, so one closed traversal of all seven vertices.
  EXPECT_EQ(
      walk(shapes::chords(f, {.sides = 7, .step = 2, .closed = true}))
          .pieces.size(),
      1u);
  // {6/2}: two in common, so the hexagram really is TWO separate
  // triangles. Emitting one contour here would be wrong geometry, not a
  // simplification.
  const Contours hex =
      walk(shapes::chords(f, {.sides = 6, .step = 2, .closed = true}));
  EXPECT_EQ(hex.pieces.size(), 2u);
  for (float len : hex.lengths)
    EXPECT_NEAR(len, 3.0f * 100.0f * std::sqrt(3.0f), 0.5f);
}

TEST(Divisions, AChordFanIsAComparableSilhouette) {
  const path::Frame f{.centre = {100, 100}, .radius = 100};
  EXPECT_FALSE(shapes::chords(f, {.sides = 7}).isEmpty());
  EXPECT_TRUE(shapes::chords(shapes::Chords{.sides = 7}) ==
              shapes::chords(shapes::Chords{.sides = 7}));
  EXPECT_FALSE(shapes::chords(shapes::Chords{.sides = 7}) ==
               shapes::chords(shapes::Chords{.sides = 8}));
}

}  // namespace

TEST(Divisions, AMarkWidthTurnsTheLadderIntoClosedNodes) {
  const path::Frame f{.centre = {0, 0}, .radius = 100};
  const shapes::Ticks node{
      .divisions = 12, .mark = {0.9f, 1.0f}, .markPx = 4.0f};
  const SkPath p = shapes::ticks(f, node);
  const Contours c = walk(p);
  ASSERT_EQ(c.pieces.size(), 12u);
  // Each mark is now a closed rectangle standing on the same radius: the
  // 10 px run twice and the 4 px width twice. The perimeter is what says
  // it closed — three sides of it would measure 24.
  for (float len : c.lengths) EXPECT_NEAR(len, 28.0f, 1e-2f);
  // A closed mark is geometry rather than a paint decision, so it fills;
  // the open ladder encloses nothing at all.
  const SkPath line = shapes::ticks(f, {.divisions = 12, .mark = {0.9f, 1.0f}});
  EXPECT_FALSE(line.contains(f.at(0, 0.95f).fX, f.at(0, 0.95f).fY));
  EXPECT_TRUE(p.contains(f.at(0, 0.95f).fX, f.at(0, 0.95f).fY));
  // …and it stands square to its own radius: the mark on the frame's zero
  // reaches its full 4 px ACROSS that radius and no further along it.
  // The default frame counts from twelve o'clock, so mark 0 stands
  // upright: 10 px of radius tall and its 4 px of width across.
  const SkRect first = c.pieces[0].computeTightBounds();
  EXPECT_NEAR(first.height(), 10.0f, 1e-2f);
  EXPECT_NEAR(first.width(), 4.0f, 1e-2f);

  // The width is part of the value, so a node ring does not prune onto a
  // line ring.
  EXPECT_FALSE(node == (shapes::Ticks{.divisions = 12, .mark = {0.9f, 1.0f}}));
}

TEST(Divisions, ArcSegmentsFollowTheRingWhereANodeStandsAcrossIt) {
  const path::Frame f{.centre = {0, 0}, .radius = 100};
  const shapes::Arcs ring{
      .divisions = 8, .mark = {0.8f, 1.0f}, .spanDeg = 30.0f};
  const SkPath p = shapes::arcs(f, ring);
  const Contours c = walk(p);
  ASSERT_EQ(c.pieces.size(), 8u);
  // A segment fattens with radius, which is the whole difference from a
  // node: its far edge is longer than its near one, and both are arcs of
  // the ring rather than chords across it.
  const float outerArc = 100.0f * 30.0f * 3.14159265f / 180.0f;
  const float innerArc = 80.0f * 30.0f * 3.14159265f / 180.0f;
  EXPECT_NEAR(c.lengths[0], outerArc + innerArc + 2.0f * 20.0f, 0.5f);
  EXPECT_TRUE(f.box().makeOutset(0.05f, 0.05f).contains(p.computeTightBounds()));

  // It is dealt round the frame the way ticks() deals marks: the first
  // segment is CENTRED on `from`, so its middle sits where a tick would.
  const SkPoint mid = f.at(0.0f, 0.9f);
  EXPECT_TRUE(p.contains(mid.fX, mid.fY));
  const SkPoint between = f.at(180.0f / 8.0f, 0.9f);  // half a pitch on
  EXPECT_FALSE(p.contains(between.fX, between.fY));

  // Nothing to enclose is nothing drawn, rather than a degenerate contour
  // that fills as nothing and strokes as a doubled arc.
  EXPECT_TRUE(shapes::arcs(f, {.divisions = 8, .mark = {0.9f, 0.9f}}).isEmpty());
  EXPECT_TRUE(shapes::arcs(f, {.divisions = 0}).isEmpty());

  // The shape form takes half the shorter side, like its two neighbours.
  const shapes::ArcsShape shape = shapes::arcs(ring);
  EXPECT_EQ(shape, shapes::arcs(ring));
  const SkRect bounds = shape.path({240, 120}).computeTightBounds();
  EXPECT_LE(bounds.width(), 120.5f);
}
