#include "support/BrushTestSupport.h"

// ---------------------------------------------------------------------------
// The corner vocabulary — bracket marks, open-corner rules, weighted
// corners, insets.
//
// Brackets and gapped rules are not decoration kinds of their own. Both are
// a span CLAIM on the node's real boundary — `.stroke(spans::corners(arm),
// …)` and `.stroke(spans::edges(arm), …)` — which is why they follow
// whatever silhouette the node has instead of assuming a rectangle. The
// Border modes are the separate family, covered through border(),
// weightedCorners() and doubleBorder().

namespace {
Fill white() { return Fill::color({1, 1, 1, 1}); }
/** A 100x100 blue panel with `dec` as its foreground, in a 200x200 host.
 *  The panel sits at (0,0), so pixel(50, 1) is the middle of its top edge
 *  and pixel(10, 1) is 10 px along from its top-left corner. */
Element panel(Decoration dec) {
  return box().child(
      box().width(100).height(100).fill(blue()).foreground(std::move(dec)));
}
/** The same panel with a span-qualified stroke pass, which is how the
 *  corner vocabulary is spelled. */
Element spanPanel(Spans where, Decoration dec) {
  return box().child(box().width(100).height(100).fill(blue()).stroke(
      std::move(where), std::move(dec)));
}
Element shapedSpanPanel(std::function<SkPath(SkSize)> outline, Spans where,
                        Decoration dec) {
  return box().child(box()
                         .width(100)
                         .height(100)
                         .shape(std::move(outline))
                         .fill(blue())
                         .stroke(std::move(where), std::move(dec)));
}
Element shapedPanel(std::function<SkPath(SkSize)> outline, Decoration dec) {
  return box().child(box()
                         .width(100)
                         .height(100)
                         .shape(std::move(outline))
                         .fill(blue())
                         .foreground(std::move(dec)));
}
}  // namespace

TEST(ComposeBorders, BracketsPaintOnlyNearTheCorners) {
  // Four L-shaped marks and nothing else — one node, one pass. The
  // alternative an author reaches for is four absolutely-placed Elements,
  // which costs four nodes and stops tracking the silhouette the moment it
  // is not a rectangle.
  Host host;
  host.composer.render(spanPanel(spans::corners(20), brush::solid(6, white())));
  host.frame();
  EXPECT_EQ(host.pixel(10, 1), SK_ColorWHITE);   // along the top, near a corner
  EXPECT_EQ(host.pixel(1, 10), SK_ColorWHITE);   // and down the left of it
  EXPECT_EQ(host.pixel(90, 98), SK_ColorWHITE);  // bottom-right bracket
  EXPECT_EQ(host.pixel(50, 1), SK_ColorBLUE);    // mid-run: bare
  EXPECT_EQ(host.pixel(98, 50), SK_ColorBLUE);
}

TEST(ComposeBorders, GappedRuleIsTheExactComplement) {
  Host host;
  host.composer.render(spanPanel(spans::edges(20), brush::solid(6, white())));
  host.frame();
  EXPECT_EQ(host.pixel(50, 1), SK_ColorWHITE);  // mid-run: drawn
  EXPECT_EQ(host.pixel(98, 50), SK_ColorWHITE);
  EXPECT_EQ(host.pixel(10, 1), SK_ColorBLUE);  // corner: left open
  EXPECT_EQ(host.pixel(1, 10), SK_ColorBLUE);
}

TEST(ComposeBorders, BracketsFollowAChamferedSilhouette) {
  // The property four hand-placed corner Elements can never have: the
  // brackets land on whatever corners the SHAPE has. A 30 px chamfer turns
  // four corners into eight, and the chamfer face itself gets marked.
  Host host;
  host.composer.render(shapedSpanPanel(
      shapes::chamfered(30), spans::corners(10), brush::solid(6, white())));
  host.frame();
  // (74, 4) sits ON the top-right chamfer face, ~5 px from its upper end.
  EXPECT_EQ(host.pixel(74, 4), SK_ColorWHITE);
  // The chamfered top edge runs (30,0)->(70,0); its midpoint is 20 px from
  // either end corner, which is outside a 10 px arm: bare.
  EXPECT_EQ(host.pixel(50, 1), SK_ColorBLUE);
  // A plain rectangle has no corner there at all, so the same call leaves
  // that pixel untouched — which is what makes this a test about the SHAPE.
  Host plain;
  plain.composer.render(
      spanPanel(spans::corners(10), brush::solid(6, white())));
  plain.frame();
  EXPECT_EQ(plain.pixel(74, 4), SK_ColorBLUE);
}

TEST(ComposeBorders, ARoundedCornerIsNotACorner) {
  // The surprise worth pinning: corner spans are found at hard tangent
  // breaks, and a rounded rect has none. So a bracket set on one paints
  // NOTHING at all, and a gapped rule runs the whole way round without a
  // single gap. Neither reports an error.
  Host host;
  host.composer.render(box().child(
      box().width(100).height(100).corners({30}).fill(blue()).stroke(
          spans::corners(20), brush::solid(6, white()))));
  host.frame();
  EXPECT_EQ(host.pixel(50, 1), SK_ColorBLUE);
  EXPECT_EQ(host.pixel(1, 50), SK_ColorBLUE);
  EXPECT_EQ(host.pixel(9, 9), SK_ColorBLUE);  // on the corner arc itself

  Host gapped;
  gapped.composer.render(box().child(
      box().width(100).height(100).corners({30}).fill(blue()).stroke(
          spans::edges(20), brush::solid(6, white()))));
  gapped.frame();
  EXPECT_EQ(gapped.pixel(50, 1), SK_ColorWHITE);
  EXPECT_EQ(gapped.pixel(9, 9), SK_ColorWHITE);
}

TEST(ComposeBorders, InsetMovesTheRuleInsideTheSilhouette) {
  Host host;
  host.composer.render(panel(decorations::border(4, white(), 10)));
  host.frame();
  EXPECT_EQ(host.pixel(50, 10), SK_ColorWHITE);  // the rule, 10 px in
  EXPECT_EQ(host.pixel(50, 1), SK_ColorBLUE);    // the edge itself is bare
  EXPECT_EQ(host.pixel(10, 50), SK_ColorWHITE);
}

TEST(ComposeBorders, WeightedCornersThickenWhereTheRuleTurns) {
  // A border whose weight varies along its run: 2 px along the edges,
  // 8 px within 20 px of each corner.
  Host host;
  host.composer.render(panel(decorations::weightedCorners(2, 8, white(), 20)));
  host.frame();
  EXPECT_EQ(host.pixel(10, 3), SK_ColorWHITE);  // inside the heavy corner run
  EXPECT_EQ(host.pixel(50, 3), SK_ColorBLUE);   // the light run is only 2 px
  EXPECT_EQ(host.pixel(50, 0), SK_ColorWHITE);  // …but it IS drawn
}

TEST(ComposeBorders, DoubleBorderStacksTwoIndependentInsets) {
  Host host;
  host.composer.render(
      box().child(box().width(100).height(100).fill(blue()).style(
          decorations::doubleBorder(decorations::border(3, white()),
                                    decorations::border(3, white(), 12)))));
  host.frame();
  EXPECT_EQ(host.pixel(50, 0), SK_ColorWHITE);   // outer rule
  EXPECT_EQ(host.pixel(50, 12), SK_ColorWHITE);  // inner rule
  EXPECT_EQ(host.pixel(50, 6), SK_ColorBLUE);    // clear paper between them
}

// ---------------------------------------------------------------------------
// shapes::chamfered / shapes::notched — the two corner cuts the kernel's
// corners() (which only rounds) could not express.

TEST(ComposeShapes, ChamferCutsTheCornerAtFortyFiveDegrees) {
  Host host;
  host.composer.render(shapedPanel(shapes::chamfered(30), PathFormat{}));
  host.frame();
  EXPECT_EQ(host.pixel(5, 5), SK_ColorBLACK);   // corner cut away
  EXPECT_EQ(host.pixel(50, 50), SK_ColorBLUE);  // body intact
  EXPECT_EQ(host.pixel(5, 95), SK_ColorBLACK);  // all four by default
}

TEST(ComposeShapes, ChamferMaskCutsOnlyTheSelectedCorners) {
  Host host;
  host.composer.render(shapedPanel(
      shapes::chamfered(30, shapes::Corner::Diagonal), PathFormat{}));
  host.frame();
  EXPECT_EQ(host.pixel(5, 5), SK_ColorBLACK);    // top-left cut
  EXPECT_EQ(host.pixel(95, 95), SK_ColorBLACK);  // bottom-right cut
  EXPECT_EQ(host.pixel(95, 5), SK_ColorBLUE);    // top-right square
  EXPECT_EQ(host.pixel(5, 95), SK_ColorBLUE);    // bottom-left square
}

TEST(ComposeShapes, NotchBitesARectangleOutOfTheCorner) {
  Host host;
  host.composer.render(shapedPanel(
      shapes::notched(20, 10, shapes::Corner::TopLeft), PathFormat{}));
  host.frame();
  EXPECT_EQ(host.pixel(5, 5), SK_ColorBLACK);  // inside the bite
  EXPECT_EQ(host.pixel(5, 15), SK_ColorBLUE);  // below it
  EXPECT_EQ(host.pixel(25, 5), SK_ColorBLUE);  // right of it
  EXPECT_EQ(host.pixel(95, 5), SK_ColorBLUE);  // other corners untouched
}

// ---------------------------------------------------------------------------
// The Illustrator brush model: scatter/pattern/ribbon + the ops pipeline.

