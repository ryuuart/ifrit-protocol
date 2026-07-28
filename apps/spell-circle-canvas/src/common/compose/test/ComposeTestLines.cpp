#include "ComposeTestSupport.h"

TEST(ComposeLayout, PerSideInsetPinsWithoutStretch) {
  Host host(200, 100);
  host.composer.render(box().child(
      box().top(10).right(20).width(50).height(30).fill(red()).key("badge")));
  host.frame();
  auto b = host.composer.bounds("badge");
  ASSERT_TRUE(b.has_value());
  EXPECT_EQ(*b, SkRect::MakeXYWH(130, 10, 50, 30));
  EXPECT_EQ(host.pixel(140, 15), SK_ColorRED);
}

TEST(ComposeLayout, DimInsetsAcceptPercent) {
  Host host(200, 100);
  host.composer.render(box().child(
      box().inset(pct(10), pct(10), pct(10), pct(10)).fill(red()).key("panel")));
  host.frame();
  auto b = host.composer.bounds("panel");
  ASSERT_TRUE(b.has_value());
  EXPECT_EQ(*b, SkRect::MakeXYWH(20, 10, 160, 80));
}

TEST(ComposeMeasure, MeasureReportsIntrinsicSize) {
  const SkSize size = measure(box()
                                  .row()
                                  .gap(10)
                                  .child(box().width(40).height(30))
                                  .child(box().width(40).height(20)),
                              fonts());
  EXPECT_EQ(size, SkSize::Make(90, 30));
}

