// The brush engine: the layered stack, the geometry ops a brush is
// restyled with, the art warp, the hatch and the contour bands the gloss
// look is built from, and the pattern art a copy rebakes.

#include <sigilcompose/brush/Hatches.h>

#include "support/BrushTestSupport.h"

TEST(ComposeBrushEngine, PipelineStylesEveryLayer) {
  Host host;
  Brush b;
  b.shaped(geometry::shapers::Wave{.amplitude = 8, .wavelength = 24})
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
    b.shaped(geometry::shapers::Rounded{6})
        .shaped(geometry::shapers::Wave{.amplitude = 3, .wavelength = 30})
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
  // contours, which invents a channel the author never drew. A hairline
  // rec keeps them open.
  SkPathBuilder b;
  b.moveTo(0, 0);
  b.lineTo(300, 0);
  const SkPath jittered =
      geometry::shapers::Jitter{8, 2, 11}.shape(b.detach());
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
  b.layer(stroke(3, green()), {geometry::shapers::Offset{12}})
      .layer(stroke(3, blue()), {geometry::shapers::Offset{-12}});
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
  const SkPath boxy = geometry::shapers::Square{8, 80}.shape(b.detach());
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
    const SkPath shifted = sigil::geometry::path::parallel(route, -10.0f, step);
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
  const int sampled = total / 4;
  const float coverage = (float)lit / (float)sampled;
  EXPECT_GT(coverage, 0.05f);
  EXPECT_LT(coverage, 0.75f);
  // Nothing escapes the clip.
  EXPECT_EQ(host.pixel(30, 30), SK_ColorBLACK);
}

TEST(ComposeBrushes, PatternCopyRebakesAllChangedArt) {
  auto art = [](Fill fill) {
    return box().width(12).height(12).fill(std::move(fill));
  };
  brush::Pattern base;
  base.side = box().width(16).height(4).fill(green());
  base.start = art(red());
  base.end = art(red());
  base.corner = brush::CornerArt{art(red()), brush::CornerAlign::Bisector};
  base.advance = 16;

  // Aggregate copies intentionally share the memoization cache. The cache
  // therefore has to key every art slot, not only the side tile.
  brush::Pattern variant = base;
  variant.start = art(blue());
  variant.end = art(blue());
  variant.corner = brush::CornerArt{art(blue()), brush::CornerAlign::Bisector};

  auto lRun = [](brush::Pattern brush) {
    return box().child(box()
                           .absolute()
                           .inset(30)
                           .shape([](SkSize size) {
                             SkPathBuilder path;
                             path.moveTo(0, 0);
                             path.lineTo(size.width(), 0);
                             path.lineTo(size.width(), size.height());
                             return path.detach();
                           })
                           .stroke(std::move(brush)));
  };

  Host donor;
  donor.composer.render(lRun(base));
  donor.frame();  // primes the shared cache with red start/end/corner art
  EXPECT_EQ(donor.pixel(38, 30), SK_ColorRED);
  EXPECT_EQ(donor.pixel(170, 162), SK_ColorRED);

  Host changed;
  changed.composer.render(lRun(variant));
  changed.frame();
  EXPECT_EQ(changed.pixel(38, 30), SK_ColorBLUE);    // changed start art
  EXPECT_EQ(changed.pixel(170, 32), SK_ColorBLUE);   // changed corner art
  EXPECT_EQ(changed.pixel(170, 162), SK_ColorBLUE);  // changed end art
}

namespace {

/** A corner tile whose EXTENT is symmetric but whose colour is not: a 24x8
 *  bar, red on its local -x half and green on its local +x half. The stamp
 *  centres on the art's cull rect, so the midpoint of the two blobs is the
 *  placement and the vector between them is the rotation — both readable
 *  off the pixels, neither inferred. */
Element directedCornerTile() {
  return box()
      .width(24)
      .height(8)
      .child(box().absolute().left(0).top(0).width(12).height(8).fill(
          Fill::color({1, 0, 0, 1})))
      .child(box().absolute().left(12).top(0).width(12).height(8).fill(
          Fill::color({0, 1, 0, 1})));
}

struct Blob {
  double x = 0, y = 0;
  size_t n = 0;
  void add(int px, int py) {
    x += px;
    y += py;
    ++n;
  }
  SkPoint centre() const {
    return {(float)(x / (double)n), (float)(y / (double)n)};
  }
};

}  // namespace