TEST(ComposeBrushes, ScatterInstancesArtAlongThePath) {
  Host host;
  host.composer.render(straightRun([] {
    brush::Scatter b;
    b.art = box().width(6).height(6).fill(red());
    b.spacing = 40;
    b.alignToPath = true;
    return b;
  }()));
  host.frame();
  // 160px run, spacing 40 → stamps at d = 20, 60, 100, 140 (x = 20+d).
  EXPECT_EQ(host.pixel(40, 100), SK_ColorRED);
  EXPECT_EQ(host.pixel(80, 100), SK_ColorRED);
  EXPECT_EQ(host.pixel(60, 100), SK_ColorBLACK);  // between stamps
}

TEST(ComposeBrushes, ScatterModSkipsAndLifts) {
  Host host;
  brush::Scatter b;
  b.art = box().width(6).height(6).fill(red());
  b.spacing = 40;
  b.mod = [](const PathSample&, size_t i, size_t) {
    brush::StampMod m;
    if (i % 2)
      m.skip = true;  // drop every other slot
    else
      m.dNormal = -20;  // lift the kept ones off the axis
    return m;
  };
  host.composer.render(straightRun(std::move(b)));
  host.frame();
  EXPECT_EQ(host.pixel(40, 80), SK_ColorRED);    // slot 0 lifted (d=20)
  EXPECT_EQ(host.pixel(80, 80), SK_ColorBLACK);  // slot 1 skipped (d=60)
  EXPECT_EQ(host.pixel(80, 100), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(120, 80), SK_ColorRED);  // slot 2 lifted (d=100)
}

TEST(ComposeBrushes, PatternIntegerFitNeverTearsTheLastTile) {
  // 160px run, 25px tile → 6 slots stretched to 26.67px: coverage reaches
  // BOTH ends with no torn tail (the Illustrator fit rule).
  Host host;
  brush::Pattern b;
  b.side = box().width(25).height(8).fill(red());
  host.composer.render(straightRun(std::move(b)));
  host.frame();
  EXPECT_EQ(host.pixel(42, 100), SK_ColorRED);   // first slot starts at run 0
  EXPECT_EQ(host.pixel(178, 100), SK_ColorRED);  // last slot ends at run end
  EXPECT_EQ(host.pixel(100, 100), SK_ColorRED);  // continuous through middle
}

TEST(ComposeBrushes, PatternCornerTileSitsOnTheBend) {
  Host host;
  brush::Pattern b;
  b.side = box().width(20).height(4).fill(red());
  b.corner = brush::CornerArt{box().width(12).height(12).fill(blue()),
                              brush::CornerAlign::Bisector};
  host.composer.render(
      box().child(box()
                      .absolute()
                      .inset(40, 40, 40, 40)
                      .shape([](SkSize s) {  // an L: right then down
                        SkPathBuilder p;
                        p.moveTo(0, 0);
                        p.lineTo(s.width(), 0);
                        p.lineTo(s.width(), s.height());
                        return p.detach();
                      })
                      .stroke(std::move(b))));
  host.frame();
  EXPECT_EQ(host.pixel(160, 40), SK_ColorBLUE);  // corner tile at the bend
  EXPECT_EQ(host.pixel(100, 40), SK_ColorRED);   // side tiles on the top leg
  EXPECT_EQ(host.pixel(160, 100), SK_ColorRED);  // and down the right leg
}

TEST(ComposeBrushes, RibbonTapersAndNibVariesWithAngle) {
  Host taperHost;
  taperHost.composer.render(straightRun(brush::taper(16, 2, green())));
  taperHost.frame();
  auto bandHeight = [](Host& h, int x) {
    int lit = 0;
    for (int y = 70; y < 130; ++y) lit += h.pixel(x, y) == SK_ColorGREEN;
    return lit;
  };
  EXPECT_GT(bandHeight(taperHost, 30), 12);  // wide near the start
  EXPECT_LT(bandHeight(taperHost, 170), 6);  // narrow near the end

  // Calligraphic nib at 0°: a horizontal run lies ALONG the nib → thin.
  Host nib;
  nib.composer.render(straightRun(brush::calligraphic(0, 16, green(), 0.2f)));
  nib.frame();
  EXPECT_LT(bandHeight(nib, 100), 6);
}

TEST(ComposeBrushes, RestyleWavesAnyDecoration) {
  Host host;
  host.composer.render(straightRun(brush::restyle(
      kit::brush::shapers::Wave{8, 24}, util::stroke(2, green()), 12)));
  host.frame();
  int offAxis = 0;
  for (int x = 30; x < 170; x += 2)
    for (int dy : {-7, 7}) offAxis += host.pixel(x, 100 + dy) == SK_ColorGREEN;
  EXPECT_GT(offAxis, 10);  // the stroke followed the waved geometry
}

// ---------------------------------------------------------------------------
// Skia capabilities surfaced as compose values: sketchy jitter, SVG path
// data as an outline, and Perlin noise as a fill.

TEST(ComposeSeams, SketchyJitterLeavesTheAxis) {
  Host host, plain;
  host.composer.render(straightRun(brush::restyle(
      kit::brush::shapers::Jitter{8, 3.0f, 11}, util::stroke(2, green()))));
  plain.composer.render(straightRun(util::stroke(2, green())));
  host.frame();
  plain.frame();
  int off = 0, offPlain = 0;
  for (int x = 30; x < 170; x += 2)
    for (int dy : {-2, 2}) {
      off += host.pixel(x, 100 + dy) == SK_ColorGREEN;
      offPlain += plain.pixel(x, 100 + dy) == SK_ColorGREEN;
    }
  EXPECT_GT(off, 8);       // the hand wobbles
  EXPECT_EQ(offPlain, 0);  // the ruler doesn't
}

TEST(ComposeSeams, SvgOutlineTracesThePathData) {
  // A right triangle authored as an SVG d-string, stretched to the node.
  Host host;
  host.composer.render(
      box().child(box()
                      .absolute()
                      .inset(50, 50, 50, 50)
                      .shape(shapes::svg("M0 0 L100 0 L100 100 Z"))
                      .fill(red())));
  host.frame();
  EXPECT_EQ(host.pixel(140, 70), SK_ColorRED);    // inside the hypotenuse
  EXPECT_EQ(host.pixel(60, 130), SK_ColorBLACK);  // outside it
  // Hit-testing follows the silhouette too.
  host.composer.render(
      box().child(box()
                      .absolute()
                      .inset(50, 50, 50, 50)
                      .shape(shapes::svg("M0 0 L100 0 L100 100 Z"))
                      .fill(red())
                      .key("tri")));
  host.frame();
  EXPECT_EQ(host.composer.hitTest({140, 70}).value_or(""), "tri");
  EXPECT_FALSE(host.composer.hitTest({60, 130}).has_value());
}

TEST(ComposeSeams, PerlinNoiseFillsWithVariation) {
  Host host(100, 100);
  host.composer.render(box().child(
      box().width(100).height(100).fill(patterns::noise(0.05f, 4, 2.0f))));
  host.frame();
  std::set<SkColor> distinct;
  for (int y = 10; y < 90; y += 8)
    for (int x = 10; x < 90; x += 8) distinct.insert(host.pixel(x, y));
  EXPECT_GT(distinct.size(), 30u);  // organic variation, not a flat fill
}

// ---------------------------------------------------------------------------
// Echo misprints, stagger origin, quantized time.

TEST(ComposePaint, EchoStampsShapeUnderTheFill) {
  Host host;
  host.composer.render(box().child(box()
                                       .absolute()
                                       .inset(50, 50, 90, 90)
                                       .fill(red())
                                       .echo({10, 10}, {0, 1, 0, 1})));
  host.frame();
  EXPECT_EQ(host.pixel(80, 80), SK_ColorRED);      // real fill on top
  EXPECT_EQ(host.pixel(115, 115), SK_ColorGREEN);  // echo peeking past it
  EXPECT_EQ(host.pixel(45, 45), SK_ColorBLACK);    // nothing before either
  host.frame();  // survives the cached replay (cull grew by the offset)
  EXPECT_EQ(host.pixel(115, 115), SK_ColorGREEN);
}

TEST(ComposePaint, EchoesAppendSoRegistrationDoublingIsTwoCalls) {
  // echo() APPENDS — the node holds a vector of stamps, not one — so
  // registration doubling (a stamp each side of a glyph run rather than one
  // behind it) is two calls, not a missing feature. This pins that, and the
  // ordering: stamps paint in declaration order beneath the real pass.
  Host host;
  host.composer.render(box().child(box()
                                       .absolute()
                                       .inset(60, 60, 100, 100)
                                       .fill(red())
                                       .echo({-14, -14}, {0, 0, 1, 1})
                                       .echo({14, 14}, {0, 1, 0, 1})
                                       .echo({20, 20}, {1, 1, 0, 1})));
  host.frame();
  EXPECT_EQ(host.pixel(90, 90), SK_ColorRED);     // the real pass, on top
  EXPECT_EQ(host.pixel(50, 50), SK_ColorBLUE);    // one stamp up-left…
  EXPECT_EQ(host.pixel(76, 110), SK_ColorGREEN);  // …one down-right
  EXPECT_EQ(host.pixel(118, 118), SK_ColorYELLOW);
  // Declaration order is bottom-first, so the LAST echo wins where they
  // overlap — three stamps, not one, and they are ordered.
  EXPECT_EQ(host.pixel(110, 110), SK_ColorYELLOW);
}

