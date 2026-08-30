// The brush binary's share of ComposeTestEngineDocs.cpp: the suites whose
// subjects are brush-tier values, cut from that file so each test binary links
// only the target it exercises.

#include "support/BrushTestSupport.h"

TEST(ComposeBrushEngine, PipelineStylesEveryLayer) {
  Host host;
  Brush b;
  b.shaped(kit::brush::shapers::Wave{.amplitude = 8, .wavelength = 24})
      .layer(stroke(2, green()))
      .layer([] {
        brush::Scatter s;
        s.art = box().width(6).height(6).fill(red());
        s.spacing = 40;
        return s;
      }());
  host.composer.render(straightRun(std::move(b)));
  host.frame();
  int off = 0;
  for (int x = 30; x < 170; x += 2)
    for (int dy : {-7, 7}) off += host.pixel(x, 100 + dy) == SK_ColorGREEN;
  EXPECT_GT(off, 10);  // the stroke layer rides the waved pipeline
  int reds = 0;        // and the scatter layer rides the SAME waved geometry
  for (int x = 24; x < 176; ++x)
    for (int y = 84; y < 116; ++y) reds += host.pixel(x, y) == SK_ColorRED;
  EXPECT_GT(reds, 30);
}

TEST(ComposeBrushEngine, BrushPrunesAsOneValue) {
  Host host;
  auto tree = [] {
    Brush b;
    b.shaped(kit::brush::shapers::Rounded{6})
        .shaped(kit::brush::shapers::Wave{.amplitude = 3, .wavelength = 30})
        .layer(lines::cased(3, Fill::color({0, 1, 0, 1}), 5));
    return box().child(
        box().absolute().inset(40, 40, 40, 40).stroke(std::move(b)));
  };
  host.composer.render(tree());
  host.frame();
  host.composer.render(tree());  // fresh Elements, identical brush values
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
}

TEST(ComposeBrushEngine, SketchyKeepsOpenContoursOpen) {
  // Under a fill StrokeRec, SkDiscretePathEffect force-closes open
  // contours — the phantom-channel bug. Hairline rec keeps them open.
  SkPathBuilder b;
  b.moveTo(0, 0);
  b.lineTo(300, 0);
  const SkPath jittered =
      kit::brush::shapers::Jitter{8, 2, 11}.shape(b.detach());
  SkContourMeasureIter iter(jittered, false);
  float total = 0;
  bool anyClosed = false;
  while (sk_sp<SkContourMeasure> c = iter.next()) {
    total += c->length();
    anyClosed |= c->isClosed();
  }
  EXPECT_FALSE(anyClosed);
  EXPECT_LT(total, 400.0f);  // a closed loop would be ~2× the 300px run
}

TEST(ComposeBrushEngine, PerLayerShapersRideTheSharedPipeline) {
  // One Brush, two layers offset to opposite sides — the asymmetric casing
  // as a single material value. Positive `px` is LEFT of travel.
  Host host;
  Brush b;
  b.layer(stroke(3, green()), {kit::brush::shapers::Offset{12}})
      .layer(stroke(3, blue()), {kit::brush::shapers::Offset{-12}});
  host.composer.render(straightRun(std::move(b)));
  host.frame();
  EXPECT_EQ(host.pixel(100, 88), SK_ColorGREEN);   // left-of-travel rail
  EXPECT_EQ(host.pixel(100, 112), SK_ColorBLUE);   // right-of-travel rail
  EXPECT_EQ(host.pixel(100, 100), SK_ColorBLACK);  // nothing on the axis
}

TEST(ComposeBrushEngine, SquareWaveHoldsPlateausAndEndsOnAxis) {
  SkPathBuilder b;
  b.moveTo(0, 0);
  b.lineTo(320, 0);
  const SkPath boxy = kit::brush::shapers::Square{8, 80}.shape(b.detach());
  // Plateaus hold ±8 for half-wavelength runs; endpoints return to 0.
  const SkRect bounds = boxy.getBounds();
  EXPECT_NEAR(bounds.top(), -8, 0.5f);
  EXPECT_NEAR(bounds.bottom(), 8, 0.5f);
  SkPoint last;
  SkContourMeasureIter iter(boxy, false);
  sk_sp<SkContourMeasure> c = iter.next();
  ASSERT_TRUE(c);
  ASSERT_TRUE(c->getPosTan(c->length(), &last, nullptr));
  EXPECT_NEAR(last.y(), 0, 0.5f);  // zero-phase exit
  EXPECT_NEAR(last.x(), 320, 1.0f);
}

