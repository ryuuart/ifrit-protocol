#include "ComposeTestSupport.h"

TEST(ComposeBindings, TheAffineChainComposesInCallOrder) {
  // Reading order IS evaluation order for the affine ops, so the two
  // spellings below are genuinely different and each does what it looks
  // like. (An "order doesn't matter" accumulate would collapse them.)
  EXPECT_FLOAT_EQ(bind(nullptr).scale(240).offset(-70).value().apply(0.5f),
                  0.5f * 240 - 70);
  EXPECT_FLOAT_EQ(bind(nullptr).offset(-70).scale(240).value().apply(0.5f),
                  (0.5f - 70) * 240);
  // to(lo,hi) is the [0,1] → range spelling…
  EXPECT_FLOAT_EQ(bind(nullptr).target(20, 60).value().apply(0.25f), 30.0f);
  // …from(lo,hi) the other direction, and they compose.
  EXPECT_FLOAT_EQ(bind(nullptr).source(0, 200).target(0, 1).value().apply(50.0f),
                  0.25f);
  // invert composes with what came before rather than resetting it.
  EXPECT_FLOAT_EQ(bind(nullptr).invert().value().apply(0.25f), 0.75f);
  EXPECT_FLOAT_EQ(bind(nullptr).target(0, 2).invert().value().apply(0.25f),
                  1.0f - 0.5f);
  // the curve runs BEFORE the affine, on the normalised value…
  EXPECT_FLOAT_EQ(
      bind(nullptr).map(&choreograph::easeNone).target(0, 10).value().apply(0.4f),
      4.0f);
  // …and the clamp always runs last, whenever it is written.
  EXPECT_FLOAT_EQ(bind(nullptr).clamp(0, 1).target(0, 4).value().apply(0.5f), 1.0f);
}

TEST(ComposeBindings, AShapedBindingDrivesThePropertyInPixels) {
  // The wall this closes: a phase in [0,1] — which is what trim() and
  // opacity() want — could not drive a translation in PIXELS without a
  // second Output carrying pixels, updated in the same steppable. Five
  // separate studies kept two Outputs where one would do.
  Host host(200, 200);
  choreograph::Output<float> phase{0.0f};
  host.composer.render(box().child(box()
                                       .width(20)
                                       .height(20)
                                       .absolute()
                                       .left(0)
                                       .top(90)
                                       .fill(red())
                                       .translateX(bind(&phase).target(0, 160))));
  auto redAt = [&](int x) { return SkColorGetR(host.pixel(x, 100)) > 180; };

  host.frame();
  EXPECT_TRUE(redAt(10));   // phase 0 → x = 0
  EXPECT_FALSE(redAt(170));

  phase = 1.0f;
  host.frame();
  EXPECT_FALSE(redAt(10));
  EXPECT_TRUE(redAt(170));  // phase 1 → x = 160, unscaled would be x = 1

  phase = 0.5f;
  host.frame();
  EXPECT_TRUE(redAt(85));   // and it is linear in between
}

TEST(ComposeBindings, AChangedShapeRepatchesRatherThanPruning) {
  // The map is read LIVE through the pointer, so a pruned node would keep
  // shaping through the OLD one forever. Same Output, different range.
  Host host(200, 200);
  choreograph::Output<float> phase{1.0f};
  auto tree = [&](float far) {
    return box().child(box()
                           .key("dot")
                           .width(20)
                           .height(20)
                           .absolute()
                           .left(0)
                           .top(90)
                           .fill(red())
                           .translateX(bind(&phase).target(0, far)));
  };
  host.composer.render(tree(40.0f));
  host.frame();
  EXPECT_TRUE(SkColorGetR(host.pixel(50, 100)) > 180);

  host.composer.render(tree(150.0f));
  host.frame();
  EXPECT_FALSE(SkColorGetR(host.pixel(50, 100)) > 180);
  EXPECT_TRUE(SkColorGetR(host.pixel(160, 100)) > 180);
}

TEST(ComposeText, OnPathReDescribeDoesNotKeepTheOldBaseline) {
  // textEqual() compared everything about a text run EXCEPT its baseline
  // when onPath landed, so re-describing with a new path or a new `at`
  // pruned and the run kept riding the old one. TextPath's defaulted
  // operator== was implicitly deleted (std::function isn't comparable) and
  // so never caught it.
  Host host(240, 240);
  auto ring = [](float at) {
    return box().child(text(u8"HHHHHHHHHH", whiteStyle(22))
                           .key("ring")
                           .width(240).height(240).absolute().left(0).top(0)
                           .onPath({.path = shapes::arc(180.0f, 359.9f),
                                    .at = at,
                                    .align = TextPath::Align::Center}));
  };
  auto lit = [&](int y0, int y1) {
    int count = 0;
    for (int y = y0; y < y1; ++y)
      for (int x = 0; x < 240; ++x)
        count += host.pixel(x, y) != SK_ColorBLACK;
    return count;
  };

  host.composer.render(ring(0.25f));
  host.frame();
  ASSERT_GT(lit(0, 110), 200);

  host.composer.render(ring(0.75f)); // same key, same text, new baseline
  host.frame();
  EXPECT_GT(lit(140, 240), 200); // it moved…
  EXPECT_LT(lit(0, 110), 40);    // …and did not stay put
}

TEST(ComposeMotion, AnEmptyEasingMeansTheDefaultRatherThanACrash) {
  // Transition is an aggregate, so `{360ms, {}, 220ms}` — the obvious way
  // to write "default curve, but I need to name the delay" — initialises
  // `ease` to an EMPTY std::function. It compiled, then threw
  // bad_function_call on the first frame and took down a whole scene.
  Host host(200, 200);
  host.composer.render(box().child(
      box()
          .width(40)
          .height(40)
          .absolute()
          .left(0)
          .top(80)
          .fill(red())
          .translateX(animate(from(0.0f).to(120.0f), {200ms, {}, 0ms}))));
  host.frame();                    // would throw here
  host.frame(0.4);                 // land the entrance
  EXPECT_TRUE(SkColorGetR(host.pixel(130, 100)) > 180);

  // and it still prunes against an explicitly-defaulted curve
  Transition blank{200ms, {}, 0ms};
  Transition spelled{200ms, &choreograph::easeOutQuad, 0ms};
  EXPECT_EQ(blank.easing().target<float (*)(float)>() != nullptr,
            spelled.easing().target<float (*)(float)>() != nullptr);
}

TEST(ComposeShapes, ParametricCurvesEvaluateInTheUnitFrame) {
  // Shapes.h generated closed shapes from parameters; a curve DEFINED by
  // a parameter had no generator, so every study that needed one wrote
  // the same SkPathBuilder loop inside its own outline lambda.
  const SkSize box{200, 100}; // deliberately non-square: unit → half-extents

  // A 1:1 Lissajous with a quarter-turn phase IS the inscribed ellipse.
  const SkPath ellipse = shapes::lissajous(1, 1, 90.0f)(box);
  const SkRect bounds = ellipse.getBounds();
  EXPECT_NEAR(bounds.width(), 200.0f, 1.5f);
  EXPECT_NEAR(bounds.height(), 100.0f, 1.5f);
  EXPECT_NEAR(bounds.centerX(), 100.0f, 0.5f);
  EXPECT_NEAR(bounds.centerY(), 50.0f, 0.5f);

  // Damping shrinks the figure AS IT DRAWS — the whole visual difference
  // between a harmonograph and a Lissajous, and why a real pen-and-
  // pendulum figure spirals inward instead of retracing one rosette. Both
  // ends sit AT the centre (sin 0 = 0), so the honest measurement is the
  // reach of each half.
  const SkPath damped = shapes::harmonograph(3, 2, 0, 0.25f, 0, 6.0f)(box);
  const SkPoint centre = SkPoint{100, 50};
  const int pts = damped.countPoints();
  ASSERT_GT(pts, 100);
  auto reach = [&](int from, int to) {
    float most = 0;
    for (int i = from; i < to; ++i)
      most = std::max(most, SkPoint::Distance(damped.getPoint(i), centre));
    return most;
  };
  EXPECT_GT(reach(0, pts / 2), reach(pts / 2, pts) * 1.5f);

  // A rose with odd k has k petals, each reaching the rim. It is NOT
  // centred on the box — r = cos(5θ) puts tips at θ = 0, 2π/5, … so the
  // bounds sit off to one side, and asserting otherwise would be
  // asserting a bug into existence.
  const SkPath five = shapes::rose(5)(box);
  EXPECT_GT(five.countPoints(), 100);
  int tips = 0;
  for (int i = 0; i < five.countPoints(); ++i)
    if (SkPoint::Distance(five.getPoint(i), centre) > 49.0f)
      ++tips;
  EXPECT_GT(tips, 5);

  // Spirals start at the centre and end at the rim.
  const SkPath coil = shapes::spiral(3)(box);
  EXPECT_NEAR(SkPoint::Distance(coil.getPoint(0), centre), 0.0f, 1.0f);
  EXPECT_GT(SkPoint::Distance(coil.getPoint(coil.countPoints() - 1), centre),
            40.0f);

  // Everything stays inside the box it was inscribed in.
  for (const SkPath *p : {&ellipse, &damped, &five, &coil}) {
    const SkRect r = p->getBounds();
    EXPECT_GE(r.left(), -1.0f);
    EXPECT_GE(r.top(), -1.0f);
    EXPECT_LE(r.right(), 201.0f);
    EXPECT_LE(r.bottom(), 101.0f);
  }
}