TEST(ComposeBrushes, PatternCornerLandsOnTheVertexAndFacesTheBisector) {
  // Corner placement has two independent ways to go subtly wrong, and a
  // large corner tile against short sides is what makes both visible.
  //
  //  a. The tangent scan STRADDLES the vertex — it compares the tangent at
  //     d − step with the tangent at d — so a bend is first detected one
  //     step AFTER it happens. Taking the midpoint of that bracket as the
  //     vertex puts the art up to half a step along the outgoing leg. The
  //     corner has to be recovered from the two legs, not from the scan
  //     position.
  //  b. A bisector built by re-probing tangents at d ± ε is wrong for the
  //     same reason: from a point already past the vertex, both probes land
  //     on the same leg and every corner ends up facing the outgoing
  //     tangent. The one exception is a closed contour's seam at d = 0,
  //     whose probes wrap onto both legs — so on a rectangle three corners
  //     agree with each other and the fourth sits 45 degrees off, which
  //     reads as "the seam is special" rather than as a general error.
  //
  // A rectangle's vertices are exact, so both claims here are arithmetic
  // rather than approximate.
  Host host(400, 340);
  brush::Pattern brush;
  brush.side = box().width(24).height(10).child(
      box().absolute().left(2).top(4).width(20).height(2).fill(
          Fill::color({0.35f, 0.45f, 0.95f, 1})));
  brush.corner =
      brush::CornerArt{directedCornerTile(), brush::CornerAlign::Bisector};
  brush.advance = 24;
  brush.cornerLength = 40;
  brush.reach = 40;
  host.composer.render(box().child(box()
                                       .absolute()
                                       .inset(0)
                                       .shape([](SkSize) {
                                         SkPathBuilder p;
                                         p.moveTo(100, 100);
                                         p.lineTo(300, 100);
                                         p.lineTo(300, 240);
                                         p.lineTo(100, 240);
                                         p.close();
                                         return p.detach();
                                       })
                                       .stroke(std::move(brush))));
  host.frame();

  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(400, 340));
  ASSERT_TRUE(host.surface->readPixels(bm.pixmap(), 0, 0));
  // Quadrant assignment about the rect's centre — unbiased, and every tile
  // sits within 30 px of its own vertex, nowhere near the middle.
  Blob red[4], green[4];
  for (int y = 0; y < 340; ++y)
    for (int x = 0; x < 400; ++x) {
      const SkColor c = bm.getColor(x, y);
      const int r = (int)SkColorGetR(c), g = (int)SkColorGetG(c),
                b = (int)SkColorGetB(c);
      const int q = (x > 200 ? 1 : 0) + (y > 170 ? 2 : 0);
      if (r > 150 && r > 2 * g && r > 2 * b)
        red[q].add(x, y);
      else if (g > 150 && g > 2 * r && g > 2 * b)
        green[q].add(x, y);
    }

  // quadrant order: 0 = top-left, 1 = top-right, 2 = bottom-left, 3 = BR
  const SkPoint vertex[4] = {{100, 100}, {300, 100}, {100, 240}, {300, 240}};
  for (int q = 0; q < 4; ++q) {
    ASSERT_GT(red[q].n, 20u) << "no corner tile in quadrant " << q;
    ASSERT_GT(green[q].n, 20u) << "no corner tile in quadrant " << q;
    const SkPoint rc = red[q].centre(), gc = green[q].centre();
    const SkPoint place{(rc.x() + gc.x()) * 0.5f, (rc.y() + gc.y()) * 0.5f};
    EXPECT_NEAR(place.x(), vertex[q].x(), 1.5f)
        << "corner " << q << " landed off its vertex in x";
    EXPECT_NEAR(place.y(), vertex[q].y(), 1.5f)
        << "corner " << q << " landed off its vertex in y";
    // Every vertex of an axis-aligned rectangle bisects to a diagonal, so
    // |dx| == |dy|. The outgoing tangent is axis aligned and one of them
    // would be ~0.
    const float dx = std::abs(gc.x() - rc.x()), dy = std::abs(gc.y() - rc.y());
    EXPECT_NEAR(dx, dy, (dx + dy) * 0.2f)
        << "corner " << q << " faces (" << (gc.x() - rc.x()) << ", "
        << (gc.y() - rc.y()) << ") — not the bisector";
  }
}

