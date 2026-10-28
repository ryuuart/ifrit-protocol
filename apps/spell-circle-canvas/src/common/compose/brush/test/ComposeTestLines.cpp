// The line vocabulary -- lines, rails, hatches, routers and the marching
// dash -- with the stroke alignment and the decorations a node wears along
// its own outline.

#include <sigilcompose/brush/Hatches.h>
#include <sigilcompose/brush/Rails.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilgeometry/path/Contour.h>

#include "support/BrushTestSupport.h"
#include "support/PathDump.h"
#include "support/Rows.h"

TEST(ComposeStrokeLines, AKnockoutShadowLeavesTheFootprintClear) {
  Host host;
  Shadow s;
  s.color = Fill::color({0, 1, 0, 1});
  s.offset = {20, 0};
  s.knockout = true;
  host.composer.render(
      box().children({box().absolute().inset(60, 80, 80, 60).background(s)}));
  host.frame();
  EXPECT_EQ(host.pixel(130, 90), SK_ColorGREEN);  // shadow right of the box
  EXPECT_EQ(host.pixel(100, 90), SK_ColorBLACK);  // footprint knocked out
}

TEST(ComposeStrokeLines, ABoundInputMovesTheMarkWithoutADescribe) {
  sigil::compose::test::checkRows({
      {"ABoundShadowOffsetSlides",
       [&] {
         Host host;
         sigil::motion::Animatable<float> lift =
             sigil::motion::animatable(0.0f);
         Shadow shadow;
         shadow.color = Fill::color({0, 1, 0, 1});
         shadow.bindOffsetX = lift;
         shadow.maximumBind = 40.0f;
         host.composer.render(box().children(
             {box().absolute().inset(60, 80, 80, 60).background(shadow)}));
         host.frame();
         EXPECT_EQ(host.pixel(90, 90),
                   SK_ColorGREEN);  // at rest: under the box
         EXPECT_EQ(host.pixel(135, 90), SK_ColorBLACK);  // nothing to the right
         lift = 30.0f;  // slide the shadow — no render()
         host.frame();
         EXPECT_EQ(host.pixel(135, 90), SK_ColorGREEN);
         EXPECT_EQ(host.pixel(65, 90), SK_ColorBLACK);
       }},
      {"ATrimWindowMarchesPerDecoration",
       [&] {
         // One node: full static band + a bound marching sliver — no overlay
         // box.
         Host host;
         sigil::motion::Animatable<float> phase =
             sigil::motion::animatable(0.0f);
         PathFormat band;
         band.width = 4;
         band.strokeFill = green();
         PathFormat sliver;
         sliver.width = 8;
         sliver.strokeFill = red();
         sliver.trimStart = 0.0f;
         sliver.trimEnd = 0.1f;
         sliver.trimPhase = phase;
         host.composer.render(box().children(
             {box().absolute().inset(50).stroke(band).stroke(sliver)}));
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
       }},
  });
}

// ---------------------------------------------------------------------------

namespace {

/** An L-shaped run: right along the bottom, then up — one hard 90° corner
 *  at local (120, 120), absolute (140, 140). */
Element corneredRun(lines::Line style) {
  return box().children({box()
                             .absolute()
                             .inset(20)
                             .shape(skiaShape([] {
                               SkPathBuilder b;
                               b.moveTo(0, 120);
                               b.lineTo(120, 120);
                               b.lineTo(120, 0);
                               return b.detach();
                             }))
                             .stroke(std::move(style))});
}

}  // namespace