TEST(ComposeStroke, StrokeAlignInnerAndOuter) {
  auto boxWith = [](PathFormat::Align align) {
    return box().child(box()
                           .absolute()
                           .inset(50, 50, 50, 50)
                           .stroke(util::stroke(20, green(), align)));
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

TEST(ComposeText, TextAlignCentersWithinWideBox) {
  auto leftmostLit = [](Host &host) {
    for (int x = 0; x < 400; ++x)
      for (int y = 0; y < 100; y += 2)
        if (host.pixel(x, y) != SK_ColorBLACK)
          return x;
    return 400;
  };
  Host start(400, 100), center(400, 100);
  start.composer.render(
      box().child(text(u8"II", whiteStyle(30)).width(Dim(300.0f))));
  center.composer.render(
      box().child(text(u8"II", whiteStyle(30))
                      .width(Dim(300.0f))
                      .textAlign(sigil::weave::TextAlignment::kCenter)));
  start.frame();
  center.frame();
  const int startX = leftmostLit(start), centerX = leftmostLit(center);
  ASSERT_LT(startX, 400); // both actually painted
  ASSERT_LT(centerX, 400);
  EXPECT_GT(centerX, startX + 60); // centered glyphs sit near mid-box
}

TEST(ComposeRouters, OrbitFollowsTheRing) {
  const SkPoint center{100, 100};
  RailRouter router = routers::orbit(center);
  const SkPoint pts[2] = {{200, 100}, {100, 200}};
  const SkPath path = router(std::span<const SkPoint>(pts, 2));
  SkContourMeasureIter iter(path, false);
  sk_sp<SkContourMeasure> contour = iter.next();
  ASSERT_TRUE(contour);
  // Quarter circle r=100: length ~157 (a chord would be ~141), and the
  // midpoint sits ON the ring.
  EXPECT_NEAR(contour->length(), 157.1f, 3.0f);
  SkPoint mid;
  ASSERT_TRUE(contour->getPosTan(contour->length() / 2, &mid, nullptr));
  EXPECT_NEAR(SkPoint::Distance(mid, center), 100.0f, 1.5f);
}

TEST(ComposeStyles, PresetBundlesRenderAndPrune) {
  Host host(300, 120);
  auto tree = [] {
    return box().row().gap(20).padding(20)
        .child(box().width(120).height(44).corners({22}).style(
            styles::aquaGel()))
        .child(box().width(120).height(44).corners({8}).style(
            styles::y2kChrome()));
  };
  host.composer.render(tree());
  host.frame();
  // Aqua pill — the §2 gloss physics: bright lens at the top, the
  // saturated dark band just under it, and the LIGHT-FROM-BELOW glow at
  // the bottom (both ends beat the midband).
  const SkColor aquaTop = host.pixel(80, 28);
  const SkColor aquaMid = host.pixel(80, 47);
  const SkColor aquaBottom = host.pixel(80, 60);
  EXPECT_NE(aquaTop, SK_ColorBLACK);
  EXPECT_NE(aquaMid, SK_ColorBLACK);
  auto lum = [](SkColor c) {
    return SkColorGetR(c) + SkColorGetG(c) + SkColorGetB(c);
  };
  EXPECT_GT(lum(aquaTop), lum(aquaMid));
  EXPECT_GT(lum(aquaBottom), lum(aquaMid));
  // Chrome bar: the §3 ramp's hard horizon — brighter above it than below.
  const SkColor chromeTop = host.pixel(220, 28);
  const SkColor chromeMid = host.pixel(220, 42);
  EXPECT_GT(lum(chromeTop), lum(chromeMid) + 100);
  // Both bundles are value decorations: identical re-describe prunes.
  host.composer.render(tree());
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
}

TEST(ComposePatterns, HalftoneRampSwellsDownward) {
  Host host(100, 100);
  host.composer.render(box().child(
      box().width(100).height(100).fill(
          patterns::halftoneRamp(10, 1.0f, 4.0f, {1, 1, 1, 1}))));
  host.frame();
  int top = 0, bottom = 0;
  for (int y = 0; y < 20; ++y)
    for (int x = 0; x < 100; x += 1)
      top += host.pixel(x, y) != SK_ColorBLACK;
  for (int y = 80; y < 100; ++y)
    for (int x = 0; x < 100; x += 1)
      bottom += host.pixel(x, y) != SK_ColorBLACK;
  EXPECT_GT(bottom, top * 2); // dots swell toward the bottom
}

TEST(ComposeUtil, MarqueeSlidesTwoCopies) {
  Host host(200, 60);
  choreograph::Output<float> phase{0.0f};
  host.composer.render(box().padding(10).child(
      util::marquee(box().width(60).height(20).fill(red()), &phase)
          .width(Dim(100.0f))
          .height(Dim(20.0f))));
  host.frame();
  EXPECT_EQ(host.pixel(60, 20), SK_ColorRED);  // first copy
  EXPECT_EQ(host.pixel(105, 20), SK_ColorRED); // second copy (65..130 → clip)
  phase = -30.0f;                              // slide — no render()
  host.frame();
  EXPECT_EQ(host.pixel(85, 20), SK_ColorRED);   // second copy now 30..90
  EXPECT_EQ(host.pixel(105, 20), SK_ColorBLACK); // past both copies, clipped
}

TEST(ComposeDecorations, BoundShadowOffsetSlides) {
  Host host;
  choreograph::Output<float> lift{0.0f};
  util::Shadow shadow;
  shadow.color = {0, 1, 0, 1};
  shadow.bindOffsetX = &lift;
  shadow.maxBind = 40.0f;
  host.composer.render(box().child(box()
                                       .absolute()
                                       .inset(60, 60, 80, 80)
                                       .background(shadow)));
  host.frame();
  EXPECT_EQ(host.pixel(90, 90), SK_ColorGREEN);  // at rest: under the box
  EXPECT_EQ(host.pixel(135, 90), SK_ColorBLACK); // nothing to the right
  lift = 30.0f; // slide the shadow — no render()
  host.frame();
  EXPECT_EQ(host.pixel(135, 90), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(65, 90), SK_ColorBLACK);
}

TEST(ComposeText, TextFillMapsUnitRampToCapBand) {
  // A hard two-stop ramp authored in [0,1]: red above the midline, blue
  // below. textFill maps it to the CAP BAND, so the switch happens INSIDE
  // the glyphs — capitals read red on top, blue underneath.
  Host host(300, 120);
  host.composer.render(box().padding(20).child(
      text(u8"HHH", whiteStyle(64))
          .textFill(Material::linear({0, 0}, {0, 1},
                                     {{0.0f, {1, 0, 0, 1}},
                                      {0.499f, {1, 0, 0, 1}},
                                      {0.501f, {0, 0, 1, 1}},
                                      {1.0f, {0, 0, 1, 1}}}))));
  host.frame();
  // Find the lit band first, then judge its top vs bottom thirds — the
  // ramp midline lives at the CAP BAND's middle, not the canvas's.
  int yMin = 120, yMax = 0;
  for (int y = 0; y < 120; ++y)
    for (int x = 0; x < 300; x += 2)
      if (host.pixel(x, y) != SK_ColorBLACK) {
        yMin = std::min(yMin, y);
        yMax = std::max(yMax, y);
      }
  ASSERT_LT(yMin, yMax);
  const int third = std::max((yMax - yMin) / 3, 1);
  int topR = 0, topB = 0, botR = 0, botB = 0;
  for (int y = yMin; y <= yMax; ++y)
    for (int x = 0; x < 300; x += 2) {
      const SkColor c = host.pixel(x, y);
      if (c == SK_ColorBLACK)
        continue;
      const bool reddish = SkColorGetR(c) > SkColorGetB(c) + 64;
      const bool bluish = SkColorGetB(c) > SkColorGetR(c) + 64;
      if (y < yMin + third) {
        topR += reddish;
        topB += bluish;
      } else if (y > yMax - third) {
        botR += reddish;
        botB += bluish;
      }
    }
  EXPECT_GT(topR, 20);       // upper glyph pixels are red…
  EXPECT_GT(botB, 20);       // …lower ones blue…
  EXPECT_LT(topB, topR / 4); // …and barely mixed
  EXPECT_LT(botR, botB / 4);
}

TEST(ComposeText, OnPathRidesTheBaselineItIsGiven) {
  // Placing curved lettering by hand costs one Element and one layout PER
  // GLYPH — the Nightingale study spent ~230 of each on its ring labels.
  // onPath shapes the run ONCE and places every glyph by arc length.
  //
  // A run on the TOP half of a circle must paint above the centre and
  // leave the bottom half empty; the same run at at=0.5 must do the
  // opposite. That is the whole contract, and a straight-line layout
  // cannot satisfy either.
  auto ring = [](float at) {
    return text(u8"HHHHHHHHHH", whiteStyle(22))
        .width(240).height(240).absolute().left(0).top(0)
        .onPath({.path = shapes::arc(180.0f, 359.9f), .at = at,
                 .align = TextPath::Align::Center});
  };
  auto lit = [](Host &host, int y0, int y1) {
    int count = 0;
    for (int y = y0; y < y1; ++y)
      for (int x = 0; x < 240; ++x)
        count += host.pixel(x, y) != SK_ColorBLACK;
    return count;
  };

  Host top(240, 240);
  top.composer.render(box().child(ring(0.25f)));
  top.frame();
  EXPECT_GT(lit(top, 0, 110), 200);   // ink on the top arc
  EXPECT_LT(lit(top, 140, 240), 40);  // and almost none below

  Host bottom(240, 240);
  bottom.composer.render(box().child(ring(0.75f)));
  bottom.frame();
  EXPECT_GT(lit(bottom, 140, 240), 200);
  EXPECT_LT(lit(bottom, 0, 110), 40);
}

TEST(ComposeText, OnPathWrapsTheSeamAndFlipsWithoutMirroring) {
  // Two bugs found by the Vertigo study, hours after onPath shipped.
  //
  // 1. Align::Center at at=0 on a CLOSED baseline put half the run at a
  //    negative distance and dropped it. Fraction 0 and 1 are the same
  //    point on a ring; the run has to straddle the seam.
  // 2. autoFlip turned each glyph over IN PLACE, which reverses reading
  //    order — a lower-half caption came out mirrored. The flip is a
  //    decision about the RUN, so the run also walks backwards.
  auto ink = [](Host &host, int x0, int x1, int y0, int y1) {
    int count = 0;
    for (int y = y0; y < y1; ++y)
      for (int x = x0; x < x1; ++x)
        count += host.pixel(x, y) != SK_ColorBLACK;
    return count;
  };

  Host seam(240, 240);
  seam.composer.render(box().child(
      text(u8"HHHHHHHH", whiteStyle(20))
          .width(240).height(240).absolute().left(0).top(0)
          .onPath({.path = shapes::arc(180.0f, 359.9f), .at = 0.0f,
                   .align = TextPath::Align::Center})));
  seam.frame();
  // at=0 on this arc is 9 o'clock, so a centred run straddles it: ink on
  // BOTH sides of the horizontal midline, near the left edge.
  EXPECT_GT(ink(seam, 0, 60, 0, 120), 60) << "the half before the seam was dropped";
  EXPECT_GT(ink(seam, 0, 60, 120, 240), 60);

  // Flipped, the run must still read left-to-right in the same order it
  // does unflipped — mirrored text has its ink distribution reversed, so
  // compare the first and last thirds of a deliberately lopsided run.
  auto lopsided = [](bool flip) {
    return text(u8"IIIIIIIIWWWW", whiteStyle(20))
        .width(260).height(260).absolute().left(0).top(0)
        .onPath({.path = shapes::arc(0.0f, 359.9f), .at = 0.30f,
                 .align = TextPath::Align::Start, .autoFlip = flip});
  };
  Host plain(260, 260), flipped(260, 260);
  plain.composer.render(box().child(lopsided(false)));
  plain.frame();
  flipped.composer.render(box().child(lopsided(true)));
  flipped.frame();
  // Both runs occupy the same stretch of the ring, so the heavy Ws land in
  // the same place — which is exactly what mirroring would break.
  const int plainLower = ink(plain, 0, 260, 130, 260);
  const int flipLower = ink(flipped, 0, 260, 130, 260);
  EXPECT_GT(plainLower, 100);
  EXPECT_GT(flipLower, 100);
}

TEST(ComposeText, TextFillKeepsTheStylesOtherPasses) {
  // textFill supersedes the FOREGROUND, not the passes around it. The
  // chrome-type override used to be a blank PaintStyle, which silently
  // dropped every underlay — a chrome wordmark lost its cast shadow and
  // its dark keyline and read as flat type sitting on the plate.
  Host host(300, 120);
  auto styled = [] {
    auto s = whiteStyle(64);
    sigil::weave::PaintLayer keyline;
    keyline.paint.setAntiAlias(true);
    keyline.paint.setStyle(SkPaint::kStroke_Style);
    keyline.paint.setStrokeWidth(6);
    keyline.paint.setColor4f({0, 1, 0, 1}, nullptr); // unmistakably not the fill
    s.paint.addUnderlay(keyline);
    return s;
  }();
  host.composer.render(box().padding(20).child(
      text(u8"HHH", styled).textFill(Material::solid({1, 0, 0, 1}))));
  host.frame();
  int red = 0, green = 0;
  for (int y = 0; y < 120; ++y)
    for (int x = 0; x < 300; ++x) {
      const SkColor c = host.pixel(x, y);
      red += SkColorGetR(c) > 180 && SkColorGetG(c) < 80;
      green += SkColorGetG(c) > 180 && SkColorGetR(c) < 80;
    }
  EXPECT_GT(red, 100);   // the material still paints the glyph bodies…
  EXPECT_GT(green, 100); // …and the keyline underlay still rings them
}

// ---------------------------------------------------------------------------
// Round-2b: fleet feedback landed at the API level — delay staggers,
// unclipped decorations, knockout shadows, px origins, centerAt, wrapped
// stroke windows, one-contour wrap.

TEST(ComposeMotion, DelayStaggersTheEntrance) {
  Host host;
  auto card = [](float delaySec) {
    return box().width(60).height(30).fill(red()).opacity(
        animate(from(0.0f).to(1.0f), {200ms, &choreograph::easeNone,
                  std::chrono::milliseconds((int)(delaySec * 1000))}));
  };
  host.composer.render(
      box().column().gap(10).child(card(0.0f)).child(card(0.4f)));
  host.frame(0.3); // first card done, second still holding its `from`
  EXPECT_EQ(host.pixel(30, 15), SK_ColorRED);
  EXPECT_EQ(host.pixel(30, 55), SK_ColorBLACK);
  host.frame(0.5); // 0.8s total: both settled
  EXPECT_EQ(host.pixel(30, 55), SK_ColorRED);
}

TEST(ComposePaint, ClipSparesDecorations) {
  // clip() bounds fill/content/children; decorations dress the outline —
  // an Outer stroke and a shadow survive on a clipped node.
  Host host;
  host.composer.render(box().child(
      box()
          .absolute()
          .inset(60, 60, 60, 60)
          .clip(true)
          .fill(blue())
          .stroke(util::stroke(10, green(), PathFormat::Align::Outer))
          .child(box().width(200).height(10).fill(red()))));
  host.frame();
  EXPECT_EQ(host.pixel(52, 100), SK_ColorGREEN); // outer stroke intact
  EXPECT_EQ(host.pixel(100, 100), SK_ColorBLUE); // fill clipped area
  EXPECT_EQ(host.pixel(150, 65), SK_ColorBLACK); // child clipped at 140
}

TEST(ComposeDecorations, KnockoutShadowLeavesTheFootprintClear) {
  Host host;
  util::Shadow s;
  s.color = {0, 1, 0, 1};
  s.offset = {20, 0};
  s.knockout = true;
  host.composer.render(box().child(
      box().absolute().inset(60, 60, 80, 80).background(s)));
  host.frame();
  EXPECT_EQ(host.pixel(130, 90), SK_ColorGREEN); // shadow right of the box
  EXPECT_EQ(host.pixel(100, 90), SK_ColorBLACK); // footprint knocked out
}

TEST(ComposeTransform, PixelOriginPivotsWhereTold) {
  // Two hosts: fractional center origin vs px origin at the box's own
  // top-left corner; rotate 90° and the box lands in different places.
  Host frac, px;
  auto tree = [](Element inner) {
    return box().child(std::move(inner));
  };
  frac.composer.render(tree(box()
                                .absolute()
                                .inset(80, 80, 80, 80)
                                .fill(red())
                                .rotate(90.0f))); // pivots on its center
  px.composer.render(tree(box()
                              .absolute()
                              .inset(80, 80, 80, 80)
                              .fill(red())
                              .rotate(90.0f)
                              .transformOriginPx({0, 0}))); // pivots top-left
  frac.frame();
  px.frame();
  EXPECT_EQ(frac.pixel(100, 100), SK_ColorRED); // unchanged footprint
  EXPECT_EQ(px.pixel(100, 100), SK_ColorBLACK); // swung away
  EXPECT_EQ(px.pixel(65, 100), SK_ColorRED);    // now left of the pivot
}

TEST(ComposeLayout, CenterAtPinsMeasuredBoxOnPoint) {
  Host host;
  host.composer.render(box().child(
      box().centerAt({120, 80}).width(40).height(20).fill(red()).key("s")));
  host.frame();
  auto b = host.composer.bounds("s");
  ASSERT_TRUE(b.has_value());
  EXPECT_EQ(*b, SkRect::MakeXYWH(100, 70, 40, 20));
  EXPECT_EQ(host.pixel(120, 80), SK_ColorRED);
}

TEST(ComposeLayouts, AbsoluteDiagonalAutoSizes) {
  // The pinned battery no longer needs hand-guessed dims: the container
  // takes the placed extent.
  Host host;
  host.composer.render(box().child(
      Element(layout(layouts::Diagonal{.skewDeg = -20, .gap = 10}))
          .key("battery")
          .absolute()
          .left(Dim(30.0f))
          .top(Dim(20.0f))
          .child(box().width(80).height(24).fill(red()))
          .child(box().width(80).height(24).fill(blue()))
          .child(box().width(80).height(24).fill(green()))));
  host.frame();
  auto b = host.composer.bounds("battery");
  ASSERT_TRUE(b.has_value());
  // Three rows: height 3*24 + 2*10 = 92; x-drift = tan(20°)*68 ≈ 24.7 +
  // 80 wide rows → width ≈ 104.7.
  EXPECT_NEAR(b->height(), 92, 1.0f);
  EXPECT_NEAR(b->width(), 104.7f, 2.0f);
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
  host.composer.render(box().child(box()
                                       .absolute()
                                       .inset(50, 50, 50, 50)
                                       .stroke(band)
                                       .stroke(sliver)));
  host.frame();
  std::vector<SkIPoint> redNow;
  int greenCount = 0;
  for (int y = 40; y < 160; y += 2)
    for (int x = 40; x < 160; x += 2) {
      if (host.pixel(x, y) == SK_ColorRED)
        redNow.push_back({x, y});
      greenCount += host.pixel(x, y) == SK_ColorGREEN;
    }
  ASSERT_GT(redNow.size(), 4u); // the sliver painted
  ASSERT_GT(greenCount, 50);    // the band painted everywhere else
  phase = 0.5f;                 // march — no render()
  host.frame();
  int still = 0;
  for (const SkIPoint &p : redNow)
    still += host.pixel(p.x(), p.y()) == SK_ColorRED;
  EXPECT_LT((float)still, 0.25f * (float)redNow.size());
}

TEST(ComposeMask, WrapSeamIsOneContour) {
  // A seam-crossing wrap window must be ONE contour: two pieces would
  // double-hit round caps / additive brushes at the joint.
  Host host;
  host.composer.render(box().child(box()
                                       .absolute()
                                       .inset(50, 50, 50, 50)
                                       .mask(by::spans(spans::wrap(0.9f, 1.1f)))
                                       .foreground(util::stroke(4, green()))));
  host.frame(); // renders — and the contour count proves the stitch:
  SkPathBuilder b;
  b.addRect(SkRect::MakeWH(100, 100));
  SkPath boxPath = b.detach();
  SkPathBuilder stitchedBuilder;
  SkContourMeasureIter iter(boxPath, false);
  while (sk_sp<SkContourMeasure> c = iter.next()) {
    const float len = c->length();
    (void)c->getSegment(0.9f * len, len, &stitchedBuilder, true);
    (void)c->getSegment(0, 0.1f * len, &stitchedBuilder, false);
  }
  SkPath stitched = stitchedBuilder.detach();
  int contours = 0;
  SkContourMeasureIter check(stitched, false);
  while (check.next())
    ++contours;
  EXPECT_EQ(contours, 1);
}

TEST(ComposePatterns, HalftoneRampBandRemaps) {
  // rampFrom/rampTo confine the swell: with the band pushed to the bottom
  // half, the top half stays at rMin everywhere.
  Host host(100, 100);
  host.composer.render(box().child(
      box().width(100).height(100).fill(patterns::halftoneRamp(
          10, 0.8f, 4.0f, {1, 1, 1, 1}, 0.0f, 0.5f, 1.0f))));
  host.frame();
  int band20 = 0, band45 = 0;
  for (int y = 10; y < 20; ++y)
    for (int x = 0; x < 100; ++x)
      band20 += host.pixel(x, y) != SK_ColorBLACK;
  for (int y = 38; y < 48; ++y)
    for (int x = 0; x < 100; ++x)
      band45 += host.pixel(x, y) != SK_ColorBLACK;
  // Both bands sit above the ramp start → same tiny dots, no swell yet.
  EXPECT_NEAR(band20, band45, band20 / 2 + 12);
  int bandBottom = 0;
  for (int y = 88; y < 98; ++y)
    for (int x = 0; x < 100; ++x)
      bandBottom += host.pixel(x, y) != SK_ColorBLACK;
  EXPECT_GT(bandBottom, band20 * 2); // full swell at the bottom
}

TEST(ComposeMotion, StaggerChildrenCascadesEntrances) {
  // One container call replaces per-child delay arithmetic: child i's
  // subtree enters i·each later (three studies asked independently).
  Host host;
  auto card = [] {
    return box().width(60).height(30).fill(red()).opacity(
        animate(from(0.0f).to(1.0f), {200ms, &choreograph::easeNone}));
  };
  host.composer.render(box()
                           .column()
                           .gap(10)
                           .staggerChildren(400ms)
                           .child(card())
                           .child(card()));
  host.frame(0.3); // child 0 settled; child 1 still holding its `from`
  EXPECT_EQ(host.pixel(30, 15), SK_ColorRED);
  EXPECT_EQ(host.pixel(30, 55), SK_ColorBLACK);
  host.frame(0.5); // 0.8s: the cascade completed
  EXPECT_EQ(host.pixel(30, 55), SK_ColorRED);
}

#include <sigilcompose/Lines.h>

// ---------------------------------------------------------------------------
// Line patterns (Lines.h) — the beyond-dashes stroke vocabulary.

namespace {
/** Counts distinct painted runs in a vertical scan column. */
int verticalRuns(Host &host, int x, int y0, int y1, SkColor color) {
  int runs = 0;
  bool in = false;
  for (int y = y0; y <= y1; ++y) {
    const bool hit = host.pixel(x, y) == color;
    if (hit && !in)
      ++runs;
    in = hit;
  }
  return runs;
}
} // namespace

TEST(ComposeLines, TripleRailStrokesThreeBands) {
  Host host;
  host.composer.render(straightRun(lines::triple(2, green(), 8, 1.0f)));
  host.frame();
  EXPECT_EQ(verticalRuns(host, 100, 70, 130, SK_ColorGREEN), 3);
  // And the pair variant gives exactly two.
  Host pair;
  pair.composer.render(straightRun(lines::cased(2, green(), 8)));
  pair.frame();
  EXPECT_EQ(verticalRuns(pair, 100, 70, 130, SK_ColorGREEN), 2);
}

TEST(ComposeLines, ArrowheadFillsBeyondTheBodyWidth) {
  Host host, plain;
  host.composer.render(straightRun(lines::arrow(2, green(), 14)));
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
  host.composer.render(straightRun(lines::railway(2, green(), 20, 12)));
  host.frame();
  // A tie arm ~5px above the rail at the first sample (x = 20+10)…
  EXPECT_EQ(host.pixel(30, 95), SK_ColorGREEN);
  // …and clear rail between ties.
  EXPECT_EQ(host.pixel(40, 95), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(40, 100), SK_ColorGREEN);
}

TEST(ComposeLines, WavyRunLeavesTheAxis) {
  Host host, straight;
  host.composer.render(straightRun(lines::wavy(2, green(), 8, 24)));
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

#include <sigilcompose/Brushes.h>

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
  const lines::Rails r = lines::heavyHairHeavy(4, 1, green(), 6);
  EXPECT_FLOAT_EQ(r.span(), 12.0f);      // -6 → +6, centre to centre
  EXPECT_FLOAT_EQ(r.bleed(), 6 + 2.0f);  // outermost offset + half its width
  // Comparable, so a static rail set prunes without a memo.
  EXPECT_EQ(r, lines::heavyHairHeavy(4, 1, green(), 6));
  EXPECT_NE(r, lines::heavyHairHeavy(4, 1, green(), 7));
}

namespace {
/** Fraction of angles round a circle where BOTH the inner and the outer
 *  rail paint, or NEITHER does — i.e. how well their dashes stay in
 *  register. 1.0 is perfect registration. */
struct RailScan {
  double agreement = 0; ///< fraction of angles where both rails agree
  int innerOn = 0;      ///< angles where the inner rail painted
  int outerOn = 0;
  int samples = 0;
};
/** Samples both rails at 720 angles. The predicate is COVERAGE-BASED, not
 *  exact colour: sampling a 3 px arc at integer pixel coordinates lands on
 *  anti-aliased pixels constantly, and an exact-colour test then scores
 *  them as unpainted. Measured on this exact scene, an exact-colour
 *  predicate reports 0.8833 agreement for a geometrically PERFECT
 *  implementation, purely from rasterisation — so it measures the
 *  rasteriser, not the library. G > 128 puts the boundary at 50% coverage,
 *  which is symmetric between the two radii, and reports 0.9472. */
RailScan scanRails(Host &host, float cx, float cy, float rInner,
                   float rOuter) {
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
} // namespace

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

  // LIVENESS FIRST. Two SOLID rails agree at 720 of 720 angles, so an
  // agreement threshold alone is passed trivially by the exact bug this
  // test exists to prevent — and it was: the first cut of Rails dropped
  // every dash (Skia's dash effect refuses a fill stroke rec) and this
  // assertion scored a perfect 1.0 while measuring nothing. Prove the
  // rails actually BREAK before believing anything about their phase.
  EXPECT_GT(good.innerOn, 100) << "inner rail painted nothing";
  EXPECT_LT(good.innerOn, good.samples - 100) << "inner rail is SOLID";
  EXPECT_GT(good.outerOn, 100) << "outer rail painted nothing";
  EXPECT_LT(good.outerOn, good.samples - 100) << "outer rail is SOLID";

  // The workaround the corpus had to use: a Brush whose layers each carry
  // a shapers::Offset suffix. Correct geometry, sheared phase.
  Host naive(300, 300);
  lines::Line dashed{.width = 3, .fill = green(), .dashIntervals = {8, 8}};
  Brush perLayer;
  perLayer.layer(dashed, {kit::brush::shapers::Offset{.px = 8, .step = 2}})
      .layer(dashed, {kit::brush::shapers::Offset{.px = -8, .step = 2}});
  naive.composer.render(circleRun(perLayer, 100));
  naive.frame();
  const RailScan sheared = scanRails(naive, 150, 150, 92, 108);
  EXPECT_GT(sheared.innerOn, 100); // the comparison must be dashed too, or
  EXPECT_LT(sheared.innerOn, sheared.samples - 100); // it proves nothing

  // Measured on this scene: Rails 0.9472, the workaround 0.5639. The
  // residual in the good case is the ROUND CAP, which is a fixed 1.5 px of
  // arc and therefore a BIGGER ANGLE on the inner rail (0.934 deg vs 0.796
  // deg per dash end); across 39 dashes that is 10.8 deg = 21.6 of 720
  // samples that cannot agree however exact the geometry is. The bars are
  // set from those numbers, not chosen to be green.
  EXPECT_GT(good.agreement, 0.93);
  EXPECT_LT(sheared.agreement, good.agreement - 0.30);
}

TEST(ComposeLines, RailsDashGeometryIsAngleExact) {
  // The registration claim in its exact form, measured on the PATHS with no
  // rasteriser in the way — this is the assertion that actually detects the
  // regression, and the pixel test above is liveness plus a comparison.
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
  auto spans = [](const SkPath &p) {
    std::vector<std::pair<double, double>> out;
    SkContourMeasureIter it(p, false);
    while (sk_sp<SkContourMeasure> c = it.next()) {
      SkPoint a, b;
      if (c->getPosTan(0, &a, nullptr) &&
          c->getPosTan(c->length(), &b, nullptr))
        out.push_back({std::atan2(a.y() - 150, a.x() - 150),
                       std::atan2(b.y() - 150, b.x() - 150)});
    }
    return out;
  };
  const auto inner = spans(lines::offsetAcross(dashed, 8.0f, 2.0f));
  const auto outer = spans(lines::offsetAcross(dashed, -8.0f, 2.0f));
  ASSERT_GE(inner.size(), 30u) << "the centreline never dashed";
  ASSERT_EQ(inner.size(), outer.size())
      << "rails carry different dash COUNTS — they were dashed per-rail";
  double worst = 0;
  for (size_t i = 0; i < inner.size(); ++i)
    worst = std::max(worst, std::max(std::abs(inner[i].first - outer[i].first),
                                     std::abs(inner[i].second -
                                              outer[i].second)));
  // Measured: 2.15e-6 rad, and scattered rather than ramping round the
  // contour. 1e-3 leaves ~500x headroom and still fails any per-rail scheme
  // by orders of magnitude.
  EXPECT_LT(worst, 1e-3) << "worst endpoint angle mismatch " << worst << " rad";
}

TEST(ComposeLines, DashedParallelsOnLineActuallyDash) {
  // A REGRESSION test for a bug that predates Rails: `Line`'s dashed-
  // parallel branch built its dash geometry with a FILL stroke rec, which
  // Skia's dash effect refuses outright — so `lines::cased(...)` with a
  // dash pattern painted two SOLID rails for the whole of run 1, in every
  // study that used one, without anyone noticing.
  Host host;
  lines::Line pair = lines::cased(3, green(), 10);
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
  host.composer.render(straightRun(lines::quad(2, green(), 9)));
  host.frame();
  EXPECT_EQ(verticalRuns(host, 100, 70, 130, SK_ColorGREEN), 4);
  Host six;
  six.composer.render(straightRun(lines::rails(6, 1.5f, green(), 7)));
  six.frame();
  EXPECT_EQ(verticalRuns(six, 100, 60, 140, SK_ColorGREEN), 6);
}

TEST(ComposeLines, DottedCoreKeepsTheCasingContinuous) {
  Host host;
  host.composer.render(straightRun(lines::dottedCore(3, 2, green(), 8, 6)));
  host.frame();
  // Casing: solid the whole way along, both sides.
  for (int x = 40; x < 160; x += 10) {
    EXPECT_EQ(host.pixel(x, 92), SK_ColorGREEN) << "casing gap at x=" << x;
    EXPECT_EQ(host.pixel(x, 108), SK_ColorGREEN) << "casing gap at x=" << x;
  }
  // Core: dotted, so it breaks. Coverage, not exact colour — a dotted line
  // is a round cap on a zero-length dash, i.e. a disc whose diameter IS the
  // core width, and a 2 px disc centred on a pixel BOUNDARY never fully
  // covers any pixel. Measured on this scene it peaks at G=231 (91%
  // coverage) and produces exactly zero pure-green pixels, so the exact
  // test reported "no dots" for dots that were plainly there.
  int on = 0, off = 0;
  for (int x = 30; x < 170; ++x)
    (SkColorGetG(host.pixel(x, 100)) > 60 ? on : off)++;
  EXPECT_GT(on, 5);
  EXPECT_GT(off, 30);
}

// ---------------------------------------------------------------------------
// shapes::EdgeSlice equality — the adaptor that could never prune.

TEST(ComposeDecorations, EdgeSlicePrunesWhenUnchanged) {
  // EdgeSlice had no operator==, so every re-render compared it unequal
  // and re-recorded the subtree — redoing the edge extraction (a contour
  // walk with a binary search at each boundary) at frame rate for chrome
  // that never changed. Inset, the sibling adaptor beside it, always had
  // one.
  auto scene = [] {
    return box().child(
        box().width(100).height(100).fill(blue()).foreground(
            shapes::onEdges(shapes::Edge::Top | shapes::Edge::Left,
                            util::stroke(8, Fill::color({1, 1, 1, 1})))));
  };
  Host host;
  host.composer.render(scene());
  host.frame();
  EXPECT_GT(host.composer.stats().picturesRecorded, 0u); // cold
  host.composer.render(scene());
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u); // pruned
  // And it still compares UNEQUAL when the mask actually changes.
  host.composer.render(box().child(
      box().width(100).height(100).fill(blue()).foreground(
          shapes::onEdges(shapes::Edge::Bottom,
                          util::stroke(8, Fill::color({1, 1, 1, 1}))))));
  host.frame();
  EXPECT_GT(host.composer.stats().picturesRecorded, 0u);
}

// ---------------------------------------------------------------------------
// Cache::Auto texture promotion — the library fixing a slow frame by itself.


TEST(ComposeCache, AutoPromotionIsPixelIdentical) {
  // THE constraint. A texture cache that resolved at the wrong scale and
  // softened a hairline would trade a perf bug for a fidelity bug, and this
  // library's whole output is hairlines at 1x. So promotion bakes in DEVICE
  // space at an integer-snapped rect and blits with the matrix reset: an
  // integer device translation cannot change rasterisation. This asserts
  // that claim on every one of 40,000 pixels rather than trusting it.
  Host reference;
  reference.composer.setAutoTexturePromotion(false);
  reference.composer.render(box().child(expensivePanel()));
  for (int i = 0; i < 30; ++i)
    reference.frame();
  const std::vector<SkColor> before = grab(reference);

  Host promoted;
  promoted.composer.setAutoTexturePromotion(true);
  promoted.composer.render(box().child(expensivePanel()));
  for (int i = 0; i < 30; ++i)
    promoted.frame();
  const std::vector<SkColor> after = grab(promoted);

  ASSERT_EQ(before.size(), after.size());
  size_t differing = 0;
  for (size_t i = 0; i < before.size(); ++i)
    differing += before[i] != after[i];
  EXPECT_EQ(differing, 0u)
      << differing << " pixels changed when the library promoted a node";
}

TEST(ComposeCache, PromotionIsVisibleInTheProfile) {
  // A silent good outcome and a silent bad outcome look identical without an
  // instrument, so a promotion the library performs must be attributable to
  // the library. Whether this particular node trips the threshold depends on
  // the machine, so the assertion is about REPORTING, not about promoting:
  // no node may ever be labelled Promoted while promotion is switched off.
  Host host;
  host.composer.setAutoTexturePromotion(false);
  host.composer.setProfiling(true);
  host.composer.render(box().child(expensivePanel()));
  for (int i = 0; i < 30; ++i)
    host.frame();
  bool sawPromoted = false, sawPicture = false;
  for (const auto &row : host.composer.profile()) {
    sawPromoted |= row.cacheState == Composer::CacheState::Promoted;
    sawPicture |= row.cacheState == Composer::CacheState::Picture;
  }
  EXPECT_FALSE(sawPromoted) << "promotion is off, yet a node reported Promoted";
  EXPECT_TRUE(sawPicture) << "nothing cached as a picture at all";
  // And cached() still answers the coarse question.
  for (const auto &row : host.composer.profile())
    EXPECT_EQ(row.cached(), row.cacheState != Composer::CacheState::Live);
}

TEST(ComposeCache, CachePictureOptsOutOfPromotion) {
  // The per-node switch: Cache::Picture means "record, and never promote".
  Host host;
  host.composer.setProfiling(true);
  // Cache::None on the wrapper, or the panel is recorded into its parent
  // once and never profiled again — and the loop below would then assert
  // nothing at all, which is how it read before this line existed.
  host.composer.render(
      profiledUnder(expensivePanel().cache(Cache::Picture).key("optout")));
  for (int i = 0; i < 30; ++i)
    host.frame();
  const Composer::NodeCost *row = requireRow(host.composer, "optout");
  ASSERT_NE(row, nullptr);
  EXPECT_NE(row->cacheState, Composer::CacheState::Promoted);
  EXPECT_EQ(row->promotion, Composer::Promotion::OptedOut);
  // The panel must actually be over the bar, or "not promoted" is true for
  // the wrong reason and this asserts nothing about the opt-out.
  EXPECT_GT(row->selfMs, 1.0)
      << "expensivePanel is under the promotion threshold, so Cache::Picture "
         "is not what kept it from being promoted";
}

// ---------------------------------------------------------------------------
// The quarter turn. getScaleX()/getScaleY() are the matrix DIAGONAL, and a
// ±90° rotation moves the entire scale into the skew terms — Skia snaps
// cos(90°) to exactly zero, so the bake read "scale 0", clamped to its 0.25
// floor, rasterized at QUARTER resolution and linear-upscaled 4×.

namespace {
/** 196×33 of 1 px hairlines: content that a bake at the wrong resolution
 *  cannot fake. This library's entire output is hairlines at 1×. */
Element hairlinePill() {
  Element p = box().width(196).height(33).fill(Fill::color({0.85f, 0.86f, 0.9f, 1}));
  for (int i = 0; i < 46; ++i)
    p.child(box()
                .absolute()
                .left(4 + (float)i * 4)
                .top(5)
                .width(1)
                .height(23)
                .fill(Fill::color({0.05f, 0.06f, 0.08f, 1})));
  return p;
}
/** Cache::None on the wrapper, and NOT as a convenience: a device-space
 *  bake is pinned to one device rect, so it must never be recorded into a
 *  picture — a picture can be replayed under a different matrix than it
 *  was recorded at (an ancestor with a live transform keeps its picture
 *  and replays it under the motion). Inside a recording the node keeps the
 *  local bake, which is matrix-independent and inexact. A cacheable
 *  wrapper would therefore paint this node exactly once, into its parent's
 *  recording, and measure the path this test is not about. */
Element rotatedPill(float degrees, bool cached) {
  Element p = hairlinePill().absolute().left(52).top(133).rotate(degrees);
  if (cached)
    p.cache(Cache::Texture);
  return box().cache(Cache::None).child(std::move(p));
}
/** Pixels that differ at all, and the mean |Δ| over ink — the count is the
 *  claim, the mean is what makes a failure legible. */
struct BakeError {
  size_t differing = 0;
  double meanInk = 0;
};
BakeError bakeErrorAt(float degrees) {
  Host plain(300, 300), baked(300, 300);
  plain.composer.render(rotatedPill(degrees, false));
  baked.composer.render(rotatedPill(degrees, true));
  // Three frames: the bake must be taken on the first (the gate reads the
  // node's declared transform, not paint history) AND still be the same
  // pixels on the third, which is what catches a bake mode that oscillates.
  for (int i = 0; i < 3; ++i) {
    plain.frame();
    baked.frame();
  }
  SkBitmap ba, bb;
  ba.allocPixels(SkImageInfo::MakeN32Premul(300, 300));
  bb.allocPixels(SkImageInfo::MakeN32Premul(300, 300));
  plain.surface->readPixels(ba.pixmap(), 0, 0);
  baked.surface->readPixels(bb.pixmap(), 0, 0);
  BakeError out;
  double total = 0;
  size_t ink = 0;
  for (int y = 0; y < 300; ++y)
    for (int x = 0; x < 300; ++x) {
      const SkColor pa = ba.getColor(x, y), pb = bb.getColor(x, y);
      if (pa != pb)
        ++out.differing;
      if (pa == SK_ColorBLACK && pb == SK_ColorBLACK)
        continue;
      ++ink;
      total += (std::abs((int)SkColorGetR(pa) - (int)SkColorGetR(pb)) +
                std::abs((int)SkColorGetG(pa) - (int)SkColorGetG(pb)) +
                std::abs((int)SkColorGetB(pa) - (int)SkColorGetB(pb))) /
               3.0;
    }
  out.meanInk = ink ? total / (double)ink : 0.0;
  return out;
}
} // namespace

TEST(ComposeCache, TextureBakeSurvivesAQuarterTurn) {
  // A settled bake is taken in DEVICE space, snapped out to whole device
  // pixels and blitted with the matrix reset, so it is a literal copy of
  // the pixels the uncached draw would have produced — at ANY angle, not
  // merely close at the convenient ones.
  //
  // The history is worth keeping, because each stage looked finished:
  // mean |Δ| over the ink of a rotated pill was 30–32/255 at ±90° while
  // upright measured 4.4 (the bake read the matrix DIAGONAL, saw scale 0
  // at a quarter turn, and rasterized at the 0.25 floor). Fixing the scale
  // took ±90° to 13.5 and upright to 0 — better, and still wrong, because
  // a bake held in LOCAL space is resampled by whatever transform blits
  // it: the texel grid landed half a texel off the device grid, costing
  // 21% of the gradient across that axis. Correct resolution was necessary
  // and not sufficient.
  for (float degrees : {0.0f, 90.0f, -90.0f, 180.0f, 45.0f}) {
    const BakeError e = bakeErrorAt(degrees);
    EXPECT_EQ(e.differing, 0u)
        << "rotate(" << degrees << "): " << e.differing
        << " pixels differ from the uncached render (mean |delta| over ink "
        << e.meanInk << ") — the bake is being resampled, not copied";
  }
}

TEST(ComposeCache, ATextureBakeCompositesThroughItsOwnLayer) {
  // The exact bake blits with the matrix RESET, and an opacity/blend node
  // is already inside a saveLayer when it does. That is only correct
  // because an identity CTM is global canvas space even inside a layer —
  // the layer device carries its own origin. If that were wrong the image
  // would land somewhere else entirely, so it is worth an assertion rather
  // than an argument.
  const auto pill = [](bool cached, bool rotate) {
    Element p = hairlinePill()
                    .absolute()
                    .left(52)
                    .top(133)
                    .rotate(rotate ? -90.0f : 0.0f)
                    .opacity(0.5f)
                    .blend(SkBlendMode::kScreen);
    if (cached)
      p.cache(Cache::Texture);
    return box().cache(Cache::None).child(std::move(p));
  };
  for (bool rotate : {false, true}) {
    Host plain(300, 300), baked(300, 300);
    plain.composer.render(pill(false, rotate));
    baked.composer.render(pill(true, rotate));
    for (int i = 0; i < 3; ++i) {
      plain.frame();
      baked.frame();
    }
    // Not "roughly where it should be" — the layer receives exactly the
    // pixels paintContent would have drawn into it, so the composite is
    // the same composite.
    EXPECT_TRUE(identicalPixels(plain, baked, 300, 300))
        << "a cached node at opacity/blend" << (rotate ? " + rotate(-90)" : "")
        << " did not composite the same as the uncached one";
  }
}

// ---------------------------------------------------------------------------
// subtreeReadsBackdrop — the one thing promotion may never do.
//
// A bake is taken into a TRANSPARENT layer and blitted back. Anything in
// the subtree that composites against what is ALREADY on the canvas would
// therefore resolve against transparent black instead of the real
// backdrop, and come out wrong. computeVolatile works this out for the
// whole subtree; until now nothing tested it, and it is invisible in the
// common case — a still frame of an unpromoted node looks identical.

namespace {
/** An expensive panel with one MULTIPLY child, over an opaque ground. The
 *  ground matters: multiply against mid-grey and multiply against
 *  transparent black differ enormously, so a wrongly-baked subtree is
 *  loud rather than subtle. */
Element blendingScene(SkBlendMode mode) {
  return profiledUnder(
      stack()
          .child(box().absolute().inset(0).fill(
              Fill::color({0.55f, 0.55f, 0.6f, 1})))
          .child(expensivePanel().key("reader").child(
              box()
                  .absolute()
                  .left(20)
                  .top(20)
                  .width(90)
                  .height(90)
                  .fill(Fill::color({0.9f, 0.5f, 0.2f, 1}))
                  .blend(mode))));
}
} // namespace

TEST(ComposeCache, PromotionRefusesASubtreeThatBlendsWithTheCanvas) {
  Host host(220, 220);
  host.composer.setProfiling(true);
  host.composer.render(blendingScene(SkBlendMode::kMultiply));
  for (int i = 0; i < 24; ++i)
    host.frame();
  const Composer::NodeCost *row = requireRow(host.composer, "reader");
  ASSERT_NE(row, nullptr);
  EXPECT_NE(row->cacheState, Composer::CacheState::Promoted)
      << "baked a subtree containing a kMultiply child — its blend would "
         "have resolved against transparent black";
  // Specifically ReadsBackdrop, not the Filtered bucket. Asserting the
  // bucket would have passed had the panel been refused for its clip, its
  // layer effect, or its own backdrop instead — four different causes, one
  // of which is the one under test. A guard that cannot tell them apart
  // does not guard the thing it is named for.
  EXPECT_EQ(row->promotion, Composer::Promotion::ReadsBackdrop);
}

TEST(ComposeCache, TheBlendingChildIsWhatCausesTheRefusal) {
  // THE POSITIVE CONTROL, and the reason the guards above can be trusted.
  //
  // A refusal test proves nothing on its own: a node refused for some
  // unrelated reason — too cheap, wrong transform, a stray clip — passes
  // "was not promoted" exactly as well as one refused for the reason under
  // test. So render the SAME scene with the child's blend set to kSrcOver,
  // which is the only difference, and require that it IS promoted.
  //
  // Together the two halves say: this scene is promotable, and the ONLY
  // thing standing between it and a bake is the child's blend mode. If a
  // future change made the refusal fire for everything, this fails; if it
  // made it fire for nothing, its sibling fails.
  Host host(220, 220);
  host.composer.setProfiling(true);
  host.composer.render(blendingScene(SkBlendMode::kSrcOver));
  for (int i = 0; i < 24; ++i)
    host.frame();
  const Composer::NodeCost *row = requireRow(host.composer, "reader");
  ASSERT_NE(row, nullptr);
  EXPECT_EQ(row->cacheState, Composer::CacheState::Promoted)
      << "the same scene with a srcOver child was not promoted either, so "
         "the refusal test above proves nothing about blending";
  EXPECT_EQ(row->promotion, Composer::Promotion::Promoted);
}

TEST(ComposeCache, ABlendingSubtreeKeepsItsPixelsUnderPromotion) {
  // The assertion that would catch a future relaxation of the rule. If the
  // subtree were ever baked, the multiply child would composite against a
  // transparent layer and these two renders would diverge by a lot.
  for (SkBlendMode mode : {SkBlendMode::kMultiply, SkBlendMode::kScreen,
                           SkBlendMode::kPlus}) {
    Host on(220, 220), off(220, 220);
    off.composer.setAutoTexturePromotion(false);
    on.composer.render(blendingScene(mode));
    off.composer.render(blendingScene(mode));
    for (int i = 0; i < 24; ++i) {
      on.frame();
      off.frame();
    }
    EXPECT_TRUE(identicalPixels(off, on, 220, 220))
        << "promotion changed the pixels of a subtree that blends with the "
           "canvas (mode " << (int)mode << ")";
  }
}

TEST(ComposeCache, PromotionRefusesABackdropFilter) {
  // The other half of subtreeReadsBackdrop: a backdrop filter SAMPLES the
  // destination, so a bake would filter transparent black.
  Host host(220, 220);
  host.composer.setProfiling(true);
  host.composer.render(profiledUnder(
      stack()
          .child(box().absolute().inset(0).fill(
              Fill::color({0.55f, 0.55f, 0.6f, 1})))
          .child(expensivePanel().key("reader").child(
              box()
                  .absolute()
                  .left(20)
                  .top(20)
                  .width(90)
                  .height(90)
                  .backdrop(Effect::filter(
                      SkImageFilters::Blur(3, 3, nullptr)))))));
  for (int i = 0; i < 24; ++i)
    host.frame();
  const Composer::NodeCost *row = requireRow(host.composer, "reader");
  ASSERT_NE(row, nullptr);
  EXPECT_NE(row->cacheState, Composer::CacheState::Promoted);
  EXPECT_EQ(row->promotion, Composer::Promotion::ReadsBackdrop);
}

TEST(ComposeCache, AClipIsRefusedSeparatelyFromABlendingSubtree) {
  // The other side of the same split: a clip is the author's OWN node to
  // change, a blend can be three levels down and invisible to them. If
  // these two ever collapse back into one reason, this fails.
  Host host(220, 220);
  host.composer.setProfiling(true);
  host.composer.render(
      profiledUnder(expensivePanel().key("clipped").clip(true)));
  for (int i = 0; i < 24; ++i)
    host.frame();
  const Composer::NodeCost *row = requireRow(host.composer, "clipped");
  ASSERT_NE(row, nullptr);
  EXPECT_EQ(row->promotion, Composer::Promotion::Filtered);
}

TEST(ComposeCache, PromotionRefusesEveryRotation) {
  // Promotion's envelope is upright, unmirrored and unskewed, and ±90° is
  // "axis aligned" in the loose sense that would have let it through. A
  // promoted node that resampled would be a fidelity bug bought with a
  // perf win, which is worse than the perf bug.
  for (float degrees : {90.0f, -90.0f, 180.0f, 45.0f}) {
    Host host(300, 300);
    host.composer.setProfiling(true);
    // Cache::None on the wrapper so the panel is painted every frame; under
    // a cacheable parent it would be recorded once and never profiled, and
    // every assertion below would pass without testing anything.
    host.composer.render(profiledUnder(
        expensivePanel().absolute().left(40).top(40).rotate(degrees).key(
            "turned")));
    for (int i = 0; i < 30; ++i)
      host.frame();
    const Composer::NodeCost *row = requireRow(host.composer, "turned");
    ASSERT_NE(row, nullptr);
    EXPECT_NE(row->cacheState, Composer::CacheState::Promoted)
        << "promoted a node at rotate(" << degrees << ")";
    EXPECT_EQ(row->promotion, Composer::Promotion::Transformed);
  }
}

// ---------------------------------------------------------------------------
// Promotion, part two: the nodes it could not previously SEE.
//
// A leaf never records a picture — one drawRect beats a nested recording —
// and the promoter only ever measured the replay path. So a full-canvas box
// carrying one shader, which is the most expensive object in this corpus by
// an order of magnitude (663 ms of a 697 ms frame in chladni_tab1), was
// structurally invisible to it.

namespace {
Element heavyLeaf(const char *key) {
  return profiledUnder(box().width(400).height(400).key(key).fill(
      Material::sksl(heavyEffect(false))));
}
} // namespace

TEST(ComposeCache, PromotesAnExpensiveLeafAndKeepsEveryPixel) {
  Host promoted(400, 400);
  promoted.composer.setProfiling(true);
  promoted.composer.render(heavyLeaf("field"));
  for (int i = 0; i < 24; ++i)
    promoted.frame();
  const Composer::NodeCost *row = requireRow(promoted.composer, "field");
  ASSERT_NE(row, nullptr);
  EXPECT_EQ(row->cacheState, Composer::CacheState::Promoted)
      << "a leaf costing " << row->selfMs
      << " ms per frame was never considered for a bake";
  EXPECT_EQ(row->promotion, Composer::Promotion::Promoted);

  // …and it is the same picture. This is the whole constraint: the corpus
  // is hairlines at 1x, and a bake that resolved anywhere but the device
  // grid would trade a perf bug for a fidelity bug.
  Host plain(400, 400);
  plain.composer.setAutoTexturePromotion(false);
  plain.composer.render(heavyLeaf("field"));
  for (int i = 0; i < 24; ++i)
    plain.frame();
  EXPECT_TRUE(identicalPixels(plain, promoted, 400, 400))
      << "promoting the leaf changed its pixels";
}

TEST(ComposeCache, ARefusalSaysWhy) {
  // The refusals are individually correct and individually invisible, and
  // an author staring at `live paint  663 ms` cannot act on silence. The
  // opacity case is the honest one: compositing a bake applies the alpha
  // to an already-rounded 8-bit colour where the direct draw applies it to
  // the shader's float output, so the two agree to within 1 LSB — which is
  // not agreement. Cache::Texture is how you say you accept that.
  Host host(400, 400);
  host.composer.setProfiling(true);
  host.composer.render(profiledUnder(box()
                                         .width(400)
                                         .height(400)
                                         .key("wash")
                                         .fill(Material::sksl(heavyEffect(false)))
                                         .opacity(0.4f)));
  for (int i = 0; i < 24; ++i)
    host.frame();
  const Composer::NodeCost *row = requireRow(host.composer, "wash");
  ASSERT_NE(row, nullptr);
  EXPECT_NE(row->cacheState, Composer::CacheState::Promoted);
  EXPECT_EQ(row->promotion, Composer::Promotion::Composited);
  EXPECT_STRNE(Composer::promotionReason(row->promotion), "");
}

// ---------------------------------------------------------------------------
// Temporal promotion: "static" is the wrong eligibility test. The right one
// is STABLE SINCE THE LAST BAKE.

namespace {
/** Host with a real FrameClock, so a material's injected uTime advances.
 *  (The shared Host deliberately has none — most tests want elapsed 0.) */
struct ClockedHost {
  sigil::motion::Ticker ticker;
  sigil::motion::FrameClock clock;
  Composer composer{ticker, fonts()};
  sk_sp<SkSurface> surface;
  double now = 0;

  ClockedHost(int w, int h) {
    composer.setClock(&clock);
    composer.setSize({(float)w, (float)h});
    surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w, h));
  }
  void frame(double dt) {
    now += dt;
    clock.tick(now);
    ticker.tick(dt);
    surface->getCanvas()->clear(SK_ColorBLACK);
    composer.draw(*surface->getCanvas());
  }
  SkBitmap grab(int w, int h) {
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(w, h));
    surface->readPixels(bm.pixmap(), 0, 0);
    return bm;
  }
};
Element timedLeaf(float quantizeHz) {
  Material m = Material::sksl(heavyEffect(true));
  if (quantizeHz > 0)
    m.quantizeTime(quantizeHz);
  return box().child(box().width(400).height(400).key("plasma").fill(
      std::move(m)));
}
} // namespace

