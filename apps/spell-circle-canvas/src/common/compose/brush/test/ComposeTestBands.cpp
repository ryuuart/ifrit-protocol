// The band a stroke is painted into: the profile and formation that shape
// it, the ribbon that dresses one, and the width profile that varies it
// along its own spine.

#include "support/BrushTestSupport.h"

namespace {

/** The boundary of a plain box, as the painter builds it. */
std::function<SkPath(SkSize)> rectSpine() {
  return [](SkSize s) {
    SkPathBuilder b;
    b.addRect(SkRect::MakeWH(s.width(), s.height()));
    return b.detach();
  };
}

}  // namespace

TEST(ComposeBand, ProfilesAreComparableAndReflexive) {
  // The seam REQUIRES equality (std::equality_comparable in
  // ProfileScheme), and a value that does not compare equal to itself
  // makes every description containing it patch forever — including the
  // default-constructed one, which is why the empty case is asserted.
  EXPECT_TRUE(geometry::path::profile::offset(4) ==
              geometry::path::profile::offset(4));
  EXPECT_FALSE(geometry::path::profile::offset(4) ==
               geometry::path::profile::offset(5));
  EXPECT_FALSE(geometry::path::profile::offset(0) ==
               geometry::path::profile::self());
  EXPECT_TRUE(geometry::path::profile::self() ==
              geometry::path::profile::self());
  EXPECT_TRUE(geometry::path::Profile() == geometry::path::Profile())
      << "two empty profiles are one nothing";
  EXPECT_FALSE(geometry::path::Profile() == geometry::path::profile::self());
  EXPECT_TRUE(across(6) == across(6));
  EXPECT_FALSE(across(6) == across(7));
}

TEST(ComposeBand, FormationsTakeTheDeclaredSide) {
  auto draw = [](geometry::path::Formation f) {
    Host host(200, 200);
    Element b =
        band(rectSpine(), across(10)).rect(SkRect::MakeXYWH(20, 20, 100, 100));
    if (f == geometry::path::Formation::Outward)
      b.outward();
    else if (f == geometry::path::Formation::Inward)
      b.inward();
    else
      b.centered();
    host.composer.render(stack().child(b.fill(red())));
    host.frame();
    return std::pair<SkColor, SkColor>{host.pixel(70, 16), host.pixel(70, 24)};
  };
  const auto centred = draw(geometry::path::Formation::Centered);
  EXPECT_EQ(centred.first, SK_ColorRED) << "centered straddles the spine";
  EXPECT_EQ(centred.second, SK_ColorRED);
  const auto out = draw(geometry::path::Formation::Outward);
  EXPECT_EQ(out.first, SK_ColorRED);
  EXPECT_EQ(out.second, SK_ColorBLACK);
  const auto in = draw(geometry::path::Formation::Inward);
  EXPECT_EQ(in.first, SK_ColorBLACK);
  EXPECT_EQ(in.second, SK_ColorRED);
}

TEST(ComposeBand, MultiContourSpinesDoNotBridge) {
  // The rails must be zipped and closed PER CONTOUR. Build one moveTo/lineTo
  // chain across every contour and close it once, and the gap between
  // contours is bridged by a chord — two concentric ring spines come out as
  // a filled disc, which looks deliberate rather than broken.
  Host host(400, 400);
  host.composer.render(
      stack().child(band(
                        [](SkSize s) {
                          SkPathBuilder b;
                          b.addCircle(s.width() * 0.5f, s.height() * 0.5f, 150);
                          b.addCircle(s.width() * 0.5f, s.height() * 0.5f, 60);
                          return b.detach();
                        },
                        across(12))
                        .inset(0)
                        .fill(red())));
  host.frame();
  EXPECT_EQ(host.pixel(200, 46), SK_ColorRED) << "the outer ring";
  EXPECT_EQ(host.pixel(200, 136), SK_ColorRED) << "the inner ring";
  // Between the two rings, and inside the inner one: paper.
  EXPECT_EQ(host.pixel(200, 90), SK_ColorBLACK)
      << "the gap between the rings was bridged";
  EXPECT_EQ(host.pixel(200, 200), SK_ColorBLACK) << "the middle was filled";
}