TEST(ComposeInstances, ThePerSpriteBlendAccumulatesWhereALayerCannot) {
  // Nothing in the chain from instances() to drawSpriteAtlas carried a
  // blend mode, so every pool composited kSrcOver. Element::blend() looks
  // like the fix and is not: it flattens the field into a layer and
  // composites it ONCE, so overlapping sprites never accumulate — which
  // is the entire colour model of an additive particle system (Reeves'
  // 1982 wall of fire has no palette, only an overlap count).
  auto build = [](SkBlendMode blend) {
    auto atlas = std::make_shared<instancing::Atlas>(1.0f);
    atlas->cell(box().width(40).height(40).fill(
                    Fill::color({0.25f, 0.25f, 0.25f, 1})),
                {40, 40});
    auto pool = std::make_shared<instancing::Pool>();
    for (int i = 0; i < 3; ++i) // three sprites stacked on one spot
      pool->add({100, 100});
    return box().absolute().inset(0).child(
        instancing::instances(atlas, pool, instancing::Mode::Data, blend));
  };

  Host over(200, 200);
  over.composer.render(build(SkBlendMode::kSrcOver));
  over.frame();
  const int overR = SkColorGetR(over.pixel(100, 100));

  Host plus(200, 200);
  plus.composer.render(build(SkBlendMode::kPlus));
  plus.frame();
  const int plusR = SkColorGetR(plus.pixel(100, 100));

  EXPECT_GT(overR, 40);            // one opaque sprite's worth
  EXPECT_LT(overR, 90);            // …and three of them are no brighter
  EXPECT_GT(plusR, overR + 60);    // additive stacks all three
}

TEST(ComposeText, OnPathWalksEveryContourNotJustTheFirst) {
  // A trajectory clipped to the frame produces SEVERAL contours, and
  // onPath used to take iter.next() once — so the KSP study's hyperbola
  // lost its label entirely, with no diagnostic. The baseline is now one
  // arc-length coordinate over the whole chain.
  auto twoSegments = [](SkSize s) {
    SkPathBuilder b;
    b.moveTo(10, 40).lineTo(190, 40);   // contour 1: across the top
    b.moveTo(10, 160).lineTo(190, 160); // contour 2: across the bottom
    return b.detach();
  };
  auto lit = [](Host &host, int y0, int y1) {
    int count = 0;
    for (int y = y0; y < y1; ++y)
      for (int x = 0; x < 200; ++x)
        count += host.pixel(x, y) != SK_ColorBLACK;
    return count;
  };

  // A run long enough to overflow contour 1 must continue onto contour 2.
  Host host(200, 200);
  host.composer.render(box().child(
      text(u8"HHHHHHHHHHHHHHHHHHHHHHHH", whiteStyle(20))
          .width(200).height(200).absolute().left(0).top(0)
          .onPath({.path = twoSegments, .at = 0.0f})));
  host.frame();
  EXPECT_GT(lit(host, 20, 60), 200);   // ink on the first contour…
  EXPECT_GT(lit(host, 140, 180), 200); // …and on the second, which used
                                       // to be silently unreachable
}

TEST(ComposeDebug, CoverageCatchesWhatAreaAndContainmentMiss) {
  // The Penrose study's sharpest finding, made into library code: a
  // subdivision that OVERLAPS in one place and GAPS in another passes
  // both cheap checks. Area conservation passes because the two errors
  // cancel exactly; containment passes because every piece really is
  // inside the parent. Only point sampling sees it.
  const SkRect region = SkRect::MakeWH(100, 100);

  // An honest split of the square into two halves.
  auto rect = [](float l, float t, float r, float b) {
    SkPathBuilder p;
    p.addRect(SkRect::MakeLTRB(l, t, r, b));
    return p.detach();
  };
  const std::vector<SkPath> exact = {rect(0, 0, 50, 100), rect(50, 0, 100, 100)};
  const auto good = debug::coverage(exact, region, 64);
  EXPECT_TRUE(good.exact());
  EXPECT_EQ(good.uncovered, 0);
  EXPECT_EQ(good.doubled, 0);

  // The same two halves, one shifted 10 px right: a 10-wide gap on the
  // left, a 10-wide overlap in the middle. Equal areas, so the total is
  // unchanged and both pieces are still inside the square.
  const std::vector<SkPath> broken = {rect(10, 0, 60, 100),
                                      rect(50, 0, 100, 100)};
  float area = 0;
  for (const SkPath &p : broken)
    area += p.getBounds().width() * p.getBounds().height();
  EXPECT_FLOAT_EQ(area, 100 * 100); // area conservation: PASSES
  for (const SkPath &p : broken)
    EXPECT_TRUE(region.contains(p.getBounds())); // containment: PASSES

  const auto bad = debug::coverage(broken, region, 64);
  EXPECT_FALSE(bad.exact()); // …and coverage does not
  EXPECT_NEAR(bad.uncoveredFraction(), 0.10f, 0.02f);
  EXPECT_NEAR(bad.doubledFraction(), 0.10f, 0.02f);
  ASSERT_FALSE(bad.uncoveredAt.empty());
  EXPECT_LT(bad.uncoveredAt.front().x(), 10.0f); // the witness is the gap
}

TEST(ComposeDebug, EndpointDegreesFindTheDanglingArc) {
  // The chaining test for a decorated tiling: on the Oxford Penrose
  // paving every interior arc endpoint must have degree 2, or the
  // stainless bands do not link up into rings.
  auto seg = [](float x0, float y0, float x1, float y1) {
    SkPathBuilder p;
    p.moveTo(x0, y0).lineTo(x1, y1);
    return p.detach();
  };
  // Three segments chained head-to-tail: two interior joints (degree 2),
  // two loose ends (degree 1).
  const std::vector<SkPath> chain = {seg(0, 0, 10, 0), seg(10, 0, 20, 0),
                                     seg(20, 0, 30, 0)};
  const auto degrees = debug::endpointDegrees(chain);
  EXPECT_EQ(degrees.points.size(), 4u);
  EXPECT_EQ(degrees.outside(2, 2).size(), 2u); // the two loose ends

  // Move one segment off its joint: now four loose ends, not two.
  const std::vector<SkPath> broken = {seg(0, 0, 10, 0), seg(11, 0, 20, 0),
                                      seg(20, 0, 30, 0)};
  EXPECT_EQ(debug::endpointDegrees(broken).outside(2, 2).size(), 4u);
}

TEST(ComposeText, AutoFlipIsOnePerRunDecisionSampledAcrossTheRun) {
  // The contract, pinned because a study reported autoFlip as a "no-op"
  // and it is not — it is a PER-RUN decision working as designed. A run
  // that stays on the bottom flips; one that stays on the top does not;
  // one that WRAPS PAST the crossover cannot be fixed by a single flip
  // and is not pretended otherwise (the engraver's answer is two runs,
  // top and bottom set separately — ROADMAP §9).
  //
  // The decision samples ACROSS the run rather than reading one midpoint
  // tangent, which is strictly more robust: a midpoint that happens to
  // land on a locally odd tangent used to decide for every glyph.
  auto ring = [](float at, bool flip) {
    return box().child(text(u8"HHHHHHHH", whiteStyle(20))
                           .width(200).height(200).absolute().left(0).top(0)
                           .onPath({.path = shapes::circle(),
                                    .at = at,
                                    .align = TextPath::Align::Center,
                                    .offset = 4.0f,
                                    .autoFlip = flip}));
  };
  auto snap = [](Host &host) {
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(200, 200));
    host.surface->readPixels(bm.pixmap(), 0, 0);
    return bm;
  };
  auto differs = [](const SkBitmap &a, const SkBitmap &b) {
    int n = 0;
    for (int y = 0; y < 200; ++y)
      for (int x = 0; x < 200; ++x)
        n += a.getColor(x, y) != b.getColor(x, y);
    return n;
  };

  // A short caption sitting squarely on the BOTTOM of the ring: every
  // sample says upside down, so the flip must fire.
  Host plain(200, 200), flipped(200, 200);
  plain.composer.render(ring(0.5f, false));
  plain.frame();
  flipped.composer.render(ring(0.5f, true));
  flipped.frame();
  EXPECT_GT(differs(snap(plain), snap(flipped)), 200);

  // …and on the TOP, where every sample says upright, it must not.
  Host topPlain(200, 200), topFlipped(200, 200);
  topPlain.composer.render(ring(0.0f, false));
  topPlain.frame();
  topFlipped.composer.render(ring(0.0f, true));
  topFlipped.frame();
  EXPECT_EQ(differs(snap(topPlain), snap(topFlipped)), 0);
}

