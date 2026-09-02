#include "support/BrushTestSupport.h"

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
  host.composer.render(
      box().child(box()
                      .inset(pct(10), pct(10), pct(10), pct(10))
                      .fill(red())
                      .key("panel")));
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

TEST(ComposeText, TextAlignCentersWithinWideBox) {
  auto leftmostLit = [](Host& host) {
    for (int x = 0; x < 400; ++x)
      for (int y = 0; y < 100; y += 2)
        if (host.pixel(x, y) != SK_ColorBLACK) return x;
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
  ASSERT_LT(startX, 400);  // both actually painted
  ASSERT_LT(centerX, 400);
  EXPECT_GT(centerX, startX + 60);  // centered glyphs sit near mid-box
}

TEST(ComposeUtil, MarqueeSlidesTwoCopies) {
  Host host(200, 60);
  choreograph::Output<float> phase{0.0f};
  host.composer.render(box().padding(10).child(
      marquee(box().width(60).height(20).fill(red()), &phase)
          .width(Dim(100.0f))
          .height(Dim(20.0f))));
  host.frame();
  EXPECT_EQ(host.pixel(60, 20), SK_ColorRED);   // first copy
  EXPECT_EQ(host.pixel(105, 20), SK_ColorRED);  // second copy (65..130 → clip)
  phase = -30.0f;                               // slide — no render()
  host.frame();
  EXPECT_EQ(host.pixel(85, 20), SK_ColorRED);     // second copy now 30..90
  EXPECT_EQ(host.pixel(105, 20), SK_ColorBLACK);  // past both copies, clipped
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

TEST(ComposeText, TextFillMapsUnitRampToCapBand) {
  // A hard two-stop ramp authored in [0,1]: red above the midline, blue
  // below. textFill maps it to the CAP BAND, so the switch happens INSIDE
  // the glyphs — capitals read red on top, blue underneath.
  Host host(300, 120);
  host.composer.render(box().padding(20).child(
      text(u8"HHH", whiteStyle(64))
          .textFill(material::skia::Paint::linear({0, 0}, {0, 1},
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
      if (c == SK_ColorBLACK) continue;
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
  EXPECT_GT(topR, 20);        // upper glyph pixels are red…
  EXPECT_GT(botB, 20);        // …lower ones blue…
  EXPECT_LT(topB, topR / 4);  // …and barely mixed
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
        .width(240)
        .height(240)
        .absolute()
        .left(0)
        .top(0)
        .onPath({.path = geometry::shapes::arc(180.0f, 359.9f),
                 .at = at,
                 .align = TextPath::Align::Center});
  };
  auto lit = [](Host& host, int y0, int y1) {
    int count = 0;
    for (int y = y0; y < y1; ++y)
      for (int x = 0; x < 240; ++x) count += host.pixel(x, y) != SK_ColorBLACK;
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

TEST(ComposeText, OnPathWrapsTheSeamAndTheFlippedRunKeepsItsHalf) {
  // Two properties of text on a closed baseline, each easy to get wrong in
  // a way that looks like a layout choice.
  //
  // 1. Align::Center at at=0 puts half the run at a NEGATIVE distance.
  //    Fraction 0 and 1 are the same point on a ring, so the run has to
  //    straddle the seam rather than be clipped off at it.
  // 2. autoFlip must turn the RUN over, not each glyph in place. Flipping
  //    glyphs individually reverses reading order, so a caption on the
  //    lower half comes out mirrored.
  //
  // Note what is and is not asserted: both arms measure ink over a whole
  // half, so this pins the seam straddle and the flipped run still
  // occupying its half — not the order of glyphs within it.
  auto ink = [](Host& host, int x0, int x1, int y0, int y1) {
    int count = 0;
    for (int y = y0; y < y1; ++y)
      for (int x = x0; x < x1; ++x) count += host.pixel(x, y) != SK_ColorBLACK;
    return count;
  };

  Host seam(240, 240);
  seam.composer.render(
      box().child(text(u8"HHHHHHHH", whiteStyle(20))
                      .width(240)
                      .height(240)
                      .absolute()
                      .left(0)
                      .top(0)
                      .onPath({.path = geometry::shapes::arc(180.0f, 359.9f),
                               .at = 0.0f,
                               .align = TextPath::Align::Center})));
  seam.frame();
  // at=0 on this arc is 9 o'clock, so a centred run straddles it: ink on
  // BOTH sides of the horizontal midline, near the left edge.
  EXPECT_GT(ink(seam, 0, 60, 0, 120), 60)
      << "the half before the seam was dropped";
  EXPECT_GT(ink(seam, 0, 60, 120, 240), 60);

  // Flipped, the run must still read left-to-right in the same order it
  // does unflipped — mirrored text has its ink distribution reversed, so
  // compare the first and last thirds of a deliberately lopsided run.
  auto lopsided = [](bool flip) {
    return text(u8"IIIIIIIIWWWW", whiteStyle(20))
        .width(260)
        .height(260)
        .absolute()
        .left(0)
        .top(0)
        .onPath({.path = geometry::shapes::arc(0.0f, 359.9f),
                 .at = 0.30f,
                 .align = TextPath::Align::Start,
                 .autoFlip = flip});
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
  // textFill supersedes the style's FOREGROUND only, never the passes
  // around it. Overriding the whole PaintStyle instead silently drops every
  // underlay — a wordmark loses its cast shadow and its keyline and reads as
  // flat type, which looks like a design choice rather than a bug.
  Host host(300, 120);
  auto styled = [] {
    auto s = whiteStyle(64);
    sigil::weave::PaintLayer keyline;
    keyline.paint.setAntiAlias(true);
    keyline.paint.setStyle(SkPaint::kStroke_Style);
    keyline.paint.setStrokeWidth(6);
    keyline.paint.setColor4f({0, 1, 0, 1},
                             nullptr);  // unmistakably not the fill
    s.paint.addUnderlay(keyline);
    return s;
  }();
  host.composer.render(box().padding(20).child(
      text(u8"HHH", styled)
          .textFill(material::skia::Paint::solid({1, 0, 0, 1}))));
  host.frame();
  int red = 0, green = 0;
  for (int y = 0; y < 120; ++y)
    for (int x = 0; x < 300; ++x) {
      const SkColor c = host.pixel(x, y);
      red += SkColorGetR(c) > 180 && SkColorGetG(c) < 80;
      green += SkColorGetG(c) > 180 && SkColorGetR(c) < 80;
    }
  EXPECT_GT(red, 100);    // the material still paints the glyph bodies…
  EXPECT_GT(green, 100);  // …and the keyline underlay still rings them
}

// ---------------------------------------------------------------------------
// Delay staggers, unclipped decorations, knockout shadows, px origins,
// centerAt, and wrapped stroke windows.

TEST(ComposeMotion, DelayStaggersTheEntrance) {
  Host host;
  auto card = [](float delaySec) {
    return box().width(60).height(30).fill(red()).opacity(
        animate(motion::from(0.0f).to(1.0f),
                {200ms, &choreograph::easeNone,
                 std::chrono::milliseconds((int)(delaySec * 1000))}));
  };
  host.composer.render(
      box().column().gap(10).child(card(0.0f)).child(card(0.4f)));
  host.frame(0.3);  // first card done, second still holding its `from`
  EXPECT_EQ(host.pixel(30, 15), SK_ColorRED);
  EXPECT_EQ(host.pixel(30, 55), SK_ColorBLACK);
  host.frame(0.5);  // 0.8s total: both settled
  EXPECT_EQ(host.pixel(30, 55), SK_ColorRED);
}

TEST(ComposePaint, ClipSparesDecorations) {
  // clip() bounds fill/content/children; decorations dress the outline —
  // an Outer stroke and a shadow survive on a clipped node.
  Host host;
  host.composer.render(
      box().child(box()
                      .absolute()
                      .inset(60, 60, 60, 60)
                      .clip(true)
                      .fill(blue())
                      .stroke(stroke(10, green(), PathFormat::Align::Outer))
                      .child(box().width(200).height(10).fill(red()))));
  host.frame();
  EXPECT_EQ(host.pixel(52, 100), SK_ColorGREEN);  // outer stroke intact
  EXPECT_EQ(host.pixel(100, 100), SK_ColorBLUE);  // fill clipped area
  EXPECT_EQ(host.pixel(150, 65), SK_ColorBLACK);  // child clipped at 140
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

TEST(ComposeTransform, PixelOriginPivotsWhereTold) {
  // Two hosts: fractional center origin vs px origin at the box's own
  // top-left corner; rotate 90° and the box lands in different places.
  Host frac, px;
  auto tree = [](Element inner) { return box().child(std::move(inner)); };
  frac.composer.render(tree(box()
                                .absolute()
                                .inset(80, 80, 80, 80)
                                .fill(red())
                                .rotate(90.0f)));  // pivots on its center
  px.composer.render(tree(box()
                              .absolute()
                              .inset(80, 80, 80, 80)
                              .fill(red())
                              .rotate(90.0f)
                              .transformOriginPx({0, 0})));  // pivots top-left
  frac.frame();
  px.frame();
  EXPECT_EQ(frac.pixel(100, 100), SK_ColorRED);  // unchanged footprint
  EXPECT_EQ(px.pixel(100, 100), SK_ColorBLACK);  // swung away
  EXPECT_EQ(px.pixel(65, 100), SK_ColorRED);     // now left of the pivot
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

TEST(ComposeMotion, StaggerChildrenCascadesEntrances) {
  // One container call replaces per-child delay arithmetic: child i's whole
  // subtree enters i·each later, so inserting a child does not require
  // renumbering its siblings.
  Host host;
  auto card = [] {
    return box().width(60).height(30).fill(red()).opacity(
        animate(motion::from(0.0f).to(1.0f), {200ms, &choreograph::easeNone}));
  };
  host.composer.render(
      box().column().gap(10).staggerChildren(400ms).child(card()).child(
          card()));
  host.frame(0.3);  // child 0 settled; child 1 still holding its `from`
  EXPECT_EQ(host.pixel(30, 15), SK_ColorRED);
  EXPECT_EQ(host.pixel(30, 55), SK_ColorBLACK);
  host.frame(0.5);  // 0.8s: the cascade completed
  EXPECT_EQ(host.pixel(30, 55), SK_ColorRED);
}

#include <sigilcompose/brush/Hatches.h>
#include <sigilcompose/brush/Lines.h>
#include <sigilcompose/brush/Rails.h>

// ---------------------------------------------------------------------------
// Line patterns (Lines.h) — the beyond-dashes stroke vocabulary.

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
  Host miter, round1, round2;
  miter.composer.render(corneredRun(cased(SkPaint::kMiter_Join)));
  round1.composer.render(corneredRun(cased(SkPaint::kRound_Join)));
  round2.composer.render(corneredRun(cased(SkPaint::kRound_Join)));
  miter.frame();
  round1.frame();
  round2.frame();
  // The outer rail rides 6 px outside the corner at (140, 140). A miter
  // join carries it to the diagonal point (146, 146); a round join arcs
  // at radius 6 and tops out ~1.8 px short of it. The named pixel sits in
  // the miter tip and past the round arc.
  EXPECT_NE(miter.pixel(146, 146), SK_ColorBLACK)
      << "the miter tip never reached the corner";
  EXPECT_EQ(round1.pixel(146, 146), SK_ColorBLACK)
      << "a round join reached the miter tip — the field is not wired";
  // Control: the two joins differ near the corner, and only there by
  // construction; two renders of the SAME join are identical.
  int cornerDiff = 0, sameDiff = 0;
  for (int y = 138; y <= 152; ++y)
    for (int x = 138; x <= 152; ++x) {
      cornerDiff += miter.pixel(x, y) != round1.pixel(x, y);
      sameDiff += round1.pixel(x, y) != round2.pixel(x, y);
    }
  EXPECT_GT(cornerDiff, 3) << "join control changed nothing at a 90° jog";
  EXPECT_EQ(sameDiff, 0);
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
  stated.composer.render(
      ringNode(lines::concentric(green(), std::vector<float>{60.0f}, 2.0f)));
  spaced.composer.render(ringNode(lines::concentric(green(), /*rings=*/1,
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
  EXPECT_TRUE(lines::concentric(green(), std::vector<float>{60.0f}) ==
              lines::concentric(green(), std::vector<float>{60.0f}));
  EXPECT_FALSE(lines::concentric(green(), std::vector<float>{60.0f}) ==
               lines::concentric(green(), std::vector<float>{61.0f}));
}

#include <sigilcompose/brush/Brushes.h>

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
  // subtends a larger ANGLE on the inner rail than on the outer one. Across
  // every dash that accumulates into a fixed number of sample angles that
  // can never agree, however exact the geometry is. The bars below are set
  // from that residual, with the sheared case an order of magnitude worse.
  EXPECT_GT(good.agreement, 0.93);
  EXPECT_LT(sheared.agreement, good.agreement - 0.30);
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
  // failure mode is silent — `lines::cased(...)` with a dash pattern simply
  // paints two SOLID rails, which looks like a design choice in every
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

// ---------------------------------------------------------------------------
// Cache::Auto texture promotion — the library fixing a slow frame by itself.

TEST(ComposeCache, TheAutoPromotionSwitchChangesNoPixels) {
  // Automatic promotion may never change a pixel. A texture cache that
  // resolved at the wrong scale and softened a hairline would trade a speed
  // problem for a fidelity problem, and hairlines at 1x are most of what
  // this library draws. So promotion bakes in DEVICE space at an
  // integer-snapped rect and blits with the matrix reset: an integer device
  // translation cannot change rasterisation. This asserts that over the
  // whole canvas rather than trusting it.
  //
  // Note the limit of the claim: nothing here establishes that a node was
  // actually promoted. If promotion stopped firing entirely, this would
  // compare two unpromoted renders and pass. It pins the SWITCH, not the
  // mechanism — see the profiling cases for that.
  Host reference;
  reference.composer.setAutoTexturePromotion(false);
  reference.composer.render(box().child(expensivePanel()));
  for (int i = 0; i < 30; ++i) reference.frame();
  const std::vector<SkColor> before = grab(reference);

  Host promoted;
  promoted.composer.setAutoTexturePromotion(true);
  promoted.composer.render(box().child(expensivePanel()));
  for (int i = 0; i < 30; ++i) promoted.frame();
  const std::vector<SkColor> after = grab(promoted);

  ASSERT_EQ(before.size(), after.size());
  size_t differing = 0;
  for (size_t i = 0; i < before.size(); ++i) differing += before[i] != after[i];
  EXPECT_EQ(differing, 0u)
      << differing << " pixels changed when the library promoted a node";
}

TEST(ComposeCache, NoRowReportsPromotedWhilePromotionIsOff) {
  // A silent good outcome and a silent bad outcome look identical without an
  // instrument, so a promotion the library performs must be attributable to
  // the library. Whether this particular node trips the threshold depends on
  // the machine, so the assertion is about REPORTING, not about promoting:
  // no node may ever be labelled Promoted while promotion is switched off.
  Host host;
  host.composer.setAutoTexturePromotion(false);
  host.composer.setProfiling(true);
  host.composer.render(box().child(expensivePanel()));
  for (int i = 0; i < 30; ++i) host.frame();
  bool sawPromoted = false, sawPicture = false;
  for (const auto& row : host.composer.profile()) {
    sawPromoted |= row.cacheState == Composer::CacheState::Promoted;
    sawPicture |= row.cacheState == Composer::CacheState::Picture;
  }
  EXPECT_FALSE(sawPromoted) << "promotion is off, yet a node reported Promoted";
  EXPECT_TRUE(sawPicture) << "nothing cached as a picture at all";
  // And cached() still answers the coarse question.
  for (const auto& row : host.composer.profile())
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
  for (int i = 0; i < 30; ++i) host.frame();
  const Composer::NodeCost* row = requireRow(host.composer, "optout");
  ASSERT_NE(row, nullptr);
  EXPECT_NE(row->cacheState, Composer::CacheState::Promoted);
  EXPECT_EQ(row->promotion, Composer::Promotion::OptedOut);
  // The panel must actually be over the bar, or "not promoted" is true for
  // the wrong reason and this asserts nothing about the opt-out.
  EXPECT_GT(row->selfMs, 1.0)
      << "expensivePanel is under the promotion threshold, so Cache::Picture "
         "is not what kept it from being promoted";
}

namespace {

/** 196×33 of 1 px hairlines: content that a bake at the wrong resolution
 *  cannot fake. This library's entire output is hairlines at 1×. */
Element hairlinePill() {
  Element p =
      box().width(196).height(33).fill(Fill::color({0.85f, 0.86f, 0.9f, 1}));
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
  if (cached) p.cache(Cache::Texture);
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
      if (pa != pb) ++out.differing;
      if (pa == SK_ColorBLACK && pb == SK_ColorBLACK) continue;
      ++ink;
      total += (std::abs((int)SkColorGetR(pa) - (int)SkColorGetR(pb)) +
                std::abs((int)SkColorGetG(pa) - (int)SkColorGetG(pb)) +
                std::abs((int)SkColorGetB(pa) - (int)SkColorGetB(pb))) /
               3.0;
    }
  out.meanInk = ink ? total / (double)ink : 0.0;
  return out;
}

}  // namespace

TEST(ComposeCache, TextureBakeSurvivesAQuarterTurn) {
  // A settled bake is taken in DEVICE space, snapped out to whole device
  // pixels and blitted with the matrix reset, so it is a literal copy of
  // the pixels the uncached draw would have produced — at ANY angle, not
  // merely close at the convenient ones.
  //
  // Two independent things have to be right, and getting only the first
  // leaves a picture that still looks nearly correct at every angle:
  //
  //  - the bake RESOLUTION, which must come from the matrix's singular
  //    values rather than its diagonal (a quarter turn puts the whole scale
  //    in the skew terms, so the diagonal reads zero and the bake clamps to
  //    the ladder's floor); and
  //  - the bake SPACE, which must be device space. A bake held in LOCAL
  //    space is resampled by whatever transform blits it, so its texel grid
  //    lands off the device grid and softens every hairline.
  //
  // Correct resolution alone is necessary and not sufficient, which is why
  // the assertion is exact pixel equality at every angle rather than a
  // tolerance.
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
    if (cached) p.cache(Cache::Texture);
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

namespace {

/** A host camera that is PURE perspective: the keystone `[1,0,0; 0,1,0;
 *  0,p,1]` — a plate tipped away from the viewer, anchored at the top
 *  edge. Deliberately NOT a perspective·rotateY SkM44: that matrix's 2D
 *  projection also carries a skew term, so the `upright` gate would
 *  refuse it for the skew alone, and this test would pass even with the
 *  perspective clause removed. This matrix is upright by every other test
 *  the gate makes — scale 1, skew 0 — so only `hasPerspective()` says no. */
SkMatrix hostCamera(float p) {
  SkMatrix m = SkMatrix::I();
  m.setPerspY(p);
  return m;
}

/** Host::frame, under a host concat — the camera compose never sees. */
void frameUnder(Host& h, const SkMatrix& camera) {
  SkCanvas* canvas = h.surface->getCanvas();
  canvas->clear(SK_ColorBLACK);
  canvas->save();
  canvas->concat(camera);
  h.composer.draw(*canvas);
  canvas->restore();
}

/** Mean |Δ| over ink between two hosts — bakeErrorAt's meter, reusable
 *  for frames drawn under a camera. */
double meanInkDiff(Host& a, Host& b, int w, int h) {
  SkBitmap ba, bb;
  ba.allocPixels(SkImageInfo::MakeN32Premul(w, h));
  bb.allocPixels(SkImageInfo::MakeN32Premul(w, h));
  a.surface->readPixels(ba.pixmap(), 0, 0);
  b.surface->readPixels(bb.pixmap(), 0, 0);
  double total = 0;
  size_t ink = 0;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      const SkColor pa = ba.getColor(x, y), pb = bb.getColor(x, y);
      if (pa == SK_ColorBLACK && pb == SK_ColorBLACK) continue;
      ++ink;
      total += (std::abs((int)SkColorGetR(pa) - (int)SkColorGetR(pb)) +
                std::abs((int)SkColorGetG(pa) - (int)SkColorGetG(pb)) +
                std::abs((int)SkColorGetB(pa) - (int)SkColorGetB(pb))) /
               3.0;
    }
  return ink ? total / (double)ink : 0.0;
}

}  // namespace

TEST(ComposeCache, PromotionRefusesAHostPerspectiveCtm) {
  // Guard #1, promotion's `upright` gate: a perspective CTM must read as
  // Transformed — a device bake under it would pin a projected quad to an
  // axis-aligned rect. The no-perspective arm is the positive control: the
  // SAME node under the same harness PROMOTES, so a silently widened gate
  // fails the arm that proves the refusal was the guard's doing.
  for (bool perspective : {false, true}) {
    Host host(300, 300);
    host.composer.setProfiling(true);
    host.composer.render(profiledUnder(
        expensivePanel().absolute().left(40).top(40).key("underCamera")));
    const SkMatrix camera = perspective ? hostCamera(0.0012f) : SkMatrix::I();
    for (int i = 0; i < 30; ++i) frameUnder(host, camera);
    const Composer::NodeCost* row = requireRow(host.composer, "underCamera");
    ASSERT_NE(row, nullptr);
    if (perspective) {
      EXPECT_NE(row->cacheState, Composer::CacheState::Promoted)
          << "promoted a node under a host perspective CTM";
      EXPECT_TRUE(row->refused(Composer::Promotion::Transformed))
          << "the refusal must name the geometry";
    } else {
      EXPECT_EQ(row->cacheState, Composer::CacheState::Promoted)
          << "the control arm did not promote — the refusal assertion above "
             "is not testing the perspective guard";
    }
  }
}

namespace {

/** Smooth content for the projected-bake meter: a ramp panel with thick
 *  bars, not hairlines — the LOCAL bake legitimately resamples through
 *  the projection, so the pin bounds the error rather than demanding
 *  byte equality (that is the device bake's claim, and the device bake
 *  is exactly what perspective refuses). */
Element rampPanel(bool cached) {
  Element p = box().width(180).height(120).absolute().left(60).top(90).fill(
      material::skia::Paint::linearUnit(
          {0, 0}, {1, 1},
          {{0.0f, {0.9f, 0.3f, 0.1f, 1}}, {1.0f, {0.1f, 0.4f, 0.9f, 1}}}));
  for (int i = 0; i < 4; ++i)
    p.child(box()
                .absolute()
                .left(15 + (float)i * 42)
                .top(18)
                .width(18)
                .height(84)
                .fill(Fill::color({0.92f, 0.93f, 0.95f, 1})));
  if (cached) p.cache(Cache::Texture);
  return box().cache(Cache::None).child(std::move(p));
}

}  // namespace

TEST(ComposeCache, ATextureBakeUnderPerspectiveTracksTheCamera) {
  // Guard #3, the Cache::Texture device bake: under a perspective CTM the
  // node must fall back to the matrix-independent LOCAL bake (unpromoted
  // in the device sense) rather than bake a device-rect-pinned texture.
  // Pinned through what an author can see: the baked node's pixels track
  // the camera, frame over frame, staying near the uncached twin — a
  // device-pinned texture would replay the OLD projection after the
  // camera moved, and the second sweep below would blow up.
  Host plain(300, 300), baked(300, 300);
  plain.composer.render(rampPanel(false));
  baked.composer.render(rampPanel(true));
  const SkMatrix before = hostCamera(0.0008f);
  size_t bakes = 0;
  for (int i = 0; i < 3; ++i) {
    frameUnder(plain, before);
    frameUnder(baked, before);
    bakes += baked.composer.stats().texturesBaked;
  }
  const double still = meanInkDiff(plain, baked, 300, 300);
  EXPECT_LT(still, 4.0) << "the perspective fallback bake drifted from the "
                           "live render while the camera held still";

  // THE CAMERA MOVES. The projection changes enough to be its own control
  // (the two plain frames must differ), and the baked node must follow.
  const SkMatrix after = hostCamera(0.0016f);
  Host plainAfter(300, 300);
  plainAfter.composer.render(rampPanel(false));
  frameUnder(plainAfter, after);
  const double cameraMoved = meanInkDiff(plain, plainAfter, 300, 300);
  EXPECT_GT(cameraMoved, 8.0)
      << "the two camera angles are too close to distinguish a pinned bake "
         "from a tracking one — the pin below is vacuous";
  frameUnder(baked, after);
  bakes += baked.composer.stats().texturesBaked;
  const double tracked = meanInkDiff(plainAfter, baked, 300, 300);
  EXPECT_LT(tracked, 4.0)
      << "the baked node did not track the camera — a device-rect-pinned "
         "texture replayed the old projection (mean |delta| over ink "
      << tracked << " against " << cameraMoved << " of real motion)";
  // The BAKE COUNT, not the pixels, is what separates the two paths. A
  // wrongly-taken device bake mostly self-heals through its rect-stability
  // test, so a pixel comparison alone passes either way. The local fallback
  // bakes ONCE and its bake is matrix-independent, so camera motion re-bakes
  // nothing; a device bake is pinned to its rect and has to be re-taken
  // every time the camera moves.
  EXPECT_EQ(bakes, 1u)
      << "a Cache::Texture node under a perspective camera took a "
         "device-space bake (or re-baked under camera motion) instead of "
         "holding one local bake";
}

namespace {

/** An expensive panel with one MULTIPLY child, over an opaque ground. The
 *  ground matters: multiply against mid-grey and multiply against
 *  transparent black differ enormously, so a wrongly-baked subtree is
 *  loud rather than subtle. */
Element blendingScene(SkBlendMode mode) {
  return profiledUnder(stack()
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

}  // namespace

TEST(ComposeCache, PromotionRefusesASubtreeThatBlendsWithTheCanvas) {
  Host host(220, 220);
  host.composer.setProfiling(true);
  host.composer.render(blendingScene(SkBlendMode::kMultiply));
  for (int i = 0; i < 24; ++i) host.frame();
  const Composer::NodeCost* row = requireRow(host.composer, "reader");
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
  for (int i = 0; i < 24; ++i) host.frame();
  const Composer::NodeCost* row = requireRow(host.composer, "reader");
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
  for (SkBlendMode mode :
       {SkBlendMode::kMultiply, SkBlendMode::kScreen, SkBlendMode::kPlus}) {
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
           "canvas (mode "
        << (int)mode << ")";
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
              box().absolute().left(20).top(20).width(90).height(90).backdrop(
                  material::skia::Effect::filter(
                      SkImageFilters::Blur(3, 3, nullptr)))))));
  for (int i = 0; i < 24; ++i) host.frame();
  const Composer::NodeCost* row = requireRow(host.composer, "reader");
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
  for (int i = 0; i < 24; ++i) host.frame();
  const Composer::NodeCost* row = requireRow(host.composer, "clipped");
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
    for (int i = 0; i < 30; ++i) host.frame();
    const Composer::NodeCost* row = requireRow(host.composer, "turned");
    ASSERT_NE(row, nullptr);
    EXPECT_NE(row->cacheState, Composer::CacheState::Promoted)
        << "promoted a node at rotate(" << degrees << ")";
    EXPECT_EQ(row->promotion, Composer::Promotion::Transformed);
  }
}

namespace {

Element heavyLeaf(const char* key) {
  return profiledUnder(box().width(400).height(400).key(key).fill(
      material::skia::Paint::sksl(heavyEffect(false))));
}

}  // namespace

TEST(ComposeCache, PromotesAnExpensiveLeafAndKeepsEveryPixel) {
  Host promoted(400, 400);
  promoted.composer.setProfiling(true);
  promoted.composer.render(heavyLeaf("field"));
  for (int i = 0; i < 24; ++i) promoted.frame();
  const Composer::NodeCost* row = requireRow(promoted.composer, "field");
  ASSERT_NE(row, nullptr);
  EXPECT_EQ(row->cacheState, Composer::CacheState::Promoted)
      << "a leaf costing " << row->selfMs
      << " ms per frame was never considered for a bake";
  EXPECT_EQ(row->promotion, Composer::Promotion::Promoted);

  // …and it is the same picture. That is the binding constraint: the output
  // is hairlines at 1x, and a bake resolved anywhere but on the device grid
  // trades a speed problem for a fidelity one.
  Host plain(400, 400);
  plain.composer.setAutoTexturePromotion(false);
  plain.composer.render(heavyLeaf("field"));
  for (int i = 0; i < 24; ++i) plain.frame();
  EXPECT_TRUE(identicalPixels(plain, promoted, 400, 400))
      << "promoting the leaf changed its pixels";
}

TEST(ComposeCache, ARefusalSaysWhy) {
  // Every refusal is individually correct and individually invisible, so an
  // author looking at a node that is painting live and slow has nothing to
  // act on unless the refusal names itself.
  //
  // Opacity is the honest refusal: compositing a bake applies the alpha to
  // an already-rounded 8-bit colour, where a direct draw applies it to the
  // shader's float output. The two agree to within one least-significant
  // bit, which is not agreement. Cache::Texture is how an author says they
  // accept that trade.
  Host host(400, 400);
  host.composer.setProfiling(true);
  host.composer.render(
      profiledUnder(box()
                        .width(400)
                        .height(400)
                        .key("wash")
                        .fill(material::skia::Paint::sksl(heavyEffect(false)))
                        .opacity(0.4f)));
  for (int i = 0; i < 24; ++i) host.frame();
  const Composer::NodeCost* row = requireRow(host.composer, "wash");
  ASSERT_NE(row, nullptr);
  EXPECT_NE(row->cacheState, Composer::CacheState::Promoted);
  EXPECT_EQ(row->promotion, Composer::Promotion::Composited);
  EXPECT_STRNE(Composer::promotionReason(row->promotion), "");
}

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
  material::skia::Paint m = material::skia::Paint::sksl(heavyEffect(true));
  if (quantizeHz > 0) m.quantizeTime(quantizeHz);
  return box().child(
      box().width(400).height(400).key("plasma").fill(std::move(m)));
}

}  // namespace

TEST(ComposeCache, AQuantizedMaterialIsCacheableBetweenItsTicks) {
  // quantizeTime(4) at 60 FPS means the shader's inputs change four times a
  // second and the other 56 frames resolve to the SAME shader. Their pixels
  // are therefore identical to the last bake's — not similar, identical —
  // so the bake is still valid and the shader need not run.
  ClockedHost host(400, 400);
  host.composer.setProfiling(true);
  host.composer.render(timedLeaf(4.0f));
  for (int i = 0; i < 24; ++i) host.frame(1.0 / 60.0);
  const Composer::NodeCost* row = requireRow(host.composer, "plasma");
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
  for (int i = 0; i < 24; ++i) host.frame(1.0 / 60.0);
  const Composer::NodeCost* row = requireRow(host.composer, "plasma");
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
    if (i < 24) continue;
    SkBitmap a = plain.grab(400, 400), b = promoted.grab(400, 400);
    size_t differing = 0;
    for (int y = 0; y < 400; ++y)
      for (int x = 0; x < 400; ++x)
        differing += a.getColor(x, y) != b.getColor(x, y);
    EXPECT_EQ(differing, 0u) << "frame " << i << ": " << differing
                             << " pixels changed under temporal promotion";
  }
}

namespace {

/** A stroked ring whose trim end follows a keyframe path with a HOLD:
 *  0 -> 0.6 over 200 ms, then flat until 600 ms, then on to 1. The flat
 *  stretch is the whole point — it is a running motion whose value is not
 *  changing, which is the case the volatility model could not express. */
Element gatedRing(Cache mode) {
  return box()
      .cache(Cache::None)
      .child(box()
                 .width(120)
                 .height(120)
                 .key("ring")
                 .cache(mode)
                 .shape(geometry::shapes::circle())
                 .stroke(stroke(6.0f, Fill::color({1, 1, 1, 1})))
                 .mask(by::spans(spans::upTo(
                     animate(sigil::motion::through(
                                 {{std::chrono::milliseconds(0), 0.0f},
                                  {std::chrono::milliseconds(200), 0.6f},
                                  {std::chrono::milliseconds(600), 0.6f},
                                  {std::chrono::milliseconds(800), 1.0f}}),
                             &choreograph::easeNone)))));
}

}  // namespace

TEST(ComposeCache, AHeldKeyframeSegmentDoesNotRepaint) {
  Host host;
  host.composer.render(gatedRing(Cache::Auto));
  host.frame();
  // Warm past the release: after enough stable paints the volatility flag
  // releases and the tree re-records ONCE, on the settling frame. The steady
  // state after that is the zero this test pins — measuring before the
  // release would count the settling record and prove nothing.
  for (int i = 0; i < 26; ++i)
    host.frame(1.0 / 60.0);  // t ~ 0.43 s: deep in the hold, post-release
  unsigned duringHold = 0;
  for (int i = 0; i < 8; ++i) {  // 0.43 -> 0.57 s, still flat
    host.frame(1.0 / 60.0);
    duringHold += host.composer.stats().picturesRecorded;
  }
  EXPECT_EQ(duringHold, 0u)
      << "re-recorded " << duringHold
      << " times across a keyframe segment whose value never changed";
  unsigned afterHold = 0;
  for (int i = 0; i < 12; ++i) {  // 0.55 -> 0.75 s: moving again
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
  for (int i = 0; i < 60; ++i) {  // a full second, over all three waypoints
    memo.frame(1.0 / 60.0);
    truth.frame(1.0 / 60.0);
    ASSERT_TRUE(identicalPixels(memo, truth, 200, 200))
        << "frame " << i << " (t = " << (double)i / 60.0
        << " s) differs from the uncached render";
  }
}

namespace {

/** @p count identical 100x100 Cache::Group leaves at (@p x, @p y), under a
 *  Cache::None wrapper so every one is painted — and its value memo run —
 *  each frame. */
Element groupField(int count, float x, float y, Cache mode = Cache::Group) {
  Element root = box().cache(Cache::None);
  for (int i = 0; i < count; ++i)
    root.child(
        box().absolute().left(x).top(y).width(100).height(100).cache(mode).fill(
            Fill::color({0.2f, 0.5f, 0.8f, 1})));
  return root;
}

/** Frame past the settle: a group's first frame seeds its value memo, its
 *  second takes the bake, the rest prove the bake holds. */
void settleGroups(Host& host) {
  for (int i = 0; i < 8; ++i) host.frame();
}

}  // namespace

TEST(ComposeCache, AGroupBakeChargesOnlyTheNodesOwnArea) {
  // 150 settled 100x100 groups charge 150 bakes of their own area — far
  // inside the bake budget, so every one of them holds a texture. A bake
  // rect that reached the canvas origin on either axis would charge that
  // whole span per node instead, exhaust the budget partway through the
  // paint order, and every node after that point would fail affordability
  // and paint live. Three placements, each on a canvas long in exactly the
  // axis it pins, so a rect grown to left = 0 and one grown to top = 0
  // are each caught alone as well as together.
  struct Placement {
    int w, h;    // canvas
    float x, y;  // the node, away from the origin toward the far corner
  };
  const Placement placements[] = {
      {800, 600, 700, 500},  // both axes at once
      {4000, 100, 3900, 0},  // the LEFT side alone
      {100, 4000, 0, 3900},  // the TOP side alone
  };
  for (const Placement& p : placements) {
    Host host(p.w, p.h);
    host.composer.render(groupField(150, p.x, p.y));
    settleGroups(host);
    EXPECT_EQ(host.composer.stats().texturesLive, 150u)
        << "on a " << p.w << "x" << p.h << " canvas, 100x100 groups at (" << p.x
        << ", " << p.y << ") did not all hold their bakes — a group "
        << "away from the origin is charging more than its own area "
        << "against the bake budget, and the overcharge evicted the rest";
    host.frame();
    EXPECT_EQ(host.composer.stats().texturesBaked, 0u)
        << "a settled group re-baked instead of blitting what it already "
           "had — the bake rect is not holding still";
  }
}

TEST(ComposeCache, AGroupBakeAtTheCanvasOriginStillBakes) {
  // The corner where the node's device bounds and the clip share a corner:
  // the intersection IS the node's rect, and it must keep baking. Pinned so
  // the placement tests above can never pass for the wrong reason (a group
  // machinery that stopped baking entirely would fail here first, loudly
  // and by name).
  Host host(800, 600);
  host.composer.setProfiling(true);
  Element root = box()
                     .cache(Cache::None)
                     .child(box()
                                .absolute()
                                .left(0)
                                .top(0)
                                .width(100)
                                .height(100)
                                .cache(Cache::Group)
                                .key("corner")
                                .fill(Fill::color({0.2f, 0.5f, 0.8f, 1})));
  host.composer.render(root);
  settleGroups(host);
  EXPECT_EQ(host.composer.stats().texturesLive, 1u);
  const Composer::NodeCost* row = requireRow(host.composer, "corner");
  ASSERT_NE(row, nullptr);
  EXPECT_EQ(row->cacheState, Composer::CacheState::Group)
      << "the group at the canvas origin is not blitting its bake";
}

TEST(ComposeCache, AGroupBakeAwayFromTheOriginChangesNoPixels) {
  // The bake rect governs allocation and budget accounting, never
  // placement: the blit draws the texture at the rect's own origin. So a
  // group anywhere on the canvas must produce byte-identical output to the
  // same subtree painted live every frame — including on the frame the
  // bake is first taken and on every blit after it.
  const SkPoint positions[] = {{0, 0}, {400, 400}, {700, 500}};
  for (const SkPoint& at : positions) {
    Host baked(800, 600), live(800, 600);
    baked.composer.render(groupField(1, at.x(), at.y(), Cache::Group));
    live.composer.render(groupField(1, at.x(), at.y(), Cache::None));
    for (int i = 0; i < 8; ++i) {
      baked.frame();
      live.frame();
      ASSERT_TRUE(identicalPixels(baked, live, 800, 600))
          << "frame " << i << ": the group bake at (" << at.x() << ", "
          << at.y() << ") drew different pixels than the live paint";
    }
  }
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
  // The kit shaper is the same cut for any brush pipeline: a closed
  // 100x100 polyline square chamfered at 30 becomes the octagon
  // geometry::shapers::chamfered() draws — 8 vertices, corners cut.
  SkPathBuilder sq;
  sq.moveTo(0, 0).lineTo(100, 0).lineTo(100, 100).lineTo(0, 100).close();
  const SkPath oct = geometry::shapers::chamfered(30).shape(sq.detach());
  PathDump o = dumpPath(oct);
  EXPECT_EQ(o.closes, 1);
  // 8 unique vertices (the iterator's synthesized closing line repeats
  // the start point, so the raw dump reads 9 with front == back).
  ASSERT_GE(o.pts.size(), 2u);
  EXPECT_EQ(o.pts.size() - (o.pts.front() == o.pts.back() ? 1 : 0), 8u);
  EXPECT_FALSE(oct.contains(2, 2));  // corner cut away
  EXPECT_TRUE(oct.contains(50, 50));
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
  Decoration wire = lines::cased(3, Fill::color({1, 1, 1, 1}), 10);
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

namespace {

/** Ink of a ring of type, described in polar terms about `centre`: how much
 *  of it there is, how far off the ring it strays, and where round the ring
 *  it sits. */
struct RingInk {
  int count = 0;
  float minRadius = 1e9f, maxRadius = 0;
  float centroidAngle = 0;  // radians, atan2 of the ink's mean position
};

RingInk ringInk(Host& host, int size, SkPoint centre) {
  RingInk ink;
  double sumX = 0, sumY = 0;
  for (int y = 0; y < size; ++y)
    for (int x = 0; x < size; ++x) {
      if (host.pixel(x, y) == SK_ColorBLACK) continue;
      const float dx = (float)x + 0.5f - centre.x();
      const float dy = (float)y + 0.5f - centre.y();
      const float radius = std::hypot(dx, dy);
      ink.minRadius = std::min(ink.minRadius, radius);
      ink.maxRadius = std::max(ink.maxRadius, radius);
      sumX += dx;
      sumY += dy;
      ++ink.count;
    }
  if (ink.count) ink.centroidAngle = (float)std::atan2(sumY, sumX);
  return ink;
}

/** Smallest turn from `a` to `b`, in radians. */
float angleGap(float a, float b) {
  float d =
      std::fmod(b - a + 3.0f * (float)M_PI, 2.0f * (float)M_PI) - (float)M_PI;
  return std::abs(d);
}

}  // namespace

TEST(ComposeTextPath, ABoundPhaseWalksTheRunRoundAClosedBaseline) {
  // The marquee. A bound phase is PAINT-ONLY motion: the run is shaped and
  // broken across the baseline once, and stepping the output re-places the
  // glyphs it already placed. Nothing is re-described between these frames.
  choreograph::Output<float> phase{0.0f};
  Host host(240, 240);
  host.composer.render(
      box().child(text(u8"MARQUEE", whiteStyle(20))
                      .key("ring")
                      .width(240)
                      .height(240)
                      .absolute()
                      .left(0)
                      .top(0)
                      .onPath({.path = geometry::shapes::circle(),
                               .at = &phase,
                               .align = TextPath::Align::Center})));
  host.frame();
  const SkPoint centre{120, 120};
  const RingInk atZero = ringInk(host, 240, centre);
  ASSERT_GT(atZero.count, 200);

  phase = 0.25f;
  host.frame();  // no render(): the phase is read at PAINT
  const RingInk atQuarter = ringInk(host, 240, centre);

  // The run travelled: a quarter turn is most of a right angle, and no
  // sampling of the same glyphs could produce that by accident.
  EXPECT_GT(angleGap(atZero.centroidAngle, atQuarter.centroidAngle), 1.0f);
  // …and it stayed ON the ring, with all of itself: a run that fell off the
  // baseline would spread its radii, and one clipped at the seam would lose
  // ink.
  EXPECT_NEAR(atQuarter.minRadius, atZero.minRadius, 6.0f);
  EXPECT_NEAR(atQuarter.maxRadius, atZero.maxRadius, 6.0f);
  EXPECT_NEAR((float)atQuarter.count, (float)atZero.count,
              (float)atZero.count * 0.25f);
}

TEST(ComposeTextPath, ThePhaseWrapsAcrossTheSeamWithNothingLost) {
  // 0.9 → 0.1 crosses fraction 1, which on a closed baseline is the same
  // point as fraction 0. The run must walk through it, not fall off it.
  choreograph::Output<float> phase{0.9f};
  Host host(240, 240);
  host.composer.render(
      box().child(text(u8"SEAMLESS", whiteStyle(20))
                      .key("ring")
                      .width(240)
                      .height(240)
                      .absolute()
                      .left(0)
                      .top(0)
                      .onPath({.path = geometry::shapes::circle(),
                               .at = &phase,
                               .align = TextPath::Align::Center})));
  host.frame();
  const SkPoint centre{120, 120};
  const RingInk before = ringInk(host, 240, centre);
  ASSERT_GT(before.count, 200);

  phase = 0.1f;
  host.frame();
  const RingInk after = ringInk(host, 240, centre);
  EXPECT_NEAR((float)after.count, (float)before.count,
              (float)before.count * 0.25f)
      << "glyphs were dropped at the seam";
  EXPECT_NEAR(after.minRadius, before.minRadius, 6.0f);
  EXPECT_NEAR(after.maxRadius, before.maxRadius, 6.0f);
  EXPECT_GT(angleGap(before.centroidAngle, after.centroidAngle), 0.5f);
}

TEST(ComposeTextPath, ASettledPhaseStopsPaintingLiveAndCaches) {
  // The declared-volatility contract, on the marquee's lane: a phase that
  // is held still long enough releases, and the node stops declaring
  // content volatility. Driving it again re-declares in the same frame.
  choreograph::Output<float> phase{0.0f};
  Host host(240, 240);
  host.composer.render(box().child(
      text(u8"HELD", whiteStyle(20))
          .key("ring")
          .width(240)
          .height(240)
          .absolute()
          .left(0)
          .top(0)
          .onPath({.path = geometry::shapes::circle(), .at = &phase})));
  for (int frame = 0; frame < 20; ++frame) host.frame();
  EXPECT_FALSE(host.composer.dirty())
      << "a phase that never moves keeps repainting";

  phase = 0.3f;
  host.frame();
  const RingInk moved = ringInk(host, 240, {120, 120});
  EXPECT_GT(moved.count, 200) << "the driven phase did not re-place the run";
}

TEST(ComposeTextPath, ATrackDeviatesInTheBaselinesOwnFrame) {
  // THE COMPOSITION ORDER, pinned: the baseline places the glyph, then the
  // track deviates from that placement IN THE FRAME THE BASELINE PUT IT IN.
  //
  // The baseline here runs straight DOWN the canvas, so its local "up" —
  // the direction fx::rise lifts from — points to canvas +x. The control is
  // the same track on the same text with no baseline, where local up is
  // canvas up. Different directions from one description is the whole
  // claim.
  auto downward = [](SkSize) {
    SkPathBuilder b;
    b.moveTo(100, 20).lineTo(100, 180);
    return b.detach();
  };
  // A bare offset, so the assertion is about DIRECTION and nothing else —
  // a preset that also fades would cull the glyphs it is being asked about.
  const TextEffect lift =
      fx::effect("test.pathframe.lift",
                 [](const GlyphInfo&, float t, core::noise::Mix64Stream&) {
                   GlyphMod mod;
                   mod.dy = -40.0f * (1.0f - t);
                   return mod;
                 });
  auto scene = [&](bool onPath, float progress) {
    Element t = text(u8"LIFT", whiteStyle(18))
                    .key("t")
                    .width(200)
                    .height(200)
                    .absolute()
                    .left(0)
                    // Room above the run for the control's lift to land in:
                    // ink clipped off the top would move the centroid for a
                    // reason that has nothing to do with the frame.
                    .top(onPath ? 0.0f : 90.0f)
                    .fx({.effect = lift, .progress = progress});
    if (onPath) t.onPath({.path = downward});
    return box().child(std::move(t));
  };
  auto inkCentroid = [](Host& host) {
    double sumX = 0, sumY = 0;
    int count = 0;
    for (int y = 0; y < 200; ++y)
      for (int x = 0; x < 200; ++x)
        if (host.pixel(x, y) != SK_ColorBLACK) {
          sumX += x;
          sumY += y;
          ++count;
        }
    return count ? SkPoint{(float)(sumX / count), (float)(sumY / count)}
                 : SkPoint{0, 0};
  };

  // On the path: at rest, then lifted. The lift moves the run along canvas
  // +x, because that is the baseline's own "up".
  Host rested(200, 200), lifted(200, 200);
  rested.composer.render(scene(true, 1.0f));
  rested.frame();
  lifted.composer.render(scene(true, 0.0f));
  lifted.frame();
  const SkPoint atRest = inkCentroid(rested);
  const SkPoint inFlight = inkCentroid(lifted);
  ASSERT_GT(atRest.x(), 0);
  ASSERT_GT(inFlight.x(), 0);
  EXPECT_GT(inFlight.x() - atRest.x(), 12.0f)
      << "the rise did not follow the baseline's own frame";
  EXPECT_LT(std::abs(inFlight.y() - atRest.y()), 12.0f);

  // The control: no baseline, so the same track lifts up the canvas.
  Host flatRest(200, 200), flatLift(200, 200);
  flatRest.composer.render(scene(false, 1.0f));
  flatRest.frame();
  flatLift.composer.render(scene(false, 0.0f));
  flatLift.frame();
  const SkPoint flatAtRest = inkCentroid(flatRest);
  const SkPoint flatInFlight = inkCentroid(flatLift);
  ASSERT_GT(flatAtRest.y(), 0);
  EXPECT_GT(flatAtRest.y() - flatInFlight.y(), 12.0f);
  EXPECT_LT(std::abs(flatInFlight.x() - flatAtRest.x()), 12.0f);
}

TEST(ComposeTextPath, ATrackAndABaselineBothRunRatherThanOneWinning) {
  // Neither seam wins the other: a ring carrying a track is still a ring,
  // and the track still deviates from where the ring put each glyph.
  Host plain(240, 240), tracked(240, 240);
  auto ring = [](bool withTrack) {
    // A ring well inside the frame: a track that throws glyphs OUTWARD off
    // a ring already touching the edges would be measuring the clip.
    Element t = text(u8"BOTH RUN", whiteStyle(18))
                    .key("ring")
                    .width(160)
                    .height(160)
                    .absolute()
                    .left(40)
                    .top(40)
                    .onPath({.path = geometry::shapes::circle(),
                             .align = TextPath::Align::Center});
    if (withTrack)
      t.fx({.effect = fx::effect(
                "test.pathframe.out",
                [](const GlyphInfo&, float, core::noise::Mix64Stream&) {
                  GlyphMod mod;
                  mod.dy = -22.0f;
                  return mod;
                }),
            .progress = 1.0f});
    return box().child(std::move(t));
  };
  plain.composer.render(ring(false));
  plain.frame();
  tracked.composer.render(ring(true));
  tracked.frame();
  const SkPoint centre{120, 120};
  const RingInk resting = ringInk(plain, 240, centre);
  const RingInk lifted = ringInk(tracked, 240, centre);
  ASSERT_GT(resting.count, 200);
  ASSERT_GT(lifted.count, 200);
  // Still on a ring — same angular place, so the baseline still placed it…
  EXPECT_LT(angleGap(resting.centroidAngle, lifted.centroidAngle), 0.4f);
  // …and off it by the track's own distance, so the track ran too.
  EXPECT_GT(std::abs(lifted.maxRadius - resting.maxRadius), 8.0f);
}
