#include <sigilmeasure/time/Stopwatch.h>

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

/** How much of [0,1] a span set covers. */
float coverage(const std::vector<Span>& spans) {
  float total = 0;
  for (const Span& s : spans) total += s.end - s.begin;
  return total;
}

/** Do two claim sets share more than float noise? (The library's own
 *  overlap test is internal; this is the same predicate, spelled here so
 *  the test asserts the PROPERTY rather than the implementation.) */
bool disjoint(const std::vector<Span>& a, const std::vector<Span>& b) {
  for (const Span& x : a)
    for (const Span& y : b)
      if (std::min(x.end, y.end) - std::max(x.begin, y.begin) > 1e-4f)
        return false;
  return true;
}

SkPath unitBox() {
  SkPathBuilder b;
  b.addRect(SkRect::MakeWH(100, 100));
  return b.detach();
}

}  // namespace

TEST(ComposeShapeRename, ShapeOverridesTheBox) {
  // `shape()` replaces the node's rect with a generated path — for fill,
  // for stroking, and for hit testing. The rect it was given is not
  // intersected with the shape, it is discarded.
  Host host(200, 200);
  Element e = box().rect(SkRect::MakeXYWH(20, 20, 100, 100)).fill(red());
  e.shape(geometry::shapes::circle());
  host.composer.render(stack().child(std::move(e)));
  host.frame();
  EXPECT_EQ(host.pixel(70, 70), SK_ColorRED) << "inside the circle";
  EXPECT_EQ(host.pixel(24, 24), SK_ColorBLACK)
      << "the rect's corner is outside the shape";
}

TEST(ComposeSpans, CornersAndEdgesPartitionTheBoundary) {
  // The claim algebra, read directly: corners() and edges() are one scan
  // seen two ways, so together they cover the boundary exactly once and
  // overlap nowhere. That partition is what lets bracket marks and gapped
  // rules be two calls on one vocabulary rather than separate primitives.
  const SkPath boundary = unitBox();
  SpanInput in;
  in.outline = &boundary;
  const std::vector<Span> corners = spans::corners(20).resolve(in);
  const std::vector<Span> edges = spans::edges(20).resolve(in);
  // Five intervals for four corners: the corner ON the seam (fraction 0)
  // has its window split there, exactly as a seam-crossing trim window
  // is. The two pieces are adjacent, so the MEASURE is still four arms.
  EXPECT_EQ(corners.size(), 5u);
  EXPECT_NEAR(coverage(corners), 4 * 40.0f / 400.0f, 1e-3f);
  EXPECT_NEAR(coverage(corners) + coverage(edges), 1.0f, 1e-3f);
  EXPECT_TRUE(disjoint(corners, edges));
}

TEST(ComposeSpans, EveryAndAtAreTheSameLadder) {
  const SkPath boundary = unitBox();
  SpanInput in;
  in.outline = &boundary;
  EXPECT_NEAR(coverage(spans::every(4).resolve(in)), 1.0f, 1e-3f);
  EXPECT_NEAR(coverage(spans::every(4, 0.5f).resolve(in)), 0.5f, 1e-3f);
  const std::vector<Span> one = spans::at(1, 4).resolve(in);
  ASSERT_EQ(one.size(), 1u);
  EXPECT_NEAR(one[0].begin, 0.25f, 1e-4f);
  EXPECT_NEAR(one[0].end, 0.5f, 1e-4f);
  // Union is a set operation, not a list: two adjacent slots merge.
  const std::vector<Span> two = (spans::at(0, 4) | spans::at(1, 4)).resolve(in);
  ASSERT_EQ(two.size(), 1u);
  EXPECT_NEAR(two[0].end, 0.5f, 1e-4f);
}

TEST(ComposeSpans, CornerPassMarksOnlyTheCorners) {
  Host host(200, 200);
  host.composer.render(
      stack().child(box()
                        .rect(SkRect::MakeXYWH(20, 20, 100, 100))
                        .stroke(spans::corners(20), stroke(6, red()))));
  host.frame();
  EXPECT_EQ(host.pixel(30, 20), SK_ColorRED) << "10px along the top edge";
  EXPECT_EQ(host.pixel(70, 20), SK_ColorBLACK) << "the middle of a run";
  EXPECT_EQ(host.pixel(20, 30), SK_ColorRED) << "and down the left edge";
}

TEST(ComposeSpans, PassesAppendAndRestFillsTheGaps) {
  // Two calls, no arithmetic: the corners get one mark and everything else
  // gets the other. Written by hand this is a path generator that has to
  // subtract the corner windows from the rect, and it stops being correct
  // as soon as the silhouette is not a rect.
  Host host(200, 200);
  host.composer.render(
      stack().child(box()
                        .rect(SkRect::MakeXYWH(20, 20, 100, 100))
                        .stroke(spans::corners(20), stroke(6, red()))
                        .stroke(spans::rest(), stroke(6, green()))));
  host.frame();
  EXPECT_EQ(host.pixel(30, 20), SK_ColorRED);
  EXPECT_EQ(host.pixel(70, 20), SK_ColorGREEN) << "rest() took the run";
}

TEST(ComposeSpans, OverlappingClaimsAreSaidOutLoud) {
  // The no-overlap law. The message is the ONLY place an author learns
  // that layering two marks on one run is a composite brush, so it must
  // actually fire.
  ::testing::internal::CaptureStderr();
  {
    Host host(200, 200);
    host.composer.render(stack().child(
        box()
            .rect(SkRect::MakeXYWH(20, 20, 100, 100))
            .stroke(spans::every(1), stroke(4, red()), "halo")
            .stroke(spans::upTo(0.5f), stroke(2, green()), "keyline")));
    host.frame();
  }
  const std::string log = ::testing::internal::GetCapturedStderr();
  EXPECT_NE(log.find("halo"), std::string::npos) << log;
  EXPECT_NE(log.find("keyline"), std::string::npos) << log;
}

TEST(ComposeSpans, UnqualifiedStrokesOverlayAndNeverCollide) {
  // The whole-boundary form of stroke() makes no CLAIM on the boundary, so
  // stacking two of them — a halo under a keyline — stays legal and silent.
  // Only span-qualified passes take part in the no-overlap law.
  ::testing::internal::CaptureStderr();
  {
    Host host(200, 200);
    host.composer.render(
        stack().child(box()
                          .rect(SkRect::MakeXYWH(20, 20, 100, 100))
                          .stroke(stroke(8, red()))
                          .stroke(stroke(3, green()))));
    host.frame();
    EXPECT_EQ(host.pixel(70, 20), SK_ColorGREEN) << "the second stroke wins";
  }
  EXPECT_EQ(::testing::internal::GetCapturedStderr(), "");
}

TEST(ComposeSpans, ReorderedTermsPruneBecauseResolveNeverReadsOrder) {
  // `corners(8) | at(0, 4)` and `at(0, 4) | corners(8)` claim the SAME runs,
  // because resolve() unions its terms and never reads their order. So
  // Spans equality has to be order-insensitive — a multiset match over the
  // same per-term comparison — or a describe that happens to build the union
  // the other way round produces a spurious patch. Never a wrong picture,
  // only a lost prune, which is why nothing else would report it.
  const auto tree = [](Spans where) {
    return stack().child(
        box()
            .key("m")
            .rect(SkRect::MakeXYWH(20, 20, 100, 100))
            .stroke(std::move(where), stroke(4, red()), "marks"));
  };
  Host host;
  host.composer.render(tree(spans::corners(8) | spans::at(0, 4)));
  host.frame();
  host.composer.render(tree(spans::at(0, 4) | spans::corners(8)));
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "a reordered union re-patched — the prune is order-sensitive "
         "while resolve() is not";
  // The in-test control: a genuinely DIFFERENT claim must still patch, or
  // the assertion above is satisfied by an equality that says yes to
  // everything.
  host.composer.render(tree(spans::corners(12) | spans::at(0, 4)));
  EXPECT_GT(host.composer.stats().patchedNodes, 0u)
      << "a different claim pruned — a reordered describe would now freeze "
         "stale marks";
  // And duplicates count: {corners, corners} is not {corners, at} however
  // you order them.
  EXPECT_FALSE((spans::corners(8) | spans::corners(8)) ==
               (spans::corners(8) | spans::at(0, 4)));
  EXPECT_TRUE((spans::corners(8) | spans::at(0, 4)) ==
              (spans::at(0, 4) | spans::corners(8)));
}

TEST(ComposeSpans, PassRevealMatchesTheNodeGatePixelForPixel) {
  // THE SUGAR LAW, in pixels: `stroke(where, what)` and
  // `stroke(what).mask(parts::marks(), by::spans(where))` are one machine,
  // so the same numbers describe the same run through both doors.
  auto draw = [](bool useLegacyTrim) {
    Host host(200, 200);
    Element e = box().rect(SkRect::MakeXYWH(20, 20, 100, 100));
    if (useLegacyTrim)
      e.mask(by::spans(spans::upTo(0.4f))).stroke(stroke(6, red()));
    else
      e.stroke(spans::upTo(0.4f), stroke(6, red()));
    host.composer.render(stack().child(std::move(e)));
    host.frame();
    std::vector<SkColor> out;
    for (int x = 15; x < 130; x += 3) out.push_back(host.pixel(x, 20));
    for (int y = 15; y < 130; y += 3) out.push_back(host.pixel(120, y));
    return out;
  };
  const std::vector<SkColor> spanned = draw(false);
  EXPECT_EQ(spanned, draw(true));
  // …and it is a genuine partial reveal, not "everything" or "nothing":
  // 0.4 of a 400px perimeter is the whole top edge and 60px of the right.
  const size_t inked =
      (size_t)std::count_if(spanned.begin(), spanned.end(),
                            [](SkColor c) { return c != SK_ColorBLACK; });
  EXPECT_GT(inked, 0u);
  EXPECT_LT(inked, spanned.size());
}

TEST(ComposeSpans, AnimatedRevealDrawsOnAndDeclaresVolatility) {
  Host host(200, 200);
  host.composer.render(stack().child(
      box()
          .rect(SkRect::MakeXYWH(20, 20, 100, 100))
          .stroke(spans::upTo(animate(motion::from(0.0f).to(1.0f), {400ms})),
                  stroke(6, red()))));
  host.frame(0.02);
  auto inked = [&] {
    int n = 0;
    for (int x = 15; x < 130; ++x)
      if (host.pixel(x, 20) == SK_ColorRED) ++n;
    return n;
  };
  const int early = inked();
  host.frame(0.3);
  EXPECT_GT(inked(), early) << "the reveal did not advance";
}

