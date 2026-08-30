#include "support/TextTestSupport.h"

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
  EXPECT_FLOAT_EQ(
      bind(nullptr).source(0, 200).target(0, 1).value().apply(50.0f), 0.25f);
  // invert composes with what came before rather than resetting it.
  EXPECT_FLOAT_EQ(bind(nullptr).invert().value().apply(0.25f), 0.75f);
  EXPECT_FLOAT_EQ(bind(nullptr).target(0, 2).invert().value().apply(0.25f),
                  1.0f - 0.5f);
  // the curve runs BEFORE the affine, on the normalised value…
  EXPECT_FLOAT_EQ(bind(nullptr)
                      .map(&choreograph::easeNone)
                      .target(0, 10)
                      .value()
                      .apply(0.4f),
                  4.0f);
  // …and the clamp always runs last, whenever it is written.
  EXPECT_FLOAT_EQ(bind(nullptr).clamp(0, 1).target(0, 4).value().apply(0.5f),
                  1.0f);
}

TEST(ComposeBindings, AShapedBindingDrivesThePropertyInPixels) {
  // One Output, two units. A phase in [0,1] is what a reveal or an opacity
  // wants; a translation wants PIXELS. Without a shaping map on the binding,
  // driving both from one motion means carrying a second Output updated
  // alongside the first — two things to keep in step for no reason.
  Host host(200, 200);
  choreograph::Output<float> phase{0.0f};
  host.composer.render(
      box().child(box()
                      .width(20)
                      .height(20)
                      .absolute()
                      .left(0)
                      .top(90)
                      .fill(red())
                      .translateX(bind(&phase).target(0, 160))));
  auto redAt = [&](int x) { return SkColorGetR(host.pixel(x, 100)) > 180; };

  host.frame();
  EXPECT_TRUE(redAt(10));  // phase 0 → x = 0
  EXPECT_FALSE(redAt(170));

  phase = 1.0f;
  host.frame();
  EXPECT_FALSE(redAt(10));
  EXPECT_TRUE(redAt(170));  // phase 1 → x = 160, unscaled would be x = 1

  phase = 0.5f;
  host.frame();
  EXPECT_TRUE(redAt(85));  // and it is linear in between
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
  // A text run's BASELINE has to reach textEqual(). Leave it out and
  // re-describing with a new path or a new `at` prunes, so the run keeps
  // riding the old baseline forever. The compiler is no help here: a
  // TextPath holding a std::function has its defaulted operator== implicitly
  // deleted, so the omission produces no error anywhere.
  Host host(240, 240);
  auto ring = [](float at) {
    return box().child(text(u8"HHHHHHHHHH", whiteStyle(22))
                           .key("ring")
                           .width(240)
                           .height(240)
                           .absolute()
                           .left(0)
                           .top(0)
                           .onPath({.path = shapes::arc(180.0f, 359.9f),
                                    .at = at,
                                    .align = TextPath::Align::Center}));
  };
  auto lit = [&](int y0, int y1) {
    int count = 0;
    for (int y = y0; y < y1; ++y)
      for (int x = 0; x < 240; ++x) count += host.pixel(x, y) != SK_ColorBLACK;
    return count;
  };

  host.composer.render(ring(0.25f));
  host.frame();
  ASSERT_GT(lit(0, 110), 200);

  host.composer.render(ring(0.75f));  // same key, same text, new baseline
  host.frame();
  EXPECT_GT(lit(140, 240), 200);  // it moved…
  EXPECT_LT(lit(0, 110), 40);     // …and did not stay put
}