TEST(ComposeLines, ALinePaintsEveryFeatureItsFieldsState) {
  sigil::compose::test::checkRows({
      // Line patterns (Lines.h) — the beyond-dashes stroke vocabulary.
      {"TripleAndCasedParallels",
       [&] {
         Host host;
         host.composer.render(
             straightRun(lines::presets::triple(2, green(), 8, 1.0f)));
         host.frame();
         EXPECT_EQ(verticalRuns(host, 100, 70, 130, SK_ColorGREEN), 3);
         // And the pair variant gives exactly two.
         Host pair;
         pair.composer.render(
             straightRun(lines::presets::cased(2, green(), 8)));
         pair.frame();
         EXPECT_EQ(verticalRuns(pair, 100, 70, 130, SK_ColorGREEN), 2);
       }},
      {"AConcentricRingSitsAtItsStatedRadius",
       [&] {
         // The evenly-spaced form distributes rings out to the bounding box's
         // HALF-DIAGONAL, so on a circle() node the outermost ring lands at
         // R·√2 — outside the shape and clipped away, drawing nothing with no
         // warning. The stated-radii overload puts a circle exactly where it
         // says. The spaced form is kept here as the control that the trap is
         // real.
         const auto ringNode = [](lines::RadialHatch hatch) {
           return box().children({box()
                                      .absolute()
                                      .inset(20)
                                      .shape(geometry::shapes::circle())
                                      .stroke(std::move(hatch))});
         };
         Host stated, spaced;
         stated.composer.render(ringNode(lines::presets::concentric(
             green(), std::vector<float>{60.0f}, 2.0f)));
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
             << "the evenly-spaced ring was expected to clip away on a "
                "circle() "
                "node — if this now draws, that limitation is gone and "
                "this control needs a rethink";
         // And the stated form is a comparable value: radii join the equality.
         EXPECT_TRUE(
             lines::presets::concentric(green(), std::vector<float>{60.0f}) ==
             lines::presets::concentric(green(), std::vector<float>{60.0f}));
         EXPECT_FALSE(
             lines::presets::concentric(green(), std::vector<float>{60.0f}) ==
             lines::presets::concentric(green(), std::vector<float>{61.0f}));
       }},
  });
}

// ---------------------------------------------------------------------------

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
  return box().children({box()
                             .absolute()
                             .inset(0)
                             .shape(skiaShape([radius](SkSize s) {
                               SkPathBuilder b;
                               b.addCircle(s.width() / 2, s.height() / 2,
                                           radius);
                               return b.detach();
                             }))
                             .stroke(std::move(style))});
}

}  // namespace