TEST(ComposeText, EchoStampsTextUnderThePass) {
  Host host(300, 120);
  host.composer.render(box().padding(20).child(
      text(u8"ECHO", whiteStyle(48)).echo({6, -8}, {1, 0, 0, 1})));
  host.frame();
  int redCount = 0, whiteCount = 0;
  for (int y = 0; y < 120; y += 2)
    for (int x = 0; x < 300; x += 2) {
      redCount += host.pixel(x, y) == SK_ColorRED;
      whiteCount += host.pixel(x, y) == SK_ColorWHITE;
    }
  EXPECT_GT(whiteCount, 50);  // the real pass
  EXPECT_GT(redCount, 20);    // the misprint peeking out at (6,−8)
}

TEST(ComposeMotion, StaggerFromEndRunsBottomUp) {
  Host host;
  auto card = [] {
    return box().width(60).height(30).fill(red()).opacity(
        animate(from(0.0f).to(1.0f), {200ms, &choreograph::easeNone}));
  };
  host.composer.render(box()
                           .column()
                           .gap(10)
                           .staggerChildren(400ms, Stagger::From::End)
                           .child(card())
                           .child(card()));
  host.frame(0.3);  // LAST child leads; first still holds its `from`
  EXPECT_EQ(host.pixel(30, 15), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(30, 55), SK_ColorRED);
  host.frame(0.5);
  EXPECT_EQ(host.pixel(30, 15), SK_ColorRED);
}

TEST(ComposeMaterials, QuantizeTimeStepsTheClock) {
  // uTime → red channel; with quantizeTime(2) samples inside one half-
  // second step resolve identically, and differ across steps.
  auto [fx, err] = SkRuntimeEffect::MakeForShader(
      SkString("uniform float uTime; half4 main(float2 p) {"
               "  return half4(fract(uTime), 0, 0, 1); }"));
  ASSERT_TRUE(fx) << err.c_str();
  Material stepped = Material::sksl(fx).quantizeTime(2.0f);
  auto sampleAt = [&](Material& m, double seconds) {
    PaintContext ctx;
    ctx.size = {8, 8};
    ctx.elapsedSeconds = seconds;
    Fill f = m.resolve(ctx);
    sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(4, 4));
    SkPaint p;
    p.setShader(f.shaderValue);
    s->getCanvas()->drawPaint(p);
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
    s->readPixels(bm.pixmap(), 1, 1);
    return bm.getColor(0, 0);
  };
  EXPECT_EQ(sampleAt(stepped, 0.6), sampleAt(stepped, 0.9));  // same step
  EXPECT_NE(sampleAt(stepped, 0.6), sampleAt(stepped, 1.1));  // next step
  Material continuous = Material::sksl(fx);
  EXPECT_NE(sampleAt(continuous, 0.6), sampleAt(continuous, 0.9));
}

TEST(ComposeMotion, KeyframesPlayTheMountPath) {
  // A cursor overshoot: +40 → −20 → 0. Mid-path the box sits LEFT of rest,
  // which is the point — a single from→to ramp can never cross its own
  // resting value, so this shape only exists if waypoints really play.
  Host host;
  host.composer.render(box().child(
      box()
          .absolute()
          .inset(100, 80, 60, 80)
          .fill(red())
          .translateX(animate(through({{std::chrono::milliseconds(0), 40.0f},
                                       {std::chrono::milliseconds(200), -20.0f},
                                       {std::chrono::milliseconds(400), 0.0f}}),
                              &choreograph::easeNone))));
  host.frame();
  EXPECT_EQ(host.pixel(145, 100), SK_ColorRED);  // starts at +40
  EXPECT_EQ(host.pixel(105, 100), SK_ColorBLACK);
  host.frame(0.2);  // waypoint 2: x = −20
  EXPECT_EQ(host.pixel(85, 100), SK_ColorRED);
  EXPECT_EQ(host.pixel(145, 100), SK_ColorBLACK);
  host.frame(0.3);  // settled at 0
  EXPECT_EQ(host.pixel(105, 100), SK_ColorRED);
  EXPECT_EQ(host.pixel(85, 100), SK_ColorBLACK);
  // Identical re-describe prunes (waypoints compare structurally).
  host.composer.render(box().child(
      box()
          .absolute()
          .inset(100, 80, 60, 80)
          .fill(red())
          .translateX(animate(through({{std::chrono::milliseconds(0), 40.0f},
                                       {std::chrono::milliseconds(200), -20.0f},
                                       {std::chrono::milliseconds(400), 0.0f}}),
                              &choreograph::easeNone))));
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
}

TEST(ComposeStyles, RippleDisplacesTheLayer) {
  // A thin horizontal red bar warped by a strong ripple: pixels appear
  // off-axis where the flat version has none.
  auto bar = [](bool warped) {
    Element e =
        box().absolute().inset(20, 96, 20, 96).fill(Fill::color({1, 0, 0, 1}));
    if (warped) e.effect(styles::ripple(10, 60));
    return box().child(std::move(e));
  };
  Host flat, warped;
  flat.composer.render(bar(false));
  warped.composer.render(bar(true));
  flat.frame();
  warped.frame();
  int off = 0, offFlat = 0;
  for (int x = 30; x < 170; x += 2)
    for (int dy : {-7, 7}) {
      off += warped.pixel(x, 100 + dy) != SK_ColorBLACK;
      offFlat += flat.pixel(x, 100 + dy) != SK_ColorBLACK;
    }
  EXPECT_GT(off, 15);
  EXPECT_EQ(offFlat, 0);
}

// ---------------------------------------------------------------------------
// Reconcile and motion cases where the failure is a value that LINGERS from
// the previous describe rather than one that is computed wrongly.

TEST(ComposeReconcile, RemovedDimsAndInsetsRelease) {
  // Patch reuses the yoga node: dims/aspect/insets REMOVED from the
  // description must actually unset, not linger from the last describe.
  Host host;
  host.composer.render(
      box().row().child(box().width(120).height(40).fill(red()).key("b")));
  host.frame();
  ASSERT_EQ(host.composer.bounds("b")->width(), 120);
  host.composer.render(box().row().child(
      box().height(40).fill(red()).key("b").child(box().width(30))));
  host.frame();
  EXPECT_EQ(host.composer.bounds("b")->width(), 30);  // released to content

  Host pins;
  pins.composer.render(
      box().child(box().inset(10, 10, 10, 10).fill(blue()).key("p")));
  pins.frame();
  ASSERT_EQ(pins.composer.bounds("p")->width(), 180);
  pins.composer.render(box().child(box()
                                       .left(Dim(20.0f))
                                       .top(Dim(20.0f))
                                       .width(50)
                                       .height(20)
                                       .fill(blue())
                                       .key("p")));
  pins.frame();
  EXPECT_EQ(*pins.composer.bounds("p"), SkRect::MakeXYWH(20, 20, 50, 20));
}

TEST(ComposeMotion, UnrelatedPatchDoesNotRestartAnEntrance) {
  // Mid-entrance, changing an UNRELATED prop must leave the running motion
  // alone. If a patch rebuilds the ramp, it also re-serves the transition's
  // delay from wherever the value happens to be, so the entrance stalls at
  // the fraction it had reached and then starts over — visible only as
  // slightly-wrong timing, never as an error.
  Host host;
  auto tree = [](Fill f) {
    return box().child(box().width(80).height(80).fill(f).opacity(
        animate(from(0.0f).to(1.0f),
                {std::chrono::milliseconds(400), &choreograph::easeNone,
                 std::chrono::milliseconds(300)})));
  };
  host.composer.render(tree(red()));
  host.frame(0.35);  // 50ms into the ramp (after the 300ms hold)
  host.composer.render(tree(blue()));  // fill changes; opacity prop identical
  host.frame(0.2);  // t = 0.55 → ramp fraction (0.55-0.3)/0.4 = 0.625
  const SkColor c = host.pixel(40, 40);
  // Continuing motion: strong blue. A restarted+re-held ramp would still
  // sit at the 0.125 it had when the patch landed.
  EXPECT_GT(SkColorGetB(c), 120u);
}

// ---------------------------------------------------------------------------
// Cases where the wrong answer looks plausible: a stale motion that lands on
// a believable value, a cache whose cull is a little too small, a mask that
// invents geometry.

TEST(ComposeMotion, ToggleBackDuringDelayHoldLands) {
  // Retargeting a slot to its CURRENT value must DISCONNECT a motion headed
  // elsewhere, not merely leave it unstarted. If it does not, the delay hold
  // expires later and the value fades to the target nobody asked for any
  // more — arbitrarily long after the describe that cancelled it.
  Host host;
  auto tree = [](float op) {
    return box().child(
        box()
            .width(80)
            .height(80)
            .fill(red())
            .transition({std::chrono::milliseconds(200), &choreograph::easeNone,
                         std::chrono::milliseconds(300)})
            .opacity(op));
  };
  host.composer.render(tree(1.0f));
  host.frame();
  host.composer.render(tree(0.0f));  // starts Hold(1, .3s) + RampTo(0)
  host.frame(0.1);                   // still holding at 1
  host.composer.render(tree(1.0f));  // toggle back DURING the hold
  host.frame(0.7);                   // any stale motion would have finished
  EXPECT_EQ(host.pixel(40, 40), SK_ColorRED);  // description wins: opaque
}

TEST(ComposeCache, ConnectorWireSurvivesParentCaching) {
  // A connector's routed path is NOT bounded by the connector node's layout
  // rect — it reaches across to the endpoints it joins. The parent's
  // recording cull has to account for that, or the wire is drawn on the
  // first frame and clipped away by every cached replay after it.
  Host host;
  host.composer.render(
      box()
          .child(box().absolute().inset(20, 90, 160, 90).fill(red()).key("a"))
          .child(box().absolute().inset(160, 90, 20, 90).fill(red()).key("b"))
          .child(connector("a", "b").stroke(util::stroke(4, green()))));
  host.frame();
  EXPECT_EQ(host.pixel(100, 100), SK_ColorGREEN);  // the wire, mid-span
  host.frame();                                    // cached replay
  EXPECT_EQ(host.pixel(100, 100), SK_ColorGREEN);
}