TEST(ComposeMotion, AnEmptyEasingMeansTheDefaultRatherThanACrash) {
  // Transition is an aggregate, so `{360ms, {}, 220ms}` — the obvious way to
  // write "default curve, but I need to name the delay" — initialises `ease`
  // to an EMPTY std::function. It compiles, so the only options are throwing
  // bad_function_call on the first frame or treating empty as "the default
  // curve". It is the latter.
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
  host.frame();     // would throw here
  host.frame(0.4);  // land the entrance
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
  const SkSize box{200, 100};  // deliberately non-square: unit → half-extents

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
    if (SkPoint::Distance(five.getPoint(i), centre) > 49.0f) ++tips;
  EXPECT_GT(tips, 5);

  // Spirals start at the centre and end at the rim.
  const SkPath coil = shapes::spiral(3)(box);
  EXPECT_NEAR(SkPoint::Distance(coil.getPoint(0), centre), 0.0f, 1.0f);
  EXPECT_GT(SkPoint::Distance(coil.getPoint(coil.countPoints() - 1), centre),
            40.0f);

  // Everything stays inside the box it was inscribed in.
  for (const SkPath* p : {&ellipse, &damped, &five, &coil}) {
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
    atlas->cell(
        box().width(40).height(40).fill(Fill::color({0.25f, 0.25f, 0.25f, 1})),
        {40, 40});
    auto pool = std::make_shared<instancing::Pool>();
    for (int i = 0; i < 3; ++i)  // three sprites stacked on one spot
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

  EXPECT_GT(overR, 40);          // one opaque sprite's worth
  EXPECT_LT(overR, 90);          // …and three of them are no brighter
  EXPECT_GT(plusR, overR + 60);  // additive stacks all three
}

TEST(ComposeText, OnPathFillsEveryContourNotJustTheFirst) {
  // A path clipped to a frame commonly comes back as SEVERAL contours, so a
  // baseline that takes only the first one drops the rest of the run with no
  // diagnostic. Every contour is one INTERVAL of the run's one line: the
  // words fill them in order, and a word that does not fit the contour it
  // reached starts the next one rather than bending across the gap between
  // two disconnected curves.
  auto twoSegments = [](SkSize s) {
    SkPathBuilder b;
    b.moveTo(10, 40).lineTo(190, 40);    // contour 1: across the top
    b.moveTo(10, 160).lineTo(190, 160);  // contour 2: across the bottom
    return b.detach();
  };
  auto lit = [](Host& host, int y0, int y1) {
    int count = 0;
    for (int y = y0; y < y1; ++y)
      for (int x = 0; x < 200; ++x) count += host.pixel(x, y) != SK_ColorBLACK;
    return count;
  };

  // A run long enough to overflow contour 1 must continue onto contour 2.
  Host host(200, 200);
  host.composer.render(
      box().child(text(u8"HHHH HHHH HHHH HHHH HHHH HHHH", whiteStyle(20))
                      .width(200)
                      .height(200)
                      .absolute()
                      .left(0)
                      .top(0)
                      .onPath({.path = twoSegments, .at = 0.0f})));
  host.frame();
  EXPECT_GT(lit(host, 20, 60), 200);    // ink on the first contour…
  EXPECT_GT(lit(host, 140, 180), 200);  // …and on the second, which a
                                        // first-contour-only walk would
                                        // leave silently unreachable
}

TEST(ComposeText, OnPathBreaksAtWordsBetweenContours) {
  // The counterpart contract, pinned so it cannot drift back: a WORD is
  // never split across two contours. The two segments here are far apart,
  // and a word bent across the gap would land letters in the empty band
  // between them.
  auto twoSegments = [](SkSize) {
    SkPathBuilder b;
    b.moveTo(10, 40).lineTo(120, 40);
    b.moveTo(10, 160).lineTo(190, 160);
    return b.detach();
  };
  auto lit = [](Host& host, int y0, int y1) {
    int count = 0;
    for (int y = y0; y < y1; ++y)
      for (int x = 0; x < 200; ++x) count += host.pixel(x, y) != SK_ColorBLACK;
    return count;
  };
  Host host(200, 200);
  host.composer.render(
      box().child(text(u8"HHHH HHHHHHHHHH", whiteStyle(20))
                      .width(200)
                      .height(200)
                      .absolute()
                      .left(0)
                      .top(0)
                      .onPath({.path = twoSegments, .at = 0.0f})));
  host.frame();
  EXPECT_GT(lit(host, 20, 60), 100);    // the short word on contour 1…
  EXPECT_GT(lit(host, 140, 180), 200);  // …the long one whole on contour 2
  EXPECT_EQ(lit(host, 70, 130), 0) << "a word bent across the gap";
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
  const std::vector<SkPath> exact = {rect(0, 0, 50, 100),
                                     rect(50, 0, 100, 100)};
  const auto good = test::coverage(exact, region, 64);
  EXPECT_TRUE(good.exact());
  EXPECT_EQ(good.uncovered, 0);
  EXPECT_EQ(good.doubled, 0);

  // The same two halves, one shifted 10 px right: a 10-wide gap on the
  // left, a 10-wide overlap in the middle. Equal areas, so the total is
  // unchanged and both pieces are still inside the square.
  const std::vector<SkPath> broken = {rect(10, 0, 60, 100),
                                      rect(50, 0, 100, 100)};
  float area = 0;
  for (const SkPath& p : broken)
    area += p.getBounds().width() * p.getBounds().height();
  EXPECT_FLOAT_EQ(area, 100 * 100);  // area conservation: PASSES
  for (const SkPath& p : broken)
    EXPECT_TRUE(region.contains(p.getBounds()));  // containment: PASSES

  const auto bad = test::coverage(broken, region, 64);
  EXPECT_FALSE(bad.exact());  // …and coverage does not
  EXPECT_NEAR(bad.uncoveredFraction(), 0.10f, 0.02f);
  EXPECT_NEAR(bad.doubledFraction(), 0.10f, 0.02f);
  ASSERT_FALSE(bad.uncoveredAt.empty());
  EXPECT_LT(bad.uncoveredAt.front().x(), 10.0f);  // the witness is the gap
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
  const auto degrees = test::endpointDegrees(chain);
  EXPECT_EQ(degrees.points.size(), 4u);
  EXPECT_EQ(degrees.outside(2, 2).size(), 2u);  // the two loose ends

  // Move one segment off its joint: now four loose ends, not two.
  const std::vector<SkPath> broken = {seg(0, 0, 10, 0), seg(11, 0, 20, 0),
                                      seg(20, 0, 30, 0)};
  EXPECT_EQ(test::endpointDegrees(broken).outside(2, 2).size(), 4u);
}

TEST(ComposeText, AutoFlipIsOnePerRunDecisionSampledAcrossTheRun) {
  // autoFlip is a PER-RUN decision, which is easy to mistake for a no-op. A
  // run that stays on the bottom flips; one that stays on the top does not;
  // and one that WRAPS PAST the crossover cannot be fixed by a single flip,
  // so it is not pretended otherwise. The answer for that case is two runs,
  // top and bottom set separately.
  //
  // The decision samples ACROSS the run rather than reading one midpoint
  // tangent, so a midpoint that happens to land on a locally odd tangent
  // cannot decide for every glyph in the run.
  auto ring = [](float at, bool flip) {
    return box().child(text(u8"HHHHHHHH", whiteStyle(20))
                           .width(200)
                           .height(200)
                           .absolute()
                           .left(0)
                           .top(0)
                           .onPath({.path = shapes::circle(),
                                    .at = at,
                                    .align = TextPath::Align::Center,
                                    .offset = 4.0f,
                                    .autoFlip = flip}));
  };
  auto snap = [](Host& host) {
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(200, 200));
    host.surface->readPixels(bm.pixmap(), 0, 0);
    return bm;
  };
  auto differs = [](const SkBitmap& a, const SkBitmap& b) {
    int n = 0;
    for (int y = 0; y < 200; ++y)
      for (int x = 0; x < 200; ++x) n += a.getColor(x, y) != b.getColor(x, y);
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
  // A stepped readout — a slider with N sprite frames, a gauge with N
  // notches — is a different widget from a smooth one sampled at draw time.
  // The quantisation is part of the design, so it belongs in the binding,
  // where it composes with the rest of the shaping map.
  auto q = [](float v) {
    return bind(nullptr).quantize(5).value().apply(v);  // levels 0,.25,.5,.75,1
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
  host.composer.render(box().child(
      box().absolute().left(20).top(80).width(160).height(40).fill(&bar)));
  host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(100, 100)), 180);
  EXPECT_LT(SkColorGetG(host.pixel(100, 100)), 80);

  bar = Fill::color({0, 1, 0, 1});  // no re-render, no re-describe
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
  auto covered = [&](const SkPath& p) {
    const SkPath pieces[] = {p};
    const auto c = test::coverage(pieces, region, 128);
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

TEST(ComposeContent, SamplingReachesTheImageLeaf) {
  // Every blessed image path hardcoded kLinear, so pixel art, tilemaps
  // and simulation buffers drawn through image() were silently blurred.
  // Material::image() has always taken sampling; the element factory did
  // not, so the fix was discoverable only by diffing two signatures.
  auto atlas = twoCellAtlas();  // 32x16: left half red, right half green
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

TEST(ComposeMotion, AddFixedRunsAtItsOwnRateWhateverTheHostDraws) {
  // Every simulation-shaped study reinvented the accumulator and its
  // spiral-of-death clamp — a cellular automaton at 27 Hz behind the DOOM
  // PlayStation titles, particles at 24. The library had declared
  // choppiness for shaders (Material::quantizeTime) and nothing for logic.
  auto stepsOverOneSecond = [](double fps) {
    sigil::motion::Ticker ticker;
    int steps = 0;
    ticker.addFixed(27.0, [&] {
      ++steps;
      return true;
    });
    const double dt = 1.0 / fps;
    for (int i = 0; i < (int)std::lround(fps); ++i) ticker.tick(dt);
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
    ticker.addFixed(
        60.0,
        [&] {
          ++steps;
          return true;
        },
        /*maxCatchUp=*/4, nullptr, &status);
    ticker.tick(10.0);  // ten seconds in one frame = 600 steps of backlog
    EXPECT_EQ(steps, 4);
    EXPECT_EQ(status.stepsRun, 4);
    EXPECT_TRUE(status.clamped);
    // …and the backlog is DISCARDED, not carried into the next frame.
    steps = 0;
    ticker.tick(1.0 / 60.0);
    EXPECT_EQ(steps, 1);
    EXPECT_FALSE(status.clamped);
  }

  // Reproducibility: the step count is derived from TOTAL elapsed time, so
  // the same instant lands on the same step whatever the draw rate. An
  // accumulator compared against a step size instead slips by one comparison
  // over a long pre-roll, and only at some frame rates — which reads as a
  // clamp bug rather than as float accumulation.
  {
    // Each rate advances to the SAME total time — otherwise the counts
    // differ for the honest reason that the clocks differ.
    auto stepsAt = [](double fps, double untilSeconds) {
      sigil::motion::Ticker ticker;
      int steps = 0;
      ticker.addFixed(
          60.0,
          [&] {
            ++steps;
            return true;
          },
          64);
      const int frames = (int)std::lround(untilSeconds * fps);
      const double dt = untilSeconds / (double)frames;
      for (int i = 0; i < frames; ++i) ticker.tick(dt);
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

TEST(ComposeMaterials, GlowUnitReachesTheInscribedCircleNotTheCorners) {
  // radialUnit's radius is a fraction of the box's HALF-DIAGONAL, so a soft
  // round light authored at radius 1 has not finished falling off where the
  // INSCRIBED circle is — and on a node also carrying shapes::circle() the
  // remaining alpha becomes a visible hard rim. glowUnit is radialUnit
  // scaled to the inscribed circle instead, so radius 1 reaches zero exactly
  // at the edge that gets clipped.
  const std::vector<Stop> ramp = {{0.0f, {1, 1, 1, 1}}, {1.0f, {0, 0, 0, 1}}};
  auto edgeValue = [&](Material m) {
    Host host(200, 200);
    host.composer.render(
        box().child(box().absolute().inset(0).fill(std::move(m))));
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
      pool->sizes()[0] = {4.0f, 0.25f};  // 80 x 5 — a streak
      pool->commit();
    }
    return box().absolute().inset(0).child(
        instancing::instances(atlas, pool, instancing::Mode::Data));
  };
  auto redSpan = [](Host& host, bool horizontal) {
    int n = 0;
    for (int i = 0; i < 200; ++i) {
      const SkColor c = horizontal ? host.pixel(i, 100) : host.pixel(100, i);
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
  EXPECT_TRUE(SkColorGetR(host.pixel(100, 100)) > 180);  // …in place
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
                           .width(240)
                           .height(240)
                           .absolute()
                           .left(0)
                           .top(0)
                           .onPath({.path = circle,
                                    .at = 0.25f,  // the bottom of the ring
                                    .align = TextPath::Align::Center,
                                    .offset = -50.0f,
                                    .orient = orient}));
  };
  auto footprint = [](Host& host) {
    int minX = 9999, maxX = -1, minY = 9999, maxY = -1;
    for (int y = 0; y < 240; ++y)
      for (int x = 0; x < 240; ++x)
        if (host.pixel(x, y) != SK_ColorBLACK) {
          minX = std::min(minX, x);
          maxX = std::max(maxX, x);
          minY = std::min(minY, y);
          maxY = std::max(maxY, y);
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
  ticker.addFixed(
      10.0,
      [&] {
        ++steps;
        return true;
      },
      8, &alpha);

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

TEST(ComposeText, MetricsExposeTheCapSlackThatPlacementNeeds) {
  // A text node's top is the LINE BOX top, while type is usually positioned
  // by its CAP TOP, so aligning a layout against a reference needs the SLACK
  // between the two. measure() returns only an SkSize, which leaves a caller
  // guessing a fraction of the line height — a constant that changes with
  // every face.
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

TEST(ComposeText, TextFillWorksWithTheUnitRamps) {
  // textFill and the Unit ramps must compose, and they very nearly do not:
  // the metric band already maps the shader's [0,1]² onto the text, so a
  // Unit ramp dividing by the NODE's size a second time collapses the whole
  // gradient to a sliver near zero. Every glyph then paints the first stop,
  // flat — a wrong picture that looks like a deliberate solid fill.
  Host host(320, 160);
  host.composer.render(box().padding(20).child(
      text(u8"HH", whiteStyle(96))
          .textFill(Material::linearUnit(
              {0, 0}, {0, 1}, {{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}}))));
  host.frame();

  // Walk the glyph band and collect the reddest and bluest inked pixels.
  int bestRedY = -1, bestBlueY = -1;
  int bestRed = 0, bestBlue = 0;
  for (int y = 0; y < 160; ++y)
    for (int x = 0; x < 320; ++x) {
      const SkColor c = host.pixel(x, y);
      if (c == SK_ColorBLACK) continue;
      if ((int)SkColorGetR(c) > bestRed) {
        bestRed = SkColorGetR(c);
        bestRedY = y;
      }
      if ((int)SkColorGetB(c) > bestBlue) {
        bestBlue = SkColorGetB(c);
        bestBlueY = y;
      }
    }
  ASSERT_GE(bestRedY, 0);
  ASSERT_GE(bestBlueY, 0);
  EXPECT_GT(bestRed, 180);
  EXPECT_GT(bestBlue, 180);
  // The ramp runs top to bottom across the CAP BAND, so red is above blue.
  EXPECT_LT(bestRedY, bestBlueY - 20);
}

TEST(ComposeFx, WipeRevealsAlongAnAxisWithoutSquashing) {
  // A wipe needs its own gate because neither neighbour can do it. A span
  // window walks the PERIMETER, so on a filled shape it sweeps a wedge round
  // the outline rather than extending the surface; scaleX/scaleY SQUASH,
  // which any striped fill shows immediately. The alternative is leaving the
  // retained tree altogether — snapshot() plus a hand-written clipRect in a
  // Cache::None custom() leaf — which forfeits decorations, hit testing and
  // pruning for the whole subtree.
  auto lit = [](Host& host, int x0, int x1) {
    int n = 0;
    for (int x = x0; x < x1; ++x) n += host.pixel(x, 100) != SK_ColorBLACK;
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
  half.composer.render(build(0.0f, 0.5f));  // left to right, half revealed
  half.frame();
  EXPECT_GT(lit(half, 25, 95), 60);   // the left half is there…
  EXPECT_EQ(lit(half, 110, 178), 0);  // …and the right half is not

  // It REVEALS rather than squashes: the revealed part keeps its own
  // scale, so the edge lands at the box's midpoint, not at 0.5 × width
  // from a shrunken origin.
  int edge = 0;
  for (int x = 20; x < 180; ++x)
    if (half.pixel(x, 100) != SK_ColorBLACK) edge = x;
  EXPECT_NEAR(edge, 100, 3);

  // Any angle: 90 degrees wipes top to bottom.
  Host down(200, 200);
  down.composer.render(build(90.0f, 0.5f));
  down.frame();
  int topInk = 0, bottomInk = 0;
  for (int y = 25; y < 95; ++y) topInk += down.pixel(100, y) != SK_ColorBLACK;
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

TEST(ComposeFx, EdgeGateIsBindableWithoutARedescribe) {
  // An edge gate binds like a transform: a bound fraction repaints with no
  // render() call at all. Note the limit of the claim — this says nothing
  // about the gate being paint-only, since nothing here checks that bounds()
  // held still.
  Host host(200, 200);
  choreograph::Output<float> reveal{0.0f};
  host.composer.render(box().child(box()
                                       .absolute()
                                       .inset(20)
                                       .fill(Fill::color({1, 0, 0, 1}))
                                       .mask(by::edge(0.0f, &reveal))));
  host.frame();
  EXPECT_EQ(host.pixel(100, 100), SK_ColorBLACK);

  reveal = 1.0f;  // no render(), no re-describe
  host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(100, 100)), 180);
}

TEST(ComposeText, TextStrokeDressesTheGlyphsNotTheBox) {
  // Element::stroke() dresses the node's BOX outline, which is a different
  // mark entirely. Without a text-level stroke, thickening a face means
  // dropping to PaintStyle::addUnderlay with a hand-built paint — or, worse,
  // spelling an outline as a ring of offset re-draws of the whole run.
  auto count = [](Host& host, bool wantGreen) {
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
          .textFill(Material::linearUnit(
              {0, 0}, {0, 1}, {{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}}))));
  host.frame();
  int green = 0, ramp = 0;
  for (int y = 0; y < 160; ++y)
    for (int x = 0; x < 320; ++x) {
      const SkColor c = host.pixel(x, y);
      green +=
          SkColorGetG(c) > 180 && SkColorGetR(c) < 90 && SkColorGetB(c) < 90;
      ramp +=
          (SkColorGetR(c) > 150 || SkColorGetB(c) > 150) && SkColorGetG(c) < 90;
    }
  EXPECT_GT(green, 200);  // the outline survives the fill override…
  EXPECT_GT(ramp, 100);   // …and the ramp still fills the bodies
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
  const std::vector<SkPath> halves = {rect(0, 0, 50, 100),
                                      rect(50, 0, 100, 100)};

  const auto onRect = test::coverage(halves, SkRect::MakeWH(100, 100), 64);
  EXPECT_TRUE(onRect.exact());  // the square really is covered exactly
  const auto onDisc = test::coverage(halves, disc, 64);
  EXPECT_TRUE(onDisc.exact());
  EXPECT_LT(onDisc.samples, onRect.samples);  // it tested fewer points…
  EXPECT_GT(onDisc.samples, 1000);            // …but a real number of them

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
  EXPECT_EQ(test::endpointDegrees(chain).components(), 1u);

  const std::vector<SkPath> split = {seg(0, 0, 10, 0), seg(10, 0, 20, 0),
                                     seg(40, 0, 50, 0)};
  EXPECT_EQ(test::endpointDegrees(split).components(), 2u);
}

TEST(ComposeDebug, ClosedContoursHaveNoEndpointsAndSaySo) {
  // A closed contour has NO endpoints, so reporting one per contour is not
  // merely wrong, it is meaningless — and silently so, since a plausible
  // count comes back either way. The endpoint count is reported instead.
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

  const auto d = test::endpointDegrees(ring);
  EXPECT_EQ(d.closedContours, 12u);
  EXPECT_TRUE(d.points.empty());  // …and no phantom degree-1 vertices
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
  const auto m = test::endpointDegrees(mixed);
  EXPECT_EQ(m.closedContours, 1u);
  EXPECT_EQ(m.points.size(), 3u);         // the chain's three endpoints
  EXPECT_EQ(m.outside(2, 2).size(), 2u);  // its two loose ends
}

TEST(ComposeMaterials, UnitRampsTakeAnyNumberOfStops) {
  // A fixed stop count with the tail clamped runs out from both directions
  // — a many-run repeating sett, a long chromatic sweep — and the only way
  // out is a hand-written pattern program. The count is baked into the
  // shader source instead, with one effect cached per count, which is the
  // same rule the noise generators follow for octaves.
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
    host.composer.render(box().child(box().absolute().inset(0).fill(
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
    atlas->cell(
        box()
            .width(16)
            .height(16)
            .row()
            .child(box().width(8).height(16).fill(Fill::color({1, 0, 0, 1})))
            .child(box().width(8).height(16).fill(Fill::color({0, 1, 0, 1}))),
        {16, 16});
    auto pool = std::make_shared<instancing::Pool>();
    pool->add({100, 100}, 0, 0.0f, 8.0f);  // 8x magnification
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

TEST(ComposeInstances, VariantsAreConsecutiveBakesOfOneRecipe) {
  // A variant is a separate BAKE of one recipe, addressed as first + v.
  // That is what tints() cannot do: a variant may differ by a whole
  // re-render — a per-channel ramp, a different shade table — rather than by
  // a multiply.
  auto atlas = std::make_shared<instancing::Atlas>(1.0f);
  const int first = atlas->variants(3, {20, 20}, [](int v) {
    const float g = 0.2f + 0.3f * (float)v;  // three distinct shades
    return box().fill(Fill::color({g, g, g, 1}));
  });
  EXPECT_EQ(atlas->frameCount(), 3);
  auto pool = std::make_shared<instancing::Pool>();
  for (int v = 0; v < 3; ++v) pool->add({30.0f + 60.0f * (float)v, 30.0f});
  auto frames = pool->frames();
  for (int v = 0; v < 3; ++v) frames[v] = first + v;
  pool->commit();
  Host host(200, 200);
  host.composer.render(box().absolute().inset(0).child(
      instancing::instances(atlas, pool, instancing::Mode::Data)));
  host.frame();
  const unsigned r0 = SkColorGetR(host.pixel(30, 30));
  const unsigned r1 = SkColorGetR(host.pixel(90, 30));
  const unsigned r2 = SkColorGetR(host.pixel(150, 30));
  EXPECT_LT(r0 + 20, r1);  // strictly brighter per variant
  EXPECT_LT(r1 + 20, r2);
}

TEST(ComposeInstances, TheAlphaLaneFadesWithoutTouchingTheTint) {
  // alphas() is an opt-in lane that composes with the authored tint, and
  // place::repeat writes IT rather than tints[].fA. Sharing one lane would
  // make a faded pool silently un-tintable.
  auto atlas = std::make_shared<instancing::Atlas>(1.0f);
  atlas->cell(box().fill(Fill::color({1, 0, 0, 1})), {40, 40});
  auto pool = std::make_shared<instancing::Pool>();
  instancing::place::repeat(*pool, 2, {40, 40}, {80, 0}, 0.0f, 1.0f, 1.0f,
                            0.25f);
  Host host(200, 200);
  host.composer.render(box().absolute().inset(0).child(
      instancing::instances(atlas, pool, instancing::Mode::Data)));
  host.frame();
  const unsigned full = SkColorGetR(host.pixel(40, 40));
  const unsigned faded = SkColorGetR(host.pixel(120, 40));
  EXPECT_GT(full, 240u);   // first copy at full opacity
  EXPECT_LT(faded, 100u);  // last copy at 25% over black
  EXPECT_GT(faded, 20u);
  // …and the tint lane was never written: the fade is alphas()'s.
  EXPECT_EQ(pool->tints()[1].fA, 1.0f);
  EXPECT_TRUE(pool->hasAlphas());
}

TEST(ComposeInstances, AddAfterAlphasKeepsEveryFade) {
  // hasAlphas() is a length comparison against the position lane, so every
  // mutator has to keep the alpha lane in step. The sharp case is add()
  // AFTER the lane exists: a lane left one short would fail that length
  // test and silently drop every fade in the pool.
  auto atlas = std::make_shared<instancing::Atlas>(1.0f);
  atlas->cell(box().fill(Fill::color({1, 0, 0, 1})), {40, 40});
  auto pool = std::make_shared<instancing::Pool>();
  pool->add({40, 40});
  pool->alphas()[0] = 0.5f;
  pool->add({120, 40});  // the append that must not orphan the lane
  pool->commit();
  ASSERT_TRUE(pool->hasAlphas());
  EXPECT_FLOAT_EQ(pool->alphas()[0], 0.5f);  // the fade survives the append
  EXPECT_FLOAT_EQ(pool->alphas()[1], 1.0f);  // the new instance is opaque
  Host host(200, 200);
  host.composer.render(box().absolute().inset(0).child(
      instancing::instances(atlas, pool, instancing::Mode::Data)));
  host.frame();
  const unsigned faded = SkColorGetR(host.pixel(40, 40));
  const unsigned opaque = SkColorGetR(host.pixel(120, 40));
  EXPECT_GT(opaque, 240u);  // the appended sprite stamps at full
  EXPECT_LT(faded, 170u);   // half-alpha red over black ≈ 128
  EXPECT_GT(faded, 80u);
}

TEST(ComposeInstances, ClearDropsTheAlphaLaneWithItsGeneration) {
  // The worse half of the same desync: clear() then re-add the SAME count.
  // A lane that survived the clear would line up with the position lane
  // again and apply the previous generation's fades to entirely different
  // sprites.
  instancing::Pool pool;
  pool.add({10, 10});
  pool.add({30, 10});
  pool.alphas()[0] = 0.25f;
  pool.alphas()[1] = 0.5f;
  pool.clear();
  pool.add({50, 50});
  pool.add({70, 50});
  EXPECT_FALSE(pool.hasAlphas());  // nothing carried forward: stamps opaque
  auto fresh = pool.alphas();      // re-opting in starts from opaque
  EXPECT_FLOAT_EQ(fresh[0], 1.0f);
  EXPECT_FLOAT_EQ(fresh[1], 1.0f);
}

TEST(ComposeInstances, ResizeKeepsTheAlphaLaneInStepBothWays) {
  instancing::Pool pool;
  pool.add({10, 10});
  pool.add({30, 10});
  pool.alphas()[0] = 0.5f;
  pool.resize(4);  // grow: existing fades survive, new slots are opaque
  ASSERT_TRUE(pool.hasAlphas());
  EXPECT_FLOAT_EQ(pool.alphas()[0], 0.5f);
  EXPECT_FLOAT_EQ(pool.alphas()[2], 1.0f);
  EXPECT_FLOAT_EQ(pool.alphas()[3], 1.0f);
  pool.resize(1);  // shrink: the lane truncates with the pool
  ASSERT_TRUE(pool.hasAlphas());
  ASSERT_EQ(pool.alphas().size(), 1u);
  EXPECT_FLOAT_EQ(pool.alphas()[0], 0.5f);  // the kept slot keeps its fade
}

TEST(ComposeInstances, PickInvertsTheStampTopmostFirst) {
  // hitTest cannot see a pool instance at all — the whole field is ONE
  // custom() draw as far as the tree is concerned. pick() is the inverse
  // projection, read against the same lanes the stamp reads: rotation,
  // scale, and topmost-wins where stamps overlap.
  using namespace sigil::compose::instancing;
  Atlas atlas(1.0f);
  atlas.cell(box().fill(Fill::color({1, 0, 0, 1})), {40, 20});
  Pool pool;
  pool.add({100, 100});     // instance 0
  pool.add({120, 100});     // instance 1, overlapping 0's right side
  pool.scales()[1] = 0.5f;  // 20x10 quad at (120,100)
  pool.rotations()[0] = (float)M_PI / 2.0f;  // 0 is rotated 90°: 20x40 now
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
  // measure() is per-Element, so hand-placing N glyphs costs N layouts.
  // measureRun() is ONE layout through the same shaping path a text() leaf
  // takes — which is only useful if it agrees with that leaf, so the
  // assertion is that its advances reproduce what the Element machinery
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

TEST(ComposeText, MeasureRunPrefixSumsAreThePenPositionsAcrossWords) {
  // The header's contract is that the prefix sums ARE the pen positions. An
  // inter-word space is a gap the flow leaves rather than a glyph, so it
  // visits nothing in the glyph walk; left out of the advances, every glyph
  // after a space is placed short and the error grows with each word. The
  // ground truth is the same layout's own placements, so one word, two words
  // and a leading space are all checked against it — a fix that satisfies
  // only the single-word case fails here.
  const sigil::weave::TextStyle style = whiteStyle(40);
  const auto lastPenEnd = [&](std::u8string_view utf8) {
    sigil::weave::Paragraph paragraph;
    paragraph.appendText(utf8, style);
    sigil::weave::BlockFlow flow(SkRect::MakeWH(1.0e6f, 1.0e6f));
    const sigil::weave::ParagraphLayout layout =
        sigil::weave::layoutParagraph(fonts(), paragraph, flow);
    // The right edge of the last glyph, and the pen the first one starts at:
    // measureRun's sums are relative to that first pen.
    float first = 0, end = 0;
    bool seen = false;
    sigil::weave::forEachPlacedGlyph(
        layout, paragraph, [&](const sigil::weave::PlacedGlyph& placed) {
          if (!seen) first = placed.rest.x();
          seen = true;
          end = placed.rest.x() + placed.advance;
        });
    return end - first;
  };
  for (std::u8string_view run :
       {std::u8string_view(u8"ONE"), std::u8string_view(u8"A B"),
        std::u8string_view(u8" A B"),
        std::u8string_view(u8"ONE PASS PER WORD PHASE")}) {
    const std::vector<float> advances = measureRun(run, style, fonts());
    ASSERT_FALSE(advances.empty());
    float sum = 0;
    for (float a : advances) sum += a;
    EXPECT_NEAR(sum, lastPenEnd(run), 0.01f)
        << "prefix sums mis-place the last glyph of \""
        << std::string(reinterpret_cast<const char*>(run.data()), run.size())
        << "\"";
  }
  // The glyph count is untouched: a space still contributes no entry, it
  // only lends its advance to the glyph before it.
  const std::vector<float> spaced = measureRun(u8"A B", style, fonts());
  ASSERT_EQ(spaced.size(), 2u);
  EXPECT_GT(spaced[0], measureRun(u8"A", style, fonts())[0])
      << "the gap must ride the advance of the glyph it follows";
  EXPECT_FLOAT_EQ(spaced[1], measureRun(u8"B", style, fonts())[0])
      << "the last glyph carries no trailing gap";
}

TEST(ComposeText, RunPensAreThePenPositionsWithOnePastTheEnd) {
  // runPens is measureRun already summed, and the whole reason it exists is
  // that everybody who calls measureRun writes that sum by hand. Two claims:
  // the sums are the pen positions the LAYOUT used (checked against its own
  // placements, so an inter-word gap folded into the wrong advance shows),
  // and there is one entry past the end whose value is the run's width.
  const sigil::weave::TextStyle style = whiteStyle(40);
  const auto placed = [&](std::u8string_view utf8) {
    sigil::weave::Paragraph paragraph;
    paragraph.appendText(utf8, style);
    sigil::weave::BlockFlow flow(SkRect::MakeWH(1.0e6f, 1.0e6f));
    const sigil::weave::ParagraphLayout layout =
        sigil::weave::layoutParagraph(fonts(), paragraph, flow);
    // Pen positions relative to the FIRST glyph's pen, which is where the
    // run starts — leading whitespace is no part of it.
    std::vector<float> pens;
    float first = 0;
    bool seen = false;
    sigil::weave::forEachPlacedGlyph(
        layout, paragraph, [&](const sigil::weave::PlacedGlyph& glyph) {
          if (!seen) first = glyph.rest.x();
          seen = true;
          pens.push_back(glyph.rest.x() - first);
        });
    return std::pair{pens, seen};
  };
  for (std::u8string_view run :
       {std::u8string_view(u8"ONE"), std::u8string_view(u8"A B"),
        std::u8string_view(u8" A B"),
        std::u8string_view(u8"ONE PASS PER WORD PHASE")}) {
    const std::vector<float> pens = runPens(run, style, fonts());
    const std::vector<float> advances = measureRun(run, style, fonts());
    ASSERT_EQ(pens.size(), advances.size() + 1)
        << "n glyphs must give n + 1 entries";
    EXPECT_FLOAT_EQ(pens.front(), 0.0f) << "the run starts at its first glyph";
    auto [truth, seen] = placed(run);
    ASSERT_TRUE(seen);
    for (size_t i = 0; i + 1 < pens.size(); ++i)
      EXPECT_NEAR(pens[i], truth[i], 0.01f)
          << "glyph " << i << " of \""
          << std::string(reinterpret_cast<const char*>(run.data()), run.size())
          << "\" is not where the layout put it";
    float width = 0;
    for (float a : advances) width += a;
    EXPECT_FLOAT_EQ(pens.back(), width)
        << "the past-the-end entry is the run's laid-out width";
  }
  // An empty run is 0 wide, and says so with the one entry the contract
  // promises rather than with nothing at all.
  const std::vector<float> nothing = runPens(u8"", style, fonts());
  ASSERT_EQ(nothing.size(), 1u);
  EXPECT_FLOAT_EQ(nothing[0], 0.0f);
}