TEST(ComposeLines, RailsCarryEachRailsOwnStrokeInRegisterWithTheCentreline) {
  sigil::compose::test::checkRows({
      {"SpanAndBleedReportTheSetsReach",
       [&] {
         const lines::Rails r =
             brush::presets::heavyHairHeavy(4, 1, green(), 6);
         EXPECT_FLOAT_EQ(r.span(), 12.0f);  // -6 → +6, centre to centre
         EXPECT_FLOAT_EQ(r.bleed(),
                         6 + 2.0f);  // outermost offset + half its width
         // Comparable, so a static rail set prunes without a memo.
         EXPECT_EQ(r, brush::presets::heavyHairHeavy(4, 1, green(), 6));
         EXPECT_NE(r, brush::presets::heavyHairHeavy(4, 1, green(), 7));
       }},
      {"DashesStayRegisteredThroughCurvature",
       [&] {
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

         // LIVENESS FIRST. Two SOLID rails agree at every angle, so an
         // agreement threshold on its own scores a perfect 1.0 on the exact
         // failure this test exists to catch — rails that lost their dashes
         // entirely. Prove the rails actually BREAK before believing anything
         // about their phase.
         EXPECT_GT(good.innerOn, 100) << "inner rail painted nothing";
         EXPECT_LT(good.innerOn, good.samples - 100) << "inner rail is SOLID";
         EXPECT_GT(good.outerOn, 100) << "outer rail painted nothing";
         EXPECT_LT(good.outerOn, good.samples - 100) << "outer rail is SOLID";

         // The alternative an author reaches for without Rails: a Brush whose
         // layers each carry a shapers::Offset. Correct geometry, sheared phase
         // — which is what the agreement comparison below has to be able to
         // tell apart from the registered case.
         Host naive(300, 300);
         lines::Line dashed{
             .width = 3, .fill = green(), .dashIntervals = {8, 8}};
         Brush perLayer;
         perLayer
             .layer(dashed, {geometry::shapers::Offset{.pixels = 8, .step = 2}})
             .layer(dashed,
                    {geometry::shapers::Offset{.pixels = -8, .step = 2}});
         naive.composer.render(circleRun(perLayer, 100));
         naive.frame();
         const RailScan sheared = scanRails(naive, 150, 150, 92, 108);
         EXPECT_GT(sheared.innerOn,
                   100);  // the comparison must be dashed too, or
         EXPECT_LT(sheared.innerOn,
                   sheared.samples - 100);  // it proves nothing

         // The registered case cannot reach 1.0, and the reason is geometric
         // rather than a tolerance: the ROUND CAP is a fixed arc LENGTH, so it
         // subtends a larger ANGLE on the inner rail than on the outer one, and
         // that residual can never agree however exact the geometry is. So the
         // comparison here is RELATIVE -- registered beats per-rail dashing --
         // and the exact form of the claim is the sibling below, read off the
         // paths with no rasteriser in the way.
         EXPECT_GT(good.agreement, sheared.agreement)
             << "dashing each rail on its own offset curve registered as well "
                "as "
                "dashing the centreline once did";
       }},
      {"DashGeometryIsAngleExact",
       [&] {
         // The registration claim in its exact form, read off the PATHS with no
         // rasteriser in the way. This is the assertion that actually detects a
         // per-rail dashing scheme; the pixel test above is liveness plus a
         // relative comparison.
         //
         // A radial displacement preserves ANGLE: dash the centreline in
         // arc-space and push each dash along its normal, and both rails' dash
         // endpoints sit at identical angular positions whatever the radius
         // difference. Dashing each rail on its own offset contour instead
         // cannot: the outer circumference (2*pi*108) fits ~42 periods where
         // the inner (2*pi*92) fits ~36, so the counts alone diverge.
         SkPathBuilder cb;
         cb.addCircle(150, 150, 100);
         const std::vector<SkScalar> pattern = {8.0f, 8.0f};
         const SkPath dashed = geometry::path::toSk(lines::dashGeometry(
             geometry::path::fromSk(cb.detach()), pattern, 0));
         auto spans = [](const SkPath& p) {
           std::vector<std::pair<double, double>> out;
           for (const sigil::geometry::path::Contour& c :
                sigil::geometry::path::contoursOf(p)) {
             const auto a = c.at(0);
             const auto b = c.at(c.length());
             if (a && b)
               out.emplace_back(
                   std::atan2(a->position.y - 150, a->position.x - 150),
                   std::atan2(b->position.y - 150, b->position.x - 150));
           }
           return out;
         };
         const auto inner =
             spans(sigil::geometry::path::parallel(dashed, 8.0f, 2.0f));
         const auto outer =
             spans(sigil::geometry::path::parallel(dashed, -8.0f, 2.0f));
         ASSERT_GE(inner.size(), 30u) << "the centreline never dashed";
         ASSERT_EQ(inner.size(), outer.size())
             << "rails carry different dash COUNTS — they were dashed per-rail";
         double worst = 0;
         for (size_t i = 0; i < inner.size(); ++i)
           worst = std::max(
               worst, std::max(std::abs(inner[i].first - outer[i].first),
                               std::abs(inner[i].second - outer[i].second)));
         // The residual here is float noise — scattered, not ramping round the
         // contour — so the bound is loose by a wide margin and still fails any
         // per-rail dashing scheme by orders of magnitude.
         EXPECT_LT(worst, 1e-3)
             << "worst endpoint angle mismatch " << worst << " rad";
       }},
      {"ADashPhaseSlidesOneRail",
       [&] {
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
       }},
      {"AnyRailCount",
       [&] {
         // Quad — one of the three counts asked for by name, and already
         // reachable through Line::parallels; nothing ever spelled it.
         Host host;
         host.composer.render(straightRun(lines::presets::quad(2, green(), 9)));
         host.frame();
         EXPECT_EQ(verticalRuns(host, 100, 70, 130, SK_ColorGREEN), 4);
         Host six;
         six.composer.render(
             straightRun(lines::presets::rails(6, 1.5f, green(), 7)));
         six.frame();
         EXPECT_EQ(verticalRuns(six, 100, 60, 140, SK_ColorGREEN), 6);
       }},
  });
}

