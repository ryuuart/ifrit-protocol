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
// The last case is a compile-only spelling of every documented signature,
// after the ComposeDocs pattern: it asserts nothing and exists so that a
// signature change breaks the build instead of quietly invalidating the
// documentation.

#include <sigilcompose/kit/Kit.h>

#include <sigilweave/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <include/core/SkContourMeasure.h>
#include <include/core/SkFont.h>
#include <include/core/SkPath.h>
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
 *  contour in order — which is what TextPath does (Compose.h:554-558). */
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
  // sigillum_aemeth.cpp:273 — P(thDeg, rNorm), 0 at 12 o'clock, clockwise.
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
  // THE case. sigillum_aemeth.cpp:279 hand-derived
  //     frac(th) = fmod((th - 90)/360 + 4, 1)
  // with a comment explaining that shapes::circle()'s contour starts due
  // east. This asserts the claim against the path Shapes.h actually
  // builds, so it fails if that default ever changes — which is exactly
  // the failure the comment could not catch.
  const SkSize size{200, 200};
  const SkPath circle = shapes::circle()(size);
  const kit::Frame f{.centre = {100, 100}, .radius = 100};
  for (float th : {0.0f, 45.0f, 90.0f, 137.0f, 180.0f, 300.0f})
    EXPECT_TRUE(near(atFraction(circle, f.fraction(th)), f.at(th, 1.0f), 0.25f))
        << "th=" << th;
}

TEST(KitFrame, TheBaselinesDirectionIsNotTheFramesSense) {
  // Measured, and it inverted the first draft: shapes::circle(kCCW) still
  // STARTS due east (startIndex is 1 either way) and then runs the other
  // way, so f = 0.25 is 12 o'clock where the kCW contour is at 6. The
  // frame's own `sense` says nothing about which way the path was wound,
  // so fraction() takes the baseline separately.
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
  // vagrant_story_target.cpp:216-218 carries a 4 px geometry grid and a
  // 2.5 px text grid at the same time. A free function cannot.
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
  // ds2_bench.cpp:255-258 — k = (i%3==0) ? 1.0 : (i%3==1 ? 0.86 : 0.93).
  // No long/short pair expresses that, which is why `classify` exists.
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
  // The whole reason this exists: TextPath walks every contour in order as
  // ONE arc-length coordinate, so 49 letters around a heptagon can be one
  // text run. Measured against the path, not against the formula.
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
  // {6/2}: gcd 2, so the hexagram is TWO triangles — the correct answer,
  // and thaumonomicon.cpp:1331 draws exactly that.
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
  // thaumonomicon.cpp:1172-1179: +2 ended the surface INSIDE the final
  // letter, at every size, because measure() returns the ADVANCE and the
  // last glyph's ink can sit outside it. The assertion is that with the
  // default pad the ink never reaches the right edge of the plane — i.e.
  // nothing was cut off.
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
  // The positive control for the case above, and the first draft of it was
  // WRONG: it used Menlo, whose ink never leaves its advance because a
  // monospace face is boxed by construction, so "pad 0 clips" was false and
  // the pad test above was proving nothing.
  //
  // Measured across faces: an italic or script face overhangs at almost
  // every size and string — Helvetica Italic "Wf" at 12 px has ink to
  // exactly its advance, Zapfino overhangs on all five probe strings. So
  // the control uses those, requires at least one to reach the edge at
  // pad 0, and requires the SAME string to be clear of it at the default.
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
      // coverage() grows the pad until nothing touches an edge. A single
      // px is enough to arm the retry, and Zapfino needs more than the
      // default 8 — which is the finding: no fixed pad is correct, and
      // that is why the component retries instead of documenting a number.
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
  // custom() leaf cannot reach addUnderlay. DIFFERENTIAL, because counting
  // absolute pixels was the first draft and it asserted the wrong thing:
  // at 20 px with a 3 px stroke the glyph interiors outnumber the ring, so
  // "more halo than ink" is simply false. What IS true is that the halo
  // colour appears only when a halo is asked for.
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
// build if a signature in any kit header changes. (The ComposeDocs pattern,
// invented in this program: a doc comment that does not compile is a lie
// nobody notices.)

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
