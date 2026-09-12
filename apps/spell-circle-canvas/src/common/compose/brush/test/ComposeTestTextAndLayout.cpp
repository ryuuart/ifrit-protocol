// What a node carries beside its stroke: the layout insets and the measured
// box, the transform pivot, the entrance ramps, the clip a decoration is
// spared, and the text a run sets along a baseline of its own.

#include <sigilcompose/kit/Marquee.h>

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
  const SkSize size = intrinsicSize(box()
                                        .row()
                                        .gap(10)
                                        .child(box().width(40).height(30))
                                        .child(box().width(40).height(20)),
                                    fonts());
  EXPECT_EQ(size, SkSize::Make(90, 30));
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
      box().child(text(u8"II", whiteStyle(30)).width(Dimension(300.0f))));
  center.composer.render(
      box().child(text(u8"II", whiteStyle(30))
                      .width(Dimension(300.0f))
                      .textAlign(sigil::weave::TextAlignment::kCenter)));
  start.frame();
  center.frame();
  const int startX = leftmostLit(start), centerX = leftmostLit(center);
  ASSERT_LT(startX, 400);  // both actually painted
  ASSERT_LT(centerX, 400);
  EXPECT_GT(centerX, startX + 60);  // centered glyphs sit near mid-box
}

TEST(ComposeKitMarquee, TwoCopiesSlideUnderOneClip) {
  Host host(200, 60);
  choreograph::Output<float> phase{0.0f};
  host.composer.render(box().padding(10).child(
      kit::marquee(box().width(60).height(20).fill(red()), {.phase = &phase})
          .width(Dimension(100.0f))
          .height(Dimension(20.0f))));
  host.frame();
  EXPECT_EQ(host.pixel(60, 20), SK_ColorRED);   // first copy
  EXPECT_EQ(host.pixel(105, 20), SK_ColorRED);  // second copy (65..130 → clip)
  phase = -30.0f;                               // slide — no render()
  host.frame();
  EXPECT_EQ(host.pixel(85, 20), SK_ColorRED);     // second copy now 30..90
  EXPECT_EQ(host.pixel(105, 20), SK_ColorBLACK);  // past both copies, clipped
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
  // GLYPH: a ring of labels is hundreds of each. onPath shapes the run
  // ONCE and places every glyph by arc length.
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
// Delay staggers, unclipped decorations, px origins and centerAt.
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
  // The ring sits 30 px inside the host on every side: glyphs on a closed
  // baseline stand outward of it, so a ring that touched the host's edge
  // would have its ink cut off there and the count would read the cut.
  // Seven letters of the instrument face at 20 px are seven bars of 8 by
  // 14 px, so the ink is at least their area less what turning them
  // spreads into partial pixels.
  constexpr int kHost = 300;
  constexpr int kRing = 240;
  constexpr int kInkArea = 7 * 8 * 14;
  choreograph::Output<float> phase{0.0f};
  Host host(kHost, kHost);
  host.composer.render(
      box().child(text(u8"MARQUEE", whiteStyle(20))
                      .key("ring")
                      .width(kRing)
                      .height(kRing)
                      .absolute()
                      .left((kHost - kRing) / 2)
                      .top((kHost - kRing) / 2)
                      .onPath({.path = geometry::shapes::circle(),
                               .at = &phase,
                               .align = TextPath::Align::Center})));
  host.frame();
  const SkPoint centre{kHost / 2.0f, kHost / 2.0f};
  const RingInk atZero = ringInk(host, kHost, centre);
  ASSERT_GT(atZero.count, kInkArea / 2);

  phase = 0.25f;
  host.frame();  // no render(): the phase is read at PAINT
  const RingInk atQuarter = ringInk(host, kHost, centre);

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
                   GlyphModifier mod;
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
                  GlyphModifier mod;
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