TEST(ComposeBindings, QuantizeSnapsBeforeTheAffineChain) {
  // Winamp's volume slider is literally round(percent * 28), and its 28
  // sprite frames are what anyone who used it remembers — a smooth
  // slider sampled at draw time is a different widget. Quantisation is
  // the design, so it belongs in the binding rather than in the caller's
  // steppable.
  auto q = [](float v) {
    return bind(nullptr).quantize(5).value().apply(v); // levels 0,.25,.5,.75,1
  };
  EXPECT_FLOAT_EQ(q(0.0f), 0.00f);
  EXPECT_FLOAT_EQ(q(0.10f), 0.00f);
  EXPECT_FLOAT_EQ(q(0.20f), 0.25f);
  EXPECT_FLOAT_EQ(q(0.60f), 0.50f);
  EXPECT_FLOAT_EQ(q(1.0f), 1.00f);
  // It runs BEFORE the affine chain, so the steps land on round pixels.
  EXPECT_FLOAT_EQ(bind(nullptr).quantize(5).target(0, 80).value().apply(0.6f),
                  40.0f);
  // …and after the curve, so an eased value still lands on a step.
  EXPECT_FLOAT_EQ(
      bind(nullptr).map(&choreograph::easeNone).quantize(5).value().apply(0.9f),
      1.0f);
}

TEST(ComposeBindings, AFillCanBeBoundLive) {
  // Pinned because a study concluded there was no bound Fill at all and
  // rebuilt its most period-authentic widget on renderSlot() instead.
  // There is one: the Output holds a Fill, and you write it from the
  // same steppable that computes the number driving everything else.
  Host host(200, 200);
  choreograph::Output<Fill> bar{Fill::color({1, 0, 0, 1})};
  host.composer.render(box().child(box()
                                       .absolute()
                                       .left(20)
                                       .top(80)
                                       .width(160)
                                       .height(40)
                                       .fill(&bar)));
  host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(100, 100)), 180);
  EXPECT_LT(SkColorGetG(host.pixel(100, 100)), 80);

  bar = Fill::color({0, 1, 0, 1}); // no re-render, no re-describe
  host.frame();
  EXPECT_LT(SkColorGetR(host.pixel(100, 100)), 80);
  EXPECT_GT(SkColorGetG(host.pixel(100, 100)), 180);
}

TEST(ComposeShapes, StarArmsCanBeWaisted) {
  // Engraved stars are almost never straight-chorded: Chladni's 1787
  // sound-figures narrow fast off the hub and then run as needles, and
  // nine figures on that one plate wanted exactly this parameter.
  const SkSize box{200, 200};
  const SkRect region = SkRect::MakeWH(200, 200);
  // Measure the covered area by sampling, not by a shoelace over the
  // endpoints — the waist lives in the QUAD CONTROL POINTS, so a polygon
  // area sees no difference at all and would pass on a no-op.
  auto covered = [&](const SkPath &p) {
    const SkPath pieces[] = {p};
    const auto c = debug::coverage(pieces, region, 128);
    return c.samples - c.uncovered;
  };

  const SkPath straight = shapes::star(6, 0.35f, 0.0f)(box);
  const SkPath waisted = shapes::star(6, 0.35f, 0.22f)(box);
  const SkPath bulged = shapes::star(6, 0.35f, -0.22f)(box);

  // The tips are unmoved — the waist pinches the EDGES, not the points.
  EXPECT_NEAR(straight.getBounds().height(), waisted.getBounds().height(),
              1.0f);
  // The figure loses ink, because every edge bows toward the centre…
  EXPECT_LT(covered(waisted), covered(straight));
  // …and a negative waist bulges instead, which is the compass-rose
  // direction.
  EXPECT_GT(covered(bulged), covered(straight));
}

