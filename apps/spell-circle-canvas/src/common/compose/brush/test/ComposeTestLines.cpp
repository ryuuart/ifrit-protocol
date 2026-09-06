// The line vocabulary -- lines, rails, hatches, routers and the marching
// dash -- with the stroke alignment and the decorations a node wears along
// its own outline.

#include <sigilcompose/brush/Hatches.h>
#include <sigilcompose/brush/Rails.h>

#include "support/BrushTestSupport.h"

TEST(ComposeStroke, StrokeAlignInnerAndOuter) {
  auto boxWith = [](PathFormat::Align align) {
    return box().child(box()
                           .absolute()
                           .inset(50, 50, 50, 50)
                           .stroke(stroke(20, green(), align)));
  };
  Host inner, outer;
  inner.composer.render(boxWith(PathFormat::Align::Inner));
  outer.composer.render(boxWith(PathFormat::Align::Outer));
  inner.frame();
  outer.frame();
  // Box edge at x=50 (spans 50..150), sampled at mid-height.
  EXPECT_EQ(inner.pixel(60, 100), SK_ColorGREEN);  // inside band
  EXPECT_EQ(inner.pixel(42, 100), SK_ColorBLACK);  // nothing outside
  EXPECT_EQ(outer.pixel(42, 100), SK_ColorGREEN);  // outside band
  EXPECT_EQ(outer.pixel(60, 100), SK_ColorBLACK);  // nothing inside
  // The outer band survives the cached replay (bleed declared).
  outer.frame();
  EXPECT_EQ(outer.pixel(42, 100), SK_ColorGREEN);
}

TEST(ComposeDecorations, BoundShadowOffsetSlides) {
  Host host;
  choreograph::Output<float> lift{0.0f};
  Shadow shadow;
  shadow.color = {0, 1, 0, 1};
  shadow.bindOffsetX = &lift;
  shadow.maxBind = 40.0f;
  host.composer.render(
      box().child(box().absolute().inset(60, 60, 80, 80).background(shadow)));
  host.frame();
  EXPECT_EQ(host.pixel(90, 90), SK_ColorGREEN);   // at rest: under the box
  EXPECT_EQ(host.pixel(135, 90), SK_ColorBLACK);  // nothing to the right
  lift = 30.0f;  // slide the shadow — no render()
  host.frame();
  EXPECT_EQ(host.pixel(135, 90), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(65, 90), SK_ColorBLACK);
}

TEST(ComposeDecorations, KnockoutShadowLeavesTheFootprintClear) {
  Host host;
  Shadow s;
  s.color = {0, 1, 0, 1};
  s.offset = {20, 0};
  s.knockout = true;
  host.composer.render(
      box().child(box().absolute().inset(60, 60, 80, 80).background(s)));
  host.frame();
  EXPECT_EQ(host.pixel(130, 90), SK_ColorGREEN);  // shadow right of the box
  EXPECT_EQ(host.pixel(100, 90), SK_ColorBLACK);  // footprint knocked out
}

TEST(ComposeDecorations, StrokeTrimWindowMarchesPerDecoration) {
  // One node: full static band + a bound marching sliver — no overlay box.
  Host host;
  choreograph::Output<float> phase{0.0f};
  PathFormat band;
  band.width = 4;
  band.strokeFill = green();
  PathFormat sliver;
  sliver.width = 8;
  sliver.strokeFill = red();
  sliver.trimStart = 0.0f;
  sliver.trimEnd = 0.1f;
  sliver.trimPhase = &phase;
  host.composer.render(box().child(
      box().absolute().inset(50, 50, 50, 50).stroke(band).stroke(sliver)));
  host.frame();
  std::vector<SkIPoint> redNow;
  int greenCount = 0;
  for (int y = 40; y < 160; y += 2)
    for (int x = 40; x < 160; x += 2) {
      if (host.pixel(x, y) == SK_ColorRED) redNow.push_back({x, y});
      greenCount += host.pixel(x, y) == SK_ColorGREEN;
    }
  ASSERT_GT(redNow.size(), 4u);  // the sliver painted
  ASSERT_GT(greenCount, 50);     // the band painted everywhere else
  phase = 0.5f;                  // march — no render()
  host.frame();
  int still = 0;
  for (const SkIPoint& p : redNow)
    still += host.pixel(p.x(), p.y()) == SK_ColorRED;
  EXPECT_LT((float)still, 0.25f * (float)redNow.size());
}