TEST(ComposeBrushes, OutgoingCornerAlignmentFacesTheNextEdge) {
  // The other alignment is what a directional marker wants — an arrow that
  // turns a corner should keep pointing the way it is going.
  Host host(400, 340);
  brush::Pattern brush;
  brush.side =
      box().width(24).height(2).fill(Fill::color({0.2f, 0.2f, 0.6f, 1}));
  brush.corner =
      brush::CornerArt{directedCornerTile(), brush::CornerAlign::Outgoing};
  brush.advance = 24;
  brush.cornerLength = 40;
  brush.reach = 40;
  host.composer.render(box().child(box()
                                       .absolute()
                                       .inset(0)
                                       .shape([](SkSize) {
                                         SkPathBuilder p;
                                         p.moveTo(100, 100);
                                         p.lineTo(300, 100);
                                         p.lineTo(300, 240);
                                         p.lineTo(100, 240);
                                         p.close();
                                         return p.detach();
                                       })
                                       .stroke(std::move(brush))));
  host.frame();
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(400, 340));
  ASSERT_TRUE(host.surface->readPixels(bm.pixmap(), 0, 0));
  Blob red, green;
  for (int y = 0; y < 170; ++y)
    for (int x = 201; x < 400; ++x) {  // the top-right vertex only
      const SkColor c = bm.getColor(x, y);
      const int r = (int)SkColorGetR(c), g = (int)SkColorGetG(c),
                b = (int)SkColorGetB(c);
      if (r > 150 && r > 2 * g && r > 2 * b)
        red.add(x, y);
      else if (g > 150 && g > 2 * r && g > 2 * b)
        green.add(x, y);
    }
  ASSERT_GT(red.n, 20u);
  ASSERT_GT(green.n, 20u);
  // Leaving (300,100) the contour heads straight DOWN: dx ~ 0, dy > 0.
  const SkPoint rc = red.centre(), gc = green.centre();
  EXPECT_NEAR(gc.x() - rc.x(), 0.0f, 2.0f);
  EXPECT_GT(gc.y() - rc.y(), 6.0f);
}

TEST(ComposeBrushTail, GlossContourRingIsWhereTheCoverageSaysNotTheAlpha) {
  // A translucent gloss whose alpha equals the ring's centre must still
  // leave the deep interior at the fill: the ring reads coverage, and the
  // alpha only dims the ring it found.
  Host plain, glossed;
  auto shape = [] {
    return box()
        .absolute()
        .inset(50, 50, 50, 50)
        .corners({24})
        .fill(Fill::color({0.2f, 0.3f, 0.5f, 1}));
  };
  plain.composer.render(box().child(shape()));
  plain.frame();
  glossed.composer.render(box().child(shape().foreground(
      kit::gloss({1, 1, 1, 0.5f}, 8, {0, -4}, /*ringCenter=*/0.5f))));
  glossed.frame();
  int changed = 0;
  for (int y = 52; y < 148; y += 2)
    for (int x = 52; x < 148; x += 2)
      if (plain.pixel(x, y) != glossed.pixel(x, y)) ++changed;
  EXPECT_GT(changed, 40);                                     // a band
  EXPECT_EQ(plain.pixel(100, 100), glossed.pixel(100, 100));  // not a wash
  EXPECT_EQ(plain.pixel(30, 30), glossed.pixel(30, 30));  // and clipped out
}