TEST(ComposeSpans, FitSizesAGapFromKeyedContent) {
  // The derive pass applied to a boundary: the stroke claims exactly the run
  // a keyed element covers, so a rule opens a gap around a label without the
  // author computing where the label is.
  //
  // ONE frame per assertion, deliberately. The derive pass runs inside
  // ensureLayout's convergence rounds, which draw() executes BEFORE paint in
  // the same frame, so the fit rects are resolved by the first paint that
  // could read them — there is no one-frame lag to absorb. Allowing a second
  // frame would hide exactly that lag if it appeared. The second half below
  // is the stronger one: after the label MOVES, the very next paint must
  // show the gap at its new position.
  Host host(200, 200);
  auto scene = [](SkRect label) {
    return stack()
        .child(box().key("lbl").rect(label))
        .child(box()
                   .rect(SkRect::MakeXYWH(20, 20, 100, 100))
                   .stroke(spans::fit("lbl", 0.0f), stroke(6, red())));
  };
  host.composer.render(scene(SkRect::MakeXYWH(40, 10, 30, 20)));
  host.frame();
  EXPECT_EQ(host.pixel(55, 20), SK_ColorRED) << "under the label";
  EXPECT_EQ(host.pixel(100, 20), SK_ColorBLACK) << "away from it";
  // The label MOVES; the very next paint must show the claim at the new
  // position and nothing at the old one.
  host.composer.render(scene(SkRect::MakeXYWH(90, 10, 30, 20)));
  host.frame();
  EXPECT_EQ(host.pixel(100, 20), SK_ColorRED)
      << "the moved label's claim lagged a frame";
  EXPECT_EQ(host.pixel(55, 20), SK_ColorBLACK)
      << "the claim at the label's OLD position replayed";
}

TEST(ComposeSpans, SpanValuesParticipateInReconcilerEquality) {
  EXPECT_TRUE(spans::corners(18) == spans::corners(18));
  EXPECT_FALSE(spans::corners(18) == spans::corners(19));
  EXPECT_FALSE(spans::corners(18) == spans::edges(18));
  EXPECT_TRUE(spans::upTo(0.4f) == spans::upTo(0.4f));
  EXPECT_FALSE(spans::upTo(0.4f) == spans::upTo(0.5f));
  EXPECT_TRUE(spans::fit("a", 2) == spans::fit("a", 2));
  EXPECT_FALSE(spans::fit("a", 2) == spans::fit("b", 2));
}

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