// ---------------------------------------------------------------------------
// Line patterns (Lines.h) — the beyond-dashes stroke vocabulary.
TEST(ComposeLines, TripleRailStrokesThreeBands) {
  Host host;
  host.composer.render(
      straightRun(lines::presets::triple(2, green(), 8, 1.0f)));
  host.frame();
  EXPECT_EQ(verticalRuns(host, 100, 70, 130, SK_ColorGREEN), 3);
  // And the pair variant gives exactly two.
  Host pair;
  pair.composer.render(straightRun(lines::presets::cased(2, green(), 8)));
  pair.frame();
  EXPECT_EQ(verticalRuns(pair, 100, 70, 130, SK_ColorGREEN), 2);
}

TEST(ComposeLines, ArrowheadFillsBeyondTheBodyWidth) {
  Host host, plain;
  host.composer.render(straightRun(lines::presets::arrow(2, green(), 14)));
  plain.composer.render(straightRun(lines::Line{.width = 2, .fill = green()}));
  host.frame();
  plain.frame();
  // The grounded convention (decorator/tldraw/D3 practice): the TIP sits
  // AT the endpoint (x=180) and the head extends BACKWARD over the run —
  // wings widen where the 2px plain body never paints.
  EXPECT_EQ(host.pixel(170, 96), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(170, 104), SK_ColorGREEN);
  EXPECT_EQ(plain.pixel(170, 96), SK_ColorBLACK);
  // Nothing pokes past the endpoint in either version.
  EXPECT_EQ(host.pixel(184, 100), SK_ColorBLACK);
  EXPECT_EQ(plain.pixel(184, 100), SK_ColorBLACK);
}

TEST(ComposeLines, RailwayTiesCrossTheLine) {
  Host host;
  host.composer.render(
      straightRun(lines::presets::railway(2, green(), 20, 12)));
  host.frame();
  // A tie arm ~5px above the rail at the first sample (x = 20+10)…
  EXPECT_EQ(host.pixel(30, 95), SK_ColorGREEN);
  // …and clear rail between ties.
  EXPECT_EQ(host.pixel(40, 95), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(40, 100), SK_ColorGREEN);
}

TEST(ComposeLines, WavyRunLeavesTheAxis) {
  Host host, straight;
  host.composer.render(straightRun(lines::presets::wavy(2, green(), 8, 24)));
  straight.composer.render(
      straightRun(lines::Line{.width = 2, .fill = green()}));
  host.frame();
  straight.frame();
  int offAxis = 0, offAxisStraight = 0;
  for (int x = 30; x < 170; x += 2)
    for (int dy : {-7, 7}) {
      offAxis += host.pixel(x, 100 + dy) == SK_ColorGREEN;
      offAxisStraight += straight.pixel(x, 100 + dy) == SK_ColorGREEN;
    }
  EXPECT_GT(offAxis, 10);
  EXPECT_EQ(offAxisStraight, 0);
}

namespace {

/** An L-shaped run: right along the bottom, then up — one hard 90° corner
 *  at local (120, 120), absolute (140, 140). */
Element corneredRun(lines::Line style) {
  return box().child(box()
                         .absolute()
                         .inset(20, 20, 20, 20)
                         .shape([](SkSize s) {
                           SkPathBuilder b;
                           b.moveTo(0, 120);
                           b.lineTo(120, 120);
                           b.lineTo(120, 0);
                           return b.detach();
                         })
                         .stroke(std::move(style)));
}

}  // namespace

TEST(ComposeLines, ParallelJoinControlKeepsACornerSharp) {
  // parallels > 1 builds its rails from a stroke OUTLINE, so the join is
  // what decides whether a hard 90° jog in a cased wire stays a corner or
  // becomes a soft S-curve. It is DATA on the Line and part of its defaulted
  // equality, so it is recipe like every other field. The discriminator is
  // exact: a miter reaches the corner's outer point, a round join provably
  // never does.
  const auto cased = [](SkPaint::Join join) {
    return lines::Line{
        .width = 3, .fill = green(), .parallels = 2, .gap = 12, .join = join};
  };
  Host miter, round;
  miter.composer.render(corneredRun(cased(SkPaint::kMiter_Join)));
  round.composer.render(corneredRun(cased(SkPaint::kRound_Join)));
  miter.frame();
  round.frame();
  // The outer rail rides 6 px outside the corner at (140, 140). A miter
  // join carries it out to the diagonal point; a round join arcs at
  // radius 6 and cuts that tip off. So the miter covers strictly more of
  // the square around the corner, which is a shape claim rather than an
  // anti-aliased byte.
  const auto inkNearTheCorner = [](Host& host) {
    int n = 0;
    for (int y = 138; y <= 152; ++y)
      for (int x = 138; x <= 152; ++x) n += host.pixel(x, y) != SK_ColorBLACK;
    return n;
  };
  const int mitered = inkNearTheCorner(miter);
  EXPECT_GT(mitered, 0) << "neither rail reached the corner at all";
  EXPECT_GT(mitered, inkNearTheCorner(round))
      << "the round join covered as much of the corner as the miter did, "
         "so the join field is not reaching the rails";
}

