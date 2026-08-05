// Kit-tier tests: <sigilcompose/kit/*.h>.
//
// The kit sits ON TOP of the API and changes none of it, so a kit failure
// must never be reported as a kernel failure — hence a separate binary.
//
// Every case here is written to FAIL WITHOUT THE COMPONENT. Where the
// component's claim is about agreement with the library (the arc-length
// fraction of shapes::circle(), the per-side coordinate TextPath walks),
// the test measures the LIBRARY'S OWN PATH rather than restating the
// component's arithmetic — a test that recomputes the formula it is
// checking proves only that the compiler is deterministic.
//
// One case is a compile-only spelling of every documented signature: it
// asserts nothing and exists so that a signature change breaks the build
// instead of quietly invalidating the documentation.

#include <sigilcompose/kit/Kit.h>

#include <sigilweave/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <include/core/SkContourMeasure.h>
#include <include/core/SkFont.h>
#include <include/core/SkPath.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkSurface.h>

#include <gtest/gtest.h>

#include <cmath>
#include <utility>
#include <vector>

using namespace sigil::compose;
namespace kit = sigil::compose::kit;

namespace {

sigil::weave::FontContext &fonts() {
  static auto *context =
      new sigil::weave::FontContext(sigil::weave::ports::systemFontManager());
  return *context;
}

::testing::AssertionResult near(SkPoint a, SkPoint b, float tol) {
  const float d = std::hypot(a.fX - b.fX, a.fY - b.fY);
  if (d <= tol)
    return ::testing::AssertionSuccess();
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
Contours walk(const SkPath &path, bool closed = false) {
  Contours out;
  SkContourMeasureIter it(path, closed);
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
 *  contour in order — the same coordinate TextPath lays glyphs along, so a
 *  multi-contour path is one continuous run to it. */
SkPoint atFraction(const SkPath &path, float f) {
  const Contours c = walk(path);
  float want = f * c.total;
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

} // namespace

// ===========================================================================
// Frame — the figure-local polar coordinate system.

TEST(KitFrame, NorthClockwiseMatchesTheHandRolledSpelling) {
  // The convention Frame promises by default: 0° at 12 o'clock, increasing
  // clockwise, radius normalized. P() below is that spelled by hand, which
  // is what a figure would otherwise write inline.
  const kit::Frame f{.centre = {100, 100}, .radius = 50};
  auto P = [](float thDeg, float rNorm) {
    const float a = thDeg * 0.01745329252f;
    return SkPoint{100 + rNorm * 50 * std::sin(a),
                   100 - rNorm * 50 * std::cos(a)};
  };
  for (float th : {0.0f, 37.5f, 90.0f, 180.0f, 271.25f, 359.0f})
    for (float r : {0.25f, 1.0f})
      EXPECT_TRUE(near(f.at(th, r), P(th, r), 1e-3f)) << "th=" << th;

  // The four cardinals, spelled out, because a sign flip here is the bug
  // this component exists to stop.
  EXPECT_TRUE(near(f.at(0, 1), {100, 50}, 1e-4f));   // 12 o'clock
  EXPECT_TRUE(near(f.at(90, 1), {150, 100}, 1e-4f)); // 3 o'clock
  EXPECT_TRUE(near(f.at(180, 1), {100, 150}, 1e-4f));
  EXPECT_TRUE(near(f.at(270, 1), {50, 100}, 1e-4f));
}

TEST(KitFrame, EastAndCounterClockwiseAreTheOtherConventions) {
  const kit::Frame east{
      .centre = {0, 0}, .radius = 1, .zero = kit::Zero::East};
  EXPECT_TRUE(near(east.at(0, 1), {1, 0}, 1e-5f));
  EXPECT_TRUE(near(east.at(90, 1), {0, 1}, 1e-5f)); // screen-clockwise

  const kit::Frame ccw{.centre = {0, 0},
                       .radius = 1,
                       .zero = kit::Zero::East,
                       .sense = kit::Sense::CCW};
  EXPECT_TRUE(near(ccw.at(90, 1), {0, -1}, 1e-5f));
}

TEST(KitFrame, FractionAgreesWithTheLibrarysOwnCircleContour) {
  // Frame::fraction() converts an angle in the frame's convention into the
  // arc-length fraction TextPath wants, and it can only be right if it knows
  // where shapes::circle()'s contour starts — due east, not due north, so
  // the conversion carries a −90° term. Asserting that against the path
  // Shapes.h actually builds means a change to circle()'s start point fails
  // here rather than silently rotating every label on a ring.
  const SkSize size{200, 200};
  const SkPath circle = shapes::circle()(size);
  const kit::Frame f{.centre = {100, 100}, .radius = 100};
  for (float th : {0.0f, 45.0f, 90.0f, 137.0f, 180.0f, 300.0f})
    EXPECT_TRUE(near(atFraction(circle, f.fraction(th)), f.at(th, 1.0f), 0.25f))
        << "th=" << th;
}

TEST(KitFrame, TheBaselinesDirectionIsNotTheFramesSense) {
  // A frame's `sense` and the winding of the path it labels are independent
  // facts, which is why fraction() takes the path direction as a separate
  // argument rather than reading it off the frame. shapes::circle(kCCW)
  // still STARTS due east — only the travel direction flips — so f = 0.25
  // is 12 o'clock on the CCW contour and 6 o'clock on the CW one.
  const SkSize size{200, 200};
  const SkPath cw = shapes::circle(SkPathDirection::kCW)(size);
  const SkPath ccw = shapes::circle(SkPathDirection::kCCW)(size);
  EXPECT_TRUE(near(atFraction(cw, 0.0f), atFraction(ccw, 0.0f), 0.25f))
      << "both contours start due east";
  EXPECT_FALSE(near(atFraction(cw, 0.25f), atFraction(ccw, 0.25f), 1.0f))
      << "…and then run opposite ways";

  // Every combination of frame sense and baseline direction must land on
  // the same point the frame names.
  for (kit::Sense sense : {kit::Sense::CW, kit::Sense::CCW}) {
    const kit::Frame f{.centre = {100, 100},
                       .radius = 100,
                       .zero = kit::Zero::North,
                       .sense = sense};
    for (auto dir : {SkPathDirection::kCW, SkPathDirection::kCCW}) {
      const SkPath &path = dir == SkPathDirection::kCW ? cw : ccw;
      for (float th : {0.0f, 60.0f, 210.0f})
        EXPECT_TRUE(near(atFraction(path, f.fraction(th, dir)), f.at(th, 1.0f),
                         0.25f))
            << "sense=" << (sense == kit::Sense::CW ? "CW" : "CCW")
            << " baseline=" << (dir == SkPathDirection::kCW ? "CW" : "CCW")
            << " th=" << th;
      for (float frac : {0.1f, 0.6f})
        EXPECT_NEAR(std::fmod(f.fraction(f.degOf(frac, dir), dir) - frac + 2.0f,
                              1.0f),
                    0.0f, 1e-3f);
    }
  }
}

TEST(KitFrame, DegOfInvertsFraction) {
  const kit::Frame f{
      .centre = {0, 0}, .radius = 1, .originDeg = -3.2f}; // a rotated scan
  for (float th : {5.0f, 120.0f, 359.0f}) {
    const float back = f.degOf(f.fraction(th));
    EXPECT_NEAR(std::fmod(back - th + 720.0f, 360.0f), 0.0f, 1e-2f);
  }
}

TEST(KitFrame, TurnedComposesAndScaledKeepsConventions) {
  const kit::Frame f{.centre = {10, 20},
                     .radius = 8,
                     .zero = kit::Zero::North,
                     .sense = kit::Sense::CCW,
                     .originDeg = 4.0f};
  EXPECT_EQ(f.turned(4.5f).turned(-4.5f), f);
  // A half-division offset must move the point by half a division.
  EXPECT_TRUE(near(f.turned(9.0f).at(0, 1), f.at(9.0f, 1), 1e-4f));

  const kit::Frame inner = f.scaled(0.5f);
  EXPECT_EQ(inner.zero, f.zero);
  EXPECT_EQ(inner.sense, f.sense);
  EXPECT_FLOAT_EQ(inner.originDeg, f.originDeg);
  EXPECT_TRUE(near(inner.at(31.0f, 1.0f), f.at(31.0f, 0.5f), 1e-4f));
}

TEST(KitFrame, BoxIsTheSquareShapesInscribeIn) {
  const kit::Frame f{.centre = {50, 60}, .radius = 20};
  const SkRect b = f.box(0.5f);
  EXPECT_FLOAT_EQ(b.width(), 20);
  EXPECT_FLOAT_EQ(b.height(), 20);
  EXPECT_FLOAT_EQ(b.centerX(), 50);
  EXPECT_FLOAT_EQ(b.centerY(), 60);
  // The circle inscribed in that box passes through at(θ, 0.5) — which is
  // what makes `.rect(f.box(k))` + `shapes::circle()` correct.
  const SkPath c = shapes::circle()(SkSize{b.width(), b.height()});
  SkPoint p = atFraction(c, f.fraction(0.0f));
  p.offset(b.fLeft, b.fTop);
  EXPECT_TRUE(near(p, f.at(0.0f, 0.5f), 0.2f));
}

// ===========================================================================
// Grid — the unit map.

TEST(KitGrid, LengthTakesNoOriginAndPositionDoes) {
  const kit::Grid g{.scale = 4.0f, .origin = {100, 50}};
  EXPECT_FLOAT_EQ(g.s(10), 40);  // a WIDTH
  EXPECT_FLOAT_EQ(g.x(10), 140); // a POSITION
  EXPECT_FLOAT_EQ(g.y(10), 90);
  const SkRect r = g.rect(10, 10, 5, 5);
  EXPECT_FLOAT_EQ(r.fLeft, 140);
  EXPECT_FLOAT_EQ(r.width(), 20);
}

TEST(KitGrid, SnapRoundsTheResultAndTwoGridsCoexist) {
  // Grid is a value rather than a free snapping function precisely so that
  // one figure can carry two of them — say a 4 px geometry grid and a
  // 2.5 px text grid — without either one being global state.
  const kit::Grid geo{.scale = 4.0f, .snap = 4.0f};
  const kit::Grid type{.scale = 2.5f, .snap = 2.5f};
  EXPECT_FLOAT_EQ(geo.x(1.3f), 4.0f);   // 5.2 → 4
  EXPECT_FLOAT_EQ(type.x(1.3f), 2.5f);  // 3.25 → 2.5
  EXPECT_NE(geo.s(3), type.s(3));
  const kit::Grid none{.scale = 4.0f};
  EXPECT_FLOAT_EQ(none.x(1.3f), 5.2f);
}

TEST(KitGrid, RectSnapsBothEdges) {
  const kit::Grid g{.scale = 1.0f, .snap = 4.0f};
  const SkRect r = g.rect(SkRect::MakeLTRB(1, 1, 11, 11));
  EXPECT_FLOAT_EQ(r.fLeft, 0);
  EXPECT_FLOAT_EQ(r.fRight, 12);
}

TEST(KitGrid, MapsAPolylineAndAMatrix) {
  const kit::Grid g{.scale = 2.0f, .origin = {5, 5}};
  const std::vector<SkPoint> units{{0, 0}, {1, 2}};
  const std::vector<SkPoint> px = g.map(units);
  ASSERT_EQ(px.size(), 2u);
  EXPECT_TRUE(near(px[1], {7, 9}, 1e-5f));
  SkPoint m = {1, 2};
  g.matrix().mapPoints({&m, 1});
  EXPECT_TRUE(near(m, {7, 9}, 1e-5f));
}

// ===========================================================================
// ticks — a division ladder as ONE path with N contours.

TEST(KitTicks, EmitsOneContourPerDivisionAndPlacesThemOnTheFrame) {
  const kit::Frame f{.centre = {0, 0}, .radius = 100};
  const SkPath p = kit::ticks(f, {.divisions = 12, .mark = {0.9f, 1.0f}});
  const Contours c = walk(p);
  EXPECT_EQ(c.pieces.size(), 12u);
  for (float len : c.lengths)
    EXPECT_NEAR(len, 10.0f, 1e-2f);
  // Mark 0 runs from at(0, .9) to at(0, 1).
  SkPoint start{0, 0};
  ASSERT_TRUE(c.pieces[0].getLastPt(&start));
  EXPECT_TRUE(near(start, f.at(0, 1.0f), 1e-3f));
}

TEST(KitTicks, LongEveryLengthensEveryNthMark) {
  const kit::Frame f{.centre = {0, 0}, .radius = 100};
  const SkPath p = kit::ticks(f, {.divisions = 72,
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

TEST(KitTicks, ClassifyReachesThreeLengthClasses) {
  // A three-way length pattern cannot be expressed by the long/short pair,
  // which is the whole reason `classify` exists: it hands each mark's index
  // to the caller and takes back that mark's span.
  const kit::Frame f{.centre = {0, 0}, .radius = 100};
  const SkPath p = kit::ticks(
      f, {.divisions = 9,
          .mark = {0.5f, 1.0f},
          .classify = [](int i, kit::Span s) {
            s.outer = (i % 3 == 0) ? 1.0f : (i % 3 == 1 ? 0.86f : 0.93f);
            return s;
          }});
  const Contours c = walk(p);
  ASSERT_EQ(c.lengths.size(), 9u);
  for (size_t i = 0; i < 9; ++i) {
    const float want = ((i % 3 == 0) ? 1.0f : (i % 3 == 1 ? 0.86f : 0.93f)) - 0.5f;
    EXPECT_NEAR(c.lengths[i], want * 100.0f, 1e-2f) << "mark " << i;
  }
}

TEST(KitTicks, ClosedAddsTheEndMarkAndSweepScopesTheLadder) {
  const kit::Frame f{.centre = {0, 0}, .radius = 100};
  const kit::Ticks quarter{
      .divisions = 9, .from = 0, .sweep = 90, .closed = true};
  const Contours c = walk(kit::ticks(f, quarter));
  EXPECT_EQ(c.pieces.size(), 10u); // 9 divisions, 10 rules
  SkPoint last{0, 0};
  ASSERT_TRUE(c.pieces.back().getLastPt(&last));
  EXPECT_TRUE(near(last, f.at(90.0f, 1.0f), 1e-3f));

  const kit::Ticks open{.divisions = 9, .from = 0, .sweep = 90};
  EXPECT_EQ(walk(kit::ticks(f, open)).pieces.size(), 9u);
}

TEST(KitTicks, OutlineFormTakesHalfTheShorterSide) {
  // A non-square box must still produce a CIRCULAR ladder, or
  // Frame::fraction stops matching and every label on it slides.
  const shapes::OutlineFn fn = kit::ticks({.divisions = 4, .mark = {0, 1}});
  const SkPath p = fn(SkSize{400, 100});
  const Contours c = walk(p);
  ASSERT_EQ(c.pieces.size(), 4u);
  for (float len : c.lengths)
    EXPECT_NEAR(len, 50.0f, 1e-2f);
}

TEST(KitTicks, ZeroLengthMarksAreSkippedRatherThanEmittedEmpty) {
  const kit::Frame f{.centre = {0, 0}, .radius = 10};
  const SkPath p = kit::ticks(f, {.divisions = 6, .mark = {1.0f, 1.0f}});
  EXPECT_TRUE(p.isEmpty());
}

// ===========================================================================
// chords — a polygon's sides as N OPEN contours of one path.

TEST(KitChords, SideKsMidpointIsAtExactlyKPlusHalfOverN) {
  // The whole reason chords() exists: TextPath walks every contour in order
  // as ONE arc-length coordinate, so a run of letters can be laid around a
  // heptagon as a single text run with each side occupying a known 1/n of
  // the coordinate. Measured against the path chords() built, not against
  // the formula it built it with.
  const kit::Frame f{.centre = {0, 0}, .radius = 100};
  for (int n : {5, 7, 12}) {
    const SkPath p = kit::chords(f, {.sides = n, .radius = 1.0f});
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

TEST(KitChords, PolygonCannotDoThat) {
  // The positive control's negative half: shapes::polygon emits ONE closed
  // contour, so a per-side coordinate does not exist on it. If this ever
  // starts passing, chords() has become redundant and should be deleted.
  const SkPath poly = shapes::polygon(7)(SkSize{200, 200});
  EXPECT_EQ(walk(poly).pieces.size(), 1u);
}

TEST(KitChords, InsetShortensBothEndsAndDropsDegenerateSides) {
  const kit::Frame f{.centre = {0, 0}, .radius = 100};
  const Contours plain = walk(kit::chords(f, {.sides = 7}));
  const Contours inset = walk(kit::chords(f, {.sides = 7, .inset = 6.0f}));
  ASSERT_EQ(inset.pieces.size(), 7u);
  EXPECT_NEAR(plain.lengths[0] - inset.lengths[0], 12.0f, 1e-2f);
  // An inset wider than the side leaves nothing to draw.
  EXPECT_TRUE(kit::chords(f, {.sides = 7, .inset = 500.0f}).isEmpty());
}

TEST(KitChords, StepMakesStarPolygonsAndGcdDecidesTheRingCount) {
  const kit::Frame f{.centre = {0, 0}, .radius = 100};
  // {7/2}: coprime, so one closed traversal of all seven vertices.
  EXPECT_EQ(walk(kit::chords(f, {.sides = 7, .step = 2, .closed = true}))
                .pieces.size(),
            1u);
  // {6/2}: gcd 2, so the hexagram really is TWO separate triangles. Emitting
  // one contour here would be wrong geometry, not a simplification.
  const Contours hex =
      walk(kit::chords(f, {.sides = 6, .step = 2, .closed = true}));
  EXPECT_EQ(hex.pieces.size(), 2u);
  for (float len : hex.lengths)
    EXPECT_NEAR(len, 3.0f * 100.0f * std::sqrt(3.0f), 0.5f);
}

// ===========================================================================
// PixelType — the aliased bitmap-font bake.

namespace {
sigil::weave::TextStyle pixelStyle(float size) {
  return sigil::compose::studio::type(
      {.face = sigil::compose::studio::pickFace({"Menlo", "DejaVu Sans Mono",
                                                 "Courier New"}),
       .size = size,
       .color = {1, 1, 1, 1},
       .aliased = true});
}
} // namespace

TEST(KitPixelType, PadsWideEnoughThatTheLastGlyphIsNotClipped) {
  // Sizing the bake plane from measure() plus a small fixed margin ends the
  // surface inside the final letter: measure() returns the ADVANCE, and a
  // glyph's ink can sit outside its advance. The assertion is that with the
  // default pad the ink never reaches the right or bottom edge of the plane
  // — if it touches an edge, something was cut off.
  const auto style = pixelStyle(10.0f);
  for (const char8_t *s : {u8"Centrifuge", u8"WAV", u8"research", u8"1234567890"}) {
    const kit::Coverage cov = kit::coverage(s, fonts(), style);
    ASSERT_TRUE(cov.valid());
    ASSERT_FALSE(cov.ink.isEmpty());
    EXPECT_LT(cov.ink.fRight, cov.width())
        << "ink touches the right edge — the surface is too small";
    EXPECT_LT(cov.ink.fBottom, cov.height());
  }
}

TEST(KitPixelType, InkReallyDoesOverhangTheAdvanceSoThePadIsLoadBearing) {
  // The positive control for the case above, and the choice of face is
  // load-bearing. A monospace face is boxed by construction and its ink
  // never leaves its advance, so with Menlo "pad 0 clips" is simply false
  // and the pad test above would be proving nothing. Italic and script
  // faces do overhang, so the control uses those, requires at least one to
  // reach the edge at pad 0, and requires the SAME string to be clear of it
  // at the default pad.
  int overhangs = 0;
  for (const char *family : {"Helvetica", "Times New Roman", "Zapfino",
                             "Apple Chancery", "Snell Roundhand"}) {
    sk_sp<SkTypeface> face = sigil::compose::studio::pickFace(
        {family}, SkFontStyle::Italic());
    if (!face)
      continue;
    const auto style = sigil::compose::studio::type(
        {.face = face, .size = 12.0f, .color = {1, 1, 1, 1}, .aliased = true});
    for (const char8_t *s : {u8"Wf", u8"of", u8"lift", u8"Ay"}) {
      const kit::Coverage tight =
          kit::coverage(s, fonts(), style, {.x = 0, .y = 0});
      if (!tight.valid() || tight.ink.isEmpty() ||
          tight.ink.fRight < tight.width())
        continue;
      ++overhangs;
      // The same string with ANY pad must come back unclipped, because
      // coverage() grows the pad and re-bakes until nothing touches an
      // edge. One pixel is enough to arm that retry. No fixed pad is
      // correct for every face — script faces overhang further than any
      // plausible default — which is why the component retries rather than
      // documenting a number for callers to pass.
      for (kit::Pad pad : {kit::Pad{1, 1}, kit::Pad{}}) {
        const kit::Coverage grown = kit::coverage(s, fonts(), style, pad);
        ASSERT_TRUE(grown.valid());
        ASSERT_FALSE(grown.ink.isEmpty());
        EXPECT_GT(grown.ink.fLeft, 0) << family << " / " << (const char *)s;
        EXPECT_GT(grown.ink.fTop, 0);
        EXPECT_LT(grown.ink.fRight, grown.width())
            << family << " / " << (const char *)s;
        EXPECT_LT(grown.ink.fBottom, grown.height());
        // The clipped bake LOST ink; the grown one recovered it.
        EXPECT_GE(grown.ink.width(), tight.ink.width());
      }
    }
  }
  EXPECT_GT(overhangs, 0)
      << "no face on this machine overhangs its advance, so this control "
         "proved nothing — the pad test above is unguarded here";
}

TEST(KitPixelType, MaskIsCroppedToItsInkAndCarriesTheOffsetBack) {
  const kit::Coverage cov = kit::coverage(u8"MM", fonts(), pixelStyle(12.0f));
  ASSERT_TRUE(cov.valid());
  const kit::Mask m = kit::threshold(cov);
  ASSERT_TRUE(m);
  EXPECT_EQ(m.w, cov.ink.width());
  EXPECT_EQ(m.h, cov.ink.height());
  EXPECT_EQ(m.inkX, cov.ink.fLeft);
  EXPECT_GT(m.advance, 0.0f);
  // Uncropped keeps the whole padded plane.
  const kit::Mask full = kit::threshold(cov, 0.5f, /*cropToInk=*/false);
  EXPECT_EQ(full.w, cov.width());
  EXPECT_GT(full.w, m.w);
}

TEST(KitPixelType, TheMaskIsOneBit) {
  const kit::Coverage cov = kit::coverage(u8"Ag", fonts(), pixelStyle(11.0f));
  ASSERT_TRUE(cov.valid());
  const kit::Mask m = kit::threshold(cov);
  ASSERT_TRUE(m);
  SkBitmap read;
  read.allocPixels(SkImageInfo::MakeA8(m.w, m.h));
  ASSERT_TRUE(m.image->readPixels(read.pixmap(), 0, 0));
  for (int y = 0; y < m.h; ++y)
    for (int x = 0; x < m.w; ++x) {
      const uint8_t v = *read.getAddr8(x, y);
      ASSERT_TRUE(v == 0 || v == 255) << "grey " << (int)v << " at " << x << ","
                                      << y;
    }
}

TEST(KitPixelType, DigitsShareOneAdvanceSoAReadoutDoesNotShiver) {
  const kit::PixFont f = kit::bakeFont(fonts(), pixelStyle(10.0f));
  EXPECT_GT(f.lineHeight, 0);
  EXPECT_GT(f.digitAdvance, 0);
  EXPECT_FLOAT_EQ(kit::widthOf(f, "111"), kit::widthOf(f, "888"));
  // …and turning it off lets them differ on a proportional face. (Menlo is
  // already tabular, so this asserts only that the flag is honoured, not
  // that the two widths differ.)
  const kit::Blit prop{.tabularDigits = false};
  EXPECT_GT(kit::widthOf(f, "888", prop), 0.0f);
}

TEST(KitPixelType, SpaceHasAnAdvanceAndNoMask) {
  const kit::PixFont f = kit::bakeFont(fonts(), pixelStyle(10.0f));
  const kit::Cell &sp = f.cell(' ');
  EXPECT_EQ(sp.mask, nullptr);
  EXPECT_GT(sp.advance, 0);
  EXPECT_GT(kit::widthOf(f, "a a"), kit::widthOf(f, "aa"));
}

TEST(KitPixelType, BlitAdvancesByTheMeasuredWidthAndSnaps) {
  const kit::PixFont f = kit::bakeFont(fonts(), pixelStyle(10.0f));
  sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 40));
  ASSERT_TRUE(s);
  const kit::Blit b{.track = 1.0f};
  const float w = kit::blit(*s->getCanvas(), f, {4, 4}, "1234", {1, 1, 1, 1}, b);
  EXPECT_NEAR(w, kit::widthOf(f, "1234", b), 1e-3f);

  const kit::Blit snapped{.track = 1.0f, .snap = 4.0f};
  const float ws =
      kit::blit(*s->getCanvas(), f, {4.9f, 4.1f}, "1", {1, 1, 1, 1}, snapped);
  EXPECT_NEAR(std::fmod(ws, 4.0f), 0.0f, 1e-3f);
}

TEST(KitPixelType, MaskedIsANodeTheSizeOfTheMask) {
  const kit::Mask m = kit::bakeRun(u8"88", fonts(), pixelStyle(10.0f));
  ASSERT_TRUE(m);
  const SkSize sz = measure(box().child(kit::masked(m, {.scale = 2.0f})), fonts());
  EXPECT_FLOAT_EQ(sz.width(), (float)m.w * 2.0f);
  EXPECT_FLOAT_EQ(sz.height(), (float)m.h * 2.0f);
}

// ===========================================================================
// Legibility.

TEST(KitLegibility, HaloedAddsExactlyOneUnderlayAndLeavesTheInputAlone) {
  sigil::weave::TextStyle base;
  base.shaping.fontSize = 12;
  const size_t before = base.paint.underlays.size();
  const sigil::weave::TextStyle out =
      kit::haloed(base, {.colour = {1, 1, 1, 1}, .width = 2.2f});
  EXPECT_EQ(base.paint.underlays.size(), before) << "input was mutated";
  ASSERT_EQ(out.paint.underlays.size(), before + 1);
  const SkPaint &p = out.paint.underlays.back().paint;
  EXPECT_EQ(p.getStyle(), SkPaint::kStroke_Style);
  EXPECT_FLOAT_EQ(p.getStrokeWidth(), 2.2f);
  EXPECT_EQ(p.getStrokeJoin(), SkPaint::kRound_Join);
}

TEST(KitLegibility, ShadeIsAnOffsetFillNotAStroke) {
  sigil::weave::TextStyle base;
  const sigil::weave::TextStyle out =
      kit::shaded(base, {.colour = {0, 0, 0, 0.9f}, .offset = {1, 1}});
  ASSERT_EQ(out.paint.underlays.size(), 1u);
  EXPECT_EQ(out.paint.underlays[0].paint.getStyle(), SkPaint::kFill_Style);
  EXPECT_FLOAT_EQ(out.paint.underlays[0].offset.fX, 1.0f);
}

TEST(KitLegibility, ScrimGrowsTheRunByItsPadding) {
  sigil::weave::TextStyle st;
  st.shaping.fontSize = 12;
  const SkSize bare = measure(box().child(text(u8"NAVI", st)), fonts());
  const SkSize plated = measure(
      box().child(kit::scrim(text(u8"NAVI", st), {.paddingX = 3, .paddingY = 4})),
      fonts());
  EXPECT_FLOAT_EQ(plated.width(), bare.width() + 6);
  EXPECT_FLOAT_EQ(plated.height(), bare.height() + 8);
}

TEST(KitLegibility, DrawHaloedPutsGroundColourAroundTheInk) {
  // The immediate-mode spelling, which exists because a caption inside a
  // custom() leaf cannot reach addUnderlay.
  //
  // The comparison has to be DIFFERENTIAL — halo render against no-halo
  // render — because no absolute pixel count is a valid criterion here.
  // "More halo than ink" is false at ordinary sizes, since glyph interiors
  // outnumber a thin surrounding ring. What is true regardless of size is
  // that the halo colour appears only when a halo was asked for, and that
  // the ink survives it.
  auto render = [](bool halo) {
    sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(160, 40));
    s->getCanvas()->clear(SkColorSetARGB(255, 128, 128, 128));
    SkFont font(sigil::compose::studio::pickFace({"Menlo", "Courier New"}),
                20.0f);
    if (halo)
      kit::drawHaloed(*s->getCanvas(), "HALO", {10, 28}, font,
                      SkColor4f{1, 1, 1, 1},
                      {.colour = {0, 0, 0, 1}, .width = 3.0f});
    else
      kit::drawHaloed(*s->getCanvas(), "HALO", {10, 28}, font,
                      SkColor4f{1, 1, 1, 1},
                      {.colour = {0, 0, 0, 0}, .width = 0.0f});
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(160, 40));
    s->readPixels(bm.pixmap(), 0, 0);
    int white = 0, black = 0;
    for (int y = 0; y < 40; ++y)
      for (int x = 0; x < 160; ++x) {
        const int r = (int)SkColorGetR(bm.getColor(x, y));
        white += r > 240;
        black += r < 16;
      }
    return std::pair<int, int>{white, black};
  };
  const auto plain = render(false);
  const auto haloed = render(true);
  EXPECT_GT(plain.first, 0) << "no ink at all";
  EXPECT_EQ(plain.second, 0) << "knockout appeared without being asked for";
  EXPECT_GT(haloed.second, 0) << "the halo did not paint";
  // A knockout ring surrounds the ink, so it must not eat it.
  EXPECT_GT(haloed.first, plain.first / 2)
      << "the halo painted over the ink instead of under it";
}

// ===========================================================================
// The documentation, spelled. Compiles, asserts nothing, and breaks the
// build if a signature in any kit header changes — a documented call that
// no longer compiles is a lie nobody would otherwise notice.

TEST(KitDocs, EverySignatureIsSpelledOnce) {
  const kit::Frame frame{.centre = {100, 100},
                         .radius = 80,
                         .zero = kit::Zero::North,
                         .sense = kit::Sense::CW,
                         .originDeg = 0};
  (void)frame.skiaDeg(30);
  (void)frame.skiaSweep(30);
  (void)frame.radians(30);
  (void)frame.fraction(30);
  (void)frame.degOf(0.25f);
  (void)frame.fraction(30, SkPathDirection::kCCW);
  (void)frame.degOf(0.25f, SkPathDirection::kCCW);
  (void)frame.at(30, 0.5f);
  (void)frame.px(30, 40);
  (void)frame.dir(30);
  (void)frame.box(0.5f);
  (void)frame.disc(0.5f);
  (void)frame.scaled(0.5f);
  (void)frame.about({0, 0});
  (void)frame.turned(4.5f);

  const kit::Grid grid{.scale = 4, .origin = {8, 8}, .snap = 4};
  (void)grid.snapped(3);
  (void)grid.s(3);
  (void)grid.x(3);
  (void)grid.y(3);
  (void)grid.at({3, 3});
  (void)grid.rect(0, 0, 4, 4);
  (void)grid.rect(SkRect::MakeWH(4, 4));
  (void)grid.map({{0, 0}});
  (void)grid.matrix();
  (void)grid.scaled(0.625f);

  (void)kit::ticks(frame, {.divisions = 72,
                           .from = 0,
                           .sweep = 360,
                           .closed = false,
                           .mark = {0.96f, 1.0f},
                           .longEvery = 6,
                           .longMark = {0.91f, 1.0f},
                           .classify = nullptr});
  (void)kit::ticks({.divisions = 12}, frame);
  (void)kit::chords(frame, {.sides = 7,
                            .step = 1,
                            .radius = 0.9f,
                            .from = 0,
                            .inset = 4,
                            .closed = false});
  (void)kit::chords({.sides = 7}, frame);

  const auto style = pixelStyle(10.0f);
  const kit::Coverage cov = kit::coverage(u8"8", fonts(), style, {.x = 8, .y = 4});
  (void)cov.alphaAt(0, 0);
  const kit::Mask mask = kit::threshold(cov, 0.5f, true);
  (void)kit::bakeRun(u8"8", fonts(), style, {.x = 8, .y = 4}, 0.5f);
  (void)kit::masked(mask, {.colour = {1, 1, 1, 1},
                           .scale = 2,
                           .shadowOffset = {2, 2},
                           .shadowMul = 0.25f});
  const kit::PixFont pix = kit::bakeFont(fonts(), style, {3, 3}, 0.5f, 0.34f);
  (void)pix.cell('8');
  const kit::Blit blitOpts{.track = 1, .tabularDigits = true, .snap = 4};
  (void)kit::widthOf(pix, "88", blitOpts);
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(64, 32));
  ASSERT_TRUE(surface);
  (void)kit::blit(*surface->getCanvas(), pix, {0, 0}, "88", {1, 1, 1, 1},
                  blitOpts);
  kit::draw(*surface->getCanvas(), mask, {0, 0}, {});

  sigil::weave::TextStyle ts;
  (void)kit::haloed(ts, {.colour = {1, 1, 1, 1},
                         .width = 2.2f,
                         .join = SkPaint::kRound_Join});
  (void)kit::shaded(ts, {.colour = {0, 0, 0, 1}, .offset = {1, 1}});
  (void)kit::emboldened(ts, 0.6f, {0, 0, 0, 1});
  (void)kit::scrim(text(u8"x", ts), {.fill = Fill::color({0, 0, 0, 0.7f}),
                                     .paddingX = 3,
                                     .paddingY = 3,
                                     .radius = 0});
  SkFont font(nullptr, 10.0f);
  SkPaint ink;
  kit::drawHaloed(*surface->getCanvas(), "x", {0, 0}, font, ink, {});
  kit::drawHaloed(*surface->getCanvas(), "x", {0, 0}, font, SkColor4f{1, 1, 1, 1},
                  {});
  SUCCEED();
}

// ---------------------------------------------------------------------------
// kit/Strokes.h — the kit's stroke values.
//
// The claim these check is that a kit value is a PEER of one you write
// yourself: every case below goes through the same public seam a
// user-written value would, and kit/Kit.cpp static_asserts the concepts.
// The other half of the boundary — that the kit compiles against public
// headers only — cannot be checked from a gtest binary, since it is a
// compile FAILURE that must be observed; kit/BoundaryProbe.cpp covers it
// and is built on demand.

#include <sigilcompose/Brushes.h>
#include <sigilcompose/Util.h>
#include <sigilcompose/kit/Strokes.h>

TEST(ComposeKitStrokes, ShapersSatisfyThePublicSeam) {
  static_assert(ShaperScheme<kit::brush::shapers::Wave>);
  static_assert(ShaperScheme<kit::brush::shapers::Jitter>);
  static_assert(ShaperScheme<kit::brush::shapers::Offset>);
  // …and the wave doubles as a PROFILE, which is what makes a braid strand
  // and an undulating band one vocabulary.
  static_assert(ProfileScheme<kit::brush::shapers::Wave>);

  SkPathBuilder b;
  b.moveTo(0, 50);
  b.lineTo(200, 50);
  const SkPath straight = b.detach();

  const SkPath waved = kit::brush::shapers::wave(6, 30).shape(straight);
  EXPECT_GT(waved.getBounds().height(), 6.0f) << "the wave did not deviate";
  const SkPath jittered = kit::brush::shapers::jitter(8, 3, 5).shape(straight);
  EXPECT_GT(jittered.getBounds().height(), 1.0f);
  const SkPath railed = kit::brush::shapers::offset(-12).shape(straight);
  EXPECT_NEAR(railed.getBounds().centerY(), 62.0f, 1.5f)
      << "positive offset is LEFT of travel — the one convention (R3's "
         "sign port), so travelling +x with y down a NEGATIVE offset goes "
         "down the screen";

  // Comparable, so a brush holding one prunes.
  EXPECT_TRUE(kit::brush::shapers::wave(6, 30) ==
              kit::brush::shapers::wave(6, 30));
  EXPECT_FALSE(kit::brush::shapers::wave(6, 30) ==
               kit::brush::shapers::wave(6, 31));
}

TEST(ComposeKitStrokes, BraidCrossesByConstruction) {
  // n waves at phase k/n MUST trade sides, which is why the braid primitive
  // is the wave and not the offset. Constant offsets are rails: they stay a
  // fixed distance apart and never cross, so no braid can be built from
  // them — the control at the end of this case is that claim.
  SkPathBuilder b;
  b.moveTo(0, 100);
  b.lineTo(400, 100);
  const SkPath spine = b.detach();

  for (int n : {2, 3, 4}) {
    const std::vector<brush::Strand> braid =
        kit::strands::braid(n, 10, 60, brush::solid(2, Fill::color({1, 0, 0, 1})));
    ASSERT_EQ(braid.size(), (size_t)n);
    std::vector<SkPath> paths;
    for (const brush::Strand &s : braid)
      paths.push_back(profileOffset(spine, s.path.profile()));
    const std::vector<Crossing> crossings = discoverCrossings(paths);
    EXPECT_GT(crossings.size(), 0u)
        << n << " braided waves produced no crossing";
    // Numbering is positional and contiguous, so a pin means the same knot
    // for as long as the geometry holds.
    for (size_t i = 0; i < crossings.size(); ++i)
      EXPECT_EQ(crossings[i].index, i);
  }

  // The control: the same strand count as PARALLELS never crosses.
  std::vector<SkPath> rails;
  for (int k = 0; k < 3; ++k)
    rails.push_back(profileOffset(spine, strand::offset((float)k * 6.0f)));
  EXPECT_TRUE(discoverCrossings(rails).empty())
      << "parallels are rails — they must not braid";
}

TEST(ComposeKitStrokes, BraidSharesOneBrushAcrossItsStrands) {
  const Decoration ink = brush::solid(2, Fill::color({0, 1, 0, 1}));
  const std::vector<brush::Strand> braid = kit::strands::braid(3, 8, 40, ink);
  for (const brush::Strand &s : braid) {
    EXPECT_TRUE(s.brush == ink) << "braid() is sugar for n offsets of ONE brush";
    EXPECT_EQ(s.path.source(), StrandPath::Source::Relative);
    EXPECT_NEAR(s.path.reach(), 8.0f, 1e-4f) << "reach is the amplitude";
  }
  // Distinct phases — otherwise they would be coincident and never cross.
  EXPECT_FALSE(braid[0].path == braid[1].path);
}

TEST(ComposeKitStrokes, SpansAndShapesAreCompositionsNotNewKinds) {
  // kit::spans::brackets is a COMPOSITION of core terms, which is what a
  // kit span can be and why Spans stays a closed value.
  EXPECT_TRUE(kit::spans::brackets(18) == spans::corners(18));
  EXPECT_FALSE(kit::spans::brackets(18) == spans::corners(19));

  // kit::shapes::ring is a plainer name for core's annulus, not a second
  // shape — same path, so a figure can use either spelling.
  const SkPath ring = kit::shapes::ring(0.6f)({100, 100});
  EXPECT_FALSE(ring.isEmpty());
  EXPECT_EQ(ring, shapes::annulus(0.6f)({100, 100}));
}

TEST(ComposeKitStrokes, TheWaveProfileIsAKitValueOverACoreSeam) {
  // Core ships strand::self()/offset() only; everything that oscillates
  // lives in the kit — but it plugs the SAME Profile seam, so core code
  // never learns that a kit profile exists.
  const Profile undulating = kit::profile::wave(9, 50);
  EXPECT_NEAR(undulating.max(), 9.0f, 1e-4f) << "max() is required by the seam";
  EXPECT_TRUE(undulating == kit::profile::wave(9, 50));
  EXPECT_FALSE(undulating == kit::profile::wave(9, 51));
  EXPECT_FALSE(undulating == strand::offset(9));

  // A band takes it, because a band's taper and a strand's path are one value.
  Element undulatingBand = band(shapes::circle(), across(undulating));
  EXPECT_TRUE(undulatingBand.node() != nullptr);
}

namespace {

/** A composer over a raster surface — the kit suite's own harness. Kept
 *  here rather than shared with compose_test because the two binaries are
 *  deliberately separate (a kit failure must not read as a kernel one). */
struct StrokeHost {
  sigil::motion::Ticker ticker;
  Composer composer{ticker, fonts()};
  sk_sp<SkSurface> surface;

  explicit StrokeHost(int w = 200, int h = 200) {
    composer.setSize({(float)w, (float)h});
    surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w, h));
  }
  SkColor pixel(int x, int y) {
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
    surface->readPixels(bm.pixmap(), x, y);
    return bm.getColor(0, 0);
  }
  void frame() {
    surface->getCanvas()->clear(SK_ColorBLACK);
    composer.draw(*surface->getCanvas());
  }
};

Fill strokeRed() { return Fill::color({1, 0, 0, 1}); }
Fill strokeGreen() { return Fill::color({0, 1, 0, 1}); }

} // namespace

// ---------------------------------------------------------------------------
// The shaper seam, exercised with KIT shaper values.
//
// These live here rather than in compose_test because the kernel suite must
// not include a kit header: if it did, a kit compile failure would be
// reported as a kernel failure.

TEST(ComposeKitStrokes, ShapedAgreesWithTheRestyleWrapper) {
  // `.shaped(value)` is the ONE geometry-deviation seam. `brush::restyle`
  // does the same job around a `GeometryOp`, and it stays because a raw
  // lambda can never be a Shaper — a Shaper is comparable by design, and a
  // lambda is not. So the claim here is that the two spellings agree, not
  // that one replaces the other.
  //
  // What is asserted is INK-COUNT SIMILARITY within 5%, not identical
  // output. The two paths build their own PaintContext and wrap the op
  // differently, so byte equality is not the property on offer; a shaper
  // that silently drew nothing, or drew something else, still fails.
  auto draw = [](bool legacySpelling) {
    StrokeHost host(200, 200);
    Element e = box().rect(SkRect::MakeXYWH(30, 30, 140, 140));
    if (legacySpelling)
      e.stroke(brush::restyle(kit::brush::shapers::Wave{5, 24},
                              brush::solid(3, strokeRed()), 8));
    else
      e.stroke(Brush{}
                   .shaped(kit::brush::shapers::wave(5, 24))
                   .layer(brush::solid(3, strokeRed())));
    host.composer.render(stack().child(std::move(e)));
    host.frame();
    int inked = 0;
    for (int x = 0; x < 200; ++x)
      for (int y = 0; y < 200; ++y)
        if (host.pixel(x, y) == SK_ColorRED)
          ++inked;
    return inked;
  };
  const int shaped = draw(false), wrapper = draw(true);
  EXPECT_GT(shaped, 100) << "the shaper drew nothing";
  EXPECT_NEAR((double)shaped, (double)wrapper, (double)wrapper * 0.05);

  // Two brushes built from equal shaper values compare equal, which is what
  // lets a node carrying a shaped brush prune instead of re-patching.
  EXPECT_TRUE(Brush{}.shaped(kit::brush::shapers::wave(5, 24)) ==
              Brush{}.shaped(kit::brush::shapers::wave(5, 24)));
  EXPECT_FALSE(Brush{}.shaped(kit::brush::shapers::wave(5, 24)) ==
               Brush{}.shaped(kit::brush::shapers::wave(5, 25)));
}

TEST(ComposeKitStrokes, ShapersAreComparableValuesAndPrune) {
  static_assert(ShaperScheme<kit::brush::shapers::Wave>);
  EXPECT_TRUE(Shaper(kit::brush::shapers::wave(4, 20)) ==
              Shaper(kit::brush::shapers::wave(4, 20)));
  EXPECT_FALSE(Shaper(kit::brush::shapers::wave(4, 20)) ==
               Shaper(kit::brush::shapers::wave(5, 20)));
  EXPECT_FALSE(Shaper(kit::brush::shapers::wave(4, 20)) ==
               Shaper(kit::brush::shapers::jitter()));
  EXPECT_TRUE(Shaper() == Shaper()) << "reflexive when empty";
}

TEST(ComposeKitStrokes, BraidAlternatesAlongTheWholeRun) {
  // The degeneracy this guards against: as mark-width/sin(crossing angle)
  // approaches the spacing between knots, neighbouring overlap lenses touch,
  // pathops merges them into ONE contour, and the first crossing's patch
  // claims the whole run — so the braid renders as a single strand laid on
  // top of the other. It shows up only at tight amplitude-to-ink ratios,
  // hence the two deliberately tight parameter sets at the bottom.
  //
  // braid() shares ONE brush across its strands by design, so the strands
  // here are rebuilt with the geometry braid() produces (waves at phase k/n,
  // asserted equal below) but two different inks — that is what makes the
  // alternation readable from pixels at all.
  auto wrongKnots = [](float amp, float wavelength, float inkWidth) {
    StrokeHost host(1000, 240);
    SkPathBuilder sp;
    sp.moveTo(0, 120);
    sp.lineTo(1000, 120);
    const SkPath spine = sp.detach();

    const std::vector<brush::Strand> strands = {
        brush::Strand{kit::profile::wave(amp, wavelength, 0.0f),
                      brush::solid(inkWidth, strokeRed())},
        brush::Strand{kit::profile::wave(amp, wavelength, 0.5f),
                      brush::solid(inkWidth, strokeGreen())}};
    // Same phases braid() would hand out for n = 2.
    const std::vector<brush::Strand> viaBraid =
        kit::strands::braid(2, amp, wavelength,
                            brush::solid(inkWidth, strokeRed()));
    EXPECT_EQ(viaBraid[0].path, strands[0].path);
    EXPECT_EQ(viaBraid[1].path, strands[1].path);

    host.composer.render(stack().child(
        box().inset(0).shape([&](SkSize) { return spine; })
            .stroke(brush::weave(strands, crossing::alternate()))));
    host.frame();

    // The knots, in the same order the rule numbers them.
    std::vector<SkPath> paths;
    for (const brush::Strand &st : strands)
      paths.push_back(profileOffset(spine, st.path.profile()));
    const std::vector<Crossing> knots = discoverCrossings(paths);
    EXPECT_GT(knots.size(), 20u) << "not enough knots to show the defect";

    // alternate() puts strand 0 (red) over at even ordinals, strand 1
    // (green) over at odd ones. Sample each knot and count disagreements.
    int wrong = 0, sampled = 0;
    for (const Crossing &k : knots) {
      const int px = (int)std::lround(k.at.fX);
      const int py = (int)std::lround(k.at.fY);
      // A knot bisected by the frame has no interior pixel to read — the
      // spine ends ON the last one, at x == width. Skip rather than count
      // the surface's out-of-bounds transparent black as a defect.
      if (px < 1 || px > 998 || py < 1 || py > 238)
        continue;
      ++sampled;
      const SkColor want = (k.index % 2 == 0) ? SK_ColorRED : SK_ColorGREEN;
      if (host.pixel(px, py) != want)
        ++wrong;
    }
    EXPECT_GT(sampled, 20) << "too few knots landed inside the frame";
    return std::pair<int, size_t>{wrong, knots.size()};
  };

  const auto a = wrongKnots(3.0f, 40.0f, 6.0f);
  EXPECT_EQ(a.first, 0) << a.first << " of " << a.second
                        << " knots wrong at amp 3 / wl 40 / ink 6";
  const auto b = wrongKnots(4.0f, 30.0f, 5.0f);
  EXPECT_EQ(b.first, 0) << b.first << " of " << b.second
                        << " knots wrong at amp 4 / wl 30 / ink 5";
}

// ---------------------------------------------------------------------------
// Rounded, Square and Zigzag are the corner-rounding and squared-off
// shapers. Each must actually deviate the path it is handed, and Zigzag
// must not collapse into Wave — a sharp zigzag and a smooth wave are
// separate marks, and a shared parameter pair makes them easy to conflate.

TEST(ComposeKitStrokes, TheThreeTwinsThatAbsorbedTheOpsStructs) {
  SkPathBuilder b;
  b.moveTo(10, 10);
  b.lineTo(160, 10);
  b.lineTo(160, 120);
  b.lineTo(10, 120);
  b.close();
  const SkPath src = b.detach();

  // (Named locals rather than braced temporaries inline: a designated
  // aggregate inside EXPECT_* hands the macro its commas.)
  const kit::brush::shapers::Square kitSquare{5, 26};
  const kit::brush::shapers::Zigzag kitZigzag{4, 28};
  const kit::brush::shapers::Wave kitWave{4, 28};

  EXPECT_FALSE(kit::brush::shapers::Rounded{9.0f}.shape(src) == src)
      << "Rounded did not round the corners";
  EXPECT_FALSE(kitSquare.shape(src) == src);
  EXPECT_FALSE(kitZigzag.shape(src) == src);
  // …and Zigzag is NOT Wave, at identical amplitude and wavelength.
  EXPECT_FALSE(kitZigzag.shape(src) == kitWave.shape(src));
}

TEST(ComposeKitStrokes, TheNewTwinsAreComparableSeamValuesLikeTheRest) {
  static_assert(ShaperScheme<kit::brush::shapers::Rounded>);
  static_assert(ShaperScheme<kit::brush::shapers::Square>);
  static_assert(ShaperScheme<kit::brush::shapers::Zigzag>);
  EXPECT_TRUE(Shaper(kit::brush::shapers::rounded(6)) ==
              Shaper(kit::brush::shapers::rounded(6)));
  EXPECT_FALSE(Shaper(kit::brush::shapers::rounded(6)) ==
               Shaper(kit::brush::shapers::rounded(7)));
  // Different KINDS never compare equal even at equal numbers — the type
  // is part of the value, which is what keeps a re-described brush honest.
  EXPECT_FALSE(Shaper(kit::brush::shapers::square(4, 28)) ==
               Shaper(kit::brush::shapers::zigzag(4, 28)));
}

TEST(ComposeKitPresets, TheFourPresetsCameOutOfCoreUNCHANGED) {
  // Each preset is pinned against a HAND-BUILT copy of its layer stack.
  // `LayeredBrush` has a defaulted `==`, so this compares every field of
  // every layer — width, colour, blur, dash, phase, blend, the lot.
  // Counting layers and spot-checking one width would keep passing on a
  // preset whose colours had all been halved, which is precisely the kind
  // of drift a shared preset suffers.
  using kit::brush::presets::circuit;
  using kit::brush::presets::filament;
  using kit::brush::presets::pulse;
  using kit::brush::presets::rope;

  const SkColor4f glow{0.435f, 0.847f, 1.0f, 1};
  const SkColor4f core{0.875f, 0.965f, 1.0f, 1};
  SkColor4f g18 = glow, g45 = glow, c90 = core;
  g18.fA = 0.18f;
  g45.fA = 0.45f;
  c90.fA = 0.90f;
  const LayeredBrush wantFilament{{
      {14, g18, 8, {}, 0, SkBlendMode::kPlus},
      {7, g45, 3, {}, 0, SkBlendMode::kPlus},
      {2.5f, c90},
      {1, {1, 1, 1, 0.7f}},
  }};
  EXPECT_TRUE(filament() == wantFilament);

  // circuit: three tiers, three different stacks, and tier 2 is the only
  // one that lays down two layers (an under-glow beneath the trace).
  const SkColor4f teal{0.208f, 0.878f, 0.824f, 1};
  SkColor4f data = teal, main = teal, power = teal, under = teal;
  data.fA = 0.55f;
  main.fA = 0.85f;
  power.fA = 1.0f;
  under.fA = 0.15f;
  // (Named locals, not braced temporaries inline: an aggregate inside
  // EXPECT_* hands the macro its commas.)
  const LayeredBrush wantData{{{1, data, 0, {}, 0, SkBlendMode::kSrcOver,
                                false}}};
  const LayeredBrush wantMain{{{2, main, 0, {}, 0, SkBlendMode::kSrcOver,
                                false}}};
  const LayeredBrush wantPower{
      {{8, under, 4}, {4, power, 0, {}, 0, SkBlendMode::kSrcOver, false}}};
  EXPECT_TRUE(circuit(teal, 0) == wantData);
  EXPECT_TRUE(circuit(teal, 1) == wantMain);
  EXPECT_TRUE(circuit(teal, 2) == wantPower);
  EXPECT_TRUE(circuit() == circuit(teal, 1)) << "the shipped defaults";

  // rope: the palette ladder, verified against Path of Building, plus the
  // Active state's halo. `scale` multiplies every width, dash and blur.
  const SkColor4f body{0.541f, 0.447f, 0.282f, 1};
  const SkColor4f ridge{0.780f, 0.659f, 0.420f, 1};
  const SkColor4f bodyLit{body.fR * 1.15f, body.fG * 1.15f, body.fB * 1.15f, 1};
  const SkColor4f ridgeLit{ridge.fR * 1.3f, ridge.fG * 1.3f, ridge.fB * 1.3f,
                           0.6f};
  const LayeredBrush wantActive{{
      {18, {1.0f, 0.788f, 0.439f, 0.13f}, 6},
      {11, body, 0, {}, 0, SkBlendMode::kSrcOver, false},
      {7, ridge, 0, {7, 5}, 0},
      {7, bodyLit, 0, {7, 5}, 6},
      {2, ridgeLit, 0, {7, 5}, 3},
  }};
  EXPECT_TRUE(rope(2) == wantActive);
  // The state index CLAMPS rather than reading off the end of the table.
  EXPECT_TRUE(rope(9) == wantActive);
  EXPECT_TRUE(rope(-3) == rope(0));
  EXPECT_FALSE(rope(0) == rope(1)) << "the three states are three palettes";

  const SkColor4f halo{1.0f, 0.79f, 0.44f, 0.35f};
  SkColor4f pulseBody = halo;
  pulseBody.fA = std::min(1.0f, halo.fA * 2.2f);
  const LayeredBrush wantPulse{{
      {12, halo, 5, {}, 0, SkBlendMode::kPlus},
      {5, pulseBody, 2, {}, 0, SkBlendMode::kPlus},
      {2, {1, 1, 1, 0.9f}},
  }};
  EXPECT_TRUE(pulse() == wantPulse);
}

TEST(ComposeKitPresets, TheDefaultArgumentsSurvivedTheMove) {
  // The presets' default arguments are part of their published shape: a
  // caller writing `rope(1)` must get the same brush as `rope(1, 1.0f)`.
  // Nothing else in the suite would notice a changed default, since every
  // other case passes all the arguments explicitly.
  EXPECT_TRUE(kit::brush::presets::rope(1) == kit::brush::presets::rope(1, 1.0f));
  const SkColor4f teal{0.2f, 0.9f, 0.8f, 1};
  EXPECT_TRUE(kit::brush::presets::circuit(teal) ==
              kit::brush::presets::circuit(teal, 1));
  EXPECT_FALSE(kit::brush::presets::circuit(teal, 2) ==
               kit::brush::presets::circuit(teal, 1));
}

TEST(ComposeKitStrokes, ABleedIsADISTANCEAndNeverNegative) {
  // bleed() grows the recording cull, so a NEGATIVE one shrinks it and
  // clips the mark it was supposed to protect. A negative amplitude is a
  // perfectly legal wave — it simply starts the other way — so every
  // oscillating value must report the magnitude, never the raw parameter.
  // The rule is the same across core and kit, so all three are checked.
  // (Named locals: a braced aggregate inside EXPECT_* hands the macro its
  // commas.)
  const kit::brush::shapers::Wave kitWave{-4.0f, 20.0f};
  const kit::brush::shapers::Square kitSquare{-5.0f, 26.0f};
  const kit::brush::shapers::Zigzag kitZigzag{-4.0f, 28.0f};
  EXPECT_FLOAT_EQ(kitWave.bleed(), 4.0f);
  EXPECT_FLOAT_EQ(kitSquare.bleed(), 5.0f);
  EXPECT_FLOAT_EQ(kitZigzag.bleed(), 4.0f);
  // …and the type-erased seams read the same number through.
  EXPECT_FLOAT_EQ(Shaper(kitWave).bleed(), 4.0f);
  EXPECT_FLOAT_EQ(GeometryOp(kitSquare).bleed(), 5.0f);
  // A negative amplitude still DRAWS — it is the same wave, half a cycle
  // over — so this is a cull fix and not a clamp on the value.
  SkPathBuilder b;
  b.moveTo(10, 60);
  b.lineTo(190, 60);
  const SkPath line = b.detach();
  EXPECT_FALSE(kitWave.shape(line).isEmpty());
}