TEST(ComposeBrushEngine, AnExplicitIntervalIsNotOverriddenBySpacing) {
  // `spacing` is Interval-mode SUGAR: it stands in for an interval the
  // author did not set. That is why `interval` is an OPTIONAL rather than a
  // float with a default — with a default, "unset" would be a number an
  // author can type by accident, and typing exactly that number would
  // silently hand the placement over to `spacing` instead.
  auto stamps = [](std::optional<float> interval, float spacing) {
    Host host;
    brush::Scatter b;
    b.art = box().width(6).height(6).fill(red());
    b.spacing = spacing;
    b.place = {brush::Placement::Mode::Interval, interval};
    b.alignToPath = false;
    host.composer.render(box().child(box()
                                         .absolute()
                                         .inset(20, 20, 20, 20)
                                         .shape([](SkSize s) {
                                           SkPathBuilder p;
                                           p.moveTo(0, 0);
                                           p.lineTo(s.width(), 0);
                                           return p.detach();
                                         })
                                         .stroke(std::move(b))));
    host.frame();
    int runs = 0;
    bool inRun = false;
    for (int x = 0; x < 200; ++x) {
      const bool ink = host.pixel(x, 20) == SK_ColorRED;
      runs += ink && !inRun;
      inRun = ink;
    }
    return runs;
  };
  // 160 px of contour: ~7 stamps at 24 px, ~2 at 80 px.
  EXPECT_EQ(stamps(24.0f, 80.0f), stamps(24.0f, 24.0f))
      << "an explicit 24 means 24, whatever spacing says";
  EXPECT_GT(stamps(24.0f, 80.0f), stamps(std::nullopt, 80.0f))
      << "and unset still takes spacing";
}

TEST(ComposeBrushEngine, PlacementGrammarLandsOnRealVertices) {
  // Vertex family reads the path's actual verbs — stamps sit ON the bends.
  Host host;
  brush::Scatter b;
  b.art = box().width(8).height(8).fill(red());
  b.place = {brush::Placement::Mode::InnerVertices};
  b.alignToPath = false;
  host.composer.render(
      box().child(box()
                      .absolute()
                      .inset(40, 40, 40, 40)
                      .shape([](SkSize s) {
                        SkPathBuilder p;  // three segments, two bends
                        p.moveTo(0, s.height());
                        p.lineTo(60, s.height());
                        p.lineTo(60, 0);
                        p.lineTo(s.width(), 0);
                        return p.detach();
                      })
                      .stroke(std::move(b))));
  host.frame();
  EXPECT_EQ(host.pixel(100, 160), SK_ColorRED);   // bend 1 (60,120)+40
  EXPECT_EQ(host.pixel(100, 40), SK_ColorRED);    // bend 2 (60,0)+40
  EXPECT_EQ(host.pixel(40, 160), SK_ColorBLACK);  // endpoints excluded

  Host centers;
  brush::Scatter c;
  c.art = box().width(8).height(8).fill(blue());
  c.place = {brush::Placement::Mode::SegmentCenter};
  c.alignToPath = false;
  centers.composer.render(box().child(box()
                                          .absolute()
                                          .inset(40, 40, 40, 40)
                                          .shape([](SkSize s) {
                                            SkPathBuilder p;
                                            p.moveTo(0, 0);
                                            p.lineTo(s.width(), 0);
                                            return p.detach();
                                          })
                                          .stroke(std::move(c))));
  centers.frame();
  EXPECT_EQ(centers.pixel(100, 40), SK_ColorBLUE);  // the segment midpoint
}

TEST(ComposeBrushEngine, AlongGradientRampsOverTheArc) {
  Host host;
  lines::Line grad;
  grad.width = 8;
  grad.alongStops = {{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}};
  host.composer.render(straightRun(std::move(grad)));
  host.frame();
  const SkColor start = host.pixel(30, 100);
  const SkColor end = host.pixel(170, 100);
  EXPECT_GT(SkColorGetR(start), 200u);  // red end
  EXPECT_LT(SkColorGetB(start), 60u);
  EXPECT_GT(SkColorGetB(end), 200u);  // blue end
  EXPECT_LT(SkColorGetR(end), 60u);
}