TEST(ComposeBand, AlongAcrossIsTheBandsOwnSpace) {
  SkPathBuilder b;
  b.moveTo(0, 50);
  b.lineTo(100, 50);
  const SkPath spine = b.detach();
  EXPECT_EQ(bandPointAt(spine, 0.0f, 0), SkPoint::Make(0, 50));
  EXPECT_EQ(bandPointAt(spine, 0.5f, 0), SkPoint::Make(50, 50));
  EXPECT_EQ(bandPointAt(spine, 1.0f, 0), SkPoint::Make(100, 50));
  // across is pixels along the normal, positive to the LEFT of travel. With
  // y down, travelling +x, a positive across therefore goes UP the screen.
  // geometry::path::parallel means the same side — there is one convention —
  // and it is asserted here so the two signs cannot drift apart.
  EXPECT_EQ(bandPointAt(spine, 0.5f, 10), SkPoint::Make(50, 40));
  EXPECT_EQ(bandPointAt(spine, 0.5f, -10), SkPoint::Make(50, 60));
}

TEST(ComposeBand, ProfileMaxKeepsTheReachOutOfTheCull) {
  // max() is REQUIRED by the seam precisely so this cannot be a silent
  // clip: a Picture-cached node's cull is grown by the profile's reach,
  // and an outward band draws entirely OUTSIDE its layout box.
  Host host(200, 200);
  host.composer.render(stack().child(band(rectSpine(), across(20))
                                         .outward()
                                         .rect(SkRect::MakeXYWH(60, 60, 40, 40))
                                         .cache(Cache::Picture)
                                         .fill(red())));
  host.frame();
  EXPECT_EQ(host.pixel(80, 45), SK_ColorRED) << "15px outside the box";
  EXPECT_EQ(host.pixel(80, 80), SK_ColorBLACK) << "and not inside it";
}

TEST(ComposeBand, StrokePassesDressABandLikeAnyShape) {
  Host host(200, 200);
  host.composer.render(
      stack().child(band(rectSpine(), across(16))
                        .rect(SkRect::MakeXYWH(30, 30, 80, 80))
                        .stroke(spans::every(1), stroke(4, green()))));
  host.frame();
  int inked = 0;
  for (int x = 0; x < 200; ++x)
    for (int y = 0; y < 200; ++y)
      if (host.pixel(x, y) == SK_ColorGREEN) ++inked;
  EXPECT_GT(inked, 100) << "a band takes a stroke pass like any shape";
}

// ---- Ribbon on the profile seam --------------------------------------------
// ----------------------------------------
TEST(ComposeRibbon, ProfileRibbonPaintsItsBand) {
  Host host(200, 200);
  brush::Ribbon r;
  r.width = geometry::path::Profile(
      geometry::path::profile::offset(16.0f));  // constant 16px wide
  r.fill = Fill::color({1, 0, 0, 1});
  host.composer.render(
      stack().child(box()
                        .rect(SkRect::MakeXYWH(40, 40, 100, 100))
                        .shape([](SkSize s) {
                          SkPathBuilder p;
                          p.moveTo(0, s.height() * 0.5f);
                          p.lineTo(s.width(), s.height() * 0.5f);
                          return p.detach();
                        })
                        .stroke(std::move(r))));
  host.frame();
  EXPECT_EQ(host.pixel(90, 90), SK_ColorRED) << "on the spine";
  EXPECT_EQ(host.pixel(90, 84), SK_ColorRED) << "6px off it, inside 16 wide";
  EXPECT_EQ(host.pixel(90, 70), SK_ColorBLACK) << "20px off it, outside";
}