TEST(ComposeLines, ConcentricPlacesARingAtAStatedRadius) {
  // The evenly-spaced form distributes rings out to the bounding box's
  // HALF-DIAGONAL, so on a circle() node the outermost ring lands at R·√2 —
  // outside the shape and clipped away, drawing nothing with no warning.
  // The stated-radii overload puts a circle exactly where it says. The
  // spaced form is kept here as the control that the trap is real.
  const auto ringNode = [](lines::RadialHatch hatch) {
    return box().child(box()
                           .absolute()
                           .inset(20, 20, 20, 20)
                           .shape(geometry::shapes::circle())
                           .stroke(std::move(hatch)));
  };
  Host stated, spaced;
  stated.composer.render(ringNode(
      lines::presets::concentric(green(), std::vector<float>{60.0f}, 2.0f)));
  spaced.composer.render(
      ringNode(lines::presets::concentric(green(), /*rings=*/1,
                                          /*width=*/2.0f)));
  stated.frame();
  spaced.frame();
  // The stated ring: radius 60 from the box centre (100, 100).
  EXPECT_NE(stated.pixel(160, 100), SK_ColorBLACK)
      << "the stated-radius ring is not at its stated radius";
  EXPECT_NE(stated.pixel(100, 160), SK_ColorBLACK);
  // The control: ONE evenly-spaced ring lands at the reach (the
  // half-diagonal, ~113 px) — entirely outside the R = 80 circle, so the
  // node draws nothing at all. That is the entry's trap, verbatim.
  int spacedInk = 0;
  for (int y = 0; y < 200; y += 2)
    for (int x = 0; x < 200; x += 2)
      spacedInk += spaced.pixel(x, y) != SK_ColorBLACK;
  EXPECT_EQ(spacedInk, 0)
      << "the evenly-spaced ring was expected to clip away on a circle() "
         "node — if this now draws, that limitation is gone and "
         "this control needs a rethink";
  // And the stated form is a comparable value: radii join the equality.
  EXPECT_TRUE(lines::presets::concentric(green(), std::vector<float>{60.0f}) ==
              lines::presets::concentric(green(), std::vector<float>{60.0f}));
  EXPECT_FALSE(lines::presets::concentric(green(), std::vector<float>{60.0f}) ==
               lines::presets::concentric(green(), std::vector<float>{61.0f}));
}

// ---------------------------------------------------------------------------
// lines::Rails — the parallel rule where every rail is its own line.
TEST(ComposeLines, RailsCarryPerRailWidthFillAndDash) {
  // `Line::parallels` shares ONE width, ONE fill and ONE dash across every
  // rail; its only per-rail knob (coreWidthFactor) reaches exactly the
  // centre rail and only when the count is odd. So heavy/hair/heavy in two
  // colours is inexpressible with Line, whatever the count.
  Host host;
  host.composer.render(straightRun(lines::rails({
      {.across = 10, .width = 6, .fill = green()},
      {.across = 0, .width = 2, .fill = red(), .dash = {6, 6}},
      {.across = -10, .width = 6, .fill = green()},
  })));
  host.frame();
  // Two heavy GREEN rails ten px either side of the route (y = 100)…
  EXPECT_EQ(host.pixel(100, 90), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(100, 110), SK_ColorGREEN);
  // …the heavy rails really are 6 px (±2 from centre still paints)…
  EXPECT_EQ(host.pixel(100, 92), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(100, 108), SK_ColorGREEN);
  // …and a thin RED core between them, in a DIFFERENT colour, dashed:
  // some x paint and some do not, which a shared-fill Line cannot do.
  int redOn = 0, redOff = 0;
  for (int x = 30; x < 170; ++x)
    (host.pixel(x, 100) == SK_ColorRED ? redOn : redOff)++;
  EXPECT_GT(redOn, 20);
  EXPECT_GT(redOff, 20);
  // The core is thin: 2 px, so ±3 from the route is clear of it.
  EXPECT_NE(host.pixel(100, 96), SK_ColorRED);
}