TEST(ComposeBand, ConstructionStaysUnderTheQuadraticCeiling) {
  // Band construction must not re-measure the spine per sample. Asking
  // bandPointAt for each sample does exactly that, and bandPointAt walks the
  // whole path every call, so construction becomes quadratic in the spine's
  // length — invisible on a small band and ruinous on a large ring.
  //
  // A single radius cannot demonstrate a growth RATE, so this is a wall-clock
  // ceiling instead: loose enough to survive a contended machine, tight
  // enough that the quadratic form cannot fit under it at this radius.
  auto ring = [](float r) {
    return [r](SkSize s) {
      SkPathBuilder b;
      b.addCircle(s.width() * 0.5f, s.height() * 0.5f, r);
      return b.detach();
    };
  };
  const auto build = [&](float r) {
    Host host(1400, 1400);
    const sigil::measure::Stopwatch watch;
    host.composer.render(
        stack().child(band(ring(r), across(14)).inset(0).fill(red())));
    host.frame();
    return watch.elapsedMs();
  };
  const double big = build(550.0f);
  EXPECT_LT(big, 250.0) << "r=550 band took " << big
                        << " ms — sampling must not be "
                           "quadratic in the spine length";
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

#include <type_traits>

namespace {

/** Two straight strands that cross once, as raw geometry. */
SkPath diagonal(SkPoint a, SkPoint b) {
  SkPathBuilder p;
  p.moveTo(a);
  p.lineTo(b);
  return p.detach();
}

}  // namespace

TEST(ComposeCrossings, CoincidentStrandsNeverCross) {
  // This is what layers() IS, so it has to be exact: N copies of one path
  // meet everywhere and cross nowhere.
  SkPathBuilder b;
  b.addRect(SkRect::MakeWH(80, 60));
  const SkPath rect = b.detach();
  EXPECT_TRUE(geometry::path::discoverCrossings({rect, rect}).empty());
  EXPECT_TRUE(geometry::path::discoverCrossings({rect, rect, rect}).empty());
}

TEST(ComposeCrossings, SharedCornersAreMeetingsNotCrossings) {
  // Two edges touching at a shared endpoint is a MEETING, not a crossing.
  // A discovery pass that counted them would put a knot at every corner of
  // every rectangular frame ever drawn.
  SkPathBuilder a;
  a.addRect(SkRect::MakeWH(80, 60));
  SkPathBuilder c;
  c.addRect(SkRect::MakeXYWH(80, 60, 80, 60));  // touches at one point only
  EXPECT_TRUE(
      geometry::path::discoverCrossings({a.detach(), c.detach()}).empty());
}

TEST(ComposeCrossings, ProperCrossingsAreFoundAndNumberedAlongTheBoundary) {
  const SkPath down = diagonal({0, 0}, {100, 100});
  const SkPath up = diagonal({0, 100}, {100, 0});
  const std::vector<geometry::path::Crossing> one =
      geometry::path::discoverCrossings({down, up});
  ASSERT_EQ(one.size(), 1u);
  EXPECT_EQ(one[0].a, 0u);
  EXPECT_EQ(one[0].b, 1u);
  EXPECT_NEAR(one[0].at.fX, 50.0f, 2.0f);
  EXPECT_NEAR(one[0].at.fY, 50.0f, 2.0f);
  EXPECT_EQ(one[0].index, 0u);

  // Numbering is positional along the lowest-indexed strand, so a horizontal
  // strand crossed by two verticals numbers them left to right.
  const SkPath across = diagonal({0, 50}, {100, 50});
  const std::vector<geometry::path::Crossing> two =
      geometry::path::discoverCrossings(
          {across, diagonal({70, 0}, {70, 100}), diagonal({30, 0}, {30, 100})});
  ASSERT_EQ(two.size(), 2u);
  EXPECT_LT(two[0].alongA, two[1].alongA);
  EXPECT_EQ(two[0].index, 0u);
  EXPECT_EQ(two[1].index, 1u);
  EXPECT_EQ(two[0].b, 2u) << "the x=30 strand is met first";
}

TEST(ComposeCrossings, TheRuleLadderClimbs) {
  geometry::path::Crossing c0, c1, c2;
  c0.index = 0;
  c0.a = 0;
  c0.b = 1;
  c1.index = 1;
  c1.a = 0;
  c1.b = 1;
  c2.index = 2;
  c2.a = 1;
  c2.b = 2;

  // Rung 0 — list order: the later strand is on top, so `a` is under.
  EXPECT_EQ(geometry::path::CrossingRule{}.decide(c0),
            geometry::path::Order::Under);

  // Rung 1 — alternate IS sequence({Over, Under}).
  const geometry::path::CrossingRule alt =
      geometry::path::crossing::alternate();
  EXPECT_EQ(alt,
            geometry::path::crossing::sequence(
                {geometry::path::Order::Over, geometry::path::Order::Under}));
  EXPECT_EQ(alt.decide(c0), geometry::path::Order::Over);
  EXPECT_EQ(alt.decide(c1), geometry::path::Order::Under);

  // Rung 2 — a generic repeating pattern.
  const geometry::path::CrossingRule three = geometry::path::crossing::sequence(
      {geometry::path::Order::Over, geometry::path::Order::Over,
       geometry::path::Order::Under});
  EXPECT_EQ(three.decide(c0), geometry::path::Order::Over);
  EXPECT_EQ(three.decide(c1), geometry::path::Order::Over);
  EXPECT_EQ(three.decide(c2), geometry::path::Order::Under);

  // Rung 3 — strand dominance, including a CYCLE (the Penrose case:
  // 0 over 1, 1 over 2, 2 over 0, which no layer order can express).
  const geometry::path::CrossingRule cyclic =
      geometry::path::crossing::pairs({{0, 1}, {1, 2}, {2, 0}});
  EXPECT_EQ(cyclic.decide(c0), geometry::path::Order::Over);  // 0 over 1
  EXPECT_EQ(cyclic.decide(c2), geometry::path::Order::Over);  // 1 over 2
  geometry::path::Crossing c20;
  c20.a = 0;
  c20.b = 2;
  EXPECT_EQ(cyclic.decide(c20), geometry::path::Order::Under);  // 2 over 0
}

namespace {

/** Strand 0 over strand 1 at every crossing — used where the point is that
 *  the REPAIR works, not which rule chose it. */
struct EveryCrossingRedOnTop {
  bool operator==(const EveryCrossingRedOnTop&) const = default;
  geometry::path::Order decide(const geometry::path::Crossing&) const {
    return geometry::path::Order::Over;
  }
};

/** Rung 4: a user rule is a comparable value with the seam's one named
 *  member — never a bare lambda, because a rule is read live. */
struct EverySecondStrandWins {
  size_t winner = 1;
  bool operator==(const EverySecondStrandWins&) const = default;
  geometry::path::Order decide(const geometry::path::Crossing& c) const {
    return c.a == winner ? geometry::path::Order::Over
                         : geometry::path::Order::Under;
  }
};

}  // namespace

TEST(ComposeCrossings, CustomRulesAreComparableValues) {
  static_assert(geometry::path::CrossingScheme<EverySecondStrandWins>);
  const geometry::path::CrossingRule mine = EverySecondStrandWins{1};
  geometry::path::Crossing c;
  c.a = 1;
  c.b = 2;
  EXPECT_EQ(mine.decide(c), geometry::path::Order::Over);
  EXPECT_TRUE(mine == geometry::path::CrossingRule(EverySecondStrandWins{1}));
  EXPECT_FALSE(mine == geometry::path::CrossingRule(EverySecondStrandWins{2}));
  EXPECT_FALSE(mine == geometry::path::crossing::alternate());
}

TEST(ComposeCrossings, PinsComposeOntoTheBaseRule) {
  // One .crossing field: a pin layers over whatever rule is already there
  // rather than becoming a second entry.
  geometry::path::CrossingRule rule = geometry::path::crossing::alternate();
  rule.except(0, geometry::path::Order::Under)
      .except(3, geometry::path::Order::Over);
  geometry::path::Crossing c;
  c.a = 0;
  c.b = 1;
  c.index = 0;
  EXPECT_EQ(rule.decide(c), geometry::path::Order::Under)
      << "pinned against alternate";
  c.index = 1;
  EXPECT_EQ(rule.decide(c), geometry::path::Order::Under)
      << "base rule still runs";
  c.index = 2;
  EXPECT_EQ(rule.decide(c), geometry::path::Order::Over);
  c.index = 3;
  EXPECT_EQ(rule.decide(c), geometry::path::Order::Over) << "second pin";
  // Re-pinning the same index REPLACES it (one answer per crossing).
  rule.except(0, geometry::path::Order::Over);
  c.index = 0;
  EXPECT_EQ(rule.decide(c), geometry::path::Order::Over);
  // …and a pinned rule is still a comparable value.
  geometry::path::CrossingRule same = geometry::path::crossing::alternate();
  same.except(0, geometry::path::Order::Over)
      .except(3, geometry::path::Order::Over);
  EXPECT_TRUE(rule == same);
  EXPECT_FALSE(rule == geometry::path::crossing::alternate());
}

TEST(ComposeComposites, LayersIsWeaveWithCoincidentSelfStrands) {
  // FORMALLY one machine. Same pixels from both spellings, and the layers
  // form really is a weave of self-strands.
  const brush::Weave stacked =
      brush::layers({brush::solid(8, red()), brush::solid(3, green())});
  ASSERT_EQ(stacked.strands.size(), 2u);
  EXPECT_EQ(stacked.strands[0].path,
            StrandPath(geometry::path::profile::self()));
  EXPECT_EQ(stacked.strands[1].path,
            StrandPath(geometry::path::profile::self()));

  const brush::Weave woven = brush::weave(
      {brush::Strand{geometry::path::profile::self(), brush::solid(8, red())},
       brush::Strand{geometry::path::profile::self(),
                     brush::solid(3, green())}},
      geometry::path::CrossingRule{});

  auto draw = [](const brush::Weave& w) {
    Host host(200, 200);
    host.composer.render(stack().child(
        box().rect(SkRect::MakeXYWH(40, 40, 100, 100)).stroke(w)));
    host.frame();
    std::vector<SkColor> out;
    for (int x = 30; x < 150; x += 3) out.push_back(host.pixel(x, 40));
    return out;
  };
  const std::vector<SkColor> layered = draw(stacked);
  EXPECT_EQ(layered, draw(woven));
  // Bottom-up: the last brush in the list is the one you see.
  EXPECT_EQ(layered[(layered.size() / 2)], SK_ColorGREEN);
}

TEST(ComposeComposites, WeaveRepairsTheCrossingsTheRuleDisagreesWith) {
  // Two authored strands crossing once. Under list order the second is on
  // top; alternate() says the FIRST passes over at crossing 0, so the
  // repair patch must put strand 0's colour at the meeting.
  auto draw = [](geometry::path::CrossingRule rule) {
    Host host(200, 200);
    brush::Weave w = brush::weave(
        {brush::Strand{strand::path(diagonal({20, 20}, {180, 180})),
                       brush::solid(9, red())},
         brush::Strand{strand::path(diagonal({20, 180}, {180, 20})),
                       brush::solid(9, green())}},
        std::move(rule));
    host.composer.render(stack().child(box().inset(0).stroke(w)));
    host.frame();
    return host.pixel(100, 100);
  };
  EXPECT_EQ(draw(geometry::path::CrossingRule{}), SK_ColorGREEN)
      << "list order: later on top";
  EXPECT_EQ(draw(geometry::path::crossing::alternate()), SK_ColorRED)
      << "rule flipped it";
  geometry::path::CrossingRule pinned = geometry::path::crossing::alternate();
  pinned.except(0, geometry::path::Order::Under);
  EXPECT_EQ(draw(pinned), SK_ColorGREEN) << "the pin overrode the rule";
}

// ---------------------------------------------------------------------------
// The crossing cache: discovery is memoized on the RESOLVED strand paths,
// which are the function's entire input, and held on the Weave value behind
// a shared_ptr so copies share it. Three things must hold — staleness
// through each of the two doors into the key, and byte identity between the
// cold and cached paints.
//
// `crossingCache->computes` counts DISCOVERIES, not paints. That is what
// makes these cases readable: with Cache::None the node repaints every
// frame, so a steady frame that does not move the counter proves a hit.

TEST(ComposeComposites, CrossingCacheRecomputesWhenAuthoredGeometryChanges) {
  // Staleness through the AUTHORED door: warm the cache, then edit a
  // strand's path on a copy that shares it.
  //
  // THREE strands, not two, and that is load-bearing. With a lone crossing
  // the knot's territory has no neighbour to bound it, so a stale repair —
  // clipped only by the CURRENT tubes — still happens to cover the new
  // meeting, and the staleness is invisible in pixels. Two knots bound each
  // other's territory, so a stale answer mis-paints somewhere readable.
  //
  // Red runs y=100; green verticals at x=60 and x=140. alternate() puts red
  // over at knot 0 (x=60) and leaves green over at knot 1 (x=140). Moving
  // the first vertical to x=180 makes (140,100) the new knot 0 — red over —
  // while (180,100) keeps green. A stale cache still numbers (140,100) as
  // knot 1 and aims its red repair at (60,100), where nothing is left to
  // repair.
  brush::Weave w =
      brush::weave({brush::Strand{strand::path(diagonal({0, 100}, {200, 100})),
                                  brush::solid(9, red())},
                    brush::Strand{strand::path(diagonal({60, 0}, {60, 200})),
                                  brush::solid(9, green())},
                    brush::Strand{strand::path(diagonal({140, 0}, {140, 200})),
                                  brush::solid(9, green())}},
                   geometry::path::crossing::alternate());
  Host host(240, 240);
  host.composer.render(
      stack().child(box().inset(0).stroke(w).cache(Cache::None)));
  host.frame();
  EXPECT_EQ(host.pixel(60, 100), SK_ColorRED) << "knot 0: red repaired over";
  EXPECT_EQ(host.pixel(140, 100), SK_ColorGREEN) << "knot 1: list order";
  EXPECT_EQ(w.crossingCache->computes, 1) << "the cold paint discovers once";
  host.frame();
  EXPECT_EQ(w.crossingCache->computes, 1)
      << "a steady repaint must HIT, not rediscover";

  brush::Weave moved = w;  // shares the WARM cache — that is the scenario
  moved.strands[1].path = strand::path(diagonal({180, 0}, {180, 200}));
  host.composer.render(
      stack().child(box().inset(0).stroke(moved).cache(Cache::None)));
  host.frame();
  EXPECT_EQ(host.pixel(140, 100), SK_ColorRED)
      << "stale crossings: (140,100) still numbered as the green knot";
  EXPECT_EQ(host.pixel(180, 100), SK_ColorGREEN)
      << "the new knot 1 keeps list order";
  EXPECT_EQ(w.crossingCache->computes, 2)
      << "the edit must land in the key and rediscover";
}

TEST(ComposeComposites, CrossingCacheFollowsTheOutlineUnderRelativeStrands) {
  // Staleness through the OUTLINE door. A relative strand resolves against
  // ctx.outline, so the SAME weave value over a changed shape must
  // rediscover: the key is the resolved paths, not the value's own fields.
  // A self-strand ring crossed by an authored line, red always on top;
  // when the ring grows, the knots move outward along the line, and only
  // a fresh discovery repairs them at the new radius. The line runs
  // VERTICALLY so both knots sit a quarter turn from the ring's contour
  // seam (addCircle starts at 3 o'clock, and a knot AT the seam is
  // rejected by the transversality walk — a discovery property, not the
  // cache's).
  brush::Weave w = brush::weave(
      {brush::Strand{geometry::path::profile::self(), brush::solid(6, red())},
       brush::Strand{strand::path(diagonal({100, 0}, {100, 200})),
                     brush::solid(6, green())}},
      geometry::path::CrossingRule(EveryCrossingRedOnTop{}));
  auto ring = [](float radius) {
    return [radius](SkSize) {
      SkPathBuilder p;
      p.addCircle(100, 100, radius);
      return p.detach();
    };
  };
  Host host(200, 200);
  host.composer.render(stack().child(
      box().inset(0).shape(ring(60.0f)).stroke(w).cache(Cache::None)));
  host.frame();
  EXPECT_EQ(host.pixel(100, 160), SK_ColorRED) << "the r=60 knot repairs";
  EXPECT_EQ(w.crossingCache->computes, 1);
  host.frame();
  EXPECT_EQ(w.crossingCache->computes, 1) << "steady outline: a hit";

  host.composer.render(stack().child(
      box().inset(0).shape(ring(85.0f)).stroke(w).cache(Cache::None)));
  host.frame();
  EXPECT_EQ(host.pixel(100, 185), SK_ColorRED)
      << "stale crossings: the repair stayed at the old radius";
  EXPECT_EQ(w.crossingCache->computes, 2)
      << "the outline change must land in the key and rediscover";
}

TEST(ComposeComposites, CrossingCacheIsByteNeutral) {
  // The cache may change WHEN discovery runs and never what is drawn.
  // Frame 1 discovers cold, frame 2 hits, and a fresh equal value discovers
  // cold again — all three surfaces must be byte-equal.
  auto bytes = [](Host& host) {
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(host.surface->width(),
                                              host.surface->height()));
    host.surface->readPixels(bm.pixmap(), 0, 0);
    const uint8_t* p = (const uint8_t*)bm.getPixels();
    return std::vector<uint8_t>(p, p + bm.computeByteSize());
  };
  auto weaveX = [] {
    return brush::weave(
        {brush::Strand{strand::path(diagonal({20, 20}, {180, 180})),
                       brush::solid(9, red())},
         brush::Strand{strand::path(diagonal({20, 180}, {180, 20})),
                       brush::solid(9, green())}},
        geometry::path::crossing::alternate());
  };
  brush::Weave w = weaveX();
  Host host(240, 240);
  host.composer.render(
      stack().child(box().inset(0).stroke(w).cache(Cache::None)));
  host.frame();
  const std::vector<uint8_t> cold = bytes(host);
  EXPECT_EQ(w.crossingCache->computes, 1);
  host.frame();
  EXPECT_EQ(w.crossingCache->computes, 1) << "frame 2 must be a HIT";
  EXPECT_TRUE(cold == bytes(host)) << "the cache-hit frame drew differently";

  brush::Weave fresh = weaveX();  // its own cold cache
  Host host2(240, 240);
  host2.composer.render(
      stack().child(box().inset(0).stroke(fresh).cache(Cache::None)));
  host2.frame();
  EXPECT_EQ(fresh.crossingCache->computes, 1);
  EXPECT_TRUE(cold == bytes(host2))
      << "cold and cached must be one picture, byte for byte";
}