namespace {

/** The linear taper, spelled as a law on the profile seam. It is exactly
 *  what `widthStart`/`widthEnd` mean, which is what lets the two
 *  constructions be compared on the same picture. */
struct TaperLaw {
  float start = 30.0f, end = 10.0f;
  float across(float along) const { return start + (end - start) * along; }
  float max() const { return std::max(start, end); }
  bool operator==(const TaperLaw&) const = default;
};

/** A law keyed in PX of arc length. A single 12 px-wide pulse `at` px from
 *  the spine's start, over a 4 px floor: its POSITION is the whole
 *  assertion, because under a reveal a fraction-keyed law drags it along. */
struct PulseAtPx {
  float at = 40.0f, wide = 12.0f, tall = 24.0f, floorPx = 4.0f;
  static constexpr bool alongIsPx = true;
  float across(float px) const {
    return std::abs(px - at) <= wide * 0.5f ? tall : floorPx;
  }
  float max() const { return std::max(tall, floorPx); }
  bool operator==(const PulseAtPx&) const = default;
};

/** The same pulse keyed in FRACTION of the spine. Kept as the contrast: the
 *  two agree with no reveal and diverge under one, which is the only way to
 *  show that the px key does something. */
struct PulseAtFraction {
  float at = 0.4f, wide = 0.12f, tall = 24.0f, floorPx = 4.0f;
  float across(float along) const {
    return std::abs(along - at) <= wide * 0.5f ? tall : floorPx;
  }
  float max() const { return std::max(tall, floorPx); }
  bool operator==(const PulseAtFraction&) const = default;
};

/** A 200 px node whose shape is a straight horizontal line at mid-height —
 *  no corners anywhere, which is the point: it is where the two
 *  constructions are supposed to agree. */
Element straightRun(brush::Ribbon r) {
  return box()
      .rect(SkRect::MakeXYWH(0, 0, 200, 200))
      .shape([](SkSize s) {
        SkPathBuilder p;
        p.moveTo(20, s.height() * 0.5f);
        p.lineTo(s.width() - 20, s.height() * 0.5f);
        return p.detach();
      })
      .stroke(std::move(r));
}

/** Lit rows in column x — the painted band's thickness, measured. */
int thicknessAt(Host& host, int x) {
  int lit = 0;
  for (int y = 0; y < 200; ++y)
    if (host.pixel(x, y) != SK_ColorBLACK) ++lit;
  return lit;
}

/** The lit rows' centre in column x, or -1. */
float centreAt(Host& host, int x) {
  int lo = -1, hi = -1;
  for (int y = 0; y < 200; ++y)
    if (host.pixel(x, y) != SK_ColorBLACK) {
      if (lo < 0) lo = y;
      hi = y;
    }
  return lo < 0 ? -1.0f : 0.5f * (float)(lo + hi);
}

}  // namespace

TEST(ComposeWidthProfile, StraightRunsAgreeWithTheLaneTheyReplaced) {
  // AWAY FROM CORNERS the profile construction and the sample-and-zip
  // construction must draw the same band. The taper widthStart=30 →
  // widthEnd=10 is one law spelled both ways, so any difference on a
  // corner-free spine is construction rather than intent — and the spine
  // used here is a straight line for exactly that reason.
  auto measure = [](bool profiled) {
    Host host(200, 200);
    brush::Ribbon r;
    r.fill = Fill::color({1, 0, 0, 1});
    r.step = 2.0f;
    if (profiled)
      r.width = geometry::path::Profile(TaperLaw{30.0f, 10.0f});
    else {
      r.widthStart = 30.0f;
      r.widthEnd = 10.0f;
    }
    host.composer.render(stack().child(straightRun(std::move(r))));
    host.frame();
    std::vector<std::pair<int, float>> out;
    for (int x : {40, 70, 100, 130, 160})
      out.emplace_back(thicknessAt(host, x), centreAt(host, x));
    return out;
  };
  const auto zipped = measure(false);
  const auto profiled = measure(true);
  ASSERT_EQ(zipped.size(), profiled.size());
  for (size_t i = 0; i < zipped.size(); ++i) {
    EXPECT_LE(std::abs(zipped[i].first - profiled[i].first), 1)
        << "band thickness at sample " << i << ": zip " << zipped[i].first
        << " vs profile " << profiled[i].first
        << " — a straight run must not change width by construction";
    EXPECT_NEAR(zipped[i].second, profiled[i].second, 0.6f)
        << "the band's centreline moved at sample " << i;
  }
}