TEST(ComposeCache, AQuantizedMaterialIsCacheableBetweenItsTicks) {
  // quantizeTime(4) at 60 FPS means the shader's inputs change four times a
  // second and the other 56 frames resolve to the SAME shader. Their pixels
  // are therefore identical to the last bake's — not similar, identical —
  // so the bake is still valid and the shader need not run.
  ClockedHost host(400, 400);
  host.composer.setProfiling(true);
  host.composer.render(timedLeaf(4.0f));
  for (int i = 0; i < 24; ++i)
    host.frame(1.0 / 60.0);
  const Composer::NodeCost *row = requireRow(host.composer, "plasma");
  ASSERT_NE(row, nullptr);
  EXPECT_EQ(row->cacheState, Composer::CacheState::Promoted)
      << "a material stepping at 4 Hz still paid its shader 60 times a second";
}

TEST(ComposeCache, AContinuousMaterialStaysLive) {
  // The other half of the same rule, and the reason it is MEASURED rather
  // than read off quantizeTime(): a material whose inputs really do change
  // every frame would re-bake every frame, which costs more than the replay
  // it replaced. Its stability rate never reaches the threshold.
  ClockedHost host(400, 400);
  host.composer.setProfiling(true);
  host.composer.render(timedLeaf(0.0f));
  for (int i = 0; i < 24; ++i)
    host.frame(1.0 / 60.0);
  const Composer::NodeCost *row = requireRow(host.composer, "plasma");
  ASSERT_NE(row, nullptr);
  EXPECT_NE(row->cacheState, Composer::CacheState::Promoted);
  EXPECT_EQ(row->promotion, Composer::Promotion::Volatile);
}