TEST(ComposeComposites, TheRepairCoversShallowCrossings) {
  // A crossing repair sized as a disc under-covers at SHALLOW angles: two
  // marks overlap in a lens whose extent grows as reach/sin(theta), so a
  // radius chosen for the perpendicular case leaves the under-strand showing
  // straight across the over-strand. The repair has to follow the lens.
  //
  // Checked by sampling ALONG the over-strand through the meeting: every
  // sample must be the over-strand's colour.
  //
  // The two strands are one segment rotated by ± half the crossing angle
  // about the centre. Building them instead from a shared dx with
  // dy = dx·tan(angle) sends the coordinates to infinity at 90 degrees.
  //
  // Align::Inner is not exercised here and cannot be: an OPEN strand has no
  // inside. It is covered by ReachReportsTheMarkWhereBleedReportsNothing
  // below, on closed strands.
  auto interruptions = [](float degrees) {
    Host host(400, 400);
    const float half = degrees * 0.5f * 3.14159265f / 180.0f;
    const float len = 180.0f;
    const SkPoint mid{200, 200};
    const SkVector dirA{std::cos(half), std::sin(half)};
    const SkVector dirB{std::cos(-half), std::sin(-half)};
    const auto through = [&](SkVector d) {
      return diagonal({mid.fX - d.x() * len, mid.fY - d.y() * len},
                      {mid.fX + d.x() * len, mid.fY + d.y() * len});
    };
    brush::Weave w = brush::weave(
        {brush::Strand{strand::path(through(dirA)), stroke(9, red())},
         brush::Strand{strand::path(through(dirB)), stroke(9, green())}},
        geometry::path::crossing::alternate());  // strand 0 (red) passes OVER
                                                 // at crossing 0
    host.composer.render(stack().child(box().inset(0).stroke(w)));
    host.frame();
    int wrong = 0;
    for (int i = -40; i <= 40; ++i) {
      const int x = (int)std::lround(mid.fX + dirA.x() * (float)i);
      const int y = (int)std::lround(mid.fY + dirA.y() * (float)i);
      if (host.pixel(x, y) != SK_ColorRED) ++wrong;
    }
    return wrong;
  };
  EXPECT_EQ(interruptions(90.0f), 0)
      << "even the perpendicular case exceeded the radius bound";
  EXPECT_EQ(interruptions(45.0f), 0);
  EXPECT_EQ(interruptions(12.5f), 0)
      << "12.5 degrees: the disc's measured failure";
}

TEST(ComposeComposites, ReachReportsTheMarkWhereBleedReportsNothing) {
  // bleed() and reach() are different numbers and a crossing repair needs
  // the second one. bleed() answers "how far outside the node does this
  // escape", which for an Align::Inner stroke is zero, while the MARK it
  // paints is still `width` wide. Sizing a repair from bleed() gives a
  // zero-sized repair for every inner stroke.
  const Decoration inner = stroke(9, red(), PathFormat::Align::Inner);
  EXPECT_EQ(inner.bleed(), 0.0f) << "unchanged: it escapes nothing";
  EXPECT_EQ(inner.reach(), 9.0f) << "…but the mark is 9px wide";
  const Decoration centred = stroke(9, red());
  EXPECT_EQ(centred.bleed(), 4.5f);
  EXPECT_EQ(centred.reach(), 9.0f);

  // And it repairs: two CLOSED strands (where Inner is meaningful) — two
  // overlapping circles, which meet at TWO points — stroked Inner, with the
  // rule against list order.
  auto circle = [](float cx, float cy, float r) {
    SkPathBuilder p;
    p.addCircle(cx, cy, r);
    return p.detach();
  };
  Host host(400, 400);
  brush::Weave w = brush::weave(
      {brush::Strand{strand::path(circle(160, 200, 90)),
                     stroke(9, red(), PathFormat::Align::Inner)},
       brush::Strand{strand::path(circle(240, 200, 90)),
                     stroke(9, green(), PathFormat::Align::Inner)}},
      geometry::path::CrossingRule(EveryCrossingRedOnTop{}));
  host.composer.render(stack().child(box().inset(0).stroke(w)));
  host.frame();
  // Walk the red circle's stroke band through the upper crossing region.
  int red = 0, green = 0;
  for (int i = 0; i < 360; ++i) {
    const float a = (float)i * 3.14159265f / 180.0f;
    const int x = (int)std::lround(160 + std::cos(a) * 86.0f);
    const int y = (int)std::lround(200 + std::sin(a) * 86.0f);
    const SkColor c = host.pixel(x, y);
    if (c == SK_ColorRED)
      ++red;
    else if (c == SK_ColorGREEN)
      ++green;
  }
  EXPECT_GT(red, 300) << "the over-strand should own its whole ring";
  EXPECT_EQ(green, 0) << "the under-strand still shows through the mark";
}

TEST(ComposeStrands, AbsoluteOnlyLeavesTheBoundaryUnpainted) {
  // "With only absolute strands the boundary is an unpainted host."
  Host host(200, 200);
  host.composer.render(stack().child(
      box()
          .rect(SkRect::MakeXYWH(40, 40, 100, 100))
          .stroke(brush::weave(
              {brush::Strand{strand::path(diagonal({0, 0}, {100, 0})),
                             brush::solid(6, red())}},
              geometry::path::CrossingRule{}))));
  host.frame();
  EXPECT_EQ(host.pixel(90, 40), SK_ColorRED) << "the authored strand paints";
  EXPECT_EQ(host.pixel(140, 90), SK_ColorBLACK)
      << "the boundary itself is only a host";
}

TEST(ComposeStrands, RelativeStrandsRideTheBandsFrame) {
  // A relative strand is a displacement in the (along, across) frame the
  // BAND owns — positive across is LEFT of travel, i.e. outside a
  // clockwise path. Same convention, one body (profileOffset).
  SkPathBuilder b;
  b.addRect(SkRect::MakeWH(100, 100));
  const SkPath rect = b.detach();
  const SkPath out =
      geometry::path::profileOffset(rect, geometry::path::profile::offset(10));
  const SkPath in =
      geometry::path::profileOffset(rect, geometry::path::profile::offset(-10));
  EXPECT_GT(out.getBounds().width(), rect.getBounds().width());
  EXPECT_LT(in.getBounds().width(), rect.getBounds().width());
  // self() is the boundary itself.
  EXPECT_EQ(geometry::path::profile::self().max(), 0.0f);
}

TEST(ComposeStrands, BorrowedStrandsRideTheDerivePass) {
  Host host(200, 200);
  host.composer.render(
      stack()
          .child(box().key("guide").rect(SkRect::MakeXYWH(60, 20, 80, 40)))
          .child(
              box()
                  .rect(SkRect::MakeXYWH(20, 20, 160, 160))
                  .stroke(brush::weave({brush::Strand{strand::from("guide"),
                                                      brush::solid(6, red())}},
                                       geometry::path::CrossingRule{}))));
  host.frame();
  host.frame();  // derive resolves against the first layout
  // The guide's own box outline, painted in the host's local space.
  EXPECT_EQ(host.pixel(100, 20), SK_ColorRED);
  EXPECT_EQ(host.pixel(100, 60), SK_ColorRED);
  EXPECT_EQ(host.pixel(100, 120), SK_ColorBLACK) << "nothing else moved";
}

TEST(ComposeBrushKinds, SolidIsPathFormatUnderItsTaughtName) {
  // brush::Solid is not a wrapper or a subclass — it is an alias for
  // PathFormat, so the two names build one value and a decoration built
  // through either compares equal to the other.
  static_assert(std::is_same_v<brush::Solid, PathFormat>);
  const brush::Solid a = brush::solid(2, red());
  const PathFormat b = stroke(2, red());
  EXPECT_TRUE(a == b) << "one value, two spellings";
}

TEST(ComposeComposites, ClosedStrandsWrapAtTheirSeam) {
  // A CYCLE has no far end: two knots at along 0.02 and 0.98 are 4% apart,
  // not 96%. Measure the distance between them linearly and crossings that
  // straddle the seam read as maximally distant, which removes the bound on
  // each knot's territory and lets the two repair lenses merge — both knots
  // of two overlapping rings then come out in ONE colour.
  //
  // The geometry here is chosen so BOTH knots are neighbours across their
  // own strand's seam: a large ring crossed by a small one near its edge.
  auto circle = [](float cx, float cy, float r) {
    SkPathBuilder p;
    p.addCircle(cx, cy, r);
    return p.detach();
  };
  const SkPath big = circle(200, 200, 100);
  const SkPath small = circle(288, 200, 13);

  Host host(400, 400);
  host.composer.render(stack().child(box().inset(0).stroke(
      brush::weave({brush::Strand{strand::path(big), stroke(6, red())},
                    brush::Strand{strand::path(small), stroke(6, green())}},
                   geometry::path::crossing::alternate()))));
  host.frame();

  const std::vector<geometry::path::Crossing> knots =
      geometry::path::discoverCrossings({big, small});
  ASSERT_EQ(knots.size(), 2u) << "the two rings meet twice";
  // alternate(): ordinal 0 puts strand 0 (red) over, ordinal 1 puts strand
  // 1 (green) over. Both knots one colour is the defect.
  EXPECT_EQ(host.pixel((int)std::lround(knots[0].at.fX),
                       (int)std::lround(knots[0].at.fY)),
            SK_ColorRED);
  EXPECT_EQ(host.pixel((int)std::lround(knots[1].at.fX),
                       (int)std::lround(knots[1].at.fY)),
            SK_ColorGREEN)
      << "the second knot was swallowed by the first knot's patch";
}