TEST(ComposeWidthProfile, APxKeyedLawStaysPutUnderAReveal) {
  // Why `alongIsPx` belongs on the seam and not in each caller. A decoration
  // under a reveal is handed the REVEALED contour, so `along` as a fraction
  // is a fraction of what has been drawn SO FAR. An author cannot correct
  // for that by dividing by a length they measured, because the length being
  // sampled is not the length they authored — only the paint-time consumer
  // knows it. Declaring `alongIsPx` makes `across` take arc-length pixels
  // from the spine's start, which does not move.
  //
  // A pulse is the right probe because its POSITION is what slides; a taper
  // would look plausible either way.
  auto pulseX = [](bool pxKeyed, float reveal) {
    Host host(200, 200);
    brush::Ribbon r;
    r.fill = Fill::color({1, 0, 0, 1});
    if (pxKeyed)
      r.width = geometry::path::Profile(PulseAtPx{});
    else
      r.width = geometry::path::Profile(PulseAtFraction{});
    // spans::upTo is the reveal; at 1.0 the whole spine is handed over.
    Element revealed = box()
                           .rect(SkRect::MakeXYWH(0, 0, 200, 200))
                           .shape([](SkSize s) {
                             SkPathBuilder p;
                             p.moveTo(20, s.height() * 0.5f);
                             p.lineTo(s.width() - 20, s.height() * 0.5f);
                             return p.detach();
                           })
                           .stroke(spans::upTo(reveal), std::move(r));
    host.composer.render(stack().child(std::move(revealed)));
    host.frame();
    // the pulse is the widest column
    int best = -1, bestT = 0;
    for (int x = 21; x < 179; ++x) {
      const int t = thicknessAt(host, x);
      if (t > bestT) {
        bestT = t;
        best = x;
      }
    }
    return std::pair<int, int>{best, bestT};
  };

  // The two laws are deliberately NOT the same law — 40 px of a 160 px run
  // is 0.25 while the fraction law's pulse sits at 0.4 — so what is compared
  // is each one against ITSELF as the reveal grows, not one against the
  // other.
  const auto pxFull = pulseX(true, 1.0f);
  const auto pxHalf = pulseX(true, 0.55f);
  ASSERT_GT(pxFull.second, 0);
  ASSERT_GT(pxHalf.second, 0);
  EXPECT_NEAR(pxFull.first, pxHalf.first, 2)
      << "a px-keyed pulse must sit at the same place at any reveal: full "
      << pxFull.first << " vs half " << pxHalf.first;

  const auto frFull = pulseX(false, 1.0f);
  const auto frHalf = pulseX(false, 0.55f);
  ASSERT_GT(frFull.second, 0);
  ASSERT_GT(frHalf.second, 0);
  EXPECT_GT(std::abs(frFull.first - frHalf.first), 8)
      << "…and a fraction-keyed one demonstrably SLIDES, which is the whole "
         "reason the px key exists (full "
      << frFull.first << " vs half " << frHalf.first << ")";
}

