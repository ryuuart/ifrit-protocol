// The brush binary's share of ComposeTestTextData.cpp: the suites whose
// subjects are brush-tier values, cut from that file so each test binary links
// only the target it exercises.

#include "support/BrushTestSupport.h"

TEST(ComposeDecorations, RadialHatchFansOutOfAPointAndRingsRoundIt) {
  // lines::hatch is a parallel lattice at one fixed angle, which is the
  // wrong field for anything engraved out of a point. The Chladni study
  // built its radial fan from 120 shapes::sector sub-wedges each carrying
  // a rotated Hatch — correct, and 120 nodes for one field.
  auto lit = [](Host& host, int x0, int y0, int x1, int y1) {
    int n = 0;
    for (int y = y0; y < y1; ++y)
      for (int x = x0; x < x1; ++x) n += host.pixel(x, y) != SK_ColorBLACK;
    return n;
  };

  Host fan(200, 200);
  fan.composer.render(
      box().child(box()
                      .absolute()
                      .inset(20)
                      .shape(shapes::circle())
                      .background(lines::radialHatch(Fill::color({1, 1, 1, 1}),
                                                     32, 1.5f))));
  fan.frame();
  // Ink everywhere around the rim…
  EXPECT_GT(lit(fan, 20, 20, 180, 180), 800);
  // …and the hole at the centre is genuinely empty, which is the whole
  // reason holeFraction exists: a fan out of a point goes solid there.
  EXPECT_EQ(lit(fan, 96, 96, 104, 104), 0);
  // Clipped to the outline, so the box corners stay dark.
  EXPECT_EQ(lit(fan, 20, 20, 32, 32), 0);

  Host rings(200, 200);
  rings.composer.render(box().child(
      box()
          .absolute()
          .inset(20)
          .shape(shapes::circle())
          .background(lines::concentric(Fill::color({1, 1, 1, 1}), 8, 1.5f))));
  rings.frame();
  EXPECT_GT(lit(rings, 20, 20, 180, 180), 800);
  EXPECT_EQ(lit(rings, 20, 20, 32, 32), 0);

  // They are different FIELDS, not one rotated: spokes converge, so a
  // fan's ink density climbs toward the centre, while evenly spaced
  // rings hold theirs. (Counting crossings along one scanline would not
  // show this — a spoke can lie along the scanline.)
  auto density = [](Host& host, float r0, float r1) {
    int ink = 0, area = 0;
    for (int y = 0; y < 200; ++y)
      for (int x = 0; x < 200; ++x) {
        const float d = std::hypot((float)x - 100.0f, (float)y - 100.0f);
        if (d < r0 || d >= r1) continue;
        ++area;
        ink += host.pixel(x, y) != SK_ColorBLACK;
      }
    return area > 0 ? (float)ink / (float)area : 0.0f;
  };
  const float fanInner = density(fan, 22, 34), fanOuter = density(fan, 66, 78);
  const float ringInner = density(rings, 22, 34),
              ringOuter = density(rings, 66, 78);
  EXPECT_GT(fanInner, fanOuter * 1.6f);
  EXPECT_NEAR(ringInner, ringOuter, std::max(ringOuter * 0.6f, 0.02f));
}

TEST(ComposeDecorations, EachStrokeCarriesItsOwnTrimWindow) {
  // Recorded twice as "one trim window per node", which is wrong and
  // worth pinning: PathFormat has had trimStart/trimEnd/trimOffset/
  // trimPhase all along, and the windows COMPOSE — a decoration receives
  // the node's already-trimmed outline, so a second window is a fraction
  // of the revealed part. That is exactly the pen-tip-behind-the-head
  // case that got rebuilt as a duplicate node re-measuring the same
  // 2000-segment path.
  auto lit = [](Host& host, int x0, int y0, int x1, int y1) {
    int n = 0;
    for (int y = y0; y < y1; ++y)
      for (int x = x0; x < x1; ++x) n += host.pixel(x, y) != SK_ColorBLACK;
    return n;
  };
  auto line = [](SkSize) {
    SkPathBuilder b;
    b.moveTo(10, 100).lineTo(190, 100);
    return b.detach();
  };

  // Node gated to the first 60% — one geometry, two windows on it:
  // a wide dim body over all of it, and a bright sliver at its head.
  PathFormat head = stroke(6, Fill::color({0, 1, 0, 1}));
  head.trimStart = 0.90f;
  head.trimEnd = 1.0f;
  Host host(200, 200);
  host.composer.render(
      box().child(box()
                      .absolute()
                      .inset(0)
                      .shape(line)
                      .fill(Fill::none())
                      .mask(by::spans(spans::upTo(0.6f)))
                      .foreground(stroke(3, Fill::color({1, 0, 0, 1})))
                      .foreground(head)));
  host.frame();

  // The body reaches ~x118 (10 + 0.6*180); nothing past it.
  EXPECT_GT(lit(host, 20, 95, 110, 105), 100);
  EXPECT_EQ(lit(host, 130, 95, 190, 105), 0);

  // The head sliver is the LAST tenth of the revealed part, so it is
  // green near x118 and red back at x40 — two independent windows.
  int greenHead = 0, redBody = 0;
  for (int x = 100; x < 120; ++x)
    greenHead += SkColorGetG(host.pixel(x, 100)) > 180 &&
                 SkColorGetR(host.pixel(x, 100)) < 80;
  for (int x = 20; x < 60; ++x)
    redBody += SkColorGetR(host.pixel(x, 100)) > 180 &&
               SkColorGetG(host.pixel(x, 100)) < 80;
  EXPECT_GT(greenHead, 5);
  EXPECT_GT(redBody, 20);
}