TEST(ComposeCache, TextureBakeKeepsBleedAndOverflow) {
  // A texture bake must cover the node's PAINT bounds, not its box: a
  // decoration that bleeds outside the box (here a shadow offset well past
  // it) is silently cropped away by a bake sized to the node.
  Host host;
  host.composer.render(
      box().child(box()
                      .absolute()
                      .inset(70, 70, 70, 70)
                      .cache(Cache::Texture)
                      .background(util::Shadow{{0, 1, 0, 1}, {30, 0}, 0})
                      .fill(red())));
  host.frame();
  EXPECT_EQ(host.pixel(140, 100), SK_ColorGREEN);  // shadow past the box
}

TEST(ComposeMask, OpenContourWrapKeepsTwoPieces) {
  // A wrapped span window on an OPEN contour is genuinely two pieces: its
  // head and its tail are at opposite ends of the path and are not adjacent.
  // Joining them into one run draws a chord across the middle that exists in
  // no path the author supplied.
  Host host;
  host.composer.render(
      box().child(box()
                      .absolute()
                      .inset(20, 80, 20, 80)
                      .shape([](SkSize s) {  // open horizontal line
                        SkPathBuilder b;
                        b.moveTo(0, s.height() / 2);
                        b.lineTo(s.width(), s.height() / 2);
                        return b.detach();
                      })
                      .mask(by::spans(spans::wrap(0.9f, 1.2f)))
                      .stroke(util::stroke(6, green()))));
  host.frame();
  EXPECT_EQ(host.pixel(170, 100), SK_ColorGREEN);  // tail piece [0.9, 1]
  EXPECT_EQ(host.pixel(40, 100), SK_ColorGREEN);   // head piece [0, 0.2]
  EXPECT_EQ(host.pixel(100, 100), SK_ColorBLACK);  // NO chord between them
}

TEST(ComposeMask, ClosedContourWrapSeamIsOnePiece) {
  // The twin of the test above: on a CLOSED contour the two halves of a
  // wrapped window ARE adjacent, so spanPath must stitch them into ONE run.
  //
  // Read the RENDER, not a path the test stitched for itself. A test that
  // rebuilds the window with SkContourMeasure and asserts on the result
  // proves something about its own arithmetic and nothing about what the
  // renderer drew.
  //
  // The seam is parked on a corner, which is what makes the difference
  // visible in pixels at all: one run puts a MITER JOIN there and the outer
  // corner square is covered; two runs put two butt caps there and that
  // square is empty — the visible notch.
  Host host;
  host.composer.render(box().child(
      box()
          .absolute()
          .inset(20, 20, 20, 20)
          .shape([](SkSize s) {  // closed rect, seam at its top-left corner
            SkPathBuilder b;
            b.moveTo(0, 0);
            b.lineTo(s.width(), 0);
            b.lineTo(s.width(), s.height());
            b.lineTo(0, s.height());
            b.close();
            return b.detach();
          })
          // perimeter 640: [0, 0.2] runs 128 px right along the top edge,
          // [0.9, 1] runs the last 64 px UP the left edge into the seam.
          .mask(by::spans(spans::wrap(0.9f, 1.2f)))
          .stroke(util::stroke(6, green()))));
  host.frame();
  EXPECT_EQ(host.pixel(80, 20), SK_ColorGREEN);    // head piece, top edge
  EXPECT_EQ(host.pixel(20, 50), SK_ColorGREEN);    // tail piece, left edge
  EXPECT_EQ(host.pixel(180, 100), SK_ColorBLACK);  // the mask still masks
  EXPECT_EQ(host.pixel(18, 18), SK_ColorGREEN);  // THE SEAM: joined, not capped
}

TEST(ComposeCache, SettledOpacityRebakesTheLeaf) {
  // When an opacity transition settles, the leaf's recording must be re-baked:
  // the recording made while opacity was 1 folded the fill straight into the
  // draw, and replaying it under the settled 0.4 would show the leaf at full
  // strength. The second frame() draws only from caches, which is where a
  // missed re-bake shows up.
  Host host;
  auto tree = [](Animatable<float> op) {
    return box().child(box()
                           .width(80)
                           .height(80)
                           .fill(Fill::color({1, 0, 0, 1}))
                           .opacity(std::move(op)));
  };
  host.composer.render(tree(1.0f));
  host.frame();
  host.composer.render(tree(animate(
      to(0.4f), {std::chrono::milliseconds(100), &choreograph::easeNone})));
  host.frame(0.5);  // settled at 0.4
  host.frame();     // draw again from caches
  const SkColor c = host.pixel(40, 40);
  EXPECT_NEAR(SkColorGetR(c), 102, 12);  // 0.4 · 255 on black
}

// ---------------------------------------------------------------------------
// The SPLIT bake. Volatility is a verdict on a NODE, so a large static
// ground plane carrying one moving child inherits the child's verdict: the
// plane is re-rasterized every frame purely so the child can be redrawn over
// it, and it reports "its content changes every frame" about a child that is
// not its own paint at all. The split bakes the node's OWN paint and draws
// the live children over the blit.

namespace {
/** THE SPLIT-BAKE FIXTURE, and every part of it is load-bearing.
 *
 *  The own paint deliberately OVERLAPS ITSELF — a background stroke under a
 *  runtime shader under an overlay stroke, plus a foreground stroke — so
 *  that compositing genuinely happens INSIDE the baked layer. That is the
 *  one real risk in the pixel argument: srcOver is associative in exact
 *  arithmetic, and a layer quantizes its intermediate to 8 bits where a
 *  direct draw quantizes a different intermediate. A fixture whose own
 *  paint is a single draw could not have found a discrepancy if there were
 *  one.
 *
 *  The foreground matters for a second reason: foregrounds paint AFTER the
 *  children, so a bake that swallowed them would draw them UNDER the child
 *  and this test would see it.
 *
 *  The child rides a bound Output, which is what makes the node volatile and
 *  is the ordinary shape of the problem: a small element driven by a loop
 *  over a large static backdrop. `clipped` and `childBlend` are
 *  parameterised because both are conditions whole-subtree promotion refuses
 *  outright, and the split must accept them. */
choreograph::Output<float> gSplitSweep{0.0f};

Element splitPlane(bool clipped, SkBlendMode childBlend) {
  Element plane =
      box()
          .key("plane")
          .absolute()
          .left(0)
          .top(0)
          .width(200)
          .height(200)
          .background(util::stroke(6.0f, Fill::color({0.2f, 0.4f, 0.9f, 0.6f})))
          .fill(Material::sksl(sharedHeavyEffect()))
          .overlay(util::stroke(3.0f, Fill::color({1.0f, 0.9f, 0.2f, 0.45f})))
          .foreground(util::stroke(1.5f, Fill::color({1, 1, 1, 0.5f})));
  if (clipped) plane.clip(true);
  plane.child(box()
                  .absolute()
                  .left(10)
                  .top(80)
                  .width(50)
                  .height(50)
                  .fill(Fill::color({1.0f, 0.35f, 0.1f, 0.85f}))
                  .blend(childBlend)
                  .translateX(bind(&gSplitSweep).scale(130.0f)));
  return profiledUnder(std::move(plane));
}

/** The worst frame-to-frame divergence over the child's WHOLE traverse.
 *
 *  Not one still: a split that is exact where the child happens to sit and
 *  wrong where it crosses the bake's own overlay would pass a single
 *  capture. The sweep is what makes the claim about the mechanism rather
 *  than about one position. */
size_t worstSplitDivergence(bool clipped, SkBlendMode childBlend) {
  Host on(200, 200), off(200, 200);
  off.composer.setAutoTexturePromotion(false);
  size_t worst = 0;
  for (int i = 0; i < 32; ++i) {
    gSplitSweep = (float)i / 32.0f;
    on.composer.render(splitPlane(clipped, childBlend));
    off.composer.render(splitPlane(clipped, childBlend));
    on.frame();
    off.frame();
    if (!identicalPixels(off, on, 200, 200)) {
      SkBitmap a, b;
      a.allocPixels(SkImageInfo::MakeN32Premul(200, 200));
      b.allocPixels(SkImageInfo::MakeN32Premul(200, 200));
      off.surface->readPixels(a.pixmap(), 0, 0);
      on.surface->readPixels(b.pixmap(), 0, 0);
      size_t differing = 0;
      for (int y = 0; y < 200; ++y)
        for (int x = 0; x < 200; ++x)
          differing += a.getColor(x, y) != b.getColor(x, y);
      worst = std::max(worst, differing);
    }
  }
  return worst;
}
}  // namespace

TEST(ComposeCache, SplitsAnExpensiveOwnPaintFromItsMovingChild) {
  Host host(200, 200);
  host.composer.setProfiling(true);
  for (int i = 0; i < 24; ++i) {
    gSplitSweep = (float)i / 24.0f;
    host.composer.render(splitPlane(false, SkBlendMode::kSrcOver));
    host.frame();
  }
  const Composer::NodeCost* row = requireRow(host.composer, "plane");
  ASSERT_NE(row, nullptr);
  EXPECT_EQ(row->cacheState, Composer::CacheState::SplitOwn)
      << "a static own paint costing " << row->selfMs
      << " ms per frame was re-rasterized to redraw a moving child over it";
  EXPECT_EQ(row->promotion, Composer::Promotion::SplitBaked);
  // And the node really is refused by the ordinary promoter, or "split" is
  // true for the wrong reason: a node the promoter would have baked whole
  // proves nothing about splitting.
  EXPECT_TRUE(row->refused(Composer::Promotion::Volatile))
      << "this node is not volatile, so nothing above tested the split";
}