// ---------------------------------------------------------------------------
// EdgeSlice equality — the adaptor that could never prune.
TEST(ComposeStrokeLines, AnEqualEdgeSlicePrunesAndAChangedOnePatches) {
  // EdgeSlice had no operator==, so every re-render compared it unequal
  // and re-recorded the subtree — redoing the edge extraction (a contour
  // walk with a binary search at each boundary) at frame rate for chrome
  // that never changed. Inset, the sibling adaptor beside it, always had
  // one.
  auto scene = [] {
    return box().children({box().width(100).height(100).fill(blue()).foreground(
        onEdges(geometry::path::Edge::Top | geometry::path::Edge::Left,
                stroke(8, Fill::color({1, 1, 1, 1}))))});
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
      box().children({box().width(100).height(100).fill(blue()).foreground(
          onEdges(geometry::path::Edge::Bottom,
                  stroke(8, Fill::color({1, 1, 1, 1}))))}));
  host.frame();
  EXPECT_GT(host.composer.stats().picturesRecorded, 0u);
}

TEST(ComposeLineRouters,
     AManhattanRouteCutsOrRoundsItsCornersAndDrawsAsCleanGeometry) {
  sigil::compose::test::checkRows({
      {"AChamferCutsWithLinesWhereRoundingCurves",
       [&] {
         const glm::vec2 run[2] = {{20, 20}, {180, 160}};
         // An 8 px chamfer on the HFirst L: the corner vertex (180,20) is
         // REPLACED by the two cut points 8 px along each leg — the 45° face.
         PathDump cut = dumpPath(routers::manhattan(routers::Bend::HFirst, 0.0f,
                                                    8.0f)(std::span(run, 2)));
         ASSERT_EQ(cut.pts.size(), 4u);
         EXPECT_EQ(cut.pts[1], SkPoint::Make(172, 20));
         EXPECT_EQ(cut.pts[2], SkPoint::Make(180, 28));
         EXPECT_EQ(cut.curves, 0);  // a cut is a line, never an arc
         for (const SkPoint& p : cut.pts)
           EXPECT_NE(p, SkPoint::Make(180, 20));  // the vertex itself is gone
         // cornerRadius on the same route rounds with curve verbs — the two
         // treatments are distinct mechanisms, not one effect at two settings.
         PathDump round = dumpPath(routers::manhattan(routers::Bend::HFirst,
                                                      8.0f)(std::span(run, 2)));
         EXPECT_GT(round.curves, 0);
       }},
      {"ACasedWireDrawsAsCleanGeometry",
       [&] {
         // A cased brush builds its rails from an offset CONTOUR, which is
         // where stray geometry flares into visible artefacts. So the check is
         // that a cased brush over a manhattan route renders byte-identically
         // to the same brush over hand-authored clean geometry: nothing the
         // router emits — no degenerate verb, no split run — reaches the
         // pixels.
         const auto stations = [] {
           return stack().children({box()
                                        .key("a")
                                        .width(20)
                                        .height(20)
                                        .inset(90, 170, 90, 10)
                                        .absolute()
                                        .fill(red()),
                                    box()
                                        .key("b")
                                        .width(20)
                                        .height(20)
                                        .inset(90, 10, 90, 170)
                                        .absolute()
                                        .fill(green())});
         };
         Decoration wire =
             lines::presets::cased(3, Fill::color({1, 1, 1, 1}), 10);
         Host routed, clean;
         routed.composer.render(stations().operators(
             {connect::Along{.stops = {{"a"}, {"b"}},
                             .router = routers::manhattan(),
                             .wire = wire}}));
         clean.composer.render(stations().children({box()
                                                        .absolute()
                                                        .inset(0)
                                                        .shape(skiaShape([] {
                                                          SkPathBuilder b;
                                                          b.moveTo(20, 100);
                                                          b.lineTo(180, 100);
                                                          return b.detach();
                                                        }))
                                                        .stroke(wire)}));
         routed.frame();
         clean.frame();
         EXPECT_TRUE(identicalPixels(routed, clean, 200, 200))
             << "the manhattan wire's cased brush differs from clean geometry";
       }},
  });
}