TEST(ComposeDecorations, OverlayPaintsOverTheFillAndUnderTheContent) {
  // overlay() is the slot BETWEEN the other two: background() hides beneath
  // the fill, foreground() paints above the children. Without a middle slot,
  // a texture applied to a button greys out its own label — hazard stripes
  // over the surface but under the digit is the case that needs it.
  auto build = [](bool useForeground) {
    auto bars = lines::hatch(Fill::color({0, 0, 0, 1}), 6.0f, 4.0f, 0.0f);
    Element cell =
        box()
            .absolute()
            .inset(0)
            .fill(Fill::color({1, 1, 1, 1}))
            .child(box().absolute().left(60).top(60).width(80).height(80).fill(
                Fill::color({0, 1, 0, 1})));
    return useForeground ? cell.foreground(bars) : cell.overlay(bars);
  };
  auto greenPixels = [](Host& host) {
    int n = 0;
    for (int y = 62; y < 138; ++y)
      for (int x = 62; x < 138; ++x) {
        const SkColor c = host.pixel(x, y);
        n += SkColorGetG(c) > 180 && SkColorGetR(c) < 80;
      }
    return n;
  };

  Host over(200, 200), under(200, 200);
  over.composer.render(box().child(build(/*useForeground=*/true)));
  over.frame();
  under.composer.render(box().child(build(/*useForeground=*/false)));
  under.frame();

  // foreground(): the stripes cross the child and eat into it.
  // overlay(): the child is painted after them and comes through whole.
  EXPECT_GT(greenPixels(under), greenPixels(over) + 500);
  // The stripes still reach the surface OUTSIDE the child in both.
  int outsideInk = 0;
  for (int x = 10; x < 50; ++x)
    outsideInk += under.pixel(x, 20) == SK_ColorBLACK;
  EXPECT_GT(outsideInk, 5);
}

TEST(ComposeDecorations, DashPhaseCanBeBoundSoDashesMarch) {
  // trimPhase took a bound Output and declared isAnimated(); dashPhase was
  // a plain float, so marching ants — the commonest animated-line idiom
  // in map and diagram UI — could only be had by re-describing every
  // frame, which defeats the pruning the library is built on. A study
  // wrote a 25-line DecorationScheme for want of this.
  choreograph::Output<float> march{0.0f};
  PathFormat dashed = stroke(4, Fill::color({1, 1, 1, 1}));
  dashed.dashIntervals = {10.0f, 10.0f};
  dashed.dashPhaseBinding = &march;

  Host host(200, 200);
  host.composer.render(box().child(box()
                                       .absolute()
                                       .left(0)
                                       .top(90)
                                       .width(200)
                                       .height(20)
                                       .fill(Fill::none())
                                       .foreground(dashed)));
  host.frame();
  auto row = [&] {
    std::vector<bool> lit;
    for (int x = 0; x < 200; ++x)
      lit.push_back(host.pixel(x, 90) != SK_ColorBLACK);
    return lit;
  };
  const std::vector<bool> before = row();

  // Advance the phase by half a dash — no render(), no re-describe.
  march = 10.0f;
  host.frame();
  const std::vector<bool> after = row();

  int moved = 0;
  for (size_t i = 0; i < before.size(); ++i) moved += before[i] != after[i];
  EXPECT_GT(moved, 20);  // the dashes actually travelled
}

TEST(ComposeDecorations, TheBrushVocabularyWorksOnGeometryYouBuiltYourself) {
  // The roadmap recorded that live geometry in custom() forfeits the
  // decoration vocabulary along with pruning. Half wrong, and caught by a
  // researcher reading the source: PathFormat, lines::Line and Brush read
  // ONLY PaintContext::outline, Decoration::paint is public, and
  // PaintContext is a plain aggregate. So a simulated rope, a live EQ
  // curve or a plotted signal can wear all of it — including
  // PathFormat's own trim window, which is the part the roadmap said was
  // lost.
  PathFormat dashedHead = stroke(5, Fill::color({0, 1, 0, 1}));
  dashedHead.trimStart = 0.75f;  // the last quarter only
  dashedHead.trimEnd = 1.0f;

  Host host(200, 200);
  host.composer.render(box().child(
      custom([dashedHead](SkCanvas& canvas, const PaintContext& ctx) {
        // Geometry computed HERE, per paint — the case the roadmap
        // claimed could not be decorated.
        SkPathBuilder b;
        b.moveTo(10, 100).lineTo(190, 100);
        decorations::paintOn(canvas, ctx, b.detach(), dashedHead);
      })
          .absolute()
          .inset(0)
          .cache(Cache::None)));
  host.frame();

  auto lit = [&](int x0, int x1) {
    int n = 0;
    for (int x = x0; x < x1; ++x) n += SkColorGetG(host.pixel(x, 100)) > 180;
    return n;
  };
  // The decoration's own trim window applied to hand-built geometry:
  // the last quarter is drawn, the first three are not.
  EXPECT_GT(lit(150, 190), 30);
  EXPECT_EQ(lit(15, 130), 0);
}