TEST(ComposeCache, SplitBakeIsPixelIdenticalAcrossTheChildsMotion) {
  // The exactness constraint, which is strictly stronger than whole-subtree
  // promotion's. Promotion only has to argue that an integer device
  // translation cannot change rasterisation. The split replaces PART of what
  // a node paints and then draws the children over the blit, so it has to
  // argue about compositing:
  //
  //   painting the own layer into a transparent device-aligned surface,
  //   blitting it, then painting the children over the result must produce
  //   the same pixels as painting own-then-children directly.
  //
  // srcOver is associative, so that holds in exact arithmetic. What is NOT
  // free is the 8-bit rounding of the intermediate, and this fixture's own
  // paint overlaps itself specifically so that the intermediate exists.
  EXPECT_EQ(worstSplitDivergence(false, SkBlendMode::kSrcOver), 0u)
      << "splitting the own paint from the children changed pixels";
}

TEST(ComposeCache, ABlendingChildIsFineUnderTheSplitAndFatalUnderPromotion) {
  // The one place the split is SAFER than whole-subtree promotion, and it
  // is worth an assertion rather than an argument because it inverts a rule
  // three other tests in this file enforce.
  //
  // Under promotion a non-srcOver child is fatal: it sits INSIDE the bake
  // and resolves against transparent black. Under the split the blit lands
  // BEFORE the children, so the child resolves against exactly the
  // destination bytes it would have found anyway. Same for a child with a
  // backdrop filter.
  for (SkBlendMode mode :
       {SkBlendMode::kMultiply, SkBlendMode::kScreen, SkBlendMode::kPlus}) {
    EXPECT_EQ(worstSplitDivergence(false, mode), 0u)
        << "a " << (int)mode << "-blended child diverged under the split";
  }
}

TEST(ComposeCache, TheSplitSurvivesTheClipThatMadeTheChildAChild) {
  // clipContent looks like it belongs beside layer effects in the split's
  // exclusion list — both appear to "wrap both halves" — but only the layer
  // effect actually does. A filter applies to the UNION of own paint and
  // children, so filtering the own half alone is a different picture. A clip
  // is opened and closed INSIDE each phase (the phase flag skips only the
  // content), so both halves get the identical clip in identical device
  // geometry.
  //
  // Getting this wrong is not a minor over-refusal. A clip is very often
  // exactly WHY a moving element is a child of the plane rather than its
  // sibling — the plane clips it to a silhouette — so excluding clips would
  // refuse the split precisely where it is most wanted, and every other case
  // in this section would still pass.
  Host host(200, 200);
  host.composer.setProfiling(true);
  for (int i = 0; i < 24; ++i) {
    gSplitSweep = (float)i / 24.0f;
    host.composer.render(splitPlane(true, SkBlendMode::kSrcOver));
    host.frame();
  }
  const Composer::NodeCost* row = requireRow(host.composer, "plane");
  ASSERT_NE(row, nullptr);
  EXPECT_EQ(row->cacheState, Composer::CacheState::SplitOwn)
      << "a clipped node was refused the split bake";
  EXPECT_EQ(worstSplitDivergence(true, SkBlendMode::kMultiply), 0u)
      << "the clip was not reproduced identically in both phases";
}

TEST(ComposeCache, TheVOLATILECHILDIsWhatCausesTheSplit) {
  // THE POSITIVE CONTROL. "It was split" proves nothing on its own: a node
  // split for some unrelated reason passes exactly as well as one split for
  // the reason under test. So render the SAME scene with the child's
  // binding replaced by the constant it would have held — the only
  // difference — and require that the node is promoted WHOLE instead.
  //
  // Together the two halves say: this node's own paint is bakeable, and the
  // only thing deciding between a whole bake and a split one is whether the
  // child moves. A change that split everything fails this; one that split
  // nothing fails its sibling.
  const auto still = [] {
    return profiledUnder(
        box()
            .key("plane")
            .absolute()
            .left(0)
            .top(0)
            .width(200)
            .height(200)
            .background(
                util::stroke(6.0f, Fill::color({0.2f, 0.4f, 0.9f, 0.6f})))
            .fill(Material::sksl(sharedHeavyEffect()))
            .overlay(util::stroke(3.0f, Fill::color({1.0f, 0.9f, 0.2f, 0.45f})))
            .foreground(util::stroke(1.5f, Fill::color({1, 1, 1, 0.5f})))
            .child(box()
                       .absolute()
                       .left(10)
                       .top(80)
                       .width(50)
                       .height(50)
                       .fill(Fill::color({1.0f, 0.35f, 0.1f, 0.85f}))
                       .translateX(65.0f)));
  };
  Host host(200, 200);
  host.composer.setProfiling(true);
  host.composer.render(still());
  for (int i = 0; i < 24; ++i) host.frame();
  const Composer::NodeCost* row = requireRow(host.composer, "plane");
  ASSERT_NE(row, nullptr);
  EXPECT_EQ(row->cacheState, Composer::CacheState::Promoted)
      << "the same node with a STILL child was not promoted whole either, "
         "so the split tests above prove nothing about the child's motion";
  EXPECT_FALSE(row->refused(Composer::Promotion::Volatile));
}

// ---------------------------------------------------------------------------
// Cache::Group — a whole subtree baked while its bindings hold still.
//
// The shape it exists for: MANY SMALL ROTATED PIECES FORMING ONE STATIC
// ASSEMBLY, each piece carrying a bound entrance that finishes and then
// holds. No per-piece cache helps, because each piece is cheap and there are
// hundreds; what is expensive is the assembly. The fixture below is that in
// miniature, and every ingredient is load bearing:
//
//  - ROTATION, because a bake is device-space and unrotated while the pieces
//    are not, which is exactly why a per-piece bake is refused;
//  - a BEVEL, because a bevel puts most of its work on the piece's edges,
//    which is where an isolating bake differs from live paint if it differs
//    anywhere;
//  - OVERLAP, because a bake into a transparent layer only reproduces live
//    compositing if srcOver's associativity survives 8-bit rounding, and
//    that can only be exercised where pieces composite with each other;
//  - a bound opacity AND a bound scale, because the memo has to see both a
//    scalar that does not move the bake rect and one that does.
//
// WHAT "PIXEL IDENTICAL" MEANS HERE, AND WHY THE GROUND MATTERS. A test that
// composites over the host's opaque BLACK clear cannot see the one error an
// isolating bake makes at all: premultiplied srcOver over opaque black is
// `result.rgb = src.rgb`, with the destination term multiplied away. Point
// the same comparison at a LIT ground and a small residual appears on
// antialiased edges — one extra 8-bit requantisation, because an edge enters
// the bake as premultiplied coverage already rounded to 8 bits and
// composites against the ground from there, where live paint composites the
// same edge from full-precision coverage in one step. It is not the
// rotation, not the shader and not the device offset: a flat-colour fill
// shows it too, a single unrotated piece shows it, and forcing the REFERENCE
// through any layer removes it entirely.
//
// So the exact claim these tests make is: a group bake is byte identical to
// compositing the same subtree through a layer — something an author can
// already ask for by hand. Both grounds are asserted below, because either
// alone would be the weaker statement.

