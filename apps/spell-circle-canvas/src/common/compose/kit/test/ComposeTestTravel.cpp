// Travel along a path, and the shape seam a node's silhouette plugs: what
// a rider reads at a fraction of the way round, and what a kit silhouette
// is when the kernel asks for one.

#include "support/ShapeTestSupport.h"


TEST(ComposeShapes, ArrowPointsAlongPositiveX) {
  Host host(120, 60);
  host.composer.render(
      box().child(box()
                      .width(120)
                      .height(60)
                      .absolute()
                      .left(0)
                      .top(0)
                      .shape(geometry::shapes::arrow())
                      .fill(material::skia::Paint::solid({0, 1, 0, 1}))));
  host.frame();
  EXPECT_GT(SkColorGetG(host.pixel(20, 30)), 200u);  // shaft on the axis
  EXPECT_LT(SkColorGetG(host.pixel(20, 6)), 60u);    // and not above it
  EXPECT_GT(SkColorGetG(host.pixel(80, 12)), 200u);  // head is tall
  EXPECT_LT(SkColorGetG(host.pixel(118, 12)), 60u);  // and tapers to a point
}

TEST(ComposeShapes, SectorIsClosedAndFillable) {
  // geometry::shapes::arc() is open by contract; a pie wedge needs a closed
  // path. A 90-degree sector starting at 0 (Skia: 0 = +x, clockwise) fills the
  // lower-right quadrant of its box and nothing else.
  Host host(200, 200);
  host.composer.render(
      box().child(box()
                      .width(200)
                      .height(200)
                      .absolute()
                      .inset(0)
                      .shape(geometry::shapes::sector(0, 90))
                      .fill(material::skia::Paint::solid({1, 0, 0, 1}))));
  host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(130, 130)), 200u);  // inside the wedge
  EXPECT_LT(SkColorGetR(host.pixel(70, 130)), 60u);    // lower-left: outside
  EXPECT_LT(SkColorGetR(host.pixel(130, 70)), 60u);    // upper-right: outside

  // innerRatio carves the donut hole out of the middle.
  Host donut(200, 200);
  donut.composer.render(
      box().child(box()
                      .width(200)
                      .height(200)
                      .absolute()
                      .inset(0)
                      .shape(geometry::shapes::sector(0, 350, 0.6f))
                      .fill(material::skia::Paint::solid({1, 0, 0, 1}))));
  donut.frame();
  EXPECT_GT(SkColorGetR(donut.pixel(180, 100)), 200u);  // on the ring
  EXPECT_LT(SkColorGetR(donut.pixel(100, 100)), 60u);   // through the hole
}

TEST(ComposeMaterial, LiveMaterialOnOutlineShapeFillsTheShape) {
  // A live material over a custom outline(): the resolved shader must
  // fill the SHAPE, not the box that contains it, and follow the Output.
  choreograph::Output<float> k{1.0f};
  Host host;
  host.composer.render(box().child(
      box()
          .width(100)
          .height(100)
          .inset(0, 0, 100, 100)
          .absolute()
          .shape(geometry::shapes::star(4, 0.3f))
          .fill(material::skia::Paint::sksl(ukEffect()).uniform("uK", &k))));
  host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(50, 50)), 200u);  // star body
  EXPECT_LT(SkColorGetR(host.pixel(8, 8)), 30u);     // outside the arms
  k = 0.2f;                                          // no render()
  host.frame();
  const uint32_t dim = SkColorGetR(host.pixel(50, 50));
  EXPECT_GT(dim, 25u);
  EXPECT_LT(dim, 90u);  // tracked the Output inside the shape
}

namespace {

/** The centroid of every pixel of @p color, or (-1,-1) when none. Motion is
 *  a VISUAL feature: these pins scan the frame, they do not read floats out
 *  of the resolver. */
SkPoint inkCentroid(Host& host, SkColor color, int w, int h) {
  double sx = 0, sy = 0;
  int n = 0;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x)
      if (host.pixel(x, y) == color) {
        sx += x;
        sy += y;
        ++n;
      }
  if (n == 0) return {-1, -1};
  return {(float)(sx / n), (float)(sy / n)};
}

/** The bounding box of everything even faintly @p color-ish — enough to
 *  ask "is this bar lying flat or standing up", which is what an
 *  orientation pin actually wants to know. */
SkIRect inkBounds(Host& host, int w, int h) {
  SkIRect box = SkIRect::MakeEmpty();
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x)
      if (SkColorGetR(host.pixel(x, y)) > 100) {
        const SkIRect one = SkIRect::MakeXYWH(x, y, 1, 1);
        if (box.isEmpty())
          box = one;
        else
          box.join(one);
      }
  return box;
}