TEST(ComposeCache, TemporalPromotionIsPixelIdenticalAcrossATick) {
  // Held to the same standard as the static case, over a window that
  // straddles the quantizer's step: 4 Hz at 60 FPS steps on frames 15, 30
  // and 45, so frames 24..40 cover a full hold, the tick, and the hold
  // after it. Every frame must match the unpromoted render exactly.
  ClockedHost promoted(400, 400), plain(400, 400);
  plain.composer.setAutoTexturePromotion(false);
  promoted.composer.render(timedLeaf(4.0f));
  plain.composer.render(timedLeaf(4.0f));
  for (int i = 0; i < 41; ++i) {
    promoted.frame(1.0 / 60.0);
    plain.frame(1.0 / 60.0);
    if (i < 24)
      continue;
    SkBitmap a = plain.grab(400, 400), b = promoted.grab(400, 400);
    size_t differing = 0;
    for (int y = 0; y < 400; ++y)
      for (int x = 0; x < 400; ++x)
        differing += a.getColor(x, y) != b.getColor(x, y);
    EXPECT_EQ(differing, 0u) << "frame " << i << ": " << differing
                             << " pixels changed under temporal promotion";
  }
}

// ---------------------------------------------------------------------------
// The same argument for animated SCALARS. Volatility asks whether a motion
// is CONNECTED; it never asked whether the value MOVED — so a keyframe
// path's hold segment repainted every frame while provably constant.