namespace {

constexpr int kBoards = 24;
constexpr double kGroupPeriod = 2.0;  // the loop
constexpr double kGroupDone = 1.25;   // every board has finished by here

std::vector<choreograph::Output<float>>& boardFade() {
  static std::vector<choreograph::Output<float>> v(kBoards);
  return v;
}
std::vector<choreograph::Output<float>>& boardPop() {
  static std::vector<choreograph::Output<float>> v(kBoards);
  return v;
}

/** The staggered entrance, driven from the test loop rather than from a
 *  ticker so both hosts read exactly the same numbers on the same frame. */
void setBoardPhase(double t) {
  const double now = std::fmod(t, kGroupPeriod);
  for (int i = 0; i < kBoards; ++i) {
    const double delay = 0.9 * (double)i / (double)kBoards;
    double raw = (now - delay) / 0.35;
    raw = raw < 0.0 ? 0.0 : (raw > 1.0 ? 1.0 : raw);
    const float e = (float)(1.0 - std::pow(1.0 - raw, 3.0));  // easeOutCubic
    boardFade()[i] = 0.30f + 0.70f * e;
    boardPop()[i] = 0.55f + 0.45f * e;
  }
}

sk_sp<SkRuntimeEffect> boardGrain() {
  // Real per-pixel work with real local coordinates. A shader reads its
  // local space by INVERTING the CTM, so a bake that lands at a different
  // device offset than live paint would sample the grain differently — a
  // flat colour would hide that entirely. Kept cheap on purpose: the loop
  // below draws this once per piece per frame on two hosts, for hundreds of
  // frames.
  static sk_sp<SkRuntimeEffect> effect = [] {
    auto [e, err] = SkRuntimeEffect::MakeForShader(
        SkString("half4 main(float2 p) {"
                 "  float g = 0.5 + 0.5 * sin(p.x * 0.71) * cos(p.y * 1.37);"
                 "  float h = 0.5 + 0.5 * sin(p.x * 0.13 + p.y * 0.09);"
                 "  return half4(half(0.20 + 0.62 * g), half(0.16 + 0.52 * h),"
                 "               half(0.10 + 0.34 * g), 1.0);"
                 "}"));
    if (!e) ADD_FAILURE() << err.c_str();
    return e;
  }();
  return effect;
}

/** One board: a mitred quad, a grain fill, a bevelled arris whose light
 *  angle follows the rotation, a seam keyline, and the two bound scalars. */
Element board(int i) {
  // Boards cycle through the three kumiko jig angles — 22.5°, 45°, 67.5°
  // — so every board is rotated (a device-space bake cannot share any of
  // them) and neighbours always meet at different angles.
  const float ang = 22.5f * (float)(1 + i % 3);
  SkPathBuilder quad;
  quad.moveTo(5, 0);
  quad.lineTo(74, 0);
  quad.lineTo(69, 11);
  quad.lineTo(0, 11);
  quad.close();
  SkPath shape = quad.detach();
  return box()
      .absolute()
      .left(14.0f + 38.0f * (float)(i % 5))
      .top(14.0f + 38.0f * (float)(i / 5))
      .width(74)
      .height(11)
      .rotate(ang)
      .shape([shape](SkSize) { return shape; })
      .fill(Material::sksl(boardGrain()))
      .foreground(styles::BevelEmboss{0.8f,
                                      1.2f,
                                      120.0f + ang,
                                      {1, 0.96f, 0.86f, 0.45f},
                                      {0.14f, 0.09f, 0.03f, 0.45f}})
      .stroke(util::stroke(0.6f, Fill::color({0.29f, 0.21f, 0.12f, 0.55f}),
                           PathFormat::Align::Inner))
      .opacity(&boardFade()[i])
      .scale(&boardPop()[i]);
}

Element lattice(Cache mode) {
  Element g = box()
                  .absolute()
                  .left(0)
                  .top(0)
                  .width(240)
                  .height(240)
                  .key("lattice")
                  .cache(mode);
  for (int i = 0; i < kBoards; ++i) g.child(board(i));
  return g;
}

/** How the reference composites, which is the whole subtlety above.
 *  `Black` puts the lattice straight on the host's opaque clear, where
 *  srcOver's destination term vanishes; `Lit` gives it a ground to composite
 *  against; `LitIsolated` gives it that ground AND wraps the subtree in a
 *  no-op image filter, so BOTH sides pay the same requantisation. */
enum class Ground { Black, Lit, LitIsolated };

/** The lattice under a Cache::None wrapper — this IS `profiledUnder()`, with
 *  the geometry pinned so the wrapper cannot move the subject: the pixel
 *  comparison below needs the two trees to differ in exactly one thing, and
 *  the Cache mode is that one thing. */
Element latticeScene(Cache mode, Ground ground) {
  Element g = lattice(mode);
  if (ground == Ground::LitIsolated)
    g.effect(Effect::filter(SkImageFilters::Offset(0, 0, nullptr)));
  Element wrapped = box()
                        .cache(Cache::None)
                        .absolute()
                        .left(0)
                        .top(0)
                        .width(240)
                        .height(240)
                        .child(std::move(g));
  Element root = stack();
  if (ground != Ground::Black)
    root.child(box().inset(0).fill(Fill::color({0.07f, 0.06f, 0.05f, 1})));
  return root.child(std::move(wrapped));
}

/** Both hosts, same frame, same numbers, promotion off on BOTH so the only
 *  difference between them is the Cache mode on the lattice. */
struct GroupPair {
  Host on{240, 240}, off{240, 240};
  explicit GroupPair(Ground ground = Ground::Black) {
    on.composer.setAutoTexturePromotion(false);
    off.composer.setAutoTexturePromotion(false);
    on.composer.setProfiling(true);
    setBoardPhase(0.0);
    // Rendered ONCE. A node carrying a shape callable compares unequal on
    // every re-describe — an incomparable callable never prunes — so a
    // per-frame render would mark every board dirty and no cache in the
    // library could hold, including the one under test.
    on.composer.render(latticeScene(Cache::Group, ground));
    off.composer.render(latticeScene(Cache::Auto, ground));
  }
  void at(double t) {
    setBoardPhase(t);
    on.frame();
    off.frame();
  }
  bool blitting() {
    const Composer::NodeCost* row = requireRow(on.composer, "lattice");
    return row && row->cacheState == Composer::CacheState::Group;
  }
  size_t bakesThisFrame() const {
    return on.composer.stats().texturesBaked;  // zeroed by every draw()
  }
  /** Differing pixels and the peak channel delta — the count is the claim,
   *  the peak is what makes a failure legible (one requantised edge reads
   *  very differently from a frozen or misplaced blit). */
  struct Divergence {
    size_t pixels = 0;
    int peak = 0;
  };
  Divergence divergence() {
    SkBitmap a, b;
    a.allocPixels(SkImageInfo::MakeN32Premul(240, 240));
    b.allocPixels(SkImageInfo::MakeN32Premul(240, 240));
    off.surface->readPixels(a.pixmap(), 0, 0);
    on.surface->readPixels(b.pixmap(), 0, 0);
    Divergence d;
    for (int y = 0; y < 240; ++y)
      for (int x = 0; x < 240; ++x) {
        const SkColor ca = a.getColor(x, y), cb = b.getColor(x, y);
        if (ca == cb) continue;
        ++d.pixels;
        d.peak = std::max(
            {d.peak, std::abs((int)SkColorGetR(ca) - (int)SkColorGetR(cb)),
             std::abs((int)SkColorGetG(ca) - (int)SkColorGetG(cb)),
             std::abs((int)SkColorGetB(ca) - (int)SkColorGetB(cb)),
             std::abs((int)SkColorGetA(ca) - (int)SkColorGetA(cb))});
      }
    return d;
  }
};

/** Walk the whole loop — entrance, settle, hold and the WRAP back to zero,
 *  at 1/60 s, twice around — and report the worst frame plus how the frames
 *  divided between blitting and painting live. */
struct LoopResult {
  int blitFrames = 0, liveFrames = 0, differingFrames = 0, frames = 0;
  double firstBadT = -1;
  GroupPair::Divergence worst;
};
LoopResult walkTheLoop(GroupPair& pair) {
  LoopResult r;
  const int steps = (int)std::lround(2.0 * kGroupPeriod * 60.0);
  for (int i = 0; i <= steps; ++i) {
    const double t = (double)i / 60.0;
    pair.at(t);
    ++r.frames;
    (pair.blitting() ? r.blitFrames : r.liveFrames)++;
    const GroupPair::Divergence d = pair.divergence();
    if (d.pixels) {
      ++r.differingFrames;
      if (r.firstBadT < 0) r.firstBadT = t;
      if (d.pixels > r.worst.pixels) r.worst = d;
    }
  }
  return r;
}

}  // namespace

TEST(ComposeCache, GroupBakesASubtreeItsChildrensBindingsMadeUncacheable) {
  // The premise first: this lattice is refused by every other cache. Its
  // children never stop being volatile — the bindings stay connected for the
  // whole loop, whether or not their values are moving — so `Cache::Texture`
  // on the container bakes nothing (a fill-less container has no own paint
  // to bake) and the picture path is blocked along with it. Cache::Group is
  // the only route by which such a subtree holds pixels at all.
  GroupPair pair;
  for (int i = 0; i < 12; ++i)
    pair.at(kGroupDone + 0.4);  // settled: the same numbers every frame
  EXPECT_TRUE(pair.blitting())
      << "the settled lattice was not baked, so every pixel test below is "
         "comparing two identical live paints and proving nothing";
  EXPECT_GE(pair.on.composer.stats().texturesLive, 1u);
  EXPECT_EQ(pair.bakesThisFrame(), 0u)
      << "a settled group re-baked instead of blitting what it already had";

  // …and the control for the premise: the SAME tree without Cache::Group
  // holds no bake at all. If this ever starts passing, something else began
  // caching the lattice and the assertion above stopped being about Group.
  EXPECT_EQ(pair.off.composer.stats().texturesLive, 0u)
      << "the Cache::Auto lattice cached pixels by some other route, so the "
         "comparison is no longer Group-against-nothing";
}

TEST(ComposeCache, GroupDropsTheBakeOnTheFrameABindingTicks) {
  // THE MECHANISM, asserted rather than argued. A group that keeps its bake
  // while its bindings move FREEZES the animation — and a frozen entrance
  // looks perfectly correct in any still, which is exactly why this needs a
  // mechanism test and not only a pixel test.
  GroupPair pair;
  for (int i = 0; i < 12; ++i) pair.at(kGroupDone + 0.4);
  ASSERT_TRUE(pair.blitting()) << "never reached the baked state";

  // One tick of the entrance: every board's opacity and scale move.
  pair.at(0.5);
  EXPECT_FALSE(pair.blitting())
      << "the bake survived a frame on which every bound opacity and scale "
         "below the node changed — the group is holding pixels that are no "
         "longer this frame's pixels";
  EXPECT_EQ(pair.bakesThisFrame(), 0u)
      << "the group re-baked on a moving frame: a bake per frame costs "
         "strictly more than the paint it replaces";

  // Hold that same phase and it comes back — the memo says "not changing",
  // not "finished".
  pair.at(0.5);
  EXPECT_TRUE(pair.blitting())
      << "the group did not re-bake once its scalars held still again";
  EXPECT_EQ(pair.bakesThisFrame(), 1u);
}