TEST(ComposeComposites, CompositesNest) {
  // "Composites NEST": a strand painted by layers, a whole weave used as
  // one strand of a bigger one. No new vocabulary needed for either.
  Host host(200, 200);
  const brush::Weave inner =
      brush::layers({brush::solid(9, red()), brush::solid(3, green())});
  host.composer.render(stack().child(
      box()
          .rect(SkRect::MakeXYWH(40, 40, 100, 100))
          .stroke(brush::weave(
              {brush::Strand{geometry::path::profile::self(), inner},
               brush::Strand{geometry::path::profile::offset(12),
                             brush::solid(2, blue())}},
              geometry::path::CrossingRule{}))));
  host.frame();
  EXPECT_EQ(host.pixel(90, 40), SK_ColorGREEN) << "the nested layers' top";
  EXPECT_EQ(host.pixel(90, 28), SK_ColorBLUE) << "the offset strand, outside";
}

// ---------------------------------------------------------------------------
// The authoring spellings, each checked against the mechanism it is sugar
// for. These say what a word MEANS — animate(to()) ramps where a bare value
// snaps, motion::from().to() is a mount entrance, a bound source/target pair is
// two explicit stages — because the words are close enough that a wrong one
// produces a plausible picture rather than an error.

// ---- 1. animate(to(v), spec) ----------------------------------------------

TEST(ComposeR1Animate, AnimateToIsTheChangeRamp) {
  // Read the property MID-RAMP, which is the only place a snap and a ramp
  // differ — both agree at the endpoints. The second arm describes the same
  // value with no animate() at all and must snap, which is what makes the
  // first arm's reading meaningful.
  auto run = [](bool plain) {
    Host host(200, 200);
    auto describe = [&](float opacity) {
      Element inner = box().width(100).height(100).fill(red());
      if (plain)
        inner.opacity(opacity);
      else
        inner.opacity(animate(sigil::motion::to(opacity), {200ms}));
      return stack().child(std::move(inner));
    };
    host.composer.render(describe(1.0f));
    host.frame();
    host.composer.render(describe(0.0f));  // the CHANGE
    host.frame(0.1);                       // half way down the ramp
    return host.pixel(50, 50);
  };
  const SkColor ramped = run(false);
  const SkColor snapped = run(true);
  EXPECT_NE(ramped, snapped) << "animate(to(v)) must ramp where a bare "
                                "value snaps";
  // …and it is genuinely mid-ramp, not "already gone" or "not started".
  EXPECT_GT((int)SkColorGetR(ramped), 20);
  EXPECT_LT((int)SkColorGetR(ramped), 235);
  EXPECT_EQ((int)SkColorGetR(snapped), 0) << "the bare value is already there";
}

TEST(ComposeR1Animate, ToAloneHasNoEntranceAndFromToDoes) {
  // The whole distinction between the two, as pixels: to() mounts already
  // holding its value, and motion::from().to() plays a path on first
  // appearance.
  auto mountedOpacity = [](bool withEntrance) {
    Host host(200, 200);
    Element inner = box().width(100).height(100).fill(red());
    if (withEntrance)
      inner.opacity(animate(motion::from(0.0f).to(1.0f), {400ms}));
    else
      inner.opacity(animate(sigil::motion::to(1.0f), {400ms}));
    host.composer.render(stack().child(std::move(inner)));
    host.frame(0.001);
    return (int)SkColorGetR(host.pixel(50, 50));
  };
  EXPECT_GT(mountedOpacity(false), 240) << "to() alone must not fade in";
  EXPECT_LT(mountedOpacity(true), 60)
      << "motion::from().to() is a mount entrance";
}

namespace {

/** A scheme that declares its volatility with the one recognised word.
 *  There is exactly one spelling the concept duck-types on. */
struct SaysAnimated {
  bool live = true;
  bool isAnimated() const { return live; }
  void paint(SkCanvas& c, const PaintContext&) const {
    SkPaint p;
    p.setColor4f({1, 0, 0, 1}, nullptr);
    c.drawRect(SkRect::MakeWH(40, 40), p);
  }
  bool operator==(const SaysAnimated&) const = default;
};

/** The same scheme spelling a NEAR-MISS of that word. It must not satisfy
 *  the concept: duck typing means a scheme spelling it wrongly is read as
 *  static, its node is cached, and it stops animating with no diagnostic —
 *  which is why exactly one spelling is recognised and this case exists. */
struct SaysTheDeadWord {
  bool live = true;
  bool animates() const { return live; }
  void paint(SkCanvas&, const PaintContext&) const {}
  bool operator==(const SaysTheDeadWord&) const = default;
};

}  // namespace

TEST(ComposeR3Volatility, OneWordDeclaresVolatilityAndTheOthersAreGone) {
  static_assert(AnimatedDecoration<SaysAnimated>);
  static_assert(!AnimatedDecoration<SaysTheDeadWord>,
                "only isAnimated() declares volatility");
  EXPECT_TRUE(Decoration(SaysAnimated{true}).isAnimated());
  EXPECT_FALSE(Decoration(SaysAnimated{false}).isAnimated());
  // A near-miss spelling declares nothing: it wraps, it paints, it is static.
  EXPECT_FALSE(Decoration(SaysTheDeadWord{true}).isAnimated());
}

TEST(ComposeR3Volatility, LibrarySchemesDeclareWithTheOneWord) {
  lines::Line line;
  choreograph::Output<float> phase;
  line.dashPhaseBinding = &phase;
  EXPECT_TRUE(line.isAnimated());

  PathFormat pf;
  EXPECT_FALSE(pf.isAnimated());

  // A Material answers the same question in the same word as every other
  // scheme, so a consumer never has to know which kind it is holding.
  const material::skia::Paint stat = material::skia::Paint::solid({1, 0, 0, 1});
  EXPECT_FALSE(stat.isAnimated());
}

// ---- 3. Bound::source / ::target -------------------------------------------

TEST(ComposeR1Bound, SourceAndTargetAreTheOldStagesRenamed) {
  choreograph::Output<float> hp;
  hp = 25.0f;
  const sigil::motion::BoundFloat named =
      motion::bind(&hp).source(0, 100).target(-70, 170).value();
  // target(lo, hi) is sugar: the same mapping written as an explicit scale
  // and offset must agree with it everywhere, including outside the source
  // range, since neither form clamps.
  const sigil::motion::BoundFloat manual =
      motion::bind(&hp).source(0, 100).scale(240).offset(-70).value();
  for (float v : {0.0f, 25.0f, 50.0f, 100.0f, 137.0f})
    EXPECT_FLOAT_EQ(named.apply(v), manual.apply(v)) << "at " << v;
  EXPECT_FLOAT_EQ(named.apply(0.0f), -70.0f);
  EXPECT_FLOAT_EQ(named.apply(100.0f), 170.0f);
}

TEST(ComposeR1Bound, WindowIsStillSourceThatClamps) {
  choreograph::Output<float> t;
  const sigil::motion::BoundFloat w =
      motion::bind(&t).window(0.2f, 0.4f).value();
  const sigil::motion::BoundFloat s =
      motion::bind(&t).source(0.2f, 0.4f).value();
  EXPECT_FLOAT_EQ(w.apply(0.3f), s.apply(0.3f));
  EXPECT_FLOAT_EQ(w.apply(0.9f), 1.0f) << "window clamps";
  EXPECT_GT(s.apply(0.9f), 1.0f) << "source does not";
}

// ---- 5. Ribbon on the profile seam ----------------------------------------