TEST(ComposeWidthProfile, ARibbonUnderAWidthProfilePrunes) {
  // A varying-width ribbon must be comparable. Carry the width as a callable
  // and the ribbon is unequal to ITSELF, so its whole band re-records on
  // every describe; carry it as a Profile over a plain struct law and two
  // identical descriptions compare equal and the node prunes.
  brush::Ribbon a;
  a.fill = Fill::color({1, 0, 0, 1});
  a.width = geometry::path::Profile(PulseAtPx{});
  brush::Ribbon b = a;
  EXPECT_TRUE(a == b) << "identical laws must compare equal — the prune";
  b.width = geometry::path::Profile(PulseAtPx{.at = 41.0f});
  EXPECT_FALSE(a == b) << "…and a different law must NOT, or it reads stale";

  // The px key is part of the value's TYPE, so two laws that differ only in
  // how they are keyed can never silently compare equal.
  brush::Ribbon c = a;
  c.width = geometry::path::Profile(PulseAtFraction{});
  EXPECT_FALSE(a == c);
  EXPECT_TRUE(geometry::path::Profile(PulseAtPx{}).keyedInPx());
  EXPECT_FALSE(geometry::path::Profile(PulseAtFraction{}).keyedInPx());

  // max() is honoured whichever key it is: the cull grows to the law's own
  // declared reach and nothing has to be told twice.
  EXPECT_FLOAT_EQ(a.bleed(), 24.0f);
  EXPECT_FLOAT_EQ(geometry::path::Profile(PulseAtPx{.tall = 90.0f}).max(),
                  90.0f);
  // acrossAt is the consumer's call: a px law is evaluated at along*length,
  // a fraction law ignores the length entirely.
  EXPECT_FLOAT_EQ(geometry::path::Profile(PulseAtPx{}).acrossAt(0.25f, 160.0f),
                  24.0f);
  EXPECT_FLOAT_EQ(geometry::path::Profile(PulseAtPx{}).acrossAt(0.25f, 320.0f),
                  4.0f);
  EXPECT_FLOAT_EQ(
      geometry::path::Profile(PulseAtFraction{}).acrossAt(0.4f, 160.0f), 24.0f);

  // And the prune OBSERVED, not inferred: an identical re-describe of a
  // profiled ribbon must record NOTHING. This is the absolute form rather
  // than "no more than the first draw", which is only available because
  // `.shape()` is itself a comparable value — an incomparable outline here
  // would force a re-patch and weaken the claim to nothing.
  {
    Host host;
    auto tree = [] {
      brush::Ribbon r;
      r.fill = Fill::color({1, 0, 0, 1});
      r.width = geometry::path::Profile(PulseAtPx{});
      return box().child(box()
                             .width(120)
                             .height(120)
                             .shape(geometry::shapes::circle())
                             .stroke(r));
    };
    host.composer.render(tree());
    host.frame();
    host.composer.render(tree());
    host.frame();
    EXPECT_EQ(host.composer.stats().picturesRecorded, 0u)
        << "an identical profiled ribbon re-recorded — the prune is not real";
  }
  EXPECT_FLOAT_EQ(
      geometry::path::Profile(PulseAtFraction{}).acrossAt(0.4f, 999.0f), 24.0f);
}

namespace {

/** A law that is NaN over one short window — astral_tome's
 *  `0.40 + 0.60·sqrt(sin(π·along))` in miniature, where float rounding
 *  made sin(π·1.0f) dip to −8.7e-08 and sqrt of it NaN. */
struct NanAtMidLaw {
  float across(float along) const {
    return along > 0.48f && along < 0.52f ? std::sqrt(-1.0f) : 20.0f;
  }
  float max() const { return 20.0f; }
  bool operator==(const NanAtMidLaw&) const = default;
};

}  // namespace