TEST(ComposeLines, RailsSpanAndBleedReportTheSetsReach) {
  const lines::Rails r = brush::presets::heavyHairHeavy(4, 1, green(), 6);
  EXPECT_FLOAT_EQ(r.span(), 12.0f);      // -6 → +6, centre to centre
  EXPECT_FLOAT_EQ(r.bleed(), 6 + 2.0f);  // outermost offset + half its width
  // Comparable, so a static rail set prunes without a memo.
  EXPECT_EQ(r, brush::presets::heavyHairHeavy(4, 1, green(), 6));
  EXPECT_NE(r, brush::presets::heavyHairHeavy(4, 1, green(), 7));
}

namespace {

/** Fraction of angles round a circle where BOTH the inner and the outer
 *  rail paint, or NEITHER does — i.e. how well their dashes stay in
 *  register. 1.0 is perfect registration. */
struct RailScan {
  double agreement = 0;  ///< fraction of angles where both rails agree
  int innerOn = 0;       ///< angles where the inner rail painted
  int outerOn = 0;
  int samples = 0;
};

/** Samples both rails at 720 angles. The predicate is COVERAGE-BASED, not
 *  exact colour: sampling a 3 px arc at integer pixel coordinates lands on
 *  anti-aliased pixels constantly, and an exact-colour test then scores
 *  them as unpainted, which reports a large disagreement for geometry that
 *  is exactly right — that measures the rasteriser, not the library.
 *  G > 128 puts the boundary at 50% coverage, which is symmetric between
 *  the two radii and therefore does not favour either rail. */
RailScan scanRails(Host& host, float cx, float cy, float rInner, float rOuter) {
  RailScan scan;
  int agree = 0;
  for (int i = 0; i < 720; ++i) {
    const double a = i * 3.14159265358979 / 360.0;
    auto hit = [&](float r) {
      return SkColorGetG(host.pixel((int)std::lround(cx + std::cos(a) * r),
                                    (int)std::lround(cy + std::sin(a) * r))) >
             128;
    };
    const bool in = hit(rInner), out = hit(rOuter);
    scan.innerOn += in;
    scan.outerOn += out;
    agree += in == out;
    ++scan.samples;
  }
  scan.agreement = scan.samples ? (double)agree / scan.samples : 0.0;
  return scan;
}

Element circleRun(Decoration style, float radius) {
  return box().child(box()
                         .absolute()
                         .inset(0, 0, 0, 0)
                         .shape([radius](SkSize s) {
                           SkPathBuilder b;
                           b.addCircle(s.width() / 2, s.height() / 2, radius);
                           return b.detach();
                         })
                         .stroke(std::move(style)));
}

}  // namespace