TEST(ComposeDecorations, RadialHatchFansOutOfAPointAndRingsRoundIt) {
  // lines::hatch is a parallel lattice at one fixed angle, which is the
  // wrong field for anything engraved out of a point. The Chladni study
  // built its radial fan from 120 shapes::sector sub-wedges each carrying
  // a rotated Hatch — correct, and 120 nodes for one field.
  auto lit = [](Host &host, int x0, int y0, int x1, int y1) {
    int n = 0;
    for (int y = y0; y < y1; ++y)
      for (int x = x0; x < x1; ++x)
        n += host.pixel(x, y) != SK_ColorBLACK;
    return n;
  };

  Host fan(200, 200);
  fan.composer.render(box().child(
      box().absolute().inset(20).shape(shapes::circle()).background(
          lines::radialHatch(Fill::color({1, 1, 1, 1}), 32, 1.5f))));
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
      box().absolute().inset(20).shape(shapes::circle()).background(
          lines::concentric(Fill::color({1, 1, 1, 1}), 8, 1.5f))));
  rings.frame();
  EXPECT_GT(lit(rings, 20, 20, 180, 180), 800);
  EXPECT_EQ(lit(rings, 20, 20, 32, 32), 0);

  // They are different FIELDS, not one rotated: spokes converge, so a
  // fan's ink density climbs toward the centre, while evenly spaced
  // rings hold theirs. (Counting crossings along one scanline would not
  // show this — a spoke can lie along the scanline.)
  auto density = [](Host &host, float r0, float r1) {
    int ink = 0, area = 0;
    for (int y = 0; y < 200; ++y)
      for (int x = 0; x < 200; ++x) {
        const float d = std::hypot((float)x - 100.0f, (float)y - 100.0f);
        if (d < r0 || d >= r1)
          continue;
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
  auto lit = [](Host &host, int x0, int y0, int x1, int y1) {
    int n = 0;
    for (int y = y0; y < y1; ++y)
      for (int x = x0; x < x1; ++x)
        n += host.pixel(x, y) != SK_ColorBLACK;
    return n;
  };
  auto line = [](SkSize) {
    SkPathBuilder b;
    b.moveTo(10, 100).lineTo(190, 100);
    return b.detach();
  };

  // Node gated to the first 60% — one geometry, two windows on it:
  // a wide dim body over all of it, and a bright sliver at its head.
  PathFormat head = util::stroke(6, Fill::color({0, 1, 0, 1}));
  head.trimStart = 0.90f;
  head.trimEnd = 1.0f;
  Host host(200, 200);
  host.composer.render(box().child(
      box().absolute().inset(0).shape(line).fill(Fill::none())
          .mask(by::spans(spans::upTo(0.6f)))
          .foreground(util::stroke(3, Fill::color({1, 0, 0, 1})))
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

TEST(ComposeContent, SamplingReachesTheImageLeaf) {
  // Every blessed image path hardcoded kLinear, so pixel art, tilemaps
  // and simulation buffers drawn through image() were silently blurred.
  // Material::image() has always taken sampling; the element factory did
  // not, so the fix was discoverable only by diffing two signatures.
  auto atlas = twoCellAtlas(); // 32x16: left half red, right half green
  auto magnified = [&](SkSamplingOptions options) {
    Host host(200, 200);
    host.composer.render(box().child(image(atlas)
                                         .sampling(options)
                                         .absolute()
                                         .left(0)
                                         .top(0)
                                         .width(200)
                                         .height(100)));
    host.frame();
    // Count columns straddling the red/green seam that are NEITHER pure
    // red nor pure green — the blend band linear filtering invents.
    int blended = 0;
    for (int x = 80; x < 120; ++x) {
      const SkColor c = host.pixel(x, 50);
      const bool pureRed = SkColorGetR(c) > 200 && SkColorGetG(c) < 40;
      const bool pureGreen = SkColorGetG(c) > 200 && SkColorGetR(c) < 40;
      blended += !pureRed && !pureGreen;
    }
    return blended;
  };

  EXPECT_GT(magnified(SkSamplingOptions(SkFilterMode::kLinear)), 3);
  EXPECT_LE(magnified(SkSamplingOptions(SkFilterMode::kNearest)), 1);
}

TEST(ComposeDecorations, OverlayPaintsOverTheFillAndUnderTheContent) {
  // The slot between the two that did not exist: background() hides
  // beneath the fill and foreground() paints above the children, so a
  // textured button greys out its own label. Hazard stripes over the
  // surface but under the digit is the canonical case.
  auto build = [](bool useForeground) {
    auto bars = lines::hatch(Fill::color({0, 0, 0, 1}), 6.0f, 4.0f, 0.0f);
    Element cell = box()
                       .absolute()
                       .inset(0)
                       .fill(Fill::color({1, 1, 1, 1}))
                       .child(box()
                                  .absolute()
                                  .left(60)
                                  .top(60)
                                  .width(80)
                                  .height(80)
                                  .fill(Fill::color({0, 1, 0, 1})));
    return useForeground ? cell.foreground(bars) : cell.overlay(bars);
  };
  auto greenPixels = [](Host &host) {
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

TEST(ComposeMotion, AddFixedRunsAtItsOwnRateWhateverTheHostDraws) {
  // Every simulation-shaped study reinvented the accumulator and its
  // spiral-of-death clamp — a cellular automaton at 27 Hz behind the DOOM
  // PlayStation titles, particles at 24. The library had declared
  // choppiness for shaders (Material::quantizeTime) and nothing for logic.
  auto stepsOverOneSecond = [](double fps) {
    sigil::motion::Ticker ticker;
    int steps = 0;
    ticker.addFixed(27.0, [&] { ++steps; return true; });
    const double dt = 1.0 / fps;
    for (int i = 0; i < (int)std::lround(fps); ++i)
      ticker.tick(dt);
    return steps;
  };
  EXPECT_EQ(stepsOverOneSecond(60.0), 27);
  EXPECT_EQ(stepsOverOneSecond(144.0), 27);
  // Below the sim rate it still lands on 27 — several steps per frame.
  EXPECT_EQ(stepsOverOneSecond(24.0), 27);

  // The clamp: one enormous hitch must not run an unbounded backlog —
  // and it must SAY it clamped, because a frame that dropped simulated
  // time makes anything measured on it meaningless.
  {
    sigil::motion::Ticker ticker;
    int steps = 0;
    sigil::motion::Ticker::FixedStatus status;
    ticker.addFixed(60.0, [&] { ++steps; return true; }, /*maxCatchUp=*/4,
                    nullptr, &status);
    ticker.tick(10.0); // ten seconds in one frame = 600 steps of backlog
    EXPECT_EQ(steps, 4);
    EXPECT_EQ(status.stepsRun, 4);
    EXPECT_TRUE(status.clamped);
    // …and the backlog is DISCARDED, not carried into the next frame.
    steps = 0;
    ticker.tick(1.0 / 60.0);
    EXPECT_EQ(steps, 1);
    EXPECT_FALSE(status.clamped);
  }

  // Reproducibility: the step count comes from TOTAL elapsed time, so the
  // same instant lands on the same step whatever the draw rate. An
  // accumulator compared against a step slips one comparison over a long
  // pre-roll — a study measured byte-identical output at 60/30/20 fps and
  // a one-step slip at 15 and 10, and correctly blamed float
  // accumulation rather than the clamp.
  {
    // Each rate advances to the SAME total time — otherwise the counts
    // differ for the honest reason that the clocks differ.
    auto stepsAt = [](double fps, double untilSeconds) {
      sigil::motion::Ticker ticker;
      int steps = 0;
      ticker.addFixed(60.0, [&] { ++steps; return true; }, 64);
      const int frames = (int)std::lround(untilSeconds * fps);
      const double dt = untilSeconds / (double)frames;
      for (int i = 0; i < frames; ++i)
        ticker.tick(dt);
      return steps;
    };
    const int reference = stepsAt(60.0, 3.1);
    EXPECT_EQ(stepsAt(30.0, 3.1), reference);
    EXPECT_EQ(stepsAt(20.0, 3.1), reference);
    EXPECT_EQ(stepsAt(15.0, 3.1), reference);
    EXPECT_EQ(stepsAt(10.0, 3.1), reference);
    EXPECT_EQ(stepsAt(120.0, 3.1), reference);
  }

  // Returning false drops it, like add().
  {
    sigil::motion::Ticker ticker;
    int steps = 0;
    ticker.addFixed(60.0, [&] { return ++steps < 3; });
    ticker.tick(1.0);
    const int after = steps;
    ticker.tick(1.0);
    EXPECT_EQ(steps, after);
  }
}

TEST(ComposeDecorations, DashPhaseCanBeBoundSoDashesMarch) {
  // trimPhase took a bound Output and declared isAnimated(); dashPhase was
  // a plain float, so marching ants — the commonest animated-line idiom
  // in map and diagram UI — could only be had by re-describing every
  // frame, which defeats the pruning the library is built on. A study
  // wrote a 25-line DecorationScheme for want of this.
  choreograph::Output<float> march{0.0f};
  PathFormat dashed = util::stroke(4, Fill::color({1, 1, 1, 1}));
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
  for (size_t i = 0; i < before.size(); ++i)
    moved += before[i] != after[i];
  EXPECT_GT(moved, 20); // the dashes actually travelled
}

TEST(ComposeMaterials, GlowUnitReachesTheInscribedCircleNotTheCorners) {
  // radialUnit's radius is a fraction of the box's HALF-DIAGONAL, so a
  // "soft round light" authored at radius 1 is still at ~10% alpha where
  // the inscribed circle is — and with .shape(shapes::circle()) on the
  // same node that becomes a visible hard rim. Two studies lost an
  // iteration to it. The magic number is 0.707, and glowUnit is it.
  const std::vector<Stop> ramp = {{0.0f, {1, 1, 1, 1}}, {1.0f, {0, 0, 0, 1}}};
  auto edgeValue = [&](Material m) {
    Host host(200, 200);
    host.composer.render(box().child(
        box().absolute().inset(0).fill(std::move(m))));
    host.frame();
    // Just inside the box edge, on the horizontal centre line — where the
    // inscribed circle touches.
    return SkColorGetR(host.pixel(197, 100));
  };

  // radialUnit(…, 1.0) is still bright at the inscribed circle, because
  // its ramp does not reach black until the corners.
  EXPECT_GT(edgeValue(Material::radialUnit({0.5f, 0.5f}, 1.0f, ramp)), 40);
  // glowUnit(…, 1.0) has landed by then. That is the whole difference.
  EXPECT_LT(edgeValue(Material::glowUnit({0.5f, 0.5f}, 1.0f, ramp)), 8);
  // And the old spelling of the same thing still works, which is what
  // makes this a convenience rather than a behaviour change.
  EXPECT_LT(edgeValue(Material::radialUnit({0.5f, 0.5f}, 0.7071f, ramp)), 8);
}

TEST(ComposeInstances, ThePerInstanceSizeLaneCarriesNonUniformScale) {
  // The most-cited gap in the program's hard half: SkRSXform carries
  // (scos, ssin) and ONE scale by construction, so Reeves' 1982
  // `streaked spherical` particle — a quad 0.5·|v| long by `size` wide,
  // aspect swinging ~2.4:1 to under 1:1 across its life — could not be
  // instanced at all. One study hand-built the vertex buffer in 69 lines
  // and lost every decoration slot and all picture caching with it.
  //
  // The lane is opt-in: a pool that never asks for it keeps the pure
  // RSXform path and costs nothing.
  auto build = [](bool stretch) {
    auto atlas = std::make_shared<instancing::Atlas>(1.0f);
    atlas->cell(box().width(20).height(20).fill(Fill::color({1, 0, 0, 1})),
                {20, 20});
    auto pool = std::make_shared<instancing::Pool>();
    pool->add({100, 100});
    if (stretch) {
      pool->sizes()[0] = {4.0f, 0.25f}; // 80 x 5 — a streak
      pool->commit();
    }
    return box().absolute().inset(0).child(
        instancing::instances(atlas, pool, instancing::Mode::Data));
  };
  auto redSpan = [](Host &host, bool horizontal) {
    int n = 0;
    for (int i = 0; i < 200; ++i) {
      const SkColor c =
          horizontal ? host.pixel(i, 100) : host.pixel(100, i);
      n += SkColorGetR(c) > 180;
    }
    return n;
  };

  Host square(200, 200);
  square.composer.render(build(false));
  square.frame();
  EXPECT_NEAR(redSpan(square, true), 20, 2);
  EXPECT_NEAR(redSpan(square, false), 20, 2);

  Host streak(200, 200);
  streak.composer.render(build(true));
  streak.frame();
  EXPECT_NEAR(redSpan(streak, true), 80, 3);  // four times as wide…
  EXPECT_NEAR(redSpan(streak, false), 5, 2);  // …and a quarter as tall
}

TEST(ComposeInstances, ANonUniformInstanceStillRotatesAboutItsCentre) {
  // The quad is built by hand on this path, so the anchor has to come out
  // where RSXform would have put it — a 90-degree turn must swap the
  // extents in place, not orbit the sprite away from its position.
  auto atlas = std::make_shared<instancing::Atlas>(1.0f);
  atlas->cell(box().width(20).height(20).fill(Fill::color({1, 0, 0, 1})),
              {20, 20});
  auto pool = std::make_shared<instancing::Pool>();
  pool->add({100, 100}, 0, (float)M_PI_2);
  pool->sizes()[0] = {4.0f, 0.25f};
  pool->commit();

  Host host(200, 200);
  host.composer.render(box().absolute().inset(0).child(
      instancing::instances(atlas, pool, instancing::Mode::Data)));
  host.frame();
  int across = 0, down = 0;
  for (int i = 0; i < 200; ++i) {
    across += SkColorGetR(host.pixel(i, 100)) > 180;
    down += SkColorGetR(host.pixel(100, i)) > 180;
  }
  EXPECT_NEAR(across, 5, 2);  // rotated: the extents swapped…
  EXPECT_NEAR(down, 80, 3);
  EXPECT_TRUE(SkColorGetR(host.pixel(100, 100)) > 180); // …in place
}

TEST(ComposeText, OnPathCanOrientGlyphsRadiallyForADial) {
  // onPath rotates glyphs to the TANGENT, which is running lettering — a
  // motto, a ring inscription. An astrolabe limb, a compass rose and a
  // radial axis want the other one: type RADIATING like a spoke, read by
  // turning the instrument. Without it each numeral costs one rotated
  // Element, which is precisely the per-glyph cost onPath exists to
  // abolish.
  //
  // (Tangent already gives "up points outward" on a circle — that is why
  // a clock face's 6 comes out upside down — so radiating is genuinely
  // the only orientation that was missing, not a restatement.)
  // Built with parametric() rather than circle() so the test knows
  // exactly where fraction 0.25 is: t runs from 3 o'clock, and with y
  // down a quarter turn lands at the BOTTOM.
  auto ring = [](TextPath::Orient orient) {
    auto circle = shapes::parametric(
        [](float t) { return SkPoint{std::cos(t), std::sin(t)}; }, 0.0f,
        2.0f * SK_FloatPI, 360, true);
    // ONE tall glyph: a run spread along the arc keeps a wide footprint
    // whichever way its glyphs face, so a multi-glyph run cannot see the
    // per-glyph rotation at all.
    return box().child(text(u8"I", whiteStyle(64))
                           .width(240).height(240).absolute().left(0).top(0)
                           .onPath({.path = circle,
                                    .at = 0.25f, // the bottom of the ring
                                    .align = TextPath::Align::Center,
                                    .offset = -50.0f,
                                    .orient = orient}));
  };
  auto footprint = [](Host &host) {
    int minX = 9999, maxX = -1, minY = 9999, maxY = -1;
    for (int y = 0; y < 240; ++y)
      for (int x = 0; x < 240; ++x)
        if (host.pixel(x, y) != SK_ColorBLACK) {
          minX = std::min(minX, x); maxX = std::max(maxX, x);
          minY = std::min(minY, y); maxY = std::max(maxY, y);
        }
    return SkISize{maxX - minX, maxY - minY};
  };

  Host tangent(240, 240), radial(240, 240);
  tangent.composer.render(ring(TextPath::Orient::Tangent));
  tangent.frame();
  radial.composer.render(ring(TextPath::Orient::Radial));
  radial.frame();

  const SkISize t = footprint(tangent), r = footprint(radial);
  ASSERT_GT(t.width(), 0);
  ASSERT_GT(r.width(), 0);
  // At the bottom of the ring the tangent is horizontal, so the glyph
  // stands (upside down, but standing): tall. Radial turns its baseline
  // down the radius, laying it on its side: wide.
  EXPECT_GT(t.height(), t.width());
  EXPECT_GT(r.width(), r.height());
}

TEST(ComposeMotion, AddFixedPublishesTheRenderInterpolant) {
  // A fixed-rate sim drawn at an unrelated rate judders unless you draw
  // lerp(previous, current, alpha). The accumulator lived inside the
  // steppable with no way to read it — and a verlet body's state is
  // literally the pair (x*, x), so the integrator was already holding
  // both ends of the interpolation while the library hid the one scalar
  // that was missing.
  sigil::motion::Ticker ticker;
  choreograph::Output<float> alpha{-1.0f};
  int steps = 0;
  ticker.addFixed(10.0, [&] { ++steps; return true; }, 8, &alpha);

  // Half a step in: no step taken, and alpha says exactly how far.
  ticker.tick(0.05);
  EXPECT_EQ(steps, 0);
  EXPECT_NEAR(alpha.value(), 0.5f, 1e-4f);

  // Cross the step: one step, and the leftover is what remains.
  ticker.tick(0.07);
  EXPECT_EQ(steps, 1);
  EXPECT_NEAR(alpha.value(), 0.2f, 1e-4f);

  // Landing exactly on a boundary leaves nothing over.
  ticker.tick(0.08);
  EXPECT_EQ(steps, 2);
  EXPECT_NEAR(alpha.value(), 0.0f, 1e-4f);
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
  PathFormat dashedHead = util::stroke(5, Fill::color({0, 1, 0, 1}));
  dashedHead.trimStart = 0.75f; // the last quarter only
  dashedHead.trimEnd = 1.0f;

  Host host(200, 200);
  host.composer.render(box().child(
      custom([dashedHead](SkCanvas &canvas, const PaintContext &ctx) {
        // Geometry computed HERE, per paint — the case the roadmap
        // claimed could not be decorated.
        SkPathBuilder b;
        b.moveTo(10, 100).lineTo(190, 100);
        decorations::paintOn(canvas, ctx, b.detach(), dashedHead);
      }).absolute().inset(0).cache(Cache::None)));
  host.frame();

  auto lit = [&](int x0, int x1) {
    int n = 0;
    for (int x = x0; x < x1; ++x)
      n += SkColorGetG(host.pixel(x, 100)) > 180;
    return n;
  };
  // The decoration's own trim window applied to hand-built geometry:
  // the last quarter is drawn, the first three are not.
  EXPECT_GT(lit(150, 190), 30);
  EXPECT_EQ(lit(15, 130), 0);
}

TEST(ComposeBindings, WindowClampsBeforeTheCurveSoEasingsStayInDomain) {
  // from(lo,hi) normalises and the curve runs after it, so on a
  // multi-beat timeline an Output outside the window feeds the easing a
  // value outside its domain — and none of ease:: is total. Every curve
  // in the tartan study had to clamp its own input first.
  auto plain = bind(nullptr).source(0.4f, 0.6f);
  auto windowed = bind(nullptr).window(0.4f, 0.6f);

  // Inside the window they agree exactly.
  EXPECT_FLOAT_EQ(plain.value().apply(0.5f), windowed.value().apply(0.5f));
  // Outside it, from() keeps running past the ends…
  EXPECT_LT(plain.value().apply(0.0f), -1.0f);
  EXPECT_GT(plain.value().apply(1.0f), 2.0f);
  // …and window() holds at the ends, which is what "this beat" means.
  EXPECT_FLOAT_EQ(windowed.value().apply(0.0f), 0.0f);
  EXPECT_FLOAT_EQ(windowed.value().apply(1.0f), 1.0f);

  // And the clamp lands BEFORE the curve: an overshoot easing evaluated
  // at 1 returns exactly 1, rather than being run far past its domain.
  const float overshoot =
      bind(nullptr).window(0.4f, 0.6f).map(ease::outBack()).value().apply(5.0f);
  EXPECT_NEAR(overshoot, 1.0f, 1e-4f);
}

// RENAMED 2026-07-28 (audit): both arms set kNearest and nothing contrasts
// kLinear, so this proves the PAN and says nothing about sampling() — the
// name no longer claims it.
TEST(ComposePattern, ARepeatCanBePanned) {
  // "Pattern cannot pan" was two studies, and the second located the fix
  // a level below where the roadmap had it: bake() hands its matrix to
  // Material::image, whose localMatrix has always taken a translation, so
  // Pattern was exposing two thirds of a matrix its own backend takes
  // whole. Phase is the defining property of a surprising number of
  // repeats — a twill advances one thread per pick.
  auto stripes = [](SkPoint pan) {
    Pattern p = Pattern::tile({8, 8}, [](SkCanvas &c, SkSize s, uint32_t) {
      SkPaint left;
      left.setColor4f({1, 0, 0, 1}, nullptr);
      c.drawRect(SkRect::MakeWH(s.width() * 0.5f, s.height()), left);
      SkPaint right;
      right.setColor4f({0, 1, 0, 1}, nullptr);
      c.drawRect(SkRect::MakeXYWH(s.width() * 0.5f, 0, s.width() * 0.5f,
                                  s.height()),
                 right);
    });
    p.offset(pan).sampling(SkSamplingOptions(SkFilterMode::kNearest));
    return p.material();
  };
  auto colourAt = [](Material m, int x) {
    Host host(64, 64);
    host.composer.render(box().child(
        box().absolute().inset(0).fill(std::move(m))));
    host.frame();
    return host.pixel(x, 32);
  };

  // Unpanned: the left half of each 8px tile is red.
  const SkColor unpanned = colourAt(stripes({0, 0}), 1);
  EXPECT_GT(SkColorGetR(unpanned), 180);
  EXPECT_LT(SkColorGetG(unpanned), 80);

  // Panned by half a tile: the same pixel is now green. Nothing rebakes.
  const SkColor panned = colourAt(stripes({4, 0}), 1);
  EXPECT_GT(SkColorGetG(panned), 180);
  EXPECT_LT(SkColorGetR(panned), 80);
}

TEST(ComposeDecorations, WashFloodsTheOutlineWithAMaterialAndPrunes) {
  // The decoration primitives were PathFormat (strokes), Slice,
  // ContourWalk and a raw PaintProgram — none of which fills a shape with
  // a Material. So "a grain pass over this whole panel, ABOVE its
  // children" had to be a raw PaintProgram, an incomparable std::function
  // that never prunes. overlay() is a different slot: it paints UNDER the
  // children.
  auto build = [](float amount) {
    return box().child(box()
                           .absolute()
                           .inset(0)
                           .fill(Fill::color({0, 0, 0, 1}))
                           .child(box()
                                      .absolute()
                                      .inset(40)
                                      .fill(Fill::color({0, 0, 1, 1})))
                           .foreground(decorations::wash(
                               Material::solid({1, 0, 0, 1}),
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
  EXPECT_GT(SkColorGetB(full.pixel(60, 60)), 200); // kPlus kept the blue
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

TEST(ComposeText, MetricsExposeTheCapSlackThatPlacementNeeds) {
  // The most-used missing primitive in the study program. A compose text
  // node's top is the LINE BOX top; almost every artefact worth
  // reconstructing positions type by its CAP TOP, so aligning a rebuild
  // to a reference needs the slack between them — and measure() returns
  // only an SkSize. One study inferred it as an empirical
  // 0.20 × measure("H").height() across ~134 runs and three faces.
  const auto m = metrics(whiteStyle(40), fonts());
  EXPECT_GT(m.ascent, 0.0f);   // reported as a positive distance, not
  EXPECT_GT(m.descent, 0.0f);  // Skia's signed convention
  EXPECT_GT(m.capHeight, 0.0f);
  EXPECT_GT(m.xHeight, 0.0f);
  // Sanity, and the reason both have fallbacks: x-height sits under cap
  // height, which sits under the ascent.
  EXPECT_LT(m.xHeight, m.capHeight);
  EXPECT_LE(m.capHeight, m.ascent + 0.01f);
  EXPECT_GT(m.capSlack(), 0.0f);
  EXPECT_NEAR(m.lineHeight, m.ascent + m.descent + m.leading, 1e-4f);

  // It scales with the size, which is what makes it usable as a constant
  // per style rather than per run.
  const auto twice = metrics(whiteStyle(80), fonts());
  EXPECT_NEAR(twice.capHeight, m.capHeight * 2.0f, 0.5f);
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
    PathFormat f = util::stroke(24, Fill::color({1, 1, 1, 1}));
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

TEST(ComposeText, TextFillWorksWithTheUnitRamps) {
  // Material.h advertises textFill and the Unit ramps as the same trick,
  // and it was the opposite: the metric band already maps the shader's
  // [0,1]² onto the text, then linearUnit's SkSL divided by uResolution
  // — the NODE's size — a second time. t came out around 0.003, every
  // glyph painted the first stop, flat, and nothing said so. A logotype
  // came back solid steel blue.
  Host host(320, 160);
  host.composer.render(box().padding(20).child(
      text(u8"HH", whiteStyle(96))
          .textFill(Material::linearUnit({0, 0}, {0, 1},
                                         {{0.0f, {1, 0, 0, 1}},
                                          {1.0f, {0, 0, 1, 1}}}))));
  host.frame();

  // Walk the glyph band and collect the reddest and bluest inked pixels.
  int bestRedY = -1, bestBlueY = -1;
  int bestRed = 0, bestBlue = 0;
  for (int y = 0; y < 160; ++y)
    for (int x = 0; x < 320; ++x) {
      const SkColor c = host.pixel(x, y);
      if (c == SK_ColorBLACK)
        continue;
      if ((int)SkColorGetR(c) > bestRed) { bestRed = SkColorGetR(c); bestRedY = y; }
      if ((int)SkColorGetB(c) > bestBlue) { bestBlue = SkColorGetB(c); bestBlueY = y; }
    }
  ASSERT_GE(bestRedY, 0);
  ASSERT_GE(bestBlueY, 0);
  EXPECT_GT(bestRed, 180);
  EXPECT_GT(bestBlue, 180);
  // The ramp runs top to bottom across the CAP BAND, so red is above blue.
  EXPECT_LT(bestRedY, bestBlueY - 20);
}

TEST(ComposeFx, WipeRevealsAlongAnAxisWithoutSquashing) {
  // Three studies asked for this. trim() walks the PERIMETER, so on a
  // filled shape it sweeps a wedge round the outline instead of extending
  // the surface; scaleX/scaleY SQUASH, which a striped fill shows
  // immediately. The last study's workaround left the retained tree
  // entirely — snapshot() plus a hand-written clipRect in a
  // custom(Cache::None) leaf — forfeiting decorations, hit-testing and
  // pruning on twelve nodes.
  auto lit = [](Host &host, int x0, int x1) {
    int n = 0;
    for (int x = x0; x < x1; ++x)
      n += host.pixel(x, 100) != SK_ColorBLACK;
    return n;
  };
  auto build = [](float angle, float t) {
    return box().child(box()
                           .absolute()
                           .left(20)
                           .top(20)
                           .width(160)
                           .height(160)
                           .fill(Fill::color({1, 0, 0, 1}))
                           .mask(by::edge(angle, t)));
  };

  Host half(200, 200);
  half.composer.render(build(0.0f, 0.5f)); // left to right, half revealed
  half.frame();
  EXPECT_GT(lit(half, 25, 95), 60);  // the left half is there…
  EXPECT_EQ(lit(half, 110, 178), 0); // …and the right half is not

  // It REVEALS rather than squashes: the revealed part keeps its own
  // scale, so the edge lands at the box's midpoint, not at 0.5 × width
  // from a shrunken origin.
  int edge = 0;
  for (int x = 20; x < 180; ++x)
    if (half.pixel(x, 100) != SK_ColorBLACK)
      edge = x;
  EXPECT_NEAR(edge, 100, 3);

  // Any angle: 90 degrees wipes top to bottom.
  Host down(200, 200);
  down.composer.render(build(90.0f, 0.5f));
  down.frame();
  int topInk = 0, bottomInk = 0;
  for (int y = 25; y < 95; ++y)
    topInk += down.pixel(100, y) != SK_ColorBLACK;
  for (int y = 110; y < 175; ++y)
    bottomInk += down.pixel(100, y) != SK_ColorBLACK;
  EXPECT_GT(topInk, 60);
  EXPECT_EQ(bottomInk, 0);

  // Fully open and fully closed are the obvious things.
  Host all(200, 200), none(200, 200);
  all.composer.render(build(0.0f, 1.0f));
  all.frame();
  none.composer.render(build(0.0f, 0.0f));
  none.frame();
  EXPECT_GT(lit(all, 25, 175), 140);
  EXPECT_EQ(lit(none, 20, 180), 0);
}

// RENAMED 2026-07-28 (audit): nothing here asserts the paint-only half (no
// bounds()-unchanged check, the way GradDrivesPaintOnlyWhenAdvanceInvariant
// does it) — what it proves is the BIND: a gate fraction repaints with no
// render() call.
TEST(ComposeFx, EdgeGateIsBindableWithoutARedescribe) {
  // Bound like the transforms: a bound fraction repaints without a
  // re-describe.
  Host host(200, 200);
  choreograph::Output<float> reveal{0.0f};
  host.composer.render(box().child(box()
                                       .absolute()
                                       .inset(20)
                                       .fill(Fill::color({1, 0, 0, 1}))
                                       .mask(by::edge(0.0f, &reveal))));
  host.frame();
  EXPECT_EQ(host.pixel(100, 100), SK_ColorBLACK);

  reveal = 1.0f; // no render(), no re-describe
  host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(100, 100)), 180);
}

TEST(ComposeText, TextStrokeDressesTheGlyphsNotTheBox) {
  // Element::stroke() dresses the node's BOX outline, which is a
  // different thing, so thickening a face meant dropping to
  // PaintStyle::addUnderlay with a hand-built stroke paint. Three studies
  // did it; one spelled "1 px outline plus offset shadow" as 117 full
  // re-draws of a paragraph through echo().
  auto count = [](Host &host, bool wantGreen) {
    int n = 0;
    for (int y = 0; y < 160; ++y)
      for (int x = 0; x < 320; ++x) {
        const SkColor c = host.pixel(x, y);
        n += wantGreen ? (SkColorGetG(c) > 180 && SkColorGetR(c) < 90)
                       : (SkColorGetR(c) > 180 && SkColorGetG(c) < 90);
      }
    return n;
  };

  Host plain(320, 160), outlined(320, 160);
  auto style = whiteStyle(96);
  style.paint.foreground.setColor4f({1, 0, 0, 1}, nullptr);
  plain.composer.render(box().padding(20).child(text(u8"HH", style)));
  plain.frame();
  outlined.composer.render(box().padding(20).child(
      text(u8"HH", style).textStroke(8.0f, Fill::color({0, 1, 0, 1}))));
  outlined.frame();

  // The letterform bodies still paint in the fill colour…
  EXPECT_GT(count(outlined, /*green=*/false), 100);
  // …with a green ring around them that was not there before.
  EXPECT_EQ(count(plain, /*green=*/true), 0);
  EXPECT_GT(count(outlined, /*green=*/true), 200);
  // The node's own box is untouched — this is glyph-level, not stroke().
  EXPECT_EQ(outlined.pixel(2, 2), SK_ColorBLACK);
}

TEST(ComposeText, TextStrokeComposesWithTextFill) {
  // The stroke is a pass BENEATH whatever fills the letterforms, so the
  // two spell "engraved chrome type" together rather than fighting.
  Host host(320, 160);
  host.composer.render(box().padding(20).child(
      text(u8"HH", whiteStyle(96))
          .textStroke(9.0f, Fill::color({0, 1, 0, 1}))
          .textFill(Material::linearUnit({0, 0}, {0, 1},
                                         {{0.0f, {1, 0, 0, 1}},
                                          {1.0f, {0, 0, 1, 1}}}))));
  host.frame();
  int green = 0, ramp = 0;
  for (int y = 0; y < 160; ++y)
    for (int x = 0; x < 320; ++x) {
      const SkColor c = host.pixel(x, y);
      green += SkColorGetG(c) > 180 && SkColorGetR(c) < 90 && SkColorGetB(c) < 90;
      ramp += (SkColorGetR(c) > 150 || SkColorGetB(c) > 150) && SkColorGetG(c) < 90;
    }
  EXPECT_GT(green, 200); // the outline survives the fill override…
  EXPECT_GT(ramp, 100);  // …and the ramp still fills the bodies
}

TEST(ComposeDebug, CoverageOverAnArbitraryRegionAndComponentCounting) {
  // An annulus, a sector, a plate — anything whose outline is not a box
  // cannot be tested against its bounds without counting the parts
  // outside it as gaps. The astrolabe study's zodiac ring needed exactly
  // this, and got 62 phantom gaps first try from chord error against a
  // true circle.
  auto rect = [](float l, float t, float r, float b) {
    SkPathBuilder p;
    p.addRect(SkRect::MakeLTRB(l, t, r, b));
    return p.detach();
  };
  // A DISC covered by two half-squares that also spill outside it. The
  // rect overload would call the spill "doubled" nowhere and the corners
  // "uncovered"; the region overload only asks about the disc.
  SkPathBuilder discBuilder;
  discBuilder.addCircle(50, 50, 40);
  const SkPath disc = discBuilder.detach();
  const std::vector<SkPath> halves = {rect(0, 0, 50, 100), rect(50, 0, 100, 100)};

  const auto onRect = debug::coverage(halves, SkRect::MakeWH(100, 100), 64);
  EXPECT_TRUE(onRect.exact()); // the square really is covered exactly
  const auto onDisc = debug::coverage(halves, disc, 64);
  EXPECT_TRUE(onDisc.exact());
  EXPECT_LT(onDisc.samples, onRect.samples); // it tested fewer points…
  EXPECT_GT(onDisc.samples, 1000);           // …but a real number of them

  // components(): "is this one piece of metal?" — the question a rete, a
  // knot and a decorated tiling all actually ask, which the degree list
  // alone cannot answer.
  auto seg = [](float x0, float y0, float x1, float y1) {
    SkPathBuilder p;
    p.moveTo(x0, y0).lineTo(x1, y1);
    return p.detach();
  };
  const std::vector<SkPath> chain = {seg(0, 0, 10, 0), seg(10, 0, 20, 0),
                                     seg(20, 0, 30, 0)};
  EXPECT_EQ(debug::endpointDegrees(chain).components(), 1u);

  const std::vector<SkPath> split = {seg(0, 0, 10, 0), seg(10, 0, 20, 0),
                                     seg(40, 0, 50, 0)};
  EXPECT_EQ(debug::endpointDegrees(split).components(), 2u);
}

TEST(ComposeDecorations, AStrokeCanTakeAMaterial) {
  // fill() takes a Material — unit-square authoring, structural
  // comparison, live uniforms — and a stroke took only the kernel Fill,
  // which is node-local pixels compared by shader pointer. On an object
  // whose surfaces are mostly STROKES, that meant writing the same brass
  // twice, once per return type.
  PathFormat f = util::stroke(30, Fill::color({1, 1, 1, 1}));
  f.strokeMaterial = Material::linearUnit({0, 0}, {1, 0},
                                          {{0.0f, {1, 0, 0, 1}},
                                           {1.0f, {0, 0, 1, 1}}});
  Host host(200, 200);
  host.composer.render(box().child(box()
                                       .absolute()
                                       .inset(20)
                                       .fill(Fill::none())
                                       .foreground(f)));
  host.frame();
  // The ramp runs across the node, so the left edge of the stroke is red
  // and the right edge blue — a Fill could only have been one colour.
  EXPECT_GT(SkColorGetR(host.pixel(20, 100)), 150);
  EXPECT_LT(SkColorGetB(host.pixel(20, 100)), 110);
  EXPECT_GT(SkColorGetB(host.pixel(180, 100)), 150);
  EXPECT_LT(SkColorGetR(host.pixel(180, 100)), 110);
}

TEST(ComposeDebug, ClosedContoursHaveNoEndpointsAndSaySo) {
  // A ring of closed sectors used to come back as N points of degree 1 —
  // neither right nor wrong, just meaningless, and silently so. A closed
  // contour has no endpoints; the count of them is now reported instead.
  auto sector = [](float a0, float a1) {
    SkPathBuilder p;
    p.moveTo(0, 0)
        .lineTo(std::cos(a0) * 50, std::sin(a0) * 50)
        .lineTo(std::cos(a1) * 50, std::sin(a1) * 50)
        .close();
    return p.detach();
  };
  std::vector<SkPath> ring;
  for (int i = 0; i < 12; ++i)
    ring.push_back(sector((float)i * SK_FloatPI / 6.0f,
                          (float)(i + 1) * SK_FloatPI / 6.0f));

  const auto d = debug::endpointDegrees(ring);
  EXPECT_EQ(d.closedContours, 12u);
  EXPECT_TRUE(d.points.empty()); // …and no phantom degree-1 vertices
  EXPECT_TRUE(d.outside(2, 2).empty());

  // Open contours still work exactly as before, and mixing the two keeps
  // the open ones' endpoints while counting the closed ones.
  auto seg = [](float x0, float y0, float x1, float y1) {
    SkPathBuilder p;
    p.moveTo(x0, y0).lineTo(x1, y1);
    return p.detach();
  };
  std::vector<SkPath> mixed = {seg(0, 0, 10, 0), seg(10, 0, 20, 0),
                               sector(0.0f, 0.5f)};
  const auto m = debug::endpointDegrees(mixed);
  EXPECT_EQ(m.closedContours, 1u);
  EXPECT_EQ(m.points.size(), 3u);       // the chain's three endpoints
  EXPECT_EQ(m.outside(2, 2).size(), 2u); // its two loose ends
}

TEST(ComposeMaterials, UnitRampsTakeAnyNumberOfStops) {
  // It used to be a fixed six with the tail clamped, which two studies
  // ran out of from opposite directions — a 24-run tartan sett and a
  // 72-step chromatic sweep — and both fell back to hand-written pattern
  // programs. The count is now baked into the source with one effect
  // cached per count, the rule Patterns.h already follows for grain's
  // octaves.
  auto sweep = [](int n) {
    std::vector<Stop> stops;
    for (int i = 0; i < n; ++i) {
      const float t = (float)i / (float)(n - 1);
      // A sawtooth the six-stop version could not have represented:
      // alternating black and white at every step.
      const float v = (i % 2) ? 1.0f : 0.0f;
      stops.push_back({t, {v, v, v, 1}});
    }
    Host host(256, 32);
    host.composer.render(box().child(
        box().absolute().inset(0).fill(
            Material::linearUnit({0, 0}, {1, 0}, stops))));
    host.frame();
    // Count the light/dark transitions across the middle scanline.
    int flips = 0;
    bool light = SkColorGetR(host.pixel(0, 16)) > 128;
    for (int x = 1; x < 256; ++x) {
      const bool now = SkColorGetR(host.pixel(x, 16)) > 128;
      flips += now != light;
      light = now;
    }
    return flips;
  };

  // Six stops = five alternations. Twenty-four and seventy-two scale with
  // the count, which is exactly what the fixed version could not do.
  EXPECT_NEAR(sweep(6), 5, 1);
  EXPECT_NEAR(sweep(24), 23, 2);
  EXPECT_NEAR(sweep(72), 71, 4);

  // Degenerate counts still behave.
  Host one(64, 64);
  one.composer.render(box().child(box().absolute().inset(0).fill(
      Material::linearUnit({0, 0}, {1, 0}, {{0.0f, {1, 0, 0, 1}}}))));
  one.frame();
  EXPECT_GT(SkColorGetR(one.pixel(32, 32)), 200);
}

TEST(ComposeInstances, TheAtlasChoosesItsOwnFilter) {
  // The last of the five hardcoded-kLinear paths. Instancing's biggest
  // real use is tilemaps and sprite sheets — pixel grids — where linear
  // filtering is exactly wrong.
  auto blend = [](SkFilterMode mode) {
    auto atlas = std::make_shared<instancing::Atlas>(1.0f);
    atlas->filter(mode);
    // A cell that is half red, half green: magnified, linear invents a
    // blend band across the seam and nearest does not.
    atlas->cell(box().width(16).height(16).row()
                    .child(box().width(8).height(16)
                               .fill(Fill::color({1, 0, 0, 1})))
                    .child(box().width(8).height(16)
                               .fill(Fill::color({0, 1, 0, 1}))),
                {16, 16});
    auto pool = std::make_shared<instancing::Pool>();
    pool->add({100, 100}, 0, 0.0f, 8.0f); // 8x magnification
    Host host(200, 200);
    host.composer.render(box().absolute().inset(0).child(
        instancing::instances(atlas, pool, instancing::Mode::Data)));
    host.frame();
    int mixed = 0;
    for (int x = 85; x < 115; ++x) {
      const SkColor c = host.pixel(x, 100);
      const bool pureRed = SkColorGetR(c) > 200 && SkColorGetG(c) < 40;
      const bool pureGreen = SkColorGetG(c) > 200 && SkColorGetR(c) < 40;
      mixed += !pureRed && !pureGreen;
    }
    return mixed;
  };
  EXPECT_GT(blend(SkFilterMode::kLinear), 2);
  EXPECT_LE(blend(SkFilterMode::kNearest), 1);
}

TEST(ComposePatterns, GridLinesTakeATwoAxisPitch) {
  // A lattice whose x and y pitch differ is not exotic — an X-COM control
  // panel's is 5 x 2 — and gridLines took one `spacing`.
  Host host(120, 120);
  host.composer.render(box().child(box().absolute().inset(0).fill(
      patterns::gridLines(20.0f, 8.0f, 2.0f, {1, 1, 1, 1}).material())));
  host.frame();
  auto rules = [&](bool vertical) {
    int runs = 0;
    bool on = false;
    for (int i = 0; i < 120; ++i) {
      // Sample mid-cell on the other axis: a column that lands ON a
      // vertical rule is lit for its whole length and counts one run.
      const SkColor c = vertical ? host.pixel(i, 63) : host.pixel(70, i);
      const bool lit = SkColorGetR(c) > 128;
      runs += lit && !on;
      on = lit;
    }
    return runs;
  };
  EXPECT_NEAR(rules(/*vertical=*/true), 6, 1);   // 120 / 20
  EXPECT_NEAR(rules(/*vertical=*/false), 15, 2); // 120 / 8
}


TEST(ComposeInstances, VariantsAreConsecutiveBakesOfOneRecipe) {
  // §2: (cell, variant) — several BAKES of one recipe. The X-COM shade
  // shape in miniature: three shades of one tile, each a re-render (a
  // per-channel ramp tints() cannot express), addressed as first + v.
  auto atlas = std::make_shared<instancing::Atlas>(1.0f);
  const int first = atlas->variants(3, {20, 20}, [](int v) {
    const float g = 0.2f + 0.3f * (float)v; // three distinct shades
    return box().fill(Fill::color({g, g, g, 1}));
  });
  EXPECT_EQ(atlas->frameCount(), 3);
  auto pool = std::make_shared<instancing::Pool>();
  for (int v = 0; v < 3; ++v)
    pool->add({30.0f + 60.0f * (float)v, 30.0f});
  auto frames = pool->frames();
  for (int v = 0; v < 3; ++v)
    frames[v] = first + v;
  pool->commit();
  Host host(200, 200);
  host.composer.render(box().absolute().inset(0).child(
      instancing::instances(atlas, pool, instancing::Mode::Data)));
  host.frame();
  const unsigned r0 = SkColorGetR(host.pixel(30, 30));
  const unsigned r1 = SkColorGetR(host.pixel(90, 30));
  const unsigned r2 = SkColorGetR(host.pixel(150, 30));
  EXPECT_LT(r0 + 20, r1); // strictly brighter per variant
  EXPECT_LT(r1 + 20, r2);
}

TEST(ComposeInstances, TheAlphaLaneFadesWithoutTouchingTheTint) {
  // §2: tints() was the only opacity lane. alphas() is opt-in, composes
  // with the authored tint, and place::repeat writes IT rather than
  // clobbering tints — the lane-hygiene repair.
  auto atlas = std::make_shared<instancing::Atlas>(1.0f);
  atlas->cell(box().fill(Fill::color({1, 0, 0, 1})), {40, 40});
  auto pool = std::make_shared<instancing::Pool>();
  instancing::place::repeat(*pool, 2, {40, 40}, {80, 0}, 0.0f, 1.0f,
                            1.0f, 0.25f);
  Host host(200, 200);
  host.composer.render(box().absolute().inset(0).child(
      instancing::instances(atlas, pool, instancing::Mode::Data)));
  host.frame();
  const unsigned full = SkColorGetR(host.pixel(40, 40));
  const unsigned faded = SkColorGetR(host.pixel(120, 40));
  EXPECT_GT(full, 240u);          // first copy at full opacity
  EXPECT_LT(faded, 100u);         // last copy at 25% over black
  EXPECT_GT(faded, 20u);
  // …and the tint lane was never written: the fade is alphas()'s.
  EXPECT_EQ(pool->tints()[1].fA, 1.0f);
  EXPECT_TRUE(pool->hasAlphas());
}

TEST(ComposeInstances, PickInvertsTheStampTopmostFirst) {
  // §2: hitTest cannot see a pool instance (the field is one custom()
  // draw). pick() is the inverse projection, against the same lanes the
  // stamp reads — rotation, scale, and topmost-wins where stamps overlap.
  using namespace sigil::compose::instancing;
  Atlas atlas(1.0f);
  atlas.cell(box().fill(Fill::color({1, 0, 0, 1})), {40, 20});
  Pool pool;
  pool.add({100, 100});          // instance 0
  pool.add({120, 100});          // instance 1, overlapping 0's right side
  pool.scales()[1] = 0.5f;       // 20x10 quad at (120,100)
  pool.rotations()[0] = (float)M_PI / 2.0f; // 0 is rotated 90°: 20x40 now
  pool.commit();

  // Overlap region: (118, 100) is inside both — topmost (1) wins.
  auto hit = pick(pool, atlas, {118, 100});
  ASSERT_TRUE(hit.has_value());
  EXPECT_EQ(*hit, 1u);
  // Rotation honoured: (100, 117) is inside 0's rotated quad (tall now),
  // outside its unrotated footprint.
  hit = pick(pool, atlas, {100, 117});
  ASSERT_TRUE(hit.has_value());
  EXPECT_EQ(*hit, 0u);
  // …and (117, 100) horizontally would have been inside UNrotated 0 but
  // is outside the rotated quad and outside 1.
  EXPECT_FALSE(pick(pool, atlas, {84, 100}).has_value());
  // Scale honoured: outside 1's shrunken quad.
  EXPECT_FALSE(pick(pool, atlas, {135, 100}).has_value());
}

TEST(ComposeText, MeasureRunShapesOnceAndMatchesTheLaidOutElement) {
  // §9: "no way to shape a run without building an Element" — measure()
  // is per-Element, so hand-placing N glyphs cost N layouts. measureRun()
  // is ONE layout through the same shaping path a text() leaf takes; the
  // pin is that its advances reproduce what the Element machinery
  // measures for the same run.
  const sigil::weave::TextStyle style = whiteStyle(24);
  const std::vector<float> advances =
      measureRun(u8"HAMBURGEFONTSIV", style, fonts());
  ASSERT_FALSE(advances.empty());
  float sum = 0;
  for (float a : advances) {
    EXPECT_GT(a, 0.0f);
    sum += a;
  }
  // The independent arm: the full Element path (reconcile + Yoga + text
  // measure) sizes the same run. measure() ceils the shaped width, so
  // agreement is to the ceil.
  const SkSize laidOut = measure(text(u8"HAMBURGEFONTSIV", style), fonts());
  EXPECT_NEAR(std::ceil(sum), laidOut.width(), 1.01f)
      << "measureRun's advances disagree with the laid-out element";
  // Controls: a doubled face doubles the run (shaping is live, not a
  // cached constant)…
  float sumBig = 0;
  for (float a : measureRun(u8"HAMBURGEFONTSIV", whiteStyle(48), fonts()))
    sumBig += a;
  EXPECT_NEAR(sumBig, sum * 2.0f, sum * 0.1f);
  // …an empty run shapes to nothing, and the count is the GLYPH count —
  // one per character here, no ligatures in play.
  EXPECT_TRUE(measureRun(u8"", style, fonts()).empty());
  EXPECT_EQ(advances.size(), 15u);
}