TEST(ComposeWidthProfile, ANonFiniteSamplePinchesInsteadOfDeletingTheBand) {
  // Skia draws NONE of a path containing a single non-finite vertex, so a
  // width law that returns NaN at one sample would delete the entire band —
  // silently, with no error and nothing on screen to explain it. The guard in
  // profileOffset turns a bad sample into a LOCAL pinch to the spine so the
  // rest of the band still draws, which fails visibly and locally instead.
  const auto bandOf = [](bool poisoned) {
    Host host(200, 200);
    brush::Ribbon r;
    r.fill = Fill::color({1, 0, 0, 1});
    if (poisoned)
      r.width = geometry::path::Profile(NanAtMidLaw{});
    else
      r.width = geometry::path::Profile(TaperLaw{20.0f, 20.0f});
    host.composer.render(stack().child(straightRun(std::move(r))));
    host.frame();
    std::vector<int> t;
    for (int x : {40, 70, 100, 130, 160}) t.push_back(thicknessAt(host, x));
    return t;
  };
  const std::vector<int> finite = bandOf(false);
  const std::vector<int> nan = bandOf(true);
  ASSERT_EQ(finite.size(), nan.size());
  // Away from the poisoned window the two bands agree — the rest of the
  // band DRAWS.
  for (size_t i = 0; i < finite.size(); ++i) {
    if (i == 2) continue;  // the poisoned column
    EXPECT_GT(finite[i], 10) << "the control band is missing at sample " << i;
    EXPECT_LE(std::abs(finite[i] - nan[i]), 2)
        << "the NaN law changed the band away from its own bad sample (" << i
        << ")";
  }
  // At the window the band pinches toward the spine rather than filling.
  EXPECT_LT(nan[2], finite[2])
      << "the NaN sample did not pinch — is the guard resolving it to a "
         "full-width value?";
}

TEST(ComposeRibbon, ARecipeCanPaintTheBandAndALiveOneDeclaresItself) {
  // A BAND IS A SURFACE, and a surface a material can dress — the same
  // door a stroke opens with strokeMaterial. Without it a ribbon beside a
  // stroked outline has to have the same paint written twice, once as a
  // Material in the unit square and once as a node-local Fill.
  Host host;
  brush::Ribbon painted =
      brush::presets::taper(24, 24, material::skia::Paint::solid({0, 1, 0, 1}));
  ASSERT_TRUE(painted.fillMaterial.has_value());
  host.composer.render(straightRun(std::move(painted)));
  host.frame();
  EXPECT_EQ(host.pixel(100, 100), SK_ColorGREEN)
      << "the recipe never reached the band";

  // …and a LIVE one declares itself, so the node repaints every frame
  // with no re-describe — the same rule a live stroke material follows.
  const auto paintedPerFrame = [](bool live) {
    Host again;
    again.composer.render(straightRun(brush::presets::taper(
        24, 24, material::skia::Paint::sksl(heavyEffect(live)))));
    again.frame();
    again.frame();
    return again.composer.stats().nodesPainted;
  };
  EXPECT_GT(paintedPerFrame(true), 0u)
      << "a live band material must declare isAnimated()";
  EXPECT_EQ(paintedPerFrame(false), 0u) << "…and a static one must cache";
}

// ---- The ribbon's corners, and the audit that finds them ------------------
namespace {

/** A spine that turns 90° at the middle: two 60 px legs meeting at (80,80).
 *  A band wider than about half a leg is where a join-less construction
 *  loses the inside of the bend. */
SkPath elbow() {
  SkPathBuilder b;
  b.moveTo(20, 80);
  b.lineTo(80, 80);
  b.lineTo(80, 140);
  return b.detach();
}

/** A constant width on the profile seam. */
struct FlatWidth {
  float w = 40.0f;
  float across(float) const { return w; }
  float max() const { return w; }
  bool operator==(const FlatWidth&) const = default;
};

}  // namespace

TEST(ComposeRibbon, TheInsideOfATightBendIsFilled) {
  // A band is the UNION of its cross-sections, and the union is what the
  // construction has to produce. Zipped into one left-forward, right-back
  // contour the inner rail crosses itself, the crossing winds the wrong
  // way, and the winding fill drops the wedge on the inside of the bend —
  // a hole wider than the band, at every corner a wide band turns on.
  brush::Ribbon r;
  r.width = geometry::path::Profile(FlatWidth{40.0f});
  const SkPath band = r.band(elbow());

  // The wedge on the inside of the bend is where the two legs' widths
  // OVERLAP — x in [60,80], y in [80,100] here — and it is exactly the
  // region a contour that crossed itself would cancel away.
  EXPECT_TRUE(band.contains(70, 90)) << "the inside of the bend";
  EXPECT_TRUE(band.contains(62, 82)) << "…out to the corner of the overlap";
  // …and the band is still a band: 40 wide means 20 either side.
  EXPECT_TRUE(band.contains(50, 95));
  EXPECT_FALSE(band.contains(50, 105)) << "25px off a 40-wide band";
}