TEST(ComposeLines, RailsDashesStayRegisteredThroughCurvature) {
  // THE property this type exists to protect. A circle's outer rail is
  // 2*pi*(2*offset) px longer than its inner one — about 100 px here —
  // so dashing each rail on its OWN offset curve drifts them ~6 whole
  // periods apart by the time they close. Rails dashes the CENTRELINE
  // once and offsets the resulting dash segments, so both rails are
  // measured in one arc parameterisation and stay in register.
  Host host(300, 300);
  lines::Rails registered = lines::rails({
      {.across = 8, .width = 3, .fill = green(), .dash = {8, 8}},
      {.across = -8, .width = 3, .fill = green(), .dash = {8, 8}},
  });
  host.composer.render(circleRun(registered, 100));
  host.frame();
  const RailScan good = scanRails(host, 150, 150, 92, 108);

  // LIVENESS FIRST. Two SOLID rails agree at every angle, so an agreement
  // threshold on its own scores a perfect 1.0 on the exact failure this
  // test exists to catch — rails that lost their dashes entirely. Prove the
  // rails actually BREAK before believing anything about their phase.
  EXPECT_GT(good.innerOn, 100) << "inner rail painted nothing";
  EXPECT_LT(good.innerOn, good.samples - 100) << "inner rail is SOLID";
  EXPECT_GT(good.outerOn, 100) << "outer rail painted nothing";
  EXPECT_LT(good.outerOn, good.samples - 100) << "outer rail is SOLID";

  // The alternative an author reaches for without Rails: a Brush whose
  // layers each carry a shapers::Offset. Correct geometry, sheared phase —
  // which is what the agreement comparison below has to be able to tell
  // apart from the registered case.
  Host naive(300, 300);
  lines::Line dashed{.width = 3, .fill = green(), .dashIntervals = {8, 8}};
  Brush perLayer;
  perLayer.layer(dashed, {geometry::shapers::Offset{.px = 8, .step = 2}})
      .layer(dashed, {geometry::shapers::Offset{.px = -8, .step = 2}});
  naive.composer.render(circleRun(perLayer, 100));
  naive.frame();
  const RailScan sheared = scanRails(naive, 150, 150, 92, 108);
  EXPECT_GT(sheared.innerOn, 100);  // the comparison must be dashed too, or
  EXPECT_LT(sheared.innerOn, sheared.samples - 100);  // it proves nothing

  // The registered case cannot reach 1.0, and the reason is geometric
  // rather than a tolerance: the ROUND CAP is a fixed arc LENGTH, so it
  // subtends a larger ANGLE on the inner rail than on the outer one, and
  // that residual can never agree however exact the geometry is. So the
  // comparison here is RELATIVE -- registered beats per-rail dashing --
  // and the exact form of the claim is the sibling below, read off the
  // paths with no rasteriser in the way.
  EXPECT_GT(good.agreement, sheared.agreement)
      << "dashing each rail on its own offset curve registered as well as "
         "dashing the centreline once did";
}

TEST(ComposeLines, RailsDashGeometryIsAngleExact) {
  // The registration claim in its exact form, read off the PATHS with no
  // rasteriser in the way. This is the assertion that actually detects a
  // per-rail dashing scheme; the pixel test above is liveness plus a
  // relative comparison.
  //
  // A radial displacement preserves ANGLE: dash the centreline in arc-space
  // and push each dash along its normal, and both rails' dash endpoints sit
  // at identical angular positions whatever the radius difference. Dashing
  // each rail on its own offset contour instead cannot: the outer
  // circumference (2*pi*108) fits ~42 periods where the inner (2*pi*92)
  // fits ~36, so the counts alone diverge.
  SkPathBuilder cb;
  cb.addCircle(150, 150, 100);
  const std::vector<SkScalar> pattern = {8.0f, 8.0f};
  const SkPath dashed = lines::dashGeometry(
      cb.detach(), SkSpan(pattern.data(), pattern.size()), 0);
  auto spans = [](const SkPath& p) {
    std::vector<std::pair<double, double>> out;
    SkContourMeasureIter it(p, false);
    while (sk_sp<SkContourMeasure> c = it.next()) {
      SkPoint a, b;
      if (c->getPosTan(0, &a, nullptr) &&
          c->getPosTan(c->length(), &b, nullptr))
        out.emplace_back(std::atan2(a.y() - 150, a.x() - 150),
                         std::atan2(b.y() - 150, b.x() - 150));
    }
    return out;
  };
  const auto inner = spans(sigil::geometry::path::parallel(dashed, 8.0f, 2.0f));
  const auto outer =
      spans(sigil::geometry::path::parallel(dashed, -8.0f, 2.0f));
  ASSERT_GE(inner.size(), 30u) << "the centreline never dashed";
  ASSERT_EQ(inner.size(), outer.size())
      << "rails carry different dash COUNTS — they were dashed per-rail";
  double worst = 0;
  for (size_t i = 0; i < inner.size(); ++i)
    worst =
        std::max(worst, std::max(std::abs(inner[i].first - outer[i].first),
                                 std::abs(inner[i].second - outer[i].second)));
  // The residual here is float noise — scattered, not ramping round the
  // contour — so the bound is loose by a wide margin and still fails any
  // per-rail dashing scheme by orders of magnitude.
  EXPECT_LT(worst, 1e-3) << "worst endpoint angle mismatch " << worst << " rad";
}