/** A 160x160 frame inset at (20,20) of a 200x200 canvas, carrying one small
 *  square that travels. The inscribed circle is then centre (100,100) r=80
 *  in CANVAS coordinates, so every quadrant point is on-screen. */
Element travelFrame(Element rider) {
  return box().child(box()
                         .key("frame")
                         .absolute()
                         .rect(SkRect::MakeXYWH(20, 20, 160, 160))
                         .child(std::move(rider)));
}

Element rider(MotionPath along, float size = 8) {
  return box()
      .key("dot")
      .absolute()
      .rect(SkRect::MakeXYWH(0, 0, size, size))
      .fill(red())
      .travel(std::move(along));
}

}  // namespace

TEST(ComposeTravel, PlacesTheTransformOriginOnTheParentSizedCurve) {
  // THE PIXEL PIN. Four values of t, four quadrant points of the circle
  // inscribed in the PARENT's box — not the rider's own 8x8 box, which is
  // the whole difficulty a size-dependent Shape brings that CameraPath
  // never faced.
  Host host(200, 200);
  choreograph::Output<float> t{0};
  const auto describe = [&] {
    return travelFrame(rider({.path = geometry::shapes::circle(), .t = &t}));
  };

  // Skia's addOval(dir=kCW, startIndex=1) begins at the RIGHT extreme and
  // runs clockwise, so quarter turns are right → bottom → left → top.
  const SkPoint want[4] = {{180, 100}, {100, 180}, {20, 100}, {100, 20}};
  for (int i = 0; i < 4; ++i) {
    t = (float)i * 0.25f;
    host.composer.render(describe());
    host.frame();
    const SkPoint ink = inkCentroid(host, SK_ColorRED, 200, 200);
    ASSERT_GE(ink.x(), 0) << "nothing painted at t=" << t.value();
    EXPECT_NEAR(ink.x(), want[i].x(), 1.5f) << "t=" << t.value();
    EXPECT_NEAR(ink.y(), want[i].y(), 1.5f) << "t=" << t.value();
  }

  // …and the point that rides is the TRANSFORM ORIGIN, so moving the origin
  // to the rider's top-left offsets the whole ink by half its box.
  t = 0.0f;
  host.composer.render(
      travelFrame(rider({.path = geometry::shapes::circle(), .t = &t})
                      .transformOrigin(0, 0)));
  host.frame();
  const SkPoint pinned = inkCentroid(host, SK_ColorRED, 200, 200);
  EXPECT_NEAR(pinned.x(), 184.0f, 1.5f)
      << "transformOrigin() is not the point on the curve";
  EXPECT_NEAR(pinned.y(), 104.0f, 1.5f);
}

TEST(ComposeTravel, WrapsOnAClosedCurveAndClampsOnAnOpenOne) {
  Host host(200, 200);
  choreograph::Output<float> t{0};
  const auto atT = [&](Shape path, float value) {
    t = value;
    host.composer.render(
        travelFrame(rider({.path = std::move(path), .t = &t})));
    host.frame();
    return inkCentroid(host, SK_ColorRED, 200, 200);
  };

  // Closed: a lap and a quarter is a quarter, and negative runs backwards.
  const SkPoint quarter = atT(geometry::shapes::circle(), 0.25f);
  const SkPoint lapAndAQuarter = atT(geometry::shapes::circle(), 1.25f);
  EXPECT_NEAR(lapAndAQuarter.x(), quarter.x(), 1.0f) << "a closed curve did "
                                                        "not WRAP";
  EXPECT_NEAR(lapAndAQuarter.y(), quarter.y(), 1.0f);
  const SkPoint back = atT(geometry::shapes::circle(), -0.25f);
  const SkPoint threeQuarters = atT(geometry::shapes::circle(), 0.75f);
  EXPECT_NEAR(back.x(), threeQuarters.x(), 1.0f);
  EXPECT_NEAR(back.y(), threeQuarters.y(), 1.0f);

  // Open: both ends park. The line runs the frame's full width at mid-height.
  const auto line = [](SkSize s) {
    SkPathBuilder b;
    b.moveTo(0, s.height() / 2);
    b.lineTo(s.width(), s.height() / 2);
    return b.detach();
  };
  const SkPoint past = atT(line, 1.5f);
  EXPECT_NEAR(past.x(), 180.0f, 1.5f) << "an open curve did not CLAMP";
  const SkPoint before = atT(line, -0.5f);
  EXPECT_NEAR(before.x(), 20.0f, 1.5f);
}