TEST(ComposeR1Ribbon, ProfileRibbonPaintsItsBand) {
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

TEST(ComposeWidthProfile, TheLastNeverPruneRibbonsCanPruneNow) {
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

// ---- 6. the derive family --------------------------------------------------

TEST(ComposeR1Derive, TheFamilyHasOneSpelling) {
  // Aliases, so the same picture, term for term.
  auto draw = [](bool qualified) {
    Host host(200, 200);
    Element a = box().key("a").rect(SkRect::MakeXYWH(20, 20, 40, 40));
    Element b = box().key("b").rect(SkRect::MakeXYWH(120, 120, 40, 40));
    Element wire =
        qualified ? derive::connector("a", "b") : connector("a", "b");
    wire.absolute().inset(0).foreground(stroke(4, red()));
    host.composer.render(
        stack().child(std::move(a)).child(std::move(b)).child(std::move(wire)));
    host.frame();
    host.frame();  // derive resolves against the first layout
    std::vector<SkColor> out;
    for (int i = 20; i < 160; i += 4) out.push_back(host.pixel(i, i));
    return out;
  };
  const std::vector<SkColor> qualified = draw(true);
  EXPECT_EQ(qualified, draw(false));
  EXPECT_GT(inkedCount(qualified), 10u) << "the wire actually drew";
}

TEST(ComposeR1Derive, FlowAroundAsAFreeVerbIsTheMethod) {
  auto draw = [](bool freeVerb) {
    Host host(300, 200);
    // whiteStyle, not styleAt: the default foreground is BLACK and so is the
    // host's ground, so with the default style both arms would compare two
    // blank grids and agree perfectly. The liveness bound at the end is the
    // second guard against that.
    Element para = text(
        u8"one two three four five six seven eight nine ten "
        u8"eleven twelve thirteen fourteen",
        whiteStyle(16));
    if (freeVerb)
      para = derive::flowAround(std::move(para), "cut", 6.0f);
    else
      para.flowAround("cut", 6.0f);
    host.composer.render(
        stack()
            .child(box().key("cut").rect(SkRect::MakeXYWH(10, 10, 90, 60)))
            .child(box().absolute().inset(0).child(std::move(para))));
    host.frame();
    host.frame();
    std::vector<SkColor> out;
    for (int y = 0; y < 200; y += 3)
      for (int x = 0; x < 300; x += 3) out.push_back(host.pixel(x, y));
    return out;
  };
  const std::vector<SkColor> freeVerb = draw(true);
  EXPECT_EQ(freeVerb, draw(false));
  EXPECT_GT(inkedCount(freeVerb), 20u) << "the paragraph actually drew";
}

// ---- the wrapping span -----------------------------------------------------
//
// `spans::wrap` is a window that may cross the contour seam. These are
// PARITY tests rather than "does it draw something": each compares the
// pass door (`stroke(spans, …)`) against the node door
// (`mask(by::spans(…))`), which are two spellings of one geometry, and each
// also bounds the ink away from "nothing" and "everything" so that two
// agreeing blank frames cannot pass.

TEST(ComposeR1Wrap, StaticSeamCrossingWindowMatchesWrapTrim) {
  auto draw = [](bool useLegacyTrim) {
    Host host(200, 200);
    Element e = revealBox();
    if (useLegacyTrim)
      e.mask(by::spans(spans::wrap(0.9f, 1.15f))).stroke(stroke(6, red()));
    else
      e.stroke(spans::wrap(0.9f, 1.15f), stroke(6, red()));
    host.composer.render(stack().child(std::move(e)));
    host.frame();
    return boundaryRing(host);
  };
  const std::vector<SkColor> spanned = draw(false);
  EXPECT_EQ(spanned, draw(true));
  // A genuine seam-crossing window: some ink, but far from all of it.
  const size_t inked = inkedCount(spanned);
  EXPECT_GT(inked, 5u);
  EXPECT_LT(inked, spanned.size() / 2);
}

TEST(ComposeR1Wrap, MarchingAntsMatchTrimAtEveryPhaseIncludingMidSeam) {
  // The full marching-ants idiom: a fixed-length window driven all the way
  // round, compared at eight phases — two of which straddle the seam, and
  // one of which sits exactly ON it.
  //
  // The node gate spells the window as (start, end, OFFSET); the pass door
  // spells it as arithmetic on the two ENDPOINTS of one Output, which is why
  // it owes no third parameter.
  constexpr float kWindow = 0.25f;
  choreograph::Output<float> phase;

  Host trimmed(200, 200);
  trimmed.composer.render(stack().child(
      revealBox()
          .mask(by::spans(spans::wrap(0.0f, kWindow).offset(&phase)))
          .stroke(stroke(6, red()))));

  Host spanned(200, 200);
  spanned.composer.render(stack().child(revealBox().stroke(
      spans::wrap(motion::bind(&phase), motion::bind(&phase).offset(kWindow)),
      stroke(6, red()))));

  for (float p : {0.0f, 0.12f, 0.37f, 0.5f, 0.66f, 0.80f, 0.90f, 0.97f}) {
    phase = p;
    trimmed.frame();
    spanned.frame();
    const std::vector<SkColor> want = boundaryRing(trimmed);
    EXPECT_EQ(boundaryRing(spanned), want) << "phase " << p;
    EXPECT_GT(inkedCount(want), 5u) << "phase " << p << " drew nothing";
    EXPECT_LT(inkedCount(want), want.size())
        << "phase " << p << " drew everything";
  }
}

TEST(ComposeR1Wrap, AnimatedEndpointsMarchAcrossTheSeamAndMatchTrim) {
  // The composer-driven half: BOTH endpoints on animate() ramps that carry
  // the window past 1.0, so the seam is crossed by a transition rather than
  // by a bound value.
  constexpr float kWindow = 0.2f;
  auto host = [](bool useLegacyTrim) {
    auto h = std::make_unique<Host>(200, 200);
    Element e = revealBox();
    if (useLegacyTrim)
      e.mask(by::spans(
                 spans::wrap(0.0f, kWindow)
                     .offset(animate(motion::from(0.0f).to(1.0f), {1000ms}))))
          .stroke(stroke(6, red()));
    else
      e.stroke(spans::wrap(
                   animate(motion::from(0.0f).to(1.0f), {1000ms}),
                   animate(motion::from(kWindow).to(1.0f + kWindow), {1000ms})),
               stroke(6, red()));
    h->composer.render(stack().child(std::move(e)));
    return h;
  };
  std::unique_ptr<Host> t = host(true), s = host(false);
  // Both easings are the house default, so the two windows stay aligned.
  for (int step = 0; step < 8; ++step) {
    t->frame(0.12);
    s->frame(0.12);
    const std::vector<SkColor> want = boundaryRing(*t);
    EXPECT_EQ(boundaryRing(*s), want) << "step " << step;
    EXPECT_GT(inkedCount(want), 3u) << "step " << step;
  }
}

TEST(ComposeR1Wrap, DegenerateWindowsMatchTrimToo) {
  auto ink = [](bool useLegacyTrim, float a, float b) {
    Host host(200, 200);
    Element e = revealBox();
    if (useLegacyTrim)
      e.mask(by::spans(spans::wrap(a, b))).stroke(stroke(6, red()));
    else
      e.stroke(spans::wrap(a, b), stroke(6, red()));
    host.composer.render(stack().child(std::move(e)));
    host.frame();
    return inkedCount(boundaryRing(host));
  };
  // end <= begin claims NOTHING (not "everything", not "the complement").
  EXPECT_EQ(ink(false, 0.6f, 0.6f), 0u);
  EXPECT_EQ(ink(false, 0.6f, 0.6f), ink(true, 0.6f, 0.6f));
  EXPECT_EQ(ink(false, 0.6f, 0.3f), ink(true, 0.6f, 0.3f));
  // A window a whole cycle long (or more) claims all of it.
  const size_t whole = ink(false, 0.3f, 1.3f);
  EXPECT_GT(whole, 100u);
  EXPECT_EQ(whole, ink(true, 0.3f, 1.3f));
  EXPECT_EQ(ink(false, 0.3f, 2.6f), ink(true, 0.3f, 2.6f));
}

TEST(ComposeR1Wrap, WrapIsItsOwnTermAndRangeStillClamps) {
  // wrap() is a separate term rather than a mode of range(), because
  // range(0.9, 0.1) already has a meaning — a reversed pair normalised into
  // one run — and because a reader looking at a claim conflict needs the
  // call site itself to say whether the window is cyclic.
  const SkPath boundary = unitBox();
  SpanInput in;
  in.outline = &boundary;
  const std::vector<float> vals{0.9f, 0.1f};
  in.values = &vals;
  EXPECT_EQ(spans::range(0.9f, 0.1f).resolve(in).size(), 1u)
      << "range still normalises a reversed pair into one run";
  const std::vector<Span> wrapped = spans::wrap(0.9f, 0.1f).resolve(in);
  EXPECT_TRUE(wrapped.empty()) << "…and wrap reads it as an EMPTY window";
  const std::vector<float> ants{0.9f, 1.1f};
  in.values = &ants;
  const std::vector<Span> two = spans::wrap(0.9f, 1.1f).resolve(in);
  ASSERT_EQ(two.size(), 2u) << "one term, two runs, at opposite seams";
  EXPECT_NEAR(two[0].begin, 0.0f, 1e-4f);
  EXPECT_NEAR(two[0].end, 0.1f, 1e-4f);
  EXPECT_NEAR(two[1].begin, 0.9f, 1e-4f);
  EXPECT_NEAR(two[1].end, 1.0f, 1e-4f);
}

TEST(ComposeR1Wrap, WrapWindowsParticipateInReconcilerEquality) {
  // Without this the marching reveal above would prune to its first frame:
  // every wrapped window would compare equal to every other one.
  EXPECT_TRUE(spans::wrap(0.1f, 0.4f) == spans::wrap(0.1f, 0.4f));
  EXPECT_FALSE(spans::wrap(0.1f, 0.4f) == spans::wrap(0.1f, 0.5f));
  EXPECT_FALSE(spans::wrap(0.1f, 0.4f) == spans::range(0.1f, 0.4f));
}

// ---- the span/gate parity table --------------------------------------------
//
// One capability per row, each drawn twice: through the pass door
// (`stroke(spans, …)`) and through the node gate (`mask(by::spans(…))`).
// The two are one geometry expressed two ways, so every row is a pixel proof
// that the sugar and the gate describe the same run — and each row also
// bounds its own ink, because two identically blank frames would otherwise
// satisfy the comparison.

TEST(ComposeR1TrimParity, ClampWindowWithBothEndsNamed) {
  // A window with a NON-ZERO start. upTo() only ever covers the start == 0
  // case, so this row is where a start offset would go wrong unnoticed.
  auto draw = [](bool useLegacyTrim) {
    Host host(200, 200);
    Element e = revealBox();
    if (useLegacyTrim)
      e.mask(by::spans(spans::range(0.15f, 0.55f))).stroke(stroke(6, red()));
    else
      e.stroke(spans::range(0.15f, 0.55f), stroke(6, red()));
    host.composer.render(stack().child(std::move(e)));
    host.frame();
    return boundaryRing(host);
  };
  const std::vector<SkColor> spanned = draw(false);
  EXPECT_EQ(spanned, draw(true));
  EXPECT_GT(inkedCount(spanned), 5u);
  EXPECT_LT(inkedCount(spanned), spanned.size());
}

TEST(ComposeR1TrimParity, ClampWindowOutsideZeroToOnePins) {
  // Clamped behaviour: fractions outside [0,1] PIN rather than wrap, and
  // normalizeSpans clamps the same way on both doors.
  auto draw = [](bool useLegacyTrim) {
    Host host(200, 200);
    Element e = revealBox();
    if (useLegacyTrim)
      e.mask(by::spans(spans::range(-0.4f, 0.6f))).stroke(stroke(6, red()));
    else
      e.stroke(spans::range(-0.4f, 0.6f), stroke(6, red()));
    host.composer.render(stack().child(std::move(e)));
    host.frame();
    return boundaryRing(host);
  };
  const std::vector<SkColor> pinned = draw(false);
  EXPECT_EQ(pinned, draw(true));
  // Agreement between the two arms cannot by itself tell a pin from a wrap,
  // and two BLANK arms agree perfectly — so the assertions below name
  // specific pixels rather than only comparing the arms.
  EXPECT_GT(inkedCount(pinned), 5u) << "the window painted at all";
  // The discriminator, at named pixels: fraction 0 is the rect's start
  // corner, so the CLAMPED window [0, 0.6] runs out partway round and the
  // far side stays dark. The extra piece a WRAPPED [-0.4, 0.6] would show
  // is exactly that far side. (An inked-fraction bound cannot say this:
  // boundaryRing samples points outside the stroke too, so "not all of the
  // ring" is true of every window.)
  Host probe(200, 200);
  probe.composer.render(stack().child(
      revealBox().stroke(spans::range(-0.4f, 0.6f), stroke(6, red()))));
  probe.frame();
  // Fraction 0 is the bottom-left corner running UP the left edge, so the
  // clamped [0, 0.6] is the left edge, the top edge and the top 40% of the
  // right — and the BOTTOM edge ([0.75, 1]) is the piece a wrapped reading
  // would add.
  EXPECT_NE(probe.pixel(70, 20), SK_ColorBLACK) << "the top edge is inside";
  EXPECT_EQ(probe.pixel(70, 120), SK_ColorBLACK)
      << "…and the bottom edge is not: [-0.4, 0.6] PINNED, it did not wrap";
}

TEST(ComposeR1TrimParity, BoundEndpointsScrubTheSameWindow) {
  // Plain bound endpoints — the case both modes share.
  choreograph::Output<float> begin, end;
  Host trimmed(200, 200), spanned(200, 200);
  trimmed.composer.render(
      stack().child(revealBox()
                        .mask(by::spans(spans::range(&begin, &end)))
                        .stroke(stroke(6, red()))));
  spanned.composer.render(stack().child(
      revealBox().stroke(spans::range(&begin, &end), stroke(6, red()))));
  for (auto [b, e] : {std::pair{0.0f, 0.2f}, std::pair{0.3f, 0.9f},
                      std::pair{0.45f, 0.55f}}) {
    begin = b;
    end = e;
    trimmed.frame();
    spanned.frame();
    const std::vector<SkColor> ring = boundaryRing(spanned);
    EXPECT_EQ(ring, boundaryRing(trimmed)) << "window " << b << ".." << e;
    // The liveness bound the sibling rows carry: two blank renders agree.
    EXPECT_GT(inkedCount(ring), 5u)
        << "window " << b << ".." << e << " painted nothing at all";
  }
}

TEST(ComposeR1TrimParity, TheOffsetArgumentIsEndpointArithmetic) {
  // The gate's third argument. A CONSTANT offset is just addition at the
  // call site; a BOUND offset over constant ends is
  // `motion::bind(&off).offset(k)` on each end. Both are checked against the
  // gate carrying the offset itself, which is what makes them spellings rather
  // than approximations.
  choreograph::Output<float> off;
  Host constTrim(200, 200), constSpan(200, 200);
  constTrim.composer.render(
      stack().child(revealBox()
                        .mask(by::spans(spans::range(0.1f, 0.4f).offset(0.25f)))
                        .stroke(stroke(6, red()))));
  constSpan.composer.render(stack().child(revealBox().stroke(
      spans::range(0.1f + 0.25f, 0.4f + 0.25f), stroke(6, red()))));
  constTrim.frame();
  constSpan.frame();
  EXPECT_EQ(boundaryRing(constSpan), boundaryRing(constTrim))
      << "constant offset";

  Host boundTrim(200, 200), boundSpan(200, 200);
  boundTrim.composer.render(
      stack().child(revealBox()
                        .mask(by::spans(spans::upTo(0.3f).offset(&off)))
                        .stroke(stroke(6, red()))));
  boundSpan.composer.render(stack().child(revealBox().stroke(
      spans::range(motion::bind(&off), motion::bind(&off).offset(0.3f)),
      stroke(6, red()))));
  for (float v : {0.0f, 0.17f, 0.42f, 0.61f}) {
    off = v;
    boundTrim.frame();
    boundSpan.frame();
    EXPECT_EQ(boundaryRing(boundSpan), boundaryRing(boundTrim))
        << "bound offset " << v;
  }
}

TEST(ComposeR1TrimParity, AnimatedEndpointsRampTheSameWindow) {
  // Composer-manufactured endpoints under Clamp: both doors must ramp the
  // same window on the same frames.
  auto host = [](bool useLegacyTrim) {
    auto h = std::make_unique<Host>(200, 200);
    Element e = revealBox();
    if (useLegacyTrim)
      e.mask(by::spans(
                 spans::upTo(animate(motion::from(0.0f).to(1.0f), {800ms}))))
          .stroke(stroke(6, red()));
    else
      e.stroke(spans::upTo(animate(motion::from(0.0f).to(1.0f), {800ms})),
               stroke(6, red()));
    h->composer.render(stack().child(std::move(e)));
    return h;
  };
  std::unique_ptr<Host> t = host(true), s = host(false);
  size_t lastInk = 0;
  for (int step = 0; step < 6; ++step) {
    t->frame(0.13);
    s->frame(0.13);
    const std::vector<SkColor> ring = boundaryRing(*s);
    EXPECT_EQ(ring, boundaryRing(*t)) << "step " << step;
    lastInk = inkedCount(ring);
  }
  // The liveness bound: the ramp has run to 0.78 of an 800 ms entrance by
  // the last step, so a render that painted nothing cannot pass here.
  EXPECT_GT(lastInk, 5u) << "the ramp never painted";
}

TEST(ComposeR1TrimParity, OnePassPerClaimIsTheNPassRule) {
  // A node gate reveals EVERY outline-following decoration at once, while a
  // span claims exactly ONE pass — and two passes claiming the same run is
  // the loud error. So revealing several marks together is spelled as one
  // pass carrying a COMPOSITE brush. That is a different spelling, not a
  // missing capability.
  Host host(200, 200);
  host.composer.render(stack().child(revealBox().stroke(
      spans::upTo(0.4f),
      brush::layers({brush::solid(8, red()), brush::solid(3, green())}))));
  host.frame();
  EXPECT_EQ(host.pixel(40, 20), SK_ColorGREEN) << "both marks, one claim";
  EXPECT_EQ(host.pixel(110, 20), SK_ColorBLACK) << "and the claim ends";
}

// ---------------------------------------------------------------------------
// The harder parity rows: a third live term, background-half passes, seams,
// and wrap under the overlap law.

TEST(ComposeR2Offset, TwoLiveSourcesSummedIntoOneEndpointMatchTrim) {
  // A bound endpoint holds ONE source pointer, so endpoint arithmetic alone
  // cannot express two independently driven values summed into one endpoint
  // — a window that both scrubs and marches. `Spans::offset()` is that third
  // live term, and this checks the sum is the same sum on both doors.
  choreograph::Output<float> begin, end, off;
  Host trimmed(200, 200), spanned(200, 200);
  trimmed.composer.render(stack().child(
      revealBox()
          .mask(by::spans(spans::range(&begin, &end).offset(&off)))
          .stroke(stroke(6, red()))));
  spanned.composer.render(stack().child(revealBox().stroke(
      spans::range(&begin, &end).offset(&off), stroke(6, red()))));
  for (auto [b, e, o] :
       {std::tuple{0.0f, 0.3f, 0.0f}, std::tuple{0.0f, 0.3f, 0.25f},
        std::tuple{0.1f, 0.5f, -0.05f}, std::tuple{0.4f, 0.45f, 0.5f},
        std::tuple{0.2f, 0.9f, 0.3f}}) {
    begin = b;
    end = e;
    off = o;
    trimmed.frame();
    spanned.frame();
    const std::vector<SkColor> want = boundaryRing(trimmed);
    EXPECT_EQ(boundaryRing(spanned), want)
        << "begin " << b << " end " << e << " offset " << o;
  }
}

TEST(ComposeR2Offset, TheSummedEndpointWrapsLikeTrimDoes) {
  // The same row in Wrap mode — where the offset is the marching term and
  // the ends are the window, each on its own Output.
  choreograph::Output<float> begin, end, off;
  Host trimmed(200, 200), spanned(200, 200);
  trimmed.composer.render(
      stack().child(revealBox()
                        .mask(by::spans(spans::wrap(&begin, &end).offset(&off)))
                        .stroke(stroke(6, red()))));
  spanned.composer.render(stack().child(revealBox().stroke(
      spans::wrap(&begin, &end).offset(&off), stroke(6, red()))));
  begin = 0.0f;
  end = 0.22f;
  for (float o : {0.0f, 0.15f, 0.44f, 0.7f, 0.88f, 0.95f, 1.3f}) {
    off = o;
    trimmed.frame();
    spanned.frame();
    const std::vector<SkColor> want = boundaryRing(trimmed);
    EXPECT_EQ(boundaryRing(spanned), want) << "offset " << o;
    EXPECT_GT(inkedCount(want), 3u) << "offset " << o << " drew nothing";
  }
  // …and the window can breathe while it marches: BOTH drive at once, which
  // is the whole point of the row.
  for (auto [e, o] : {std::pair{0.1f, 0.2f}, std::pair{0.35f, 0.6f},
                      std::pair{0.05f, 0.93f}}) {
    end = e;
    off = o;
    trimmed.frame();
    spanned.frame();
    EXPECT_EQ(boundaryRing(spanned), boundaryRing(trimmed))
        << "end " << e << " offset " << o;
  }
}

TEST(ComposeR2Offset, TheOffsetIsAComparableEndpointLikeTheOthers) {
  // The offset participates in equality exactly as begin/end do. Without it
  // a claim that only SLIDES compares equal frame to frame, prunes, and
  // freezes at its first position — the marching reveal simply stops, with
  // nothing to indicate why.
  EXPECT_TRUE(spans::range(0.1f, 0.4f).offset(0.2f) ==
              spans::range(0.1f, 0.4f).offset(0.2f));
  EXPECT_FALSE(spans::range(0.1f, 0.4f).offset(0.2f) ==
               spans::range(0.1f, 0.4f).offset(0.3f));
  EXPECT_FALSE(spans::range(0.1f, 0.4f).offset(0.2f) ==
               spans::range(0.1f, 0.4f));
  // And a constant offset is genuinely the same claim as the arithmetic —
  // the two spellings describe one window, so they must also COMPARE equal
  // to their own endpoint-shifted twin at resolve time.
  const SkPath boundary = unitBox();
  SpanInput in;
  in.outline = &boundary;
  const std::vector<float> shifted{0.1f, 0.4f, 0.25f};
  in.values = &shifted;
  const std::vector<Span> withOffset =
      spans::range(0.1f, 0.4f).offset(0.25f).resolve(in);
  ASSERT_EQ(withOffset.size(), 1u);
  EXPECT_NEAR(withOffset[0].begin, 0.35f, 1e-4f);
  EXPECT_NEAR(withOffset[0].end, 0.65f, 1e-4f);
}

TEST(ComposeR2Background, TrimmedBackgroundFollowerHasASpanSpelling) {
  // A node gate reveals BACKGROUND-slot followers too, so a span pass needs
  // a background twin: without one, every span-claimed mark is forced above
  // the children. Same claim, same brush, opposite z-half.
  auto draw = [](bool useLegacyTrim) {
    Host host(200, 200);
    Element e = revealBox();
    if (useLegacyTrim)
      e.mask(by::spans(spans::upTo(0.45f))).background(stroke(6, red()));
    else
      e.background(spans::upTo(0.45f), stroke(6, red()));
    host.composer.render(stack().child(std::move(e)));
    host.frame();
    return boundaryRing(host);
  };
  const std::vector<SkColor> spanned = draw(false);
  EXPECT_EQ(spanned, draw(true));
  EXPECT_GT(inkedCount(spanned), 5u);
  EXPECT_LT(inkedCount(spanned), spanned.size());
}

TEST(ComposeR2Background, ThePassPaintsUNDERTheChildren) {
  // The z-order is the entire reason the slot exists, so it is pinned:
  // the same brush in the two halves lands on opposite sides of a child.
  auto topPixel = [](bool asBackground) {
    Host host(200, 200);
    Element e = revealBox();
    if (asBackground)
      e.background(spans::every(1), stroke(10, red()));
    else
      e.stroke(spans::every(1), stroke(10, red()));
    // A child straddling the top edge, opaque, painted between the halves.
    e.child(
        box().absolute().rect(SkRect::MakeXYWH(30, -6, 40, 12)).fill(green()));
    host.composer.render(stack().child(std::move(e)));
    host.frame();
    return host.pixel(70, 20);
  };
  EXPECT_EQ(topPixel(true), SK_ColorGREEN) << "background half: child on top";
  EXPECT_EQ(topPixel(false), SK_ColorRED) << "foreground half: mark on top";
}

TEST(ComposeR2Background, OneBoundaryIsOneClaimLedgerAcrossBothHalves) {
  // The halves share a ledger because they share a BOUNDARY. Two passes
  // claiming the same run is the same mistake whichever half they paint in,
  // and it is still said out loud.
  ::testing::internal::CaptureStderr();
  {
    Host host(200, 200);
    host.composer.render(stack().child(
        revealBox()
            .background(spans::every(1), stroke(4, red()), "under")
            .stroke(spans::upTo(0.5f), stroke(2, green()), "over")));
    host.frame();
  }
  const std::string log = ::testing::internal::GetCapturedStderr();
  EXPECT_NE(log.find("under"), std::string::npos) << log;
  EXPECT_NE(log.find("over"), std::string::npos) << log;
}

TEST(ComposeR2Background, RestReadsAcrossTheHalvesToo) {
  // rest() is the complement of what the OTHER claiming passes took — and
  // "the other passes" is the whole ledger, not this half of it.
  Host host(200, 200);
  host.composer.render(
      stack().child(revealBox()
                        .background(spans::upTo(0.25f), stroke(6, red()))
                        .stroke(spans::rest(), stroke(6, green()))));
  host.frame();
  // The seam (fraction 0) of an rrect outline is its BOTTOM-LEFT corner and
  // the boundary runs UP the left edge from there, so the first quarter of
  // the 400 px perimeter is that edge; rest() gets everything after it.
  EXPECT_EQ(host.pixel(20, 60), SK_ColorRED) << "the background claim";
  EXPECT_EQ(host.pixel(60, 20), SK_ColorGREEN) << "rest() took the remainder";
}

TEST(ComposeR2Seam, AWholeContourClaimKeepsItsCornerJoin) {
  // A claim covering the WHOLE contour must be re-closed. SkContourMeasure's
  // getSegment returns an OPEN run whose ends merely coincide, so stroking it
  // as-is puts two butt caps at the seam vertex where a miter join belongs —
  // a notch two pixels wide, at ONE corner, invisible until a wide additive
  // brush walks over it.
  //
  // THE SEAM IS THE BOTTOM-LEFT CORNER: `addRRect` starts at index 3 and runs
  // up the left edge, so fraction 0 sits at (left, bottom). Sampling any
  // other corner makes this test vacuous — every other corner is mid-run and
  // joins correctly whether or not the contour was closed, so the assertions
  // would pass with the close() removed.
  auto corner = [](int form) {
    Host host(200, 200);
    Element e = revealBox();
    PathFormat wide = stroke(12, red());
    wide.join = SkPaint::kMiter_Join;
    switch (form) {
      case 0:
        e.stroke(std::move(wide));
        break;  // untrimmed truth
      case 1:
        e.stroke(spans::every(1), std::move(wide));
        break;
      case 2:
        e.stroke(spans::range(0.0f, 1.0f), std::move(wide));
        break;
      case 3:
        e.stroke(spans::wrap(0.0f, 1.0f), std::move(wide));
        break;
      default:
        e.stroke(spans::rest(), std::move(wide));
        break;
    }
    host.composer.render(stack().child(std::move(e)));
    host.frame();
    std::vector<SkColor> out;
    // The miter's own square at the SEAM corner (20, 120), reaching out to
    // the 6 px the 12 px stroke throws beyond it: a butt-capped pair leaves
    // that outer square uncovered, so these are exactly the pixels that
    // differ.
    for (int y = 120; y <= 125; ++y)
      for (int x = 15; x <= 20; ++x) out.push_back(host.pixel(x, y));
    return out;
  };
  const std::vector<SkColor> truth = corner(0);
  EXPECT_EQ((size_t)std::count(truth.begin(), truth.end(), SK_ColorRED),
            truth.size())
      << "an untrimmed miter fills its own corner square";
  EXPECT_EQ(corner(1), truth) << "every(1)";
  EXPECT_EQ(corner(2), truth) << "range(0, 1)";
  EXPECT_EQ(corner(3), truth) << "a full-cycle wrap";
  EXPECT_EQ(corner(4), truth) << "bare rest() against nothing";
}

TEST(ComposeR2Wrap, WrapIsUnderTheOverlapLawLikeEveryOtherTerm) {
  // wrap() is the only term that yields TWO runs from one pair of endpoints,
  // which is the stated reason it is its own word. So the law has to read
  // BOTH runs: a claim that overlaps only the piece on the far side of the
  // seam is still an overlap, and must still be loud.
  ::testing::internal::CaptureStderr();
  {
    Host host(200, 200);
    host.composer.render(stack().child(
        revealBox()
            // [0.9, 1] + [0, 0.15]
            .stroke(spans::wrap(0.9f, 1.15f), stroke(4, red()), "ants")
            // touches only the SECOND run
            .stroke(spans::range(0.05f, 0.3f), stroke(2, green()), "keyline")));
    host.frame();
  }
  const std::string log = ::testing::internal::GetCapturedStderr();
  EXPECT_NE(log.find("ants"), std::string::npos) << log;
  EXPECT_NE(log.find("keyline"), std::string::npos) << log;

  // …and a wrap that clears both runs is silent.
  ::testing::internal::CaptureStderr();
  {
    Host host(200, 200);
    host.composer.render(stack().child(
        revealBox()
            .stroke(spans::wrap(0.9f, 1.05f), stroke(4, red()), "ants")
            .stroke(spans::range(0.3f, 0.6f), stroke(2, green()), "keyline")));
    host.frame();
  }
  EXPECT_EQ(::testing::internal::GetCapturedStderr(), "");
}

TEST(ComposeR2Wrap, RestIsTheComplementOfBothOfWrapsRuns) {
  // rest() reads RESOLVED runs, so a seam-crossing claim leaves rest() a
  // single interval in the middle — not two, and not the naive [end, begin].
  Host host(200, 200);
  host.composer.render(
      stack().child(revealBox()
                        .stroke(spans::wrap(0.9f, 1.15f), stroke(6, red()))
                        .stroke(spans::rest(), stroke(6, green()))));
  host.frame();
  // Perimeter 400 px, seam at the BOTTOM-LEFT corner, running UP the left
  // edge: [0.9,1] is the last 40 px of the bottom edge, arriving at the
  // seam, and [0,0.15] is the left edge's first 60 px leaving it.
  EXPECT_EQ(host.pixel(40, 120), SK_ColorRED) << "the run before the seam";
  EXPECT_EQ(host.pixel(20, 90), SK_ColorRED) << "the run after it";
  EXPECT_EQ(host.pixel(20, 40), SK_ColorGREEN) << "rest() took the middle";
  EXPECT_EQ(host.pixel(70, 20), SK_ColorGREEN) << "…all the way round";
  // The two runs meet AT the seam and nowhere else: the pixel on the
  // corner itself is claimed, which is what makes them one term rather
  // than two claims that happen to abut.
  EXPECT_EQ(host.pixel(20, 120), SK_ColorRED) << "the seam vertex itself";
}

TEST(ComposeR2Volatility, ALiveMaterialOnASpanPassDeclaresItself) {
  // spanVolatile reads the PASS BRUSH's isAnimated(), which means a live
  // MATERIAL on a span pass must declare itself just as a bound endpoint
  // does: a stroke whose colour comes from a uTime shader has to repaint
  // every frame with no re-describe anywhere. The static arm is the other
  // half — declaring volatility unconditionally would be equally wrong.
  auto paintedPerFrame = [](bool live) {
    Host host(200, 200);
    PathFormat mark = stroke(8, red());
    mark.strokeMaterial = material::skia::Paint::sksl(heavyEffect(live));
    host.composer.render(
        stack().child(revealBox().stroke(spans::upTo(0.6f), std::move(mark))));
    host.frame();
    host.frame();  // no re-describe: only declared volatility can paint now
    return host.composer.stats().nodesPainted;
  };
  EXPECT_GT(paintedPerFrame(true), 0u)
      << "a live stroke material on a span pass must declare isAnimated()";
  EXPECT_EQ(paintedPerFrame(false), 0u) << "…and a static one must still cache";
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

TEST(ComposeR1Ribbon, TheInsideOfATightBendIsFilled) {
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

TEST(ComposeR1Ribbon, TheJoinShapesTheOutsideOfTheCorner) {
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
  const SkPoint onTheArc{92.0f, 68.0f};     // 17 px out — past the chord
  const SkPoint atThePoint{97.0f, 63.0f};   // 24 px out — past the arc
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

TEST(ComposeR1Ribbon, WidthAlongMeasuresTheBandTheRibbonDrew) {
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

TEST(ComposeR1Ribbon, WidthAlongSkipsTheCapsAndSeesTheCorner) {
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