TEST(ComposeLines, DashedParallelsOnLineActuallyDash) {
  // `Line`'s dashed-parallel branch must not build its dash geometry with a
  // FILL stroke rec: Skia's dash effect refuses one outright, and the
  // failure mode is silent: `lines::presets::cased(...)` with a dash pattern
  // paints two SOLID rails, which reads as a design choice rather than as a
  // dropped dash.
  Host host;
  lines::Line pair = lines::presets::cased(3, green(), 10);
  pair.dashIntervals = {8, 8};
  host.composer.render(straightRun(pair));
  host.frame();
  int on = 0, off = 0;
  for (int x = 30; x < 170; ++x)
    (host.pixel(x, 95) == SK_ColorGREEN ? on : off)++;
  EXPECT_GT(on, 20);
  EXPECT_GT(off, 20) << "the rail is solid — the dash was dropped";
  // Both rails, and in register with each other.
  int agree = 0;
  for (int x = 30; x < 170; ++x)
    agree += (host.pixel(x, 95) == SK_ColorGREEN) ==
             (host.pixel(x, 105) == SK_ColorGREEN);
  EXPECT_GT(agree, 130);
}

TEST(ComposeLines, RailsDashPhaseSlidesOneRailAgainstItsNeighbours) {
  // The counter-dashed strand: same pattern, half a period apart, so the
  // inner rail's marks fall in the outer rail's gaps.
  Host host;
  host.composer.render(straightRun(lines::rails({
      {.across = 6, .width = 3, .fill = green(), .dash = {8, 8}},
      {.across = -6,
       .width = 3,
       .fill = green(),
       .dash = {8, 8},
       .dashPhase = 8},
  })));
  host.frame();
  int opposed = 0, together = 0;
  for (int x = 30; x < 170; ++x)
    ((host.pixel(x, 94) == SK_ColorGREEN) !=
             (host.pixel(x, 106) == SK_ColorGREEN)
         ? opposed
         : together)++;
  EXPECT_GT(opposed, together);
}

TEST(ComposeLines, RailsCountIsArbitrary) {
  // Quad — one of the three counts asked for by name, and already
  // reachable through Line::parallels; nothing ever spelled it.
  Host host;
  host.composer.render(straightRun(lines::presets::quad(2, green(), 9)));
  host.frame();
  EXPECT_EQ(verticalRuns(host, 100, 70, 130, SK_ColorGREEN), 4);
  Host six;
  six.composer.render(straightRun(lines::presets::rails(6, 1.5f, green(), 7)));
  six.frame();
  EXPECT_EQ(verticalRuns(six, 100, 60, 140, SK_ColorGREEN), 6);
}

TEST(ComposeLines, DottedCoreKeepsTheCasingContinuous) {
  Host host;
  host.composer.render(
      straightRun(brush::presets::dottedCore(3, 2, green(), 8, 6)));
  host.frame();
  // Casing: solid the whole way along, both sides.
  for (int x = 40; x < 160; x += 10) {
    EXPECT_EQ(host.pixel(x, 92), SK_ColorGREEN) << "casing gap at x=" << x;
    EXPECT_EQ(host.pixel(x, 108), SK_ColorGREEN) << "casing gap at x=" << x;
  }
  // Core: dotted, so it breaks. Coverage, not exact colour — a dotted line
  // is a round cap on a zero-length dash, so each dot is a disc whose
  // diameter IS the core width, and a thin disc centred on a pixel BOUNDARY
  // never fully covers any pixel. An exact-colour test therefore reports
  // "no dots" for dots that are plainly there.
  int on = 0, off = 0;
  for (int x = 30; x < 170; ++x)
    (SkColorGetG(host.pixel(x, 100)) > 60 ? on : off)++;
  EXPECT_GT(on, 5);
  EXPECT_GT(off, 30);
}

// ---------------------------------------------------------------------------
// EdgeSlice equality — the adaptor that could never prune.
TEST(ComposeDecorations, EdgeSlicePrunesWhenUnchanged) {
  // EdgeSlice had no operator==, so every re-render compared it unequal
  // and re-recorded the subtree — redoing the edge extraction (a contour
  // walk with a binary search at each boundary) at frame rate for chrome
  // that never changed. Inset, the sibling adaptor beside it, always had
  // one.
  auto scene = [] {
    return box().child(box().width(100).height(100).fill(blue()).foreground(
        onEdges(geometry::path::Edge::Top | geometry::path::Edge::Left,
                stroke(8, Fill::color({1, 1, 1, 1})))));
  };
  Host host;
  host.composer.render(scene());
  host.frame();
  EXPECT_GT(host.composer.stats().picturesRecorded, 0u);  // cold
  host.composer.render(scene());
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);  // pruned
  // And it still compares UNEQUAL when the mask actually changes.
  host.composer.render(
      box().child(box().width(100).height(100).fill(blue()).foreground(
          onEdges(geometry::path::Edge::Bottom,
                  stroke(8, Fill::color({1, 1, 1, 1}))))));
  host.frame();
  EXPECT_GT(host.composer.stats().picturesRecorded, 0u);
}