TEST(ComposeTravel, OutranksTheTranslateLanesAndHandsThemBack) {
  Host host(200, 200);
  choreograph::Output<float> t{0.25f};
  // A path and a contradicting lane on the same node: the path wins whole.
  host.composer.render(
      travelFrame(rider({.path = geometry::shapes::circle(), .t = &t})
                      .translateX(-60)
                      .translateY(-60)));
  host.frame();
  SkPoint ink = inkCentroid(host, SK_ColorRED, 200, 200);
  EXPECT_NEAR(ink.x(), 100.0f, 1.5f) << "the lanes were blended into the path";
  EXPECT_NEAR(ink.y(), 180.0f, 1.5f);

  // Drop the path and the very same lanes take over, live.
  host.composer.render(travelFrame(box()
                                       .key("dot")
                                       .absolute()
                                       .rect(SkRect::MakeXYWH(0, 0, 8, 8))
                                       .fill(red())
                                       .translateX(-60)
                                       .translateY(-60)));
  host.frame();
  ink = inkCentroid(host, SK_ColorRED, 200, 200);
  EXPECT_LT(ink.x(), 0) << "the lanes should have taken the dot off-canvas";
}

TEST(ComposeTravel, AutoOrientAddsToRotateAndHoldsTheLastGoodChord) {
  // A 40x4 bar: WIDE at 0 degrees, TALL at 90. At t=0 on a clockwise circle
  // the tangent points straight down, so auto-orient must stand it up.
  Host host(200, 200);
  choreograph::Output<float> t{0};
  const auto bar = [&](float lookAhead, std::optional<float> spin) {
    Element e = box()
                    .key("dot")
                    .absolute()
                    .rect(SkRect::MakeXYWH(0, 0, 40, 4))
                    .fill(red())
                    .travel({.path = geometry::shapes::circle(),
                             .t = &t,
                             .lookAhead = lookAhead});
    if (spin) e.rotate(*spin);
    return travelFrame(std::move(e));
  };

  host.composer.render(bar(0.0f, std::nullopt));
  host.frame();
  SkIRect ink = inkBounds(host, 200, 200);
  EXPECT_GT(ink.width(), 3 * ink.height()) << "the bar is not lying flat";

  host.composer.render(bar(0.02f, std::nullopt));
  host.frame();
  ink = inkBounds(host, 200, 200);
  EXPECT_GT(ink.height(), 3 * ink.width())
      << "auto-orient did not turn the bar onto the tangent";

  // …and an authored rotate() ADDS to it (90 + 90 lies flat again). If the
  // path replaced rotate() the bar would still be standing.
  host.composer.render(bar(0.02f, 90.0f));
  host.frame();
  ink = inkBounds(host, 200, 200);
  EXPECT_GT(ink.width(), 3 * ink.height())
      << "auto-orient REPLACED rotate() instead of composing with it";

  // At the far end of an OPEN curve the forward chord collapses. The last
  // good one is held, so a path that ends going DOWN leaves the bar standing
  // rather than snapping back to zero.
  const auto ell = [](SkSize) {
    SkPathBuilder b;
    b.moveTo(0, 0);
    b.lineTo(100, 0);
    b.lineTo(100, 60);
    return b.detach();
  };
  t = 1.0f;
  host.composer.render(
      travelFrame(box()
                      .key("dot")
                      .absolute()
                      .rect(SkRect::MakeXYWH(0, 0, 40, 4))
                      .fill(red())
                      .travel({.path = ell, .t = &t, .lookAhead = 0.02f})));
  host.frame();
  ink = inkBounds(host, 200, 200);
  EXPECT_GT(ink.height(), 3 * ink.width())
      << "the collapsed end chord was not replaced by the last good one";
}