TEST(ComposeRibbon, TheJoinShapesTheOutsideOfTheCorner) {
  // Outside the turn the three joins differ by construction, and the
  // difference is exactly what each word means: the chord, the arc, the
  // point. The corner is at (80,80) and the outer side is down-right.
  brush::Ribbon bevel;
  bevel.width = geometry::path::Profile(FlatWidth{40.0f});
  brush::Ribbon round = bevel;
  round.join = SkPaint::kRound_Join;
  brush::Ribbon miter = bevel;
  miter.join = SkPaint::kMiter_Join;

  // Outside a right turn is the far side of the elbow, up and to the
  // right of (80,80). The bevel's chord cuts that corner off at 14.1 px;
  // the arc reaches the full 20; the miter carries the two rails to their
  // meeting point at 28.3.
  const SkPoint onTheArc{92.0f, 68.0f};    // 17 px out — past the chord
  const SkPoint atThePoint{97.0f, 63.0f};  // 24 px out — past the arc
  EXPECT_FALSE(bevel.band(elbow()).contains(onTheArc.x(), onTheArc.y()));
  EXPECT_TRUE(round.band(elbow()).contains(onTheArc.x(), onTheArc.y()));
  EXPECT_TRUE(miter.band(elbow()).contains(atThePoint.x(), atThePoint.y()));
  EXPECT_FALSE(round.band(elbow()).contains(atThePoint.x(), atThePoint.y()));

  // A miter reaches past the width, which is the one join whose bleed is
  // not the width — a cull sized from the width would clip its point.
  EXPECT_FLOAT_EQ(bevel.bleed(), 40.0f);
  EXPECT_FLOAT_EQ(round.bleed(), 40.0f);
  EXPECT_FLOAT_EQ(miter.bleed(), 40.0f * 4.0f);

  // Past the limit a miter bevels, exactly as a stroke's does, so a
  // near-reversal cannot fire a spike off the end of the picture.
  brush::Ribbon tight = miter;
  tight.miterLimit = 1.05f;  // only turns gentler than ~145° may point
  EXPECT_FALSE(tight.band(elbow()).contains(atThePoint.x(), atThePoint.y()));

  // And the join is part of the value, or two ribbons differing only in
  // their corners would prune into each other.
  EXPECT_FALSE(bevel == round);
  EXPECT_FALSE(miter == tight);
}

TEST(ComposeRibbon, WidthAlongMeasuresTheBandTheRibbonDrew) {
  // The audit reads the geometry the ribbon hands back, so what is
  // measured is what was drawn rather than a transcription of how it is
  // built — which is what goes stale the moment the sampling changes.
  const geometry::path::Profile flat{FlatWidth{24.0f}};
  brush::Ribbon r;
  r.width = flat;

  SkPathBuilder straight;
  straight.moveTo(20, 100);
  straight.lineTo(180, 100);
  const SkPath spine = straight.detach();

  const test::WidthAlong audit = test::widthAlong(r.band(spine), spine, flat);
  EXPECT_GT(audit.samples, 10);
  EXPECT_TRUE(audit.within(0.5f)) << "worst " << audit.maxError << " px";
  EXPECT_LT(audit.rmsError, 0.5f);

  // Point it at a band built to a DIFFERENT law and it says so, in px,
  // and names where — which the cheap total-ink check cannot do at all,
  // because a band that loses area at one place and gains it at another
  // conserves the sum.
  brush::Ribbon thin;
  thin.width = geometry::path::Profile(FlatWidth{16.0f});
  const test::WidthAlong wrong =
      test::widthAlong(thin.band(spine), spine, flat);
  EXPECT_NEAR(wrong.maxError, 8.0f, 0.5f);
  ASSERT_FALSE(wrong.worst.empty());
  EXPECT_NEAR(wrong.worst.front().measured, 16.0f, 0.5f);
  EXPECT_NEAR(wrong.worst.front().intended, 24.0f, 0.01f);
  EXPECT_FALSE(wrong.within(1.0f));
}