TEST(ComposeDecorations, WashFloodsTheOutlineWithAMaterialAndPrunes) {
  // The decoration primitives were PathFormat (strokes), Slice,
  // ContourWalk and a raw PaintProgram — none of which fills a shape with
  // a Material. So "a grain pass over this whole panel, ABOVE its
  // children" had to be a raw PaintProgram, an incomparable std::function
  // that never prunes. overlay() is a different slot: it paints UNDER the
  // children.
  auto build = [](float amount) {
    return box().child(
        box()
            .absolute()
            .inset(0)
            .fill(Fill::color({0, 0, 0, 1}))
            .child(box().absolute().inset(40).fill(Fill::color({0, 0, 1, 1})))
            .foreground(decorations::wash(Material::solid({1, 0, 0, 1}),
                                          SkBlendMode::kPlus, amount)));
  };
  Host full(120, 120), half(120, 120), none(120, 120);
  full.composer.render(build(1.0f));
  full.frame();
  half.composer.render(build(0.5f));
  half.frame();
  none.composer.render(build(0.0f));
  none.frame();

  // It reaches OVER the child, which is what foreground() means and what
  // overlay() deliberately does not do.
  EXPECT_GT(SkColorGetR(full.pixel(60, 60)), 200);
  EXPECT_GT(SkColorGetB(full.pixel(60, 60)), 200);  // kPlus kept the blue
  // amount is a real dial, not a flag.
  EXPECT_NEAR(SkColorGetR(half.pixel(60, 60)), 128, 12);
  EXPECT_EQ(SkColorGetR(none.pixel(60, 60)), 0);

  // And it is a comparable VALUE, so a static wash prunes.
  EXPECT_TRUE(decorations::wash(Material::solid({1, 0, 0, 1}),
                                SkBlendMode::kPlus, 0.5f) ==
              decorations::wash(Material::solid({1, 0, 0, 1}),
                                SkBlendMode::kPlus, 0.5f));
  EXPECT_FALSE(decorations::wash(Material::solid({1, 0, 0, 1})) ==
               decorations::wash(Material::solid({0, 1, 0, 1})));
}

TEST(ComposeDecorations, PathFormatCarriesStrokeCapAndJoin) {
  // ~30 open contours of line art all ended square and mitred because
  // this paint was built and never asked.
  auto elbow = [](SkSize) {
    SkPathBuilder b;
    b.moveTo(40, 40).lineTo(160, 40).lineTo(160, 160);
    return b.detach();
  };
  auto corner = [&](SkPaint::Join join) {
    PathFormat f = stroke(24, Fill::color({1, 1, 1, 1}));
    f.join = join;
    Host host(200, 200);
    host.composer.render(box().child(box()
                                         .absolute()
                                         .inset(0)
                                         .shape(elbow)
                                         .fill(Fill::none())
                                         .foreground(f)));
    host.frame();
    // The outer corner of the elbow: a miter reaches it, a round does not.
    return host.pixel(171, 29) != SK_ColorBLACK;
  };
  EXPECT_TRUE(corner(SkPaint::kMiter_Join));
  EXPECT_FALSE(corner(SkPaint::kRound_Join));
}

TEST(ComposeDecorations, AStrokeCanTakeAMaterial) {
  // fill() takes a Material — unit-square authoring, structural
  // comparison, live uniforms — and a stroke took only the kernel Fill,
  // which is node-local pixels compared by shader pointer. On an object
  // whose surfaces are mostly STROKES, that meant writing the same brass
  // twice, once per return type.
  PathFormat f = stroke(30, Fill::color({1, 1, 1, 1}));
  f.strokeMaterial = Material::linearUnit(
      {0, 0}, {1, 0}, {{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}});
  Host host(200, 200);
  host.composer.render(
      box().child(box().absolute().inset(20).fill(Fill::none()).foreground(f)));
  host.frame();
  // The ramp runs across the node, so the left edge of the stroke is red
  // and the right edge blue — a Fill could only have been one colour.
  EXPECT_GT(SkColorGetR(host.pixel(20, 100)), 150);
  EXPECT_LT(SkColorGetB(host.pixel(20, 100)), 110);
  EXPECT_GT(SkColorGetB(host.pixel(180, 100)), 150);
  EXPECT_LT(SkColorGetR(host.pixel(180, 100)), 110);
}