TEST(ComposeCache, GroupIsPixelIdenticalAcrossTheWholeLoop) {
  // Not one still, and not only the settled window: the entrance, the
  // settle, the hold, and the WRAP back to zero, at 1/60 s, twice around. A
  // group that is exact where it happens to be baked and wrong on the frames
  // it takes or drops the bake would pass any single capture.
  //
  // Over the host's opaque clear, where the destination term of srcOver
  // vanishes and the bake's one extra requantisation cannot show, the
  // standard is BYTE IDENTITY and nothing less.
  GroupPair pair(Ground::Black);
  const LoopResult r = walkTheLoop(pair);
  EXPECT_EQ(r.differingFrames, 0)
      << r.differingFrames << " of " << r.frames
      << " frames differ, first at t=" << r.firstBadT << ", worst frame "
      << r.worst.pixels << " pixels at peak " << r.worst.peak;
  // Both halves, or the loop proved nothing. A run that never baked is two
  // live paints compared with each other; a run that never dropped is a
  // frozen lattice, which is also "identical" to itself.
  EXPECT_GT(r.blitFrames, 0) << "the group never baked over the whole loop";
  EXPECT_GT(r.liveFrames, 0)
      << "the group never dropped its bake over an entrance that moves every "
         "board — the drop is not being exercised, so this test cannot see "
         "the bug it exists for";
}

TEST(ComposeCache, AGroupBakeIsExactlyALayerAndNothingMore) {
  // The same loop over a LIT ground, which is where an isolating bake and
  // live paint genuinely part company.
  //
  // The control is the point: wrap the reference subtree in a no-op image
  // filter, so the reference is isolated into a layer exactly as the bake is,
  // and the difference goes to zero. That is a stronger statement than a
  // tolerance, because a tolerance is fitted and this is not: it says the
  // bake's ONLY deviation from live paint is the layer, which the author can
  // already ask for by hand.
  {
    GroupPair isolated(Ground::LitIsolated);
    const LoopResult r = walkTheLoop(isolated);
    EXPECT_EQ(r.differingFrames, 0)
        << "a group bake differs from compositing the same subtree through a "
           "layer: "
        << r.worst.pixels << " pixels at peak " << r.worst.peak
        << ", first at t=" << r.firstBadT;
    EXPECT_GT(r.blitFrames, 0);
    EXPECT_GT(r.liveFrames, 0);
  }
  // …and the unisolated comparison, kept as a CEILING rather than an
  // equality, because it cannot be an equality: antialiased coverage rounds
  // to 8 bits once more on the way through a bake, so edge pixels land a
  // level or two off. The ceiling is loose on purpose — what it is built to
  // catch is a frozen bake or a misplaced blit, which move whole boards and
  // blow past any edge-sized budget.
  {
    GroupPair lit(Ground::Lit);
    const LoopResult r = walkTheLoop(lit);
    EXPECT_LE(r.worst.peak, 4)
        << "the divergence over a lit ground is no longer edge requantisation "
           "— "
        << r.worst.pixels << " pixels at peak " << r.worst.peak;
    EXPECT_LT(r.worst.pixels, (size_t)(240 * 240 / 10))
        << "more than a tenth of the canvas moved, which is not an edge "
           "effect";
    EXPECT_GT(r.blitFrames, 0);
  }
}

TEST(ComposeCache, AGroupsOwnFadeDoesNotDropItsBake) {
  // A deliberate exclusion, tested because it is a decision and not a
  // consequence. The root's own opacity and transform are applied by
  // paint()'s saveLayer and matrix OUTSIDE the bake — so a group fading in
  // as a whole must keep baking through the fade, and the fade must still
  // composite exactly. Including them in the memo would have cost the bake
  // on every frame of every entrance, for a change the bake does not
  // contain.
  static choreograph::Output<float> groupFade{1.0f};
  const auto scene = [](Cache mode) {
    Element g = lattice(mode).opacity(&groupFade);
    return stack().child(box()
                             .cache(Cache::None)
                             .absolute()
                             .left(0)
                             .top(0)
                             .width(240)
                             .height(240)
                             .child(std::move(g)));
  };
  Host on(240, 240), off(240, 240);
  on.composer.setAutoTexturePromotion(false);
  off.composer.setAutoTexturePromotion(false);
  on.composer.setProfiling(true);
  setBoardPhase(kGroupDone + 0.4);  // the boards hold still…
  groupFade = 1.0f;
  on.composer.render(scene(Cache::Group));
  off.composer.render(scene(Cache::Auto));
  for (int i = 0; i < 8; ++i) {
    on.frame();
    off.frame();
  }
  const Composer::NodeCost* row = requireRow(on.composer, "lattice");
  ASSERT_NE(row, nullptr);
  ASSERT_EQ(row->cacheState, Composer::CacheState::Group);

  int fadeFrames = 0, blits = 0;
  size_t bakes = 0;
  for (int i = 1; i <= 20; ++i) {  // …while the GROUP fades
    groupFade = 1.0f - 0.045f * (float)i;
    on.frame();
    off.frame();
    ++fadeFrames;
    bakes += on.composer.stats().texturesBaked;
    const Composer::NodeCost* r = requireRow(on.composer, "lattice");
    if (r && r->cacheState == Composer::CacheState::Group) ++blits;
    ASSERT_TRUE(identicalPixels(off, on, 240, 240))
        << "the group's own opacity composited differently through the blit "
           "than through the live paint, at frame "
        << i;
  }
  EXPECT_EQ(blits, fadeFrames)
      << "the group dropped its bake for its OWN fade, which the bake does "
         "not contain — the root's paint slots are in the memo and must not "
         "be";
  EXPECT_EQ(bakes, 0u) << "the group re-baked during its own fade";
}

// ---- the refusals: every limit the header documents, exercised -----------
//
// A documented limit is a claim, so each sentence in Cache::Group's doc
// comment gets a case here. Each is paired with the SAME tree minus the
// offending ingredient, because "it did not bake" is worth nothing without
// "and this one does" — a fixture that stopped baking for an unrelated
// reason would satisfy every refusal at once.

namespace {
/** Did a lattice carrying `extra` as an extra child ever reach the baked
 *  state over ten settled frames? */
bool groupBakesWith(Element extra) {
  Host host(240, 240);
  host.composer.setAutoTexturePromotion(false);
  host.composer.setProfiling(true);
  setBoardPhase(kGroupDone + 0.4);
  Element g = lattice(Cache::Group).child(std::move(extra));
  host.composer.render(stack().child(box()
                                         .cache(Cache::None)
                                         .absolute()
                                         .left(0)
                                         .top(0)
                                         .width(240)
                                         .height(240)
                                         .child(std::move(g))));
  bool sawBlit = false;
  for (int i = 0; i < 10; ++i) {
    host.frame();
    const Composer::NodeCost* row = requireRow(host.composer, "lattice");
    sawBlit |= row && row->cacheState == Composer::CacheState::Group;
  }
  return sawBlit;
}
Element plainExtra() {
  return box().absolute().left(100).top(100).width(30).height(30).fill(red());
}
}  // namespace

TEST(ComposeCache, GroupRefusesWhatItsMemoCannotSee) {
  // The control first: a plain extra child changes nothing.
  ASSERT_TRUE(groupBakesWith(plainExtra()))
      << "the fixture does not bake even with an innocuous child, so none of "
         "the refusals below is being attributed to the right thing";

  // A LIVE MATERIAL. uTime moves pixels every frame with no float anywhere in
  // the tree to compare, so a group holding a bake across one would blit last
  // second's picture forever.
  EXPECT_FALSE(
      groupBakesWith(plainExtra().fill(Material::sksl(heavyEffect(true)))))
      << "a group baked over a live material";

  // A NON-SRCOVER BLEND below the root: inside the bake it resolves against
  // transparent black instead of against the ground.
  EXPECT_FALSE(groupBakesWith(plainExtra().blend(SkBlendMode::kMultiply)))
      << "a group baked over a kMultiply child, which resolves against "
         "transparent black inside a bake";

  // A Cache::None LEAF — declared per-frame volatility, by definition
  // invisible to a value comparison.
  EXPECT_FALSE(groupBakesWith(plainExtra().cache(Cache::None)))
      << "a group baked over a Cache::None leaf";

  // An ANIMATED DECORATION: the same argument as the live material, one
  // level out from the fill.
  static choreograph::Output<float> dash{0};
  PathFormat marching = util::stroke(2.0f, Fill::color({1, 1, 1, 1}));
  marching.dashIntervals = {4.0f, 4.0f};
  marching.dashPhaseBinding = &dash;
  EXPECT_FALSE(groupBakesWith(plainExtra().background(marching)))
      << "a group baked over an animated decoration";
}

TEST(ComposeCache, AMovingGroupRefusesTheBakeRatherThanRemakingIt) {
  // The other documented limit, and the one whose failure is a slowdown
  // rather than a wrong picture: a device-pinned bake remade every frame
  // costs strictly more than the paint it replaces, so a moving group must
  // REFUSE the bake instead of remaking it. The group's own transform is the
  // case a declaration can see coming; a resizing host is the case only the
  // device rect can.
  static choreograph::Output<float> slide{0};
  Host host(240, 240);
  host.composer.setAutoTexturePromotion(false);
  host.composer.setProfiling(true);
  setBoardPhase(kGroupDone + 0.4);
  slide = 0.0f;
  Element g = lattice(Cache::Group).translateX(&slide);
  host.composer.render(stack().child(box()
                                         .cache(Cache::None)
                                         .absolute()
                                         .left(0)
                                         .top(0)
                                         .width(240)
                                         .height(240)
                                         .child(std::move(g))));
  for (int i = 0; i < 6; ++i) host.frame();
  size_t bakes = 0;
  for (int i = 1; i <= 20; ++i) {
    slide = 0.7f * (float)i;
    host.frame();
    bakes += host.composer.stats().texturesBaked;
  }
  EXPECT_EQ(bakes, 0u) << "the group re-baked while it was sliding: 20 frames, "
                       << bakes << " bakes";
  const Composer::NodeCost* row = requireRow(host.composer, "lattice");
  ASSERT_NE(row, nullptr);
  EXPECT_NE(row->cacheState, Composer::CacheState::Group);
}