TEST(ComposeLines, OffsetAlongClampsNonPositiveStep) {
  SkPathBuilder builder;
  builder.moveTo(10, 50);
  builder.lineTo(190, 50);
  const SkPath route = builder.detach();

  for (float step : {0.0f, -4.0f}) {
    const SkPath shifted = sigil::geometry::parallel(route, -10.0f, step);
    ASSERT_FALSE(shifted.isEmpty()) << "step=" << step;
    EXPECT_NEAR(shifted.getBounds().top(), 60.0f, 0.01f);
    EXPECT_NEAR(shifted.getBounds().bottom(), 60.0f, 0.01f);
  }
}

TEST(ComposeDecorations, ContourWalkCopyRebakesChangedStamp) {
  ContourWalk base;
  base.spacing = 1000.0f;  // one stamp at the open route's first point
  base.stamp = box().width(12).height(12).fill(red());
  ContourWalk variant = base;
  variant.stamp = box().width(12).height(12).fill(blue());

  Host donor;
  donor.composer.render(straightRun(base));
  donor.frame();  // primes the shared cache with the red stamp
  EXPECT_EQ(donor.pixel(20, 100), SK_ColorRED);

  Host changed;
  changed.composer.render(straightRun(variant));
  changed.frame();
  EXPECT_EQ(changed.pixel(20, 100), SK_ColorBLUE);
}

TEST(ComposeTrim, PathFormatOpenContourWrapKeepsTwoPieces) {
  // PathFormat owns a separate wrapping trim window. It must apply the same
  // open-contour rule as node-level trim instead of connecting both pieces.
  Host host;
  PathFormat format;
  format.width = 6;
  format.strokeFill = green();
  format.trimStart = 0.9f;
  format.trimEnd = 1.2f;
  host.composer.render(box().child(box()
                                       .absolute()
                                       .inset(20, 80, 20, 80)
                                       .shape([](SkSize s) {
                                         SkPathBuilder b;
                                         b.moveTo(0, s.height() / 2);
                                         b.lineTo(s.width(), s.height() / 2);
                                         return b.detach();
                                       })
                                       .stroke(format)));
  host.frame();
  EXPECT_EQ(host.pixel(170, 100), SK_ColorGREEN);  // tail piece [0.9, 1]
  EXPECT_EQ(host.pixel(40, 100), SK_ColorGREEN);   // head piece [0, 0.2]
  EXPECT_EQ(host.pixel(100, 100), SK_ColorBLACK);  // NO invented chord
}

TEST(ComposeBrushTail, BrushArtWarpsArtAlongTheOutline) {
  Host host;
  // A straight horizontal outline through the node's middle: the warped
  // ribbon must be a horizontal band of the art's height around it.
  auto lineOutline = [](SkSize s) {
    SkPathBuilder b;
    b.moveTo(0, s.height() / 2);
    b.lineTo(s.width(), s.height() / 2);
    return b.detach();
  };
  brush::Art brush = brush::artAlong(
      box().width(40).height(20).fill(Fill::color({1, 1, 1, 1})), 20);
  host.composer.render(box().child(box()
                                       .absolute()
                                       .inset(20, 60, 20, 60)
                                       .shape(lineOutline)
                                       .foreground(brush)));
  host.frame();
  EXPECT_EQ(host.pixel(100, 100), SK_ColorWHITE);  // on the ribbon
  EXPECT_EQ(host.pixel(100, 130), SK_ColorBLACK);  // 30px off: outside height
}

TEST(ComposeBrushTail, HatchFillsInteriorSparsely) {
  Host host;
  host.composer.render(box().child(
      box()
          .absolute()
          .inset(50, 50, 50, 50)
          .background(lines::hatch(Fill::color({1, 1, 1, 1}), 8, 1.5f, 45))));
  host.frame();
  // Count lit pixels in the hatched interior: strictly between "empty"
  // and "solid fill" — the lattice is present but sparse.
  int lit = 0;
  const int total = 100 * 100;
  for (int y = 50; y < 150; y += 2)
    for (int x = 50; x < 150; x += 2)
      if (host.pixel(x, y) != SK_ColorBLACK) ++lit;
  const float coverage = (float)lit / (float)(total / 4);
  EXPECT_GT(coverage, 0.05f);
  EXPECT_LT(coverage, 0.75f);
  // Nothing escapes the clip.
  EXPECT_EQ(host.pixel(30, 30), SK_ColorBLACK);
}