TEST(ComposeTravel, PrunesOnlyWhenEveryFieldOfThePathMatches) {
  // THE PRUNE PIN. A motion path is read live at paint, so every field of
  // it must participate in reconciler equality — one control per field.
  Host host(200, 200);
  const auto describe = [](MotionPath p) {
    return travelFrame(rider(std::move(p)));
  };
  const auto renderAndCount = [&](MotionPath p) {
    host.composer.render(describe(std::move(p)));
    host.frame();
    return host.composer.stats().patchedNodes;
  };

  renderAndCount(
      {.path = geometry::shapes::circle(), .t = 0.25f, .lookAhead = 0.02f});
  EXPECT_EQ(
      renderAndCount(
          {.path = geometry::shapes::circle(), .t = 0.25f, .lookAhead = 0.02f}),
      0u)
      << "an identical comparable scheme did not prune";

  EXPECT_EQ(renderAndCount({.path = geometry::shapes::polygon(6),
                            .t = 0.25f,
                            .lookAhead = 0.02f}),
            1u)
      << "the path FIELD does not participate in equality";
  EXPECT_EQ(renderAndCount({.path = geometry::shapes::polygon(6),
                            .t = 0.60f,
                            .lookAhead = 0.02f}),
            1u)
      << "the t FIELD does not participate in equality";
  EXPECT_EQ(renderAndCount({.path = geometry::shapes::polygon(6),
                            .t = 0.60f,
                            .lookAhead = 0.05f}),
            1u)
      << "the lookAhead FIELD does not participate in equality";

  // Gaining and losing the path is itself a patch.
  host.composer.render(travelFrame(box()
                                       .key("dot")
                                       .absolute()
                                       .rect(SkRect::MakeXYWH(0, 0, 8, 8))
                                       .fill(red())));
  host.frame();
  EXPECT_EQ(host.composer.stats().patchedNodes, 1u)
      << "dropping travel() pruned into the travelling description";

  // The escape hatch keeps its documented cost: a raw callable never
  // compares equal, so a travelling node built from one never prunes.
  const auto raw = [] {
    return travelFrame(rider({.path =
                                  [](SkSize s) {
                                    SkPathBuilder b;
                                    b.addOval(
                                        SkRect::MakeWH(s.width(), s.height()));
                                    return b.detach();
                                  },
                              .t = 0.25f}));
  };
  host.composer.render(raw());
  host.frame();
  host.composer.render(raw());
  host.frame();
  EXPECT_EQ(host.composer.stats().patchedNodes, 1u)
      << "a raw-callable motion path compared equal — the shape seam's "
         "escape-hatch contract is not being carried through travel()";
}

TEST(ComposeTravel, IsPaintOnlyAndAResizedFrameKeepsT) {
  // Paint-only: the LAID-OUT box never moves, whatever t does.
  Host host(200, 200);
  choreograph::Output<float> t{0};
  const auto describe = [&](float frameSize) {
    return box().child(
        box()
            .key("frame")
            .absolute()
            .rect(SkRect::MakeXYWH(20, 20, frameSize, frameSize))
            .child(rider({.path = geometry::shapes::circle(), .t = &t})));
  };
  host.composer.render(describe(160));
  host.frame();
  const auto laid = host.composer.bounds("dot");
  ASSERT_TRUE(laid.has_value());
  const SkPoint atZero = inkCentroid(host, SK_ColorRED, 200, 200);

  t = 0.5f;
  host.composer.render(describe(160));
  host.frame();
  EXPECT_EQ(host.composer.bounds("dot"), laid)
      << "travelling RELAYOUT the node — the motion path became a layout "
         "input";
  const SkPoint atHalf = inkCentroid(host, SK_ColorRED, 200, 200);
  EXPECT_GT(std::fabs(atHalf.x() - atZero.x()), 100.0f)
      << "…and it did not move either, so the assertion above is vacuous";

  // THE SIZE RULING: a re-laid frame RE-SHAPES the curve under the rider,
  // and t is untouched — the dot sits at the same FRACTION of the new
  // curve (here: half way round an 80 px circle inset at 20,20 → its left
  // extreme, x = 20) rather than jumping phase or freezing on the old one.
  t = 0.5f;
  host.composer.render(describe(80));
  host.frame();
  const SkPoint resized = inkCentroid(host, SK_ColorRED, 200, 200);
  EXPECT_NEAR(resized.x(), 20.0f, 1.5f)
      << "a resize did not re-measure the curve (stale table) or did not "
         "keep t";
  EXPECT_NEAR(resized.y(), 60.0f, 1.5f);
}

TEST(ComposeTravel, TheHitTestUndoesTheSameMatrixPaintApplied) {
  Host host(200, 200);
  choreograph::Output<float> t{0.25f};
  host.composer.render(
      travelFrame(rider({.path = geometry::shapes::circle(), .t = &t}, 20)));
  host.frame();
  // The rider is laid out at the frame's top-left and painted at the
  // circle's bottom. Only the painted place may hit.
  EXPECT_EQ(host.composer.hitTest({100, 180}).value_or(""), "dot");
  EXPECT_NE(host.composer.hitTest({25, 25}).value_or(""), "dot");
}