TEST(ComposeRibbon, WidthAlongMeasuresATrunkOfHundredsOfOverlappingSteps) {
  // THE CASE THAT BREAKS EVERY OUTLINE-FIRST MEASUREMENT. A band is a
  // union of one quadrilateral per sampled step, and a long one is
  // hundreds of them; resolving that union into a boundary gives an
  // outline that walks in and out along every interior seam — enclosing
  // no area, and edges all the same. A cast that trusted such an outline
  // finds its shortest chord on one of those excursions and reports a
  // few pixels across a band a hundred wide, on a straight leg the same
  // band fills to within a couple of percent of the width integral.
  //
  // A wide band on a long polyline with one hard corner is that shape at
  // the smallest size that still has it.
  const geometry::path::Profile wide{FlatWidth{100.0f}};
  brush::Ribbon r;
  r.width = wide;
  r.step = 2.0f;  // ~700 steps over the run below

  SkPathBuilder trunk;
  trunk.moveTo(100, 400);
  for (int i = 1; i <= 40; ++i) trunk.lineTo(100 + 20.0f * i, 400);
  for (int i = 1; i <= 30; ++i) trunk.lineTo(900, 400 - 20.0f * i);
  const SkPath spine = trunk.detach();

  // A coarser station spacing and fewer headings than the defaults: the
  // cost is stations x headings x band edges, and this band has thousands
  // of edges by construction.
  const test::WidthAlong audit =
      test::widthAlong(r.band(spine), spine, wide, 12.0f, 45);
  EXPECT_GT(audit.samples, 60);
  // The corner itself is measured across the turn rather than across the
  // band, which is a property of the measurement; every straight station
  // must be the width the law asked for.
  EXPECT_LT(audit.rmsError, 4.0f)
      << "worst " << audit.maxError << " px at " << audit.worst.front().at.x()
      << "," << audit.worst.front().at.y();
  int badOnAStraightLeg = 0;
  for (const test::WidthStation& st : audit.worst) {
    const bool nearCorner =
        std::abs(st.at.x() - 900.0f) < 120.0f && st.at.y() > 280.0f;
    if (!nearCorner && st.error() > 4.0f) ++badOnAStraightLeg;
  }
  EXPECT_EQ(badOnAStraightLeg, 0);
}

TEST(ComposeRibbon, WidthAlongSkipsTheCapsAndSeesTheCorner) {
  // Within half a width of an end the shortest chord through a point runs
  // diagonally out through the cap rather than across the band, so the
  // margin is not a nicety: without it every audit reports its own ends as
  // the worst defect in the picture and buries whatever is really wrong.
  const geometry::path::Profile flat{FlatWidth{40.0f}};
  brush::Ribbon r;
  r.width = flat;
  const SkPath spine = elbow();
  const test::WidthAlong audit = test::widthAlong(r.band(spine), spine, flat);
  ASSERT_GT(audit.samples, 4);
  for (const test::WidthStation& s : audit.worst)
    EXPECT_GT(s.along, 20.0f) << "a cap was measured as a defect";
  // Away from the corner the joined band is the width it claims. At the
  // corner itself the shortest chord runs across the turn rather than
  // across the band, which is a property of the measurement and not of
  // the band — so the audit is read as a run, never as one number.
  EXPECT_LT(audit.rmsError, 12.0f);
}