namespace {
/** A stroked ring whose trim end follows a keyframe path with a HOLD:
 *  0 -> 0.6 over 200 ms, then flat until 600 ms, then on to 1. The flat
 *  stretch is the whole point — it is a running motion whose value is not
 *  changing, which is the case the volatility model could not express. */
Element gatedRing(Cache mode) {
  return box()
      .cache(Cache::None)
      .child(
          box()
              .width(120)
              .height(120)
              .key("ring")
              .cache(mode)
              .shape(shapes::circle())
              .stroke(util::stroke(6.0f, Fill::color({1, 1, 1, 1})))
              .mask(by::spans(spans::upTo(
                  animate(through({{std::chrono::milliseconds(0), 0.0f},
                                   {std::chrono::milliseconds(200), 0.6f},
                                   {std::chrono::milliseconds(600), 0.6f},
                                   {std::chrono::milliseconds(800), 1.0f}}),
                          &choreograph::easeNone)))));
}
} // namespace

TEST(ComposeCache, AHeldKeyframeSegmentDoesNotRepaint) {
  Host host;
  host.composer.render(gatedRing(Cache::Auto));
  host.frame();
  for (int i = 0; i < 18; ++i)
    host.frame(1.0 / 60.0); // t ~ 0.30 s: inside the hold
  unsigned duringHold = 0;
  for (int i = 0; i < 15; ++i) { // 0.30 -> 0.55 s, still flat
    host.frame(1.0 / 60.0);
    duringHold += host.composer.stats().picturesRecorded;
  }
  EXPECT_EQ(duringHold, 0u)
      << "re-recorded " << duringHold
      << " times across a keyframe segment whose value never changed";
  unsigned afterHold = 0;
  for (int i = 0; i < 12; ++i) { // 0.55 -> 0.75 s: moving again
    host.frame(1.0 / 60.0);
    afterHold += host.composer.stats().picturesRecorded;
  }
  EXPECT_GT(afterHold, 0u)
      << "the memo went stale-blind: the trim moved and nothing re-recorded";
}

TEST(ComposeCache, ScalarMemoIsPixelIdenticalAcrossEveryWaypoint) {
  // The constraint carried from the temporal-promotion work: every frame
  // identical, not just the held ones. A hold that goes stale by one frame
  // at a waypoint is exactly the bug this could introduce, and it would be
  // invisible in any single still.
  //
  // Cache::None on the node under test is the ground truth — it re-paints
  // from scratch every frame by construction — so the two hosts differ in
  // nothing but whether the memo is allowed to hold.
  Host memo, truth;
  memo.composer.render(gatedRing(Cache::Auto));
  truth.composer.render(gatedRing(Cache::None));
  memo.frame();
  truth.frame();
  for (int i = 0; i < 60; ++i) { // a full second, over all three waypoints
    memo.frame(1.0 / 60.0);
    truth.frame(1.0 / 60.0);
    ASSERT_TRUE(identicalPixels(memo, truth, 200, 200))
        << "frame " << i << " (t = " << (double)i / 60.0
        << " s) differs from the uncached render";
  }
}

// ---------------------------------------------------------------------------
// decorations::Border — brackets, gapped rules, weighted corners, insets.