namespace {

/** The path's line-verb skeleton: every on-curve point in order, with the
 *  verb census alongside — the geometry assertions below read this. */
struct PathDump {
  std::vector<SkPoint> pts;
  int moves = 0, lines = 0, curves = 0, closes = 0;
};

PathDump dumpPath(const SkPath& p) {
  PathDump d;
  SkPath::Iter it(p, false);
  SkPoint v[4];
  for (SkPath::Verb verb; (verb = it.next(v)) != SkPath::kDone_Verb;) {
    switch (verb) {
      case SkPath::kMove_Verb:
        ++d.moves;
        d.pts.push_back(v[0]);
        break;
      case SkPath::kLine_Verb:
        ++d.lines;
        d.pts.push_back(v[1]);
        break;
      case SkPath::kQuad_Verb:
      case SkPath::kConic_Verb:
        ++d.curves;
        d.pts.push_back(v[2]);
        break;
      case SkPath::kCubic_Verb:
        ++d.curves;
        d.pts.push_back(v[3]);
        break;
      case SkPath::kClose_Verb:
        ++d.closes;
        break;
      default:
        break;
    }
  }
  return d;
}

}  // namespace

TEST(ComposeRouters, ChamferCutsTheCornerRoundingCannot) {
  const SkPoint run[2] = {{20, 20}, {180, 160}};
  // An 8 px chamfer on the HFirst L: the corner vertex (180,20) is
  // REPLACED by the two cut points 8 px along each leg — the 45° face.
  PathDump cut = dumpPath(
      routers::manhattan(routers::Bend::HFirst, 0.0f, 8.0f)(std::span(run, 2)));
  ASSERT_EQ(cut.pts.size(), 4u);
  EXPECT_EQ(cut.pts[1], SkPoint::Make(172, 20));
  EXPECT_EQ(cut.pts[2], SkPoint::Make(180, 28));
  EXPECT_EQ(cut.curves, 0);  // a cut is a line, never an arc
  for (const SkPoint& p : cut.pts)
    EXPECT_NE(p, SkPoint::Make(180, 20));  // the vertex itself is gone
  // cornerRadius on the same route rounds with curve verbs — the two
  // treatments are distinct mechanisms, not one effect at two settings.
  PathDump round = dumpPath(
      routers::manhattan(routers::Bend::HFirst, 8.0f)(std::span(run, 2)));
  EXPECT_GT(round.curves, 0);
}

TEST(ComposeRouters, ManhattanCasedRailMatchesCleanGeometry) {
  // A cased brush builds its rails from an offset CONTOUR, which is where
  // stray geometry flares into visible artefacts. So the check is that a
  // cased brush over a manhattan route renders byte-identically to the same
  // brush over hand-authored clean geometry: nothing the router emits — no
  // degenerate verb, no split run — reaches the pixels.
  auto boxes = [](Element route) {
    return stack()
        .child(box()
                   .key("a")
                   .width(20)
                   .height(20)
                   .inset(10, 90, 170, 90)
                   .absolute()
                   .fill(red()))
        .child(box()
                   .key("b")
                   .width(20)
                   .height(20)
                   .inset(170, 90, 10, 90)
                   .absolute()
                   .fill(green()))
        .child(std::move(route));
  };
  Decoration wire = lines::presets::cased(3, Fill::color({1, 1, 1, 1}), 10);
  Host railed, clean;
  railed.composer.render(
      boxes(rail({{"a"}, {"b"}}, routers::manhattan()).inset(0).stroke(wire)));
  clean.composer.render(boxes(box()
                                  .absolute()
                                  .inset(0)
                                  .shape([](SkSize) {
                                    SkPathBuilder b;
                                    b.moveTo(20, 100);
                                    b.lineTo(180, 100);
                                    return b.detach();
                                  })
                                  .stroke(wire)));
  railed.frame();
  clean.frame();
  EXPECT_TRUE(identicalPixels(railed, clean, 200, 200))
      << "the manhattan rail's cased brush differs from clean geometry";
}