TEST(ComposeCache, ARefusalNamesEveryReasonAndNotJustTheFirst) {
  // `promotion` is a FIRST-MATCH verdict, so a node that is both volatile
  // and clipped reports only Volatile — and an author who removes the
  // volatility then meets a second refusal nobody mentioned. The `refusals`
  // mask carries all of them at once; `promotion` stays the primary outcome,
  // so every assertion elsewhere that reads it still means what it meant.
  //
  // The tree here is refused three ways at once: a bound child (Volatile),
  // a rotation (Transformed) and clip(true) (Filtered). Composited is
  // deliberately NOT in the set — opacity only reaches that refusal through
  // the childless-leaf fast path, which a clipped node with children can
  // never take, so including it would make this test unfalsifiable.
  Host host(220, 220);
  host.composer.setProfiling(true);
  host.composer.render(
      profiledUnder(expensivePanel().key("many").clip(true).rotate(30.0f).child(
          box()
              .absolute()
              .left(4)
              .top(4)
              .width(20)
              .height(20)
              .fill(red())
              .translateX(bind(&gSplitSweep).scale(40.0f)))));
  for (int i = 0; i < 24; ++i) {
    gSplitSweep = (float)i / 24.0f;
    host.frame();
  }
  const Composer::NodeCost* row = requireRow(host.composer, "many");
  ASSERT_NE(row, nullptr);
  EXPECT_TRUE(row->refused(Composer::Promotion::Volatile));
  EXPECT_TRUE(row->refused(Composer::Promotion::Transformed));
  EXPECT_TRUE(row->refused(Composer::Promotion::Filtered));
  // …and the primary verdict is exactly the first of them in the documented
  // order. The two can never disagree because the verdict is DERIVED from
  // the mask rather than computed alongside it.
  EXPECT_EQ(row->promotion, Composer::Promotion::Volatile);
  // A node with nothing wrong with it must report an EMPTY mask, or
  // "refused(X) is true" above is true of everything and tests nothing.
  Host clean(220, 220);
  clean.composer.setProfiling(true);
  clean.composer.render(profiledUnder(expensivePanel().key("clean")));
  for (int i = 0; i < 24; ++i) clean.frame();
  const Composer::NodeCost* ok = requireRow(clean.composer, "clean");
  ASSERT_NE(ok, nullptr);
  EXPECT_EQ(ok->refusals, 0u);
}

TEST(ComposePaint, BackdropLeavesDecorationsUnclipped) {
  // A backdrop filter reads and re-draws the region behind the node, which
  // needs a clip — but that clip must not reach the node's own decorations.
  // An outer-aligned stroke lies OUTSIDE the node's shape, so clipping it
  // away is silent and total.
  Host host;
  host.composer.render(box().child(
      box()
          .absolute()
          .inset(60, 60, 60, 60)
          .backdrop(Effect::filter(SkImageFilters::Blur(2, 2, nullptr)))
          .stroke(util::stroke(10, green(), PathFormat::Align::Outer))));
  host.frame();
  EXPECT_EQ(host.pixel(52, 100), SK_ColorGREEN);  // outer stroke intact
}

TEST(ComposeMotion, AppendedItemEntersWithoutInheritedDelay) {
  // A stagger delay is an ordinal times a step, so an item appended to a
  // list whose cascade already finished would sit invisible for its full
  // ordinal delay before entering. Only newly mounted children take a
  // stagger, and their delay is counted from the mount, not from the list.
  Host host;
  auto card = [](std::string_view key) {
    return box().width(60).height(20).fill(red()).key(key).opacity(
        animate(from(0.0f).to(1.0f),
                {std::chrono::milliseconds(100), &choreograph::easeNone}));
  };
  host.composer.render(box()
                           .column()
                           .gap(10)
                           .staggerChildren(std::chrono::milliseconds(400))
                           .child(card("a"))
                           .child(card("b")));
  host.frame(1.2);  // initial cascade done
  host.composer.render(box()
                           .column()
                           .gap(10)
                           .staggerChildren(std::chrono::milliseconds(400))
                           .child(card("a"))
                           .child(card("b"))
                           .child(card("c")));  // appended: only new mount
  host.frame(0.15);  // > its 100ms entrance, << 2·400ms ordinal delay
  EXPECT_EQ(host.pixel(30, 70), SK_ColorRED);  // "c" already in
}

TEST(ComposeBrushes, PatternCornerTileAtTheClosedSeam) {
  // The seam of a closed contour is a corner like any other, even though no
  // moveTo/lineTo pair announces it: the last segment turns into the first.
  // A corner tile belongs there too.
  Host host;
  brush::Pattern b;
  b.side = box().width(20).height(4).fill(red());
  b.corner = brush::CornerArt{box().width(12).height(12).fill(blue()),
                              brush::CornerAlign::Bisector};
  host.composer.render(
      box().child(box()
                      .absolute()
                      .inset(50, 50, 50, 50)
                      .shape([](SkSize s) {  // closed rect starting at (0,0)
                        SkPathBuilder p;
                        p.moveTo(0, 0);
                        p.lineTo(s.width(), 0);
                        p.lineTo(s.width(), s.height());
                        p.lineTo(0, s.height());
                        p.close();
                        return p.detach();
                      })
                      .stroke(std::move(b))));
  host.frame();
  EXPECT_EQ(host.pixel(50, 50), SK_ColorBLUE);  // the seam corner tile
}

// ---------------------------------------------------------------------------
// Stamped-brush bakes live with the INSTANCE, not inside the brush value.

TEST(ComposeBrushes, AStampBakeSurvivesABrushRebuiltEveryDescribe) {
  // A brush is a value an author rebuilds freely, so any cache stored in the
  // brush itself is empty on every describe. Here a FRESH Scatter value is
  // constructed each frame around pointer-stable art, on a node that
  // repaints every frame. The bake has to be keyed on the instance for the
  // art to rasterize once; keyed on the brush, re-describing a scene would
  // cost raster work, which nothing else in the library does.
  static int bakes;
  bakes = 0;
  const Element art =  // stable: its node pointer is the cache key
      box().width(8).height(8).child(
          custom([](SkCanvas& c, const PaintContext&) {
            ++bakes;
            SkPaint p;
            p.setColor(SK_ColorRED);
            c.drawRect(SkRect::MakeWH(8, 8), p);
          })
              .width(8)
              .height(8)
              .cache(Cache::None));
  Host host;
  auto tree = [&] {
    brush::Scatter s;  // fresh VALUE: empty member cache, on purpose
    s.art = art;
    s.spacing = 20;
    s.alignToPath = false;
    return box().child(box()
                           .width(120)
                           .height(120)
                           .cache(Cache::None)  // repaints every frame
                           .stroke(std::move(s)));
  };
  for (int i = 0; i < 5; ++i) {
    host.composer.render(tree());
    host.frame();
  }
  EXPECT_EQ(bakes, 1)
      << "a pointer-stable art re-baked under rebuilt brush values";
  int reds = 0;  // and the stamps really draw, wherever spacing lands them
  for (int x = 0; x < 130; ++x)
    for (int y = 0; y < 10; ++y) reds += host.pixel(x, y) == SK_ColorRED;
  EXPECT_GT(reds, 20);
}

TEST(ComposeBrushes, AFreshArtNodePerDescribeRebakesByContract) {
  // The same boundary from the other side (the case above is the hit
  // direction). The stamp cache is keyed on the art Element's NODE, so art
  // constructed INSIDE the describe is a fresh node every frame and re-bakes
  // every frame. That is the author's half of the bargain: keep the art
  // pointer-stable if you want the bake.
  //
  // It is also the safety direction. Because the key is node identity, a
  // fresh node is never served another node's bake, so art whose content
  // genuinely differs always reaches pixels. Any cache that hit here on
  // something weaker than real content identity would show the previous
  // frame's art instead — a stale picture with no diagnostic. A
  // content-identity bake cache could legitimately turn the
  // identical-content half of this into a hit, but only deliberately, and
  // the differing-content half below must keep failing it.
  static int bakes;
  bakes = 0;
  Host host;
  auto tree = [&](SkColor color) {
    Element art =  // fresh node EVERY call, on purpose — the contract's cost
        box().width(8).height(8).child(
            custom([color](SkCanvas& c, const PaintContext&) {
              ++bakes;
              SkPaint p;
              p.setColor(color);
              c.drawRect(SkRect::MakeWH(8, 8), p);
            })
                .width(8)
                .height(8)
                .cache(Cache::None));
    brush::Scatter s;
    s.art = std::move(art);
    s.spacing = 20;
    s.alignToPath = false;
    return box().child(box()
                           .width(120)
                           .height(120)
                           .cache(Cache::None)  // repaints every frame
                           .stroke(std::move(s)));
  };
  for (int i = 0; i < 4; ++i) {
    host.composer.render(tree(SK_ColorRED));
    host.frame();
  }
  EXPECT_EQ(bakes, 4) << "a fresh art node was served a bake it does not own";
  // And content that genuinely differs lands on pixels: the fifth
  // describe's art is GREEN, and green is what draws.
  host.composer.render(tree(SK_ColorGREEN));
  host.frame();
  EXPECT_EQ(bakes, 5);
  int greens = 0, reds = 0;
  for (int x = 0; x < 130; ++x)
    for (int y = 0; y < 10; ++y) {
      greens += host.pixel(x, y) == SK_ColorGREEN;
      reds += host.pixel(x, y) == SK_ColorRED;
    }
  EXPECT_GT(greens, 20);
  EXPECT_EQ(reds, 0) << "stale art survived a content change";
}
