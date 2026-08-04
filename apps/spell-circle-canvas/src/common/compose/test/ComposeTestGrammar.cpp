#include "ComposeTestSupport.h"

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
float coverage(const std::vector<Span> &spans) {
  float total = 0;
  for (const Span &s : spans)
    total += s.end - s.begin;
  return total;
}

/** Do two claim sets share more than float noise? (The library's own
 *  overlap test is internal; this is the same predicate, spelled here so
 *  the test asserts the PROPERTY rather than the implementation.) */
bool disjoint(const std::vector<Span> &a, const std::vector<Span> &b) {
  for (const Span &x : a)
    for (const Span &y : b)
      if (std::min(x.end, y.end) - std::max(x.begin, y.begin) > 1e-4f)
        return false;
  return true;
}

SkPath unitBox() {
  SkPathBuilder b;
  b.addRect(SkRect::MakeWH(100, 100));
  return b.detach();
}

} // namespace

// RENAMED 2026-07-28 (audit): the OutlineIsGone half asserted nothing — a
// deleted word is a compile-time fact and this test only ever exercised the
// surviving one.
TEST(ComposeShapeRename, ShapeOverridesTheBox) {
  // `outline()` was deleted in R3; `shape()` is the one spelling, and what
  // it does is override the node's rect with a generated path.
  Host host(200, 200);
  Element e = box().rect(SkRect::MakeXYWH(20, 20, 100, 100)).fill(red());
  e.shape(shapes::circle());
  host.composer.render(stack().child(std::move(e)));
  host.frame();
  EXPECT_EQ(host.pixel(70, 70), SK_ColorRED) << "inside the circle";
  EXPECT_EQ(host.pixel(24, 24), SK_ColorBLACK)
      << "the rect's corner is outside the shape";
}

TEST(ComposeSpans, CornersAndEdgesPartitionTheBoundary) {
  // The claim algebra, read directly: corners() and edges() are one scan
  // seen two ways, so together they cover the boundary exactly once. This
  // is the four-way duplicate the audit filed (decorations::brackets /
  // gappedRule / lines::cornerBrackets / cornerGaps) answered as one
  // vocabulary.
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
                        .stroke(spans::corners(20),
                                util::stroke(6, red()))));
  host.frame();
  EXPECT_EQ(host.pixel(30, 20), SK_ColorRED) << "10px along the top edge";
  EXPECT_EQ(host.pixel(70, 20), SK_ColorBLACK) << "the middle of a run";
  EXPECT_EQ(host.pixel(20, 30), SK_ColorRED) << "and down the left edge";
}

TEST(ComposeSpans, PassesAppendAndRestFillsTheGaps) {
  // Two calls, no arithmetic: the corners get one mark and everything
  // else gets the other. This is thaumonomicon's innerRule() (a ten-line
  // "rect minus corners" path generator) as two lines.
  Host host(200, 200);
  host.composer.render(
      stack().child(box()
                        .rect(SkRect::MakeXYWH(20, 20, 100, 100))
                        .stroke(spans::corners(20), util::stroke(6, red()))
                        .stroke(spans::rest(), util::stroke(6, green()))));
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
    host.composer.render(
        stack().child(box()
                          .rect(SkRect::MakeXYWH(20, 20, 100, 100))
                          .stroke(spans::every(1), util::stroke(4, red()),
                                  "halo")
                          .stroke(spans::upTo(0.5f), util::stroke(2, green()),
                                  "keyline")));
    host.frame();
  }
  const std::string log = ::testing::internal::GetCapturedStderr();
  EXPECT_NE(log.find("halo"), std::string::npos) << log;
  EXPECT_NE(log.find("keyline"), std::string::npos) << log;
}

TEST(ComposeSpans, UnqualifiedStrokesOverlayAndNeverCollide) {
  // §27 alias-first: the whole-boundary form does not claim, so the
  // corpus's stacked strokes (a halo under a keyline) stay legal.
  ::testing::internal::CaptureStderr();
  {
    Host host(200, 200);
    host.composer.render(
        stack().child(box()
                          .rect(SkRect::MakeXYWH(20, 20, 100, 100))
                          .stroke(util::stroke(8, red()))
                          .stroke(util::stroke(3, green()))));
    host.frame();
    EXPECT_EQ(host.pixel(70, 20), SK_ColorGREEN) << "the second stroke wins";
  }
  EXPECT_EQ(::testing::internal::GetCapturedStderr(), "");
}

TEST(ComposeSpans, ReorderedTermsPruneBecauseResolveNeverReadsOrder) {
  // §33-f: `corners(8) | at(0, 4)` and `at(0, 4) | corners(8)` claim the
  // SAME runs — resolve() unions its terms and never reads their order —
  // but equality used to walk the term lists index by index, so a
  // describe that reorders terms produced a spurious patch. Never a wrong
  // picture; only a lost prune. Equality is order-insensitive now (a
  // multiset match over the same term comparison), so the reorder PRUNES.
  // (Control for the fix itself: revert Spans::operator== to the indexed
  // walk and the patchedNodes == 0 below fails.)
  const auto tree = [](Spans where) {
    return stack().child(box()
                             .key("m")
                             .rect(SkRect::MakeXYWH(20, 20, 100, 100))
                             .stroke(std::move(where),
                                     util::stroke(4, red()), "marks"));
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
      e.mask(by::spans(spans::upTo(0.4f))).stroke(util::stroke(6, red()));
    else
      e.stroke(spans::upTo(0.4f), util::stroke(6, red()));
    host.composer.render(stack().child(std::move(e)));
    host.frame();
    std::vector<SkColor> out;
    for (int x = 15; x < 130; x += 3)
      out.push_back(host.pixel(x, 20));
    for (int y = 15; y < 130; y += 3)
      out.push_back(host.pixel(120, y));
    return out;
  };
  const std::vector<SkColor> spanned = draw(false);
  EXPECT_EQ(spanned, draw(true));
  // …and it is a genuine partial reveal, not "everything" or "nothing":
  // 0.4 of a 400px perimeter is the whole top edge and 60px of the right.
  const size_t inked = (size_t)std::count_if(
      spanned.begin(), spanned.end(),
      [](SkColor c) { return c != SK_ColorBLACK; });
  EXPECT_GT(inked, 0u);
  EXPECT_LT(inked, spanned.size());
}

TEST(ComposeSpans, AnimatedRevealDrawsOnAndDeclaresVolatility) {
  Host host(200, 200);
  host.composer.render(
      stack().child(box()
                        .rect(SkRect::MakeXYWH(20, 20, 100, 100))
                        .stroke(spans::upTo(animate(from(0.0f).to(1.0f),
                                                    {400ms})),
                                util::stroke(6, red()))));
  host.frame(0.02);
  auto inked = [&] {
    int n = 0;
    for (int x = 15; x < 130; ++x)
      if (host.pixel(x, 20) == SK_ColorRED)
        ++n;
    return n;
  };
  const int early = inked();
  host.frame(0.3);
  EXPECT_GT(inked(), early) << "the reveal did not advance";
}

TEST(ComposeSpans, FitSizesAGapFromKeyedContent) {
  // The derive pass, applied to a boundary: the pass claims exactly the
  // run the keyed element covers (the flowAround pattern).
  //
  // §33-i, RESOLVED 2026-08-04: this test used to call frame() TWICE
  // before its first assertion, with a note that nobody knew whether that
  // was correct derive timing or a user-visible one-frame lag. It is
  // NEITHER — the extra frame was never load-bearing. The derive pass
  // runs INSIDE ensureLayout's convergence rounds (Layout.cpp), which
  // draw() executes BEFORE paint in the SAME frame, so the fit rects a
  // stroke pass sizes its gap from are resolved by the first paint that
  // could read them — and ensureLayout was already shaped that way at the
  // commit that introduced this test (77f0d8d). Both halves are pinned
  // below with ONE frame each: the first paint after describe, and — the
  // half a harness quirk could not excuse — the first paint after a
  // CONTENT CHANGE (the keyed element moves; a one-frame lag would show
  // the gap at the label's old position).
  Host host(200, 200);
  auto scene = [](SkRect label) {
    return stack()
        .child(box().key("lbl").rect(label))
        .child(box()
                   .rect(SkRect::MakeXYWH(20, 20, 100, 100))
                   .stroke(spans::fit("lbl", 0.0f),
                           util::stroke(6, red())));
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
  EXPECT_TRUE(strand::offset(4) == strand::offset(4));
  EXPECT_FALSE(strand::offset(4) == strand::offset(5));
  EXPECT_FALSE(strand::offset(0) == strand::self());
  EXPECT_TRUE(strand::self() == strand::self());
  EXPECT_TRUE(Profile() == Profile()) << "two empty profiles are one nothing";
  EXPECT_FALSE(Profile() == strand::self());
  EXPECT_TRUE(across(6) == across(6));
  EXPECT_FALSE(across(6) == across(7));
}

TEST(ComposeBand, FormationsTakeTheDeclaredSide) {
  auto draw = [](Formation f) {
    Host host(200, 200);
    Element b = band(rectSpine(), across(10)).rect(
        SkRect::MakeXYWH(20, 20, 100, 100));
    if (f == Formation::Outward)
      b.outward();
    else if (f == Formation::Inward)
      b.inward();
    else
      b.centered();
    host.composer.render(stack().child(b.fill(red())));
    host.frame();
    return std::pair<SkColor, SkColor>{host.pixel(70, 16), host.pixel(70, 24)};
  };
  const auto centred = draw(Formation::Centered);
  EXPECT_EQ(centred.first, SK_ColorRED) << "centered straddles the spine";
  EXPECT_EQ(centred.second, SK_ColorRED);
  const auto out = draw(Formation::Outward);
  EXPECT_EQ(out.first, SK_ColorRED);
  EXPECT_EQ(out.second, SK_ColorBLACK);
  const auto in = draw(Formation::Inward);
  EXPECT_EQ(in.first, SK_ColorBLACK);
  EXPECT_EQ(in.second, SK_ColorRED);
}

TEST(ComposeBand, MultiContourSpinesDoNotBridge) {
  // A single moveTo/lineTo chain across every contour, closed once, fills
  // the gap between them with a chord — two concentric ring spines came out
  // as a filled disc. The rails are zipped and closed PER CONTOUR.
  Host host(400, 400);
  host.composer.render(stack().child(
      band([](SkSize s) {
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
  EXPECT_EQ(host.pixel(200, 200), SK_ColorBLACK)
      << "the middle was filled";
}

// RENAMED 2026-07-28 (audit): one radius cannot show linearity. This is a
// wall-clock ceiling against the measured 700 ms quadratic regression —
// which is what the body says and now what the name says.
TEST(ComposeBand, ConstructionStaysUnderTheQuadraticCeiling) {
  // sampleRail asked bandPointAt per sample, and bandPointAt re-measures the
  // whole path every call — quadratic. Measured at 700 ms for an r=550 ring.
  // The guard is a wall-clock ceiling, deliberately loose enough to survive a
  // contended machine and tight enough that the quadratic form cannot pass.
  auto ring = [](float r) {
    return [r](SkSize s) {
      SkPathBuilder b;
      b.addCircle(s.width() * 0.5f, s.height() * 0.5f, r);
      return b.detach();
    };
  };
  const auto build = [&](float r) {
    Host host(1400, 1400);
    const auto t0 = std::chrono::steady_clock::now();
    host.composer.render(
        stack().child(band(ring(r), across(14)).inset(0).fill(red())));
    host.frame();
    return std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now() - t0)
        .count();
  };
  const double big = build(550.0f);
  EXPECT_LT(big, 250.0) << "r=550 band took " << big << " ms — the quadratic "
                           "construction measured 700 ms here";
}

TEST(ComposeBand, AlongAcrossIsTheBandsOwnSpace) {
  SkPathBuilder b;
  b.moveTo(0, 50);
  b.lineTo(100, 50);
  const SkPath spine = b.detach();
  EXPECT_EQ(bandPointAt(spine, 0.0f, 0), SkPoint::Make(0, 50));
  EXPECT_EQ(bandPointAt(spine, 0.5f, 0), SkPoint::Make(50, 50));
  EXPECT_EQ(bandPointAt(spine, 1.0f, 0), SkPoint::Make(100, 50));
  // across is px on the normal, positive to the LEFT of travel: y is
  // down, so travelling +x, positive across goes UP the screen. That is
  // the same side lines::offsetAcross means — one convention since R3 —
  // asserted here precisely so the two signs cannot drift apart again.
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
                        .stroke(spans::every(1), util::stroke(4, green()))));
  host.frame();
  int inked = 0;
  for (int x = 0; x < 200; ++x)
    for (int y = 0; y < 200; ++y)
      if (host.pixel(x, y) == SK_ColorGREEN)
        ++inked;
  EXPECT_GT(inked, 100) << "a band takes a stroke pass like any shape";
}

#include <type_traits>

// ---------------------------------------------------------------------------
// Stage two: brush kinds, composites, strands, crossings, the shaper seam.

namespace {

/** Two straight strands that cross once, as raw geometry. */
SkPath diagonal(SkPoint a, SkPoint b) {
  SkPathBuilder p;
  p.moveTo(a);
  p.lineTo(b);
  return p.detach();
}

} // namespace

TEST(ComposeCrossings, CoincidentStrandsNeverCross) {
  // This is what layers() IS, so it has to be exact: N copies of one path
  // meet everywhere and cross nowhere.
  SkPathBuilder b;
  b.addRect(SkRect::MakeWH(80, 60));
  const SkPath rect = b.detach();
  EXPECT_TRUE(discoverCrossings({rect, rect}).empty());
  EXPECT_TRUE(discoverCrossings({rect, rect, rect}).empty());
}

TEST(ComposeCrossings, SharedCornersAreMeetingsNotCrossings) {
  // A rectangle's own corners are two edges touching at an endpoint. A
  // discovery pass that counted them would put a knot at every corner of
  // every frame in the corpus.
  SkPathBuilder a;
  a.addRect(SkRect::MakeWH(80, 60));
  SkPathBuilder c;
  c.addRect(SkRect::MakeXYWH(80, 60, 80, 60)); // touches at one point only
  EXPECT_TRUE(discoverCrossings({a.detach(), c.detach()}).empty());
}

TEST(ComposeCrossings, ProperCrossingsAreFoundAndNumberedAlongTheBoundary) {
  const SkPath down = diagonal({0, 0}, {100, 100});
  const SkPath up = diagonal({0, 100}, {100, 0});
  const std::vector<Crossing> one = discoverCrossings({down, up});
  ASSERT_EQ(one.size(), 1u);
  EXPECT_EQ(one[0].a, 0u);
  EXPECT_EQ(one[0].b, 1u);
  EXPECT_NEAR(one[0].at.fX, 50.0f, 2.0f);
  EXPECT_NEAR(one[0].at.fY, 50.0f, 2.0f);
  EXPECT_EQ(one[0].index, 0u);

  // Numbering is positional along the lowest-indexed strand, so a horizontal
  // strand crossed by two verticals numbers them left to right.
  const SkPath across = diagonal({0, 50}, {100, 50});
  const std::vector<Crossing> two =
      discoverCrossings({across, diagonal({70, 0}, {70, 100}),
                         diagonal({30, 0}, {30, 100})});
  ASSERT_EQ(two.size(), 2u);
  EXPECT_LT(two[0].alongA, two[1].alongA);
  EXPECT_EQ(two[0].index, 0u);
  EXPECT_EQ(two[1].index, 1u);
  EXPECT_EQ(two[0].b, 2u) << "the x=30 strand is met first";
}

TEST(ComposeCrossings, TheRuleLadderClimbs) {
  Crossing c0, c1, c2;
  c0.index = 0; c0.a = 0; c0.b = 1;
  c1.index = 1; c1.a = 0; c1.b = 1;
  c2.index = 2; c2.a = 1; c2.b = 2;

  // Rung 0 — list order: the later strand is on top, so `a` is under.
  EXPECT_EQ(CrossingRule{}.decide(c0), Order::Under);

  // Rung 1 — alternate IS sequence({Over, Under}).
  const CrossingRule alt = crossing::alternate();
  EXPECT_EQ(alt, crossing::sequence({Order::Over, Order::Under}));
  EXPECT_EQ(alt.decide(c0), Order::Over);
  EXPECT_EQ(alt.decide(c1), Order::Under);

  // Rung 2 — a generic repeating pattern.
  const CrossingRule three =
      crossing::sequence({Order::Over, Order::Over, Order::Under});
  EXPECT_EQ(three.decide(c0), Order::Over);
  EXPECT_EQ(three.decide(c1), Order::Over);
  EXPECT_EQ(three.decide(c2), Order::Under);

  // Rung 3 — strand dominance, including a CYCLE (the Penrose case:
  // 0 over 1, 1 over 2, 2 over 0, which no layer order can express).
  const CrossingRule cyclic = crossing::pairs({{0, 1}, {1, 2}, {2, 0}});
  EXPECT_EQ(cyclic.decide(c0), Order::Over);  // 0 over 1
  EXPECT_EQ(cyclic.decide(c2), Order::Over);  // 1 over 2
  Crossing c20;
  c20.a = 0; c20.b = 2;
  EXPECT_EQ(cyclic.decide(c20), Order::Under); // 2 over 0
}

namespace {
/** Strand 0 over strand 1 at every crossing — used where the point is that
 *  the REPAIR works, not which rule chose it. */
struct EveryCrossingRedOnTop {
  bool operator==(const EveryCrossingRedOnTop &) const = default;
  Order decide(const Crossing &) const { return Order::Over; }
};

/** Rung 4: a user rule is a comparable value with the seam's one named
 *  member — never a bare lambda, because a rule is read live. */
struct EverySecondStrandWins {
  size_t winner = 1;
  bool operator==(const EverySecondStrandWins &) const = default;
  Order decide(const Crossing &c) const {
    return c.a == winner ? Order::Over : Order::Under;
  }
};
} // namespace

TEST(ComposeCrossings, CustomRulesAreComparableValues) {
  static_assert(CrossingScheme<EverySecondStrandWins>);
  const CrossingRule mine = EverySecondStrandWins{1};
  Crossing c;
  c.a = 1; c.b = 2;
  EXPECT_EQ(mine.decide(c), Order::Over);
  EXPECT_TRUE(mine == CrossingRule(EverySecondStrandWins{1}));
  EXPECT_FALSE(mine == CrossingRule(EverySecondStrandWins{2}));
  EXPECT_FALSE(mine == crossing::alternate());
}

TEST(ComposeCrossings, PinsComposeOntoTheBaseRule) {
  // One .crossing field: a pin layers over whatever rule is already there
  // rather than becoming a second entry.
  CrossingRule rule = crossing::alternate();
  rule.except(0, Order::Under).except(3, Order::Over);
  Crossing c;
  c.a = 0; c.b = 1;
  c.index = 0;
  EXPECT_EQ(rule.decide(c), Order::Under) << "pinned against alternate";
  c.index = 1;
  EXPECT_EQ(rule.decide(c), Order::Under) << "base rule still runs";
  c.index = 2;
  EXPECT_EQ(rule.decide(c), Order::Over);
  c.index = 3;
  EXPECT_EQ(rule.decide(c), Order::Over) << "second pin";
  // Re-pinning the same index REPLACES it (one answer per crossing).
  rule.except(0, Order::Over);
  c.index = 0;
  EXPECT_EQ(rule.decide(c), Order::Over);
  // …and a pinned rule is still a comparable value.
  CrossingRule same = crossing::alternate();
  same.except(0, Order::Over).except(3, Order::Over);
  EXPECT_TRUE(rule == same);
  EXPECT_FALSE(rule == crossing::alternate());
}

TEST(ComposeComposites, LayersIsWeaveWithCoincidentSelfStrands) {
  // FORMALLY one machine. Same pixels from both spellings, and the layers
  // form really is a weave of self-strands.
  const brush::Weave stacked = brush::layers(
      {brush::solid(8, red()), brush::solid(3, green())});
  ASSERT_EQ(stacked.strands.size(), 2u);
  EXPECT_EQ(stacked.strands[0].path, StrandPath(strand::self()));
  EXPECT_EQ(stacked.strands[1].path, StrandPath(strand::self()));

  const brush::Weave woven = brush::weave(
      {brush::Strand{strand::self(), brush::solid(8, red())},
       brush::Strand{strand::self(), brush::solid(3, green())}},
      CrossingRule{});

  auto draw = [](const brush::Weave &w) {
    Host host(200, 200);
    host.composer.render(
        stack().child(box().rect(SkRect::MakeXYWH(40, 40, 100, 100)).stroke(w)));
    host.frame();
    std::vector<SkColor> out;
    for (int x = 30; x < 150; x += 3)
      out.push_back(host.pixel(x, 40));
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
  auto draw = [](CrossingRule rule) {
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
  EXPECT_EQ(draw(CrossingRule{}), SK_ColorGREEN) << "list order: later on top";
  EXPECT_EQ(draw(crossing::alternate()), SK_ColorRED) << "rule flipped it";
  CrossingRule pinned = crossing::alternate();
  pinned.except(0, Order::Under);
  EXPECT_EQ(draw(pinned), SK_ColorGREEN) << "the pin overrode the rule";
}

// RENAMED 2026-07-28 (audit): the Inner half is conceded untested in this
// test's own closing comment (every arm is Align::Center, because an open
// rail has no inside) — Inner is covered by
// ReachReportsTheMarkWhereBleedReportsNothing below. The dead align
// parameter went with the name.
TEST(ComposeComposites, TheRepairCoversShallowCrossings) {
  // The disc this replaced under-covered at a SHALLOW angle: the two marks
  // overlap in a long lens whose extent goes as reach/sin(theta), so a disc
  // sized for the perpendicular case left the under-strand showing straight
  // across the over-strand.
  //
  // Checked by sampling ALONG the over-strand through the meeting: every
  // sample must be the over-strand's colour.
  //
  // The two strands are one segment rotated by +/- half the crossing angle
  // about the centre — NOT a shared dx with dy = dx*tan(angle), which sends
  // the coordinates to infinity at 90 degrees (it hung the first draft).
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
        {brush::Strand{strand::path(through(dirA)), util::stroke(9, red())},
         brush::Strand{strand::path(through(dirB)),
                       util::stroke(9, green())}},
        crossing::alternate()); // strand 0 (red) passes OVER at crossing 0
    host.composer.render(stack().child(box().inset(0).stroke(w)));
    host.frame();
    int wrong = 0;
    for (int i = -40; i <= 40; ++i) {
      const int x = (int)std::lround(mid.fX + dirA.x() * (float)i);
      const int y = (int)std::lround(mid.fY + dirA.y() * (float)i);
      if (host.pixel(x, y) != SK_ColorRED)
        ++wrong;
    }
    return wrong;
  };
  EXPECT_EQ(interruptions(90.0f), 0)
      << "even the perpendicular case exceeded the old derived radius";
  EXPECT_EQ(interruptions(45.0f), 0);
  EXPECT_EQ(interruptions(12.5f), 0)
      << "12.5 degrees: the disc's measured failure";
  // (Align::Inner is checked separately, below: it is meaningless on an OPEN
  // strand — an open rail has no inside — so this geometry cannot show it,
  // and the parameter that pretended it could is gone.)
}

TEST(ComposeComposites, ReachReportsTheMarkWhereBleedReportsNothing) {
  // The f-caveat, directly: bleed() is the CULL's number and an Align::Inner
  // stroke escapes the node by nothing while painting a mark `width` wide.
  // A repair region derived from bleed() was therefore nonsense for it.
  const Decoration inner = util::stroke(9, red(), PathFormat::Align::Inner);
  EXPECT_EQ(inner.bleed(), 0.0f) << "unchanged: it escapes nothing";
  EXPECT_EQ(inner.reach(), 9.0f) << "…but the mark is 9px wide";
  const Decoration centred = util::stroke(9, red());
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
                     util::stroke(9, red(), PathFormat::Align::Inner)},
       brush::Strand{strand::path(circle(240, 200, 90)),
                     util::stroke(9, green(), PathFormat::Align::Inner)}},
      CrossingRule(EveryCrossingRedOnTop{}));
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
      box().rect(SkRect::MakeXYWH(40, 40, 100, 100))
          .stroke(brush::weave({brush::Strand{strand::path(diagonal({0, 0}, {100, 0})),
                                       brush::solid(6, red())}},
                               CrossingRule{}))));
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
  const SkPath out = profileOffset(rect, strand::offset(10));
  const SkPath in = profileOffset(rect, strand::offset(-10));
  EXPECT_GT(out.getBounds().width(), rect.getBounds().width());
  EXPECT_LT(in.getBounds().width(), rect.getBounds().width());
  // self() is the boundary itself.
  EXPECT_EQ(strand::self().max(), 0.0f);
}

TEST(ComposeStrands, BorrowedStrandsRideTheDerivePass) {
  Host host(200, 200);
  host.composer.render(
      stack()
          .child(box().key("guide").rect(SkRect::MakeXYWH(60, 20, 80, 40)))
          .child(box().rect(SkRect::MakeXYWH(20, 20, 160, 160))
                     .stroke(brush::weave({brush::Strand{strand::from("guide"),
                                                  brush::solid(6, red())}},
                                          CrossingRule{}))));
  host.frame();
  host.frame(); // derive resolves against the first layout
  // The guide's own box outline, painted in the host's local space.
  EXPECT_EQ(host.pixel(100, 20), SK_ColorRED);
  EXPECT_EQ(host.pixel(100, 60), SK_ColorRED);
  EXPECT_EQ(host.pixel(100, 120), SK_ColorBLACK) << "nothing else moved";
}

// TRIMMED + RENAMED 2026-07-28 (audit): three of the four static_asserts
// compared a type to ITSELF — R3's rename sweep turned the brushes::-identity
// pins into tautologies when it deleted the second spelling. The one that
// still says something (brush::Solid IS PathFormat, the §27 no-behaviour-
// change claim) stays, with the value equality under it.
TEST(ComposeBrushKinds, SolidIsPathFormatUnderItsTaughtName) {
  // Naming alignment only — no behaviour change, and the legacy spelling
  // is the SAME type (§27).
  static_assert(std::is_same_v<brush::Solid, PathFormat>);
  const brush::Solid a = brush::solid(2, red());
  const PathFormat b = util::stroke(2, red());
  EXPECT_TRUE(a == b) << "one value, two spellings";
}

TEST(ComposeComposites, ClosedStrandsWrapAtTheirSeam) {
  // A CYCLE has no far end: two knots at along 0.02 and 0.98 sit 4% apart,
  // not 96%. Treating the fractions as linear made crossings that straddle
  // the seam read as maximally distant, which vanished the territory bound
  // and let the two lenses merge — both knots of two overlapping rings then
  // came out in ONE colour.
  //
  // The reviewer's repro, exactly: r=100 at (200,200) crossed by r=13 at
  // (288,200). Both strands closed, and BOTH neighbours across their seams.
  auto circle = [](float cx, float cy, float r) {
    SkPathBuilder p;
    p.addCircle(cx, cy, r);
    return p.detach();
  };
  const SkPath big = circle(200, 200, 100);
  const SkPath small = circle(288, 200, 13);

  Host host(400, 400);
  host.composer.render(stack().child(
      box().inset(0).stroke(brush::weave(
          {brush::Strand{strand::path(big), util::stroke(6, red())},
           brush::Strand{strand::path(small), util::stroke(6, green())}},
          crossing::alternate()))));
  host.frame();

  const std::vector<Crossing> knots = discoverCrossings({big, small});
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
      box().rect(SkRect::MakeXYWH(40, 40, 100, 100))
          .stroke(brush::weave({brush::Strand{strand::self(), inner},
                                brush::Strand{strand::offset(12),
                                       brush::solid(2, blue())}},
                               CrossingRule{}))));
  host.frame();
  EXPECT_EQ(host.pixel(90, 40), SK_ColorGREEN) << "the nested layers' top";
  EXPECT_EQ(host.pixel(90, 28), SK_ColorBLUE) << "the offset strand, outside";
}

// ---------------------------------------------------------------------------
// PHASE R1 — the ruled spellings, landed additively (ROADMAP §33)
//
// Every test here is a pair: the new spelling must describe EXACTLY what the
// old one described, because R2 ports the corpus onto these words and R3
// deletes the old ones. A difference found here is a difference that would
// ship as a silent picture change.

#include <sigilcompose/Instances.h>


// ---- 1. animate(to(v), spec) ----------------------------------------------

TEST(ComposeR1Animate, AnimateToIsTheChangeRamp) {
  // Read the property MID-RAMP, where a snap and a ramp differ. The second
  // arm describes the same value with no animate() at all, which must
  // snap — that is the contrast the deleted with() arm used to provide.
  auto run = [](bool plain) {
    Host host(200, 200);
    auto describe = [&](float opacity) {
      Element inner = box().width(100).height(100).fill(red());
      if (plain)
        inner.opacity(opacity);
      else
        inner.opacity(animate(to(opacity), {200ms}));
      return stack().child(std::move(inner));
    };
    host.composer.render(describe(1.0f));
    host.frame();
    host.composer.render(describe(0.0f)); // the CHANGE
    host.frame(0.1);                      // half way down the ramp
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
  // The doc's whole distinction, as pixels: to() mounts holding its value;
  // from().to() plays a path on first appearance.
  auto mountedOpacity = [](bool withEntrance) {
    Host host(200, 200);
    Element inner = box().width(100).height(100).fill(red());
    if (withEntrance)
      inner.opacity(animate(from(0.0f).to(1.0f), {400ms}));
    else
      inner.opacity(animate(to(1.0f), {400ms}));
    host.composer.render(stack().child(std::move(inner)));
    host.frame(0.001);
    return (int)SkColorGetR(host.pixel(50, 50));
  };
  EXPECT_GT(mountedOpacity(false), 240) << "to() alone must not fade in";
  EXPECT_LT(mountedOpacity(true), 60) << "from().to() is a mount entrance";
}

// ---- 2. isAnimated() ---------------------------------------------------------

namespace {

/** A scheme that declares volatility in THE word — R3 deleted the other
 *  four (animated / animates / isLive / "volatile"), so there is exactly
 *  one spelling left to duck-type. */
struct SaysAnimated {
  bool live = true;
  bool isAnimated() const { return live; }
  void paint(SkCanvas &c, const PaintContext &) const {
    SkPaint p;
    p.setColor4f({1, 0, 0, 1}, nullptr);
    c.drawRect(SkRect::MakeWH(40, 40), p);
  }
  bool operator==(const SaysAnimated &) const = default;
};
/** The same scheme spelling the R1/R2 word. It must NOT satisfy the
 *  concept any more: a scheme that still says `animates()` is silently
 *  static, and a static-by-accident live decoration is the exact defect
 *  the one-word ruling exists to prevent. */
struct SaysTheDeadWord {
  bool live = true;
  bool animates() const { return live; }
  void paint(SkCanvas &, const PaintContext &) const {}
  bool operator==(const SaysTheDeadWord &) const = default;
};

} // namespace

TEST(ComposeR3Volatility, OneWordDeclaresVolatilityAndTheOthersAreGone) {
  static_assert(AnimatedDecoration<SaysAnimated>);
  static_assert(!AnimatedDecoration<SaysTheDeadWord>,
                "the R1/R2 word must not be heard after R3");
  EXPECT_TRUE(Decoration(SaysAnimated{true}).isAnimated());
  EXPECT_FALSE(Decoration(SaysAnimated{false}).isAnimated());
  // And the dead word declares nothing: it wraps, it paints, it is static.
  EXPECT_FALSE(Decoration(SaysTheDeadWord{true}).isAnimated());
}

TEST(ComposeR3Volatility, LibrarySchemesDeclareWithTheOneWord) {
  lines::Line line;
  choreograph::Output<float> phase;
  line.dashPhaseBinding = &phase;
  EXPECT_TRUE(line.isAnimated());

  PathFormat pf;
  EXPECT_FALSE(pf.isAnimated());

  // Material: the fifth spelling folded in — isLive() is gone and the
  // material answers the same question as every scheme, in the same word.
  const Material stat = Material::solid({1, 0, 0, 1});
  EXPECT_FALSE(stat.isAnimated());
}

// ---- 3. Bound::source / ::target -------------------------------------------

TEST(ComposeR1Bound, SourceAndTargetAreTheOldStagesRenamed) {
  choreograph::Output<float> hp;
  hp = 25.0f;
  const BoundFloat named = bind(&hp).source(0, 100).target(-70, 170).value();
  // The stages spelled out by hand — what from()/to() (deleted in R3) did.
  const BoundFloat manual = bind(&hp).source(0, 100).scale(240).offset(-70).value();
  for (float v : {0.0f, 25.0f, 50.0f, 100.0f, 137.0f})
    EXPECT_FLOAT_EQ(named.apply(v), manual.apply(v)) << "at " << v;
  EXPECT_FLOAT_EQ(named.apply(0.0f), -70.0f);
  EXPECT_FLOAT_EQ(named.apply(100.0f), 170.0f);
}

TEST(ComposeR1Bound, WindowIsStillSourceThatClamps) {
  choreograph::Output<float> t;
  const BoundFloat w = bind(&t).window(0.2f, 0.4f).value();
  const BoundFloat s = bind(&t).source(0.2f, 0.4f).value();
  EXPECT_FLOAT_EQ(w.apply(0.3f), s.apply(0.3f));
  EXPECT_FLOAT_EQ(w.apply(0.9f), 1.0f) << "window clamps";
  EXPECT_GT(s.apply(0.9f), 1.0f) << "source does not";
}

// ---- 4. Pool::commit() -----------------------------------------------------

TEST(ComposeR1Pool, CommitPublishesABulkEdit) {
  instancing::Pool pool;
  pool.add({10, 10});
  pool.add({20, 20});
  const uint64_t after = pool.revision();
  for (SkPoint &p : pool.positions())
    p.fY += 5;
  EXPECT_EQ(pool.revision(), after) << "a span write is staging, not publish";
  pool.commit();
  const uint64_t committed = pool.revision();
  EXPECT_NE(committed, after);
  pool.commit();
  EXPECT_NE(pool.revision(), committed) << "each publish is its own revision";
}

// ---- 5. Ribbon on the profile seam ----------------------------------------

// (`ProfileIsComparableAndBoundsItsOwnReach` was deleted by the 2026-07-28
//  audit ruling: comparability, non-equality and bleed() are all remade,
//  with more around them, in ComposeWidthProfile.
//  TheLastNeverPruneRibbonsCanPruneNow and ComposeBand.
//  ProfilesAreComparableAndReflexive.)

TEST(ComposeR1Ribbon, ProfileRibbonPaintsItsBand) {
  Host host(200, 200);
  brush::Ribbon r;
  r.width = Profile(strand::offset(16.0f)); // constant 16px wide
  r.fill = Fill::color({1, 0, 0, 1});
  host.composer.render(stack().child(
      box()
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

// ---- the widthFn → Profile migration --------------------------------------
//
// `widthFn`/`widthMax` are DELETED. The migration was approved knowing it
// moves pixels — a profiled ribbon is `bandRegion()` (rails through
// `profileOffset`, real corner joins on a constant law) and the deleted
// lane sampled the contour and zipped two point lists — so the gate was a
// designer reading before/after plates, not byte identity. What is pinned
// HERE is everything the eye cannot check: that away from corners the two
// constructions agree to the pixel, that the px key holds a law still
// under a reveal, and that the comparability the old pair could never have
// is real.

namespace {
/** The linear taper, spelled as a law on the profile seam. It is exactly
 *  what `widthStart`/`widthEnd` mean, which is what lets the two
 *  constructions be compared on the same picture. */
struct TaperLaw {
  float start = 30.0f, end = 10.0f;
  float across(float along) const { return start + (end - start) * along; }
  float max() const { return std::max(start, end); }
  bool operator==(const TaperLaw &) const = default;
};

/** A law keyed in PX of arc length — the bridge the four ported corpus
 *  sites take. A single 12 px-wide pulse `at` px from the spine's start,
 *  over a 4 px floor: its POSITION is the whole assertion, because under a
 *  reveal a fraction-keyed law would drag it along. */
struct PulseAtPx {
  float at = 40.0f, wide = 12.0f, tall = 24.0f, floorPx = 4.0f;
  static constexpr bool alongIsPx = true;
  float across(float px) const {
    return std::abs(px - at) <= wide * 0.5f ? tall : floorPx;
  }
  float max() const { return std::max(tall, floorPx); }
  bool operator==(const PulseAtPx &) const = default;
};

/** The same pulse keyed in FRACTION of the spine — the spelling the doc
 *  warns against, kept here so the test can show the two diverge under a
 *  reveal and agree without one. */
struct PulseAtFraction {
  float at = 0.4f, wide = 0.12f, tall = 24.0f, floorPx = 4.0f;
  float across(float along) const {
    return std::abs(along - at) <= wide * 0.5f ? tall : floorPx;
  }
  float max() const { return std::max(tall, floorPx); }
  bool operator==(const PulseAtFraction &) const = default;
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
int thicknessAt(Host &host, int x) {
  int lit = 0;
  for (int y = 0; y < 200; ++y)
    if (host.pixel(x, y) != SK_ColorBLACK)
      ++lit;
  return lit;
}

/** The lit rows' centre in column x, or -1. */
float centreAt(Host &host, int x) {
  int lo = -1, hi = -1;
  for (int y = 0; y < 200; ++y)
    if (host.pixel(x, y) != SK_ColorBLACK) {
      if (lo < 0)
        lo = y;
      hi = y;
    }
  return lo < 0 ? -1.0f : 0.5f * (float)(lo + hi);
}
} // namespace

TEST(ComposeWidthProfile, StraightRunsAgreeWithTheLaneTheyReplaced) {
  // AWAY FROM CORNERS the profile lane and the deleted sample-and-zip lane
  // draw the same band, and this quantifies "the same": the taper
  // widthStart=30 → widthEnd=10 is the identical law spelled both ways, so
  // any difference here is construction, not intent.
  //
  // The zip lane still exists — it is what widthStart/widthEnd have always
  // used — so the comparison is live rather than historical.
  auto measure = [](bool profiled) {
    Host host(200, 200);
    brush::Ribbon r;
    r.fill = Fill::color({1, 0, 0, 1});
    r.step = 2.0f;
    if (profiled)
      r.width = Profile(TaperLaw{30.0f, 10.0f});
    else {
      r.widthStart = 30.0f;
      r.widthEnd = 10.0f;
    }
    host.composer.render(stack().child(straightRun(std::move(r))));
    host.frame();
    std::vector<std::pair<int, float>> out;
    for (int x : {40, 70, 100, 130, 160})
      out.push_back({thicknessAt(host, x), centreAt(host, x)});
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
  // THE BRIDGE, and the reason it is not a per-site adapter. A decoration
  // under a reveal is handed the REVEALED contour, so `along` as a
  // fraction is a fraction of what has been drawn SO FAR. Convert with the
  // length the author measured and it is still wrong — the length being
  // sampled is not the length authored. Only the paint-time consumer knows
  // it, so the seam converts: `alongIsPx` makes `across` take arc-length px
  // from the spine's start, which does not move.
  //
  // A pulse 40 px along a 160 px run is the assertion, because its
  // POSITION is what slides.
  auto pulseX = [](bool pxKeyed, float reveal) {
    Host host(200, 200);
    brush::Ribbon r;
    r.fill = Fill::color({1, 0, 0, 1});
    if (pxKeyed)
      r.width = Profile(PulseAtPx{});
    else
      r.width = Profile(PulseAtFraction{});
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

  // Fully revealed, the two spellings are the SAME PICTURE: 40 px of a
  // 160 px run is 0.25 — the fraction law's pulse sits at 0.4, so they are
  // deliberately different laws, and what matters is each one's own
  // behaviour as the reveal grows.
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
         "reason the px key exists (full " << frFull.first << " vs half "
      << frHalf.first << ")";
}

TEST(ComposeWidthProfile, TheLastNeverPruneRibbonsCanPruneNow) {
  // THE COMPARABILITY WIN, pinned. `Ribbon::operator==` used to end
  // `&& !widthFn && !o.widthFn`, so a varying-width ribbon was unequal to
  // ITSELF and its whole band re-recorded on every describe. Every one of
  // the four px-keyed corpus laws is a plain struct now, so two identical
  // descriptions compare equal and the node prunes.
  brush::Ribbon a;
  a.fill = Fill::color({1, 0, 0, 1});
  a.width = Profile(PulseAtPx{});
  brush::Ribbon b = a;
  EXPECT_TRUE(a == b) << "identical laws must compare equal — the prune";
  b.width = Profile(PulseAtPx{.at = 41.0f});
  EXPECT_FALSE(a == b) << "…and a different law must NOT, or it reads stale";

  // The px key is part of the value's TYPE, so two laws that differ only in
  // how they are keyed can never silently compare equal.
  brush::Ribbon c = a;
  c.width = Profile(PulseAtFraction{});
  EXPECT_FALSE(a == c);
  EXPECT_TRUE(Profile(PulseAtPx{}).keyedInPx());
  EXPECT_FALSE(Profile(PulseAtFraction{}).keyedInPx());

  // max() is honoured whichever key it is: the cull grows to the law's own
  // declared reach and nothing has to be told twice.
  EXPECT_FLOAT_EQ(a.bleed(), 24.0f);
  EXPECT_FLOAT_EQ(Profile(PulseAtPx{.tall = 90.0f}).max(), 90.0f);
  // acrossAt is the consumer's call: a px law is evaluated at along*length,
  // a fraction law ignores the length entirely.
  EXPECT_FLOAT_EQ(Profile(PulseAtPx{}).acrossAt(0.25f, 160.0f), 24.0f);
  EXPECT_FLOAT_EQ(Profile(PulseAtPx{}).acrossAt(0.25f, 320.0f), 4.0f);
  EXPECT_FLOAT_EQ(Profile(PulseAtFraction{}).acrossAt(0.4f, 160.0f), 24.0f);

  // And the prune OBSERVED, not inferred: an identical re-describe of a
  // profiled ribbon must not re-record its picture. When this pin was
  // written, `.shape()` was an incomparable callable that forced a
  // re-patch (§3), so the strongest available claim was "no MORE
  // recordings than the first draw". Shapes are comparable values now,
  // so the honest pin is the absolute one: an identical re-describe
  // records NOTHING.
  {
    Host host;
    auto tree = [] {
      brush::Ribbon r;
      r.fill = Fill::color({1, 0, 0, 1});
      r.width = Profile(PulseAtPx{});
      return box().child(
          box().width(120).height(120).shape(shapes::circle()).stroke(r));
    };
    host.composer.render(tree());
    host.frame();
    host.composer.render(tree());
    host.frame();
    EXPECT_EQ(host.composer.stats().picturesRecorded, 0u)
        << "an identical profiled ribbon re-recorded — the prune is not real";
  }
  EXPECT_FLOAT_EQ(Profile(PulseAtFraction{}).acrossAt(0.4f, 999.0f), 24.0f);
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
  bool operator==(const NanAtMidLaw &) const = default;
};
} // namespace

TEST(ComposeWidthProfile, ANonFiniteSamplePinchesInsteadOfDeletingTheBand) {
  // §33-m / astral_tome: Skia draws NONE of a path that contains one
  // non-finite vertex, so a law returning NaN at a single sample deleted
  // its WHOLE band — bloom and body silently absent for the sketch's
  // entire life — and nothing said why. The guard in profileOffset turns
  // the bad sample into a LOCAL pinch to the spine; the rest of the band
  // draws. (Control: revert the one-line guard and `nan` below goes to
  // zero everywhere — the band vanishes outright.)
  const auto bandOf = [](bool poisoned) {
    Host host(200, 200);
    brush::Ribbon r;
    r.fill = Fill::color({1, 0, 0, 1});
    if (poisoned)
      r.width = Profile(NanAtMidLaw{});
    else
      r.width = Profile(TaperLaw{20.0f, 20.0f});
    host.composer.render(stack().child(straightRun(std::move(r))));
    host.frame();
    std::vector<int> t;
    for (int x : {40, 70, 100, 130, 160})
      t.push_back(thicknessAt(host, x));
    return t;
  };
  const std::vector<int> finite = bandOf(false);
  const std::vector<int> nan = bandOf(true);
  ASSERT_EQ(finite.size(), nan.size());
  // Away from the poisoned window the two bands agree — the rest of the
  // band DRAWS.
  for (size_t i = 0; i < finite.size(); ++i) {
    if (i == 2)
      continue; // the poisoned column
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

// ---- the brush:: fold, now a deletion -------------------------------------

TEST(ComposeR3Brush, TheFoldIsOneNamespaceAndOneNamePerKind) {
  // R3 deleted `namespace brushes` outright and the *Brush suffixes with
  // it: every kind answers to exactly one name, under `brush::`.
  const brush::Ribbon taught = brush::taper(10, 2, red());
  EXPECT_FLOAT_EQ(taught.widthStart, 10.0f);
  // The taught constructor is the PROFILE one.
  const brush::Ribbon profiled = brush::ribbon(strand::offset(9.0f), red());
  EXPECT_TRUE(profiled.hasProfile());
  EXPECT_FLOAT_EQ(profiled.bleed(), 9.0f);
  // The kinds are values under the taught spelling, nothing else.
  static_assert(std::is_default_constructible_v<brush::Pattern>);
  static_assert(std::is_default_constructible_v<brush::Scatter>);
  static_assert(std::is_default_constructible_v<brush::Art>);
}

// ---- 6. the derive family --------------------------------------------------

TEST(ComposeR1Derive, TheFamilyHasOneSpelling) {
  // Aliases, so the same picture, term for term.
  auto draw = [](bool qualified) {
    Host host(200, 200);
    Element a = box().key("a").rect(SkRect::MakeXYWH(20, 20, 40, 40));
    Element b = box().key("b").rect(SkRect::MakeXYWH(120, 120, 40, 40));
    Element wire = qualified ? derive::connector("a", "b")
                             : connector("a", "b");
    wire.absolute().inset(0).foreground(util::stroke(4, red()));
    host.composer.render(
        stack().child(std::move(a)).child(std::move(b)).child(std::move(wire)));
    host.frame();
    host.frame(); // derive resolves against the first layout
    std::vector<SkColor> out;
    for (int i = 20; i < 160; i += 4)
      out.push_back(host.pixel(i, i));
    return out;
  };
  const std::vector<SkColor> qualified = draw(true);
  EXPECT_EQ(qualified, draw(false));
  EXPECT_GT(inkedCount(qualified), 10u) << "the wire actually drew";
}

TEST(ComposeR1Derive, FlowAroundAsAFreeVerbIsTheMethod) {
  auto draw = [](bool freeVerb) {
    Host host(300, 200);
    // whiteStyle, not styleAt: the default foreground is BLACK on this
    // host's black ground, so both arms used to compare two blank grids
    // (the liveness bound below is what caught it).
    Element para = text(u8"one two three four five six seven eight nine ten "
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
      for (int x = 0; x < 300; x += 3)
        out.push_back(host.pixel(x, y));
    return out;
  };
  const std::vector<SkColor> freeVerb = draw(true);
  EXPECT_EQ(freeVerb, draw(false));
  EXPECT_GT(inkedCount(freeVerb), 20u) << "the paragraph actually drew";
}

// ---- 7. the wrapping span (N7) ---------------------------------------------
//
// THE PARITY GATE. `spans::wrap` exists so that wrap mode — the last thing
// the deleted `trim()` could do that spans could not — has a span spelling.
// The tests are parity tests, not "does it draw something": the legacy arm
// was trim() until R4 deleted it, and is now the node-level gate that
// replaced it — `mask(by::spans(...))`, whose geometry is trim's geometry.
// The pixel expectations below were pinned against trim itself.

TEST(ComposeR1Wrap, StaticSeamCrossingWindowMatchesWrapTrim) {
  auto draw = [](bool useLegacyTrim) {
    Host host(200, 200);
    Element e = revealBox();
    if (useLegacyTrim)
      e.mask(by::spans(spans::wrap(0.9f, 1.15f)))
          .stroke(util::stroke(6, red()));
    else
      e.stroke(spans::wrap(0.9f, 1.15f), util::stroke(6, red()));
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
  // trim spells the window as (start, end, OFFSET); spans spell it as
  // arithmetic on the two ENDPOINTS of one Output, which is why no third
  // parameter is owed.
  constexpr float kWindow = 0.25f;
  choreograph::Output<float> phase;

  Host trimmed(200, 200);
  trimmed.composer.render(stack().child(
      revealBox()
          .mask(by::spans(spans::wrap(0.0f, kWindow).offset(&phase)))
          .stroke(util::stroke(6, red()))));

  Host spanned(200, 200);
  spanned.composer.render(stack().child(revealBox().stroke(
      spans::wrap(bind(&phase), bind(&phase).offset(kWindow)),
      util::stroke(6, red()))));

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
      e.mask(by::spans(spans::wrap(0.0f, kWindow)
                           .offset(animate(from(0.0f).to(1.0f), {1000ms}))))
          .stroke(util::stroke(6, red()));
    else
      e.stroke(spans::wrap(animate(from(0.0f).to(1.0f), {1000ms}),
                           animate(from(kWindow).to(1.0f + kWindow),
                                   {1000ms})),
               util::stroke(6, red()));
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
      e.mask(by::spans(spans::wrap(a, b))).stroke(util::stroke(6, red()));
    else
      e.stroke(spans::wrap(a, b), util::stroke(6, red()));
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
  // The design judgement, pinned: range() did NOT learn to wrap, because
  // range(0.9, 0.1) already means something (§27), and the reader of a
  // claim conflict needs the call site to say "cyclic".
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

// ---- 8. cornerAlign is a required argument --------------------------------

TEST(ComposeR1Corner, AlignmentCannotBeOmitted) {
  // The §27 break, enforced by the type system rather than by a warning:
  // there is no way to describe corner art with no stated alignment.
  static_assert(!std::is_default_constructible_v<brush::CornerArt>);
  static_assert(!std::is_constructible_v<brush::CornerArt, Element>);
  static_assert(std::is_constructible_v<brush::CornerArt, Element,
                                        brush::CornerAlign>);
  // And the alignment participates in equality, so two brushes that differ
  // only in how their corners face do not prune into each other.
  const Element art = box().width(10).height(10).fill(red());
  brush::Pattern a, b;
  a.side = box().width(10).height(2).fill(red());
  b.side = a.side;
  a.corner = brush::CornerArt{art, brush::CornerAlign::Bisector};
  b.corner = brush::CornerArt{art, brush::CornerAlign::Outgoing};
  EXPECT_FALSE(a == b);
  b.corner = brush::CornerArt{art, brush::CornerAlign::Bisector};
  EXPECT_TRUE(a == b);
}

// ---- THE TRIM PARITY TABLE -------------------------------------------------
//
// Expressiveness parity was the gate for deleting Element::trim: every
// capability of it had to have a spelling, or trim could not go. Each row
// below is one capability. The rows were verified against trim() itself
// while it existed; R4 deleted it and the "legacy" arm of each row is now
// the node-level gate that inherited its geometry, `mask(by::spans(...))`.
// The rows therefore keep working as the SUGAR LAW's pixel proof: the pass
// door and the node door describe one run.

TEST(ComposeR1TrimParity, ClampWindowWithBothEndsNamed) {
  // Row: trim(start, end) with a NON-ZERO start — upTo() was only ever the
  // start == 0 case.
  auto draw = [](bool useLegacyTrim) {
    Host host(200, 200);
    Element e = revealBox();
    if (useLegacyTrim)
      e.mask(by::spans(spans::range(0.15f, 0.55f)))
          .stroke(util::stroke(6, red()));
    else
      e.stroke(spans::range(0.15f, 0.55f), util::stroke(6, red()));
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
  // Row: clamped behaviour — fractions outside [0,1] pin rather than
  // wrap. normalizeSpans clamps the same way.
  auto draw = [](bool useLegacyTrim) {
    Host host(200, 200);
    Element e = revealBox();
    if (useLegacyTrim)
      e.mask(by::spans(spans::range(-0.4f, 0.6f)))
          .stroke(util::stroke(6, red()));
    else
      e.stroke(spans::range(-0.4f, 0.6f), util::stroke(6, red()));
    host.composer.render(stack().child(std::move(e)));
    host.frame();
    return boundaryRing(host);
  };
  const std::vector<SkColor> pinned = draw(false);
  EXPECT_EQ(pinned, draw(true));
  // STRENGTHENED 2026-07-28 (audit): two agreeing arms cannot tell a pin
  // from a wrap, and two BLANK arms agree perfectly.
  EXPECT_GT(inkedCount(pinned), 5u) << "the window painted at all";
  // The discriminator, at named pixels: fraction 0 is the rect's start
  // corner, so the CLAMPED window [0, 0.6] runs out partway round and the
  // far side stays dark. The extra piece a WRAPPED [-0.4, 0.6] would show
  // is exactly that far side. (An inked-fraction bound cannot say this:
  // boundaryRing samples points outside the stroke too, so "not all of the
  // ring" is true of every window.)
  Host probe(200, 200);
  probe.composer.render(stack().child(
      revealBox().stroke(spans::range(-0.4f, 0.6f), util::stroke(6, red()))));
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
  // Row: plain bound endpoints, both modes' shared case.
  choreograph::Output<float> begin, end;
  Host trimmed(200, 200), spanned(200, 200);
  trimmed.composer.render(stack().child(
      revealBox()
          .mask(by::spans(spans::range(&begin, &end)))
          .stroke(util::stroke(6, red()))));
  spanned.composer.render(stack().child(revealBox().stroke(
      spans::range(&begin, &end), util::stroke(6, red()))));
  for (auto [b, e] : {std::pair{0.0f, 0.2f}, std::pair{0.3f, 0.9f},
                      std::pair{0.45f, 0.55f}}) {
    begin = b;
    end = e;
    trimmed.frame();
    spanned.frame();
    const std::vector<SkColor> ring = boundaryRing(spanned);
    EXPECT_EQ(ring, boundaryRing(trimmed)) << "window " << b << ".." << e;
    // The liveness bound the sibling rows carry: two blank renders agree.
    EXPECT_GT(inkedCount(ring), 5u) << "window " << b << ".." << e
                                    << " painted nothing at all";
  }
}

TEST(ComposeR1TrimParity, TheOffsetArgumentIsEndpointArithmetic) {
  // Row: trim's third argument. A CONSTANT offset is addition at the call
  // site; a BOUND offset over constant ends is `bind(&off).offset(k)` on
  // each end. Both checked against trim carrying the offset itself.
  choreograph::Output<float> off;
  Host constTrim(200, 200), constSpan(200, 200);
  constTrim.composer.render(stack().child(
      revealBox()
          .mask(by::spans(spans::range(0.1f, 0.4f).offset(0.25f)))
          .stroke(util::stroke(6, red()))));
  constSpan.composer.render(stack().child(revealBox().stroke(
      spans::range(0.1f + 0.25f, 0.4f + 0.25f), util::stroke(6, red()))));
  constTrim.frame();
  constSpan.frame();
  EXPECT_EQ(boundaryRing(constSpan), boundaryRing(constTrim))
      << "constant offset";

  Host boundTrim(200, 200), boundSpan(200, 200);
  boundTrim.composer.render(stack().child(
      revealBox()
          .mask(by::spans(spans::upTo(0.3f).offset(&off)))
          .stroke(util::stroke(6, red()))));
  boundSpan.composer.render(stack().child(revealBox().stroke(
      spans::range(bind(&off), bind(&off).offset(0.3f)),
      util::stroke(6, red()))));
  for (float v : {0.0f, 0.17f, 0.42f, 0.61f}) {
    off = v;
    boundTrim.frame();
    boundSpan.frame();
    EXPECT_EQ(boundaryRing(boundSpan), boundaryRing(boundTrim))
        << "bound offset " << v;
  }
}

TEST(ComposeR1TrimParity, AnimatedEndpointsRampTheSameWindow) {
  // Row: composer-manufactured endpoints under Clamp.
  auto host = [](bool useLegacyTrim) {
    auto h = std::make_unique<Host>(200, 200);
    Element e = revealBox();
    if (useLegacyTrim)
      e.mask(by::spans(spans::upTo(animate(from(0.0f).to(1.0f), {800ms}))))
          .stroke(util::stroke(6, red()));
    else
      e.stroke(spans::upTo(animate(from(0.0f).to(1.0f), {800ms})),
               util::stroke(6, red()));
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
  // Row: a node gate reveals EVERY outline-following decoration of the
  // node at once. A span claims ONE pass — and two claiming the same run is
  // the LOUD error, whose message names the fix. So the N-decoration
  // reveal is spelled as one pass with a COMPOSITE brush, which is a
  // spelling, not a gap.
  Host host(200, 200);
  host.composer.render(stack().child(revealBox().stroke(
      spans::upTo(0.4f),
      brush::layers({brush::solid(8, red()), brush::solid(3, green())}))));
  host.frame();
  EXPECT_EQ(host.pixel(40, 20), SK_ColorGREEN) << "both marks, one claim";
  EXPECT_EQ(host.pixel(110, 20), SK_ColorBLACK) << "and the claim ends";
}

// ---------------------------------------------------------------------------
// PHASE R2 — the two closable parity gaps, and R1's pinned obligations
//
// R1 left the parity table with two open rows and a review list of untested
// branches. Both rows close here, additively, and every pinned branch gets a
// test: parity is the condition for the R3 deletion, so a row that closes
// without a test has not closed.

TEST(ComposeR2Offset, TwoLiveSourcesSummedIntoOneEndpointMatchTrim) {
  // THE ROW: the deleted `trim(&start, &end, &offset)` summed TWO
  // independently-driven values into each endpoint. A bound endpoint holds ONE source pointer, so
  // endpoint arithmetic cannot spell it — `Spans::offset()` is the third
  // live term, and this is the test that says the sum is the same sum.
  choreograph::Output<float> begin, end, off;
  Host trimmed(200, 200), spanned(200, 200);
  trimmed.composer.render(stack().child(
      revealBox()
          .mask(by::spans(spans::range(&begin, &end).offset(&off)))
          .stroke(util::stroke(6, red()))));
  spanned.composer.render(stack().child(revealBox().stroke(
      spans::range(&begin, &end).offset(&off), util::stroke(6, red()))));
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
  trimmed.composer.render(stack().child(
      revealBox()
          .mask(by::spans(spans::wrap(&begin, &end).offset(&off)))
          .stroke(util::stroke(6, red()))));
  spanned.composer.render(stack().child(revealBox().stroke(
      spans::wrap(&begin, &end).offset(&off), util::stroke(6, red()))));
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
  // It participates in equality exactly as begin/end do — without this a
  // claim that only SLIDES would prune to its first frame, which is the bug
  // R1 fixed for Wrap's endpoints and would have re-introduced here.
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
  // THE OTHER ROW: a node gate reveals BACKGROUND-slot followers too, and
  // a span pass could only ever paint above the children. The twin slot
  // closes it — same claim, same brush, same z-half.
  auto draw = [](bool useLegacyTrim) {
    Host host(200, 200);
    Element e = revealBox();
    if (useLegacyTrim)
      e.mask(by::spans(spans::upTo(0.45f)))
          .background(util::stroke(6, red()));
    else
      e.background(spans::upTo(0.45f), util::stroke(6, red()));
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
      e.background(spans::every(1), util::stroke(10, red()));
    else
      e.stroke(spans::every(1), util::stroke(10, red()));
    // A child straddling the top edge, opaque, painted between the halves.
    e.child(box().absolute().rect(SkRect::MakeXYWH(30, -6, 40, 12))
                .fill(green()));
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
            .background(spans::every(1), util::stroke(4, red()), "under")
            .stroke(spans::upTo(0.5f), util::stroke(2, green()), "over")));
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
  host.composer.render(stack().child(
      revealBox()
          .background(spans::upTo(0.25f), util::stroke(6, red()))
          .stroke(spans::rest(), util::stroke(6, green()))));
  host.frame();
  // The seam (fraction 0) of an rrect outline is its BOTTOM-LEFT corner and
  // the boundary runs UP the left edge from there, so the first quarter of
  // the 400 px perimeter is that edge; rest() gets everything after it.
  EXPECT_EQ(host.pixel(20, 60), SK_ColorRED) << "the background claim";
  EXPECT_EQ(host.pixel(60, 20), SK_ColorGREEN) << "rest() took the remainder";
}

TEST(ComposeR2Seam, AWholeContourClaimKeepsItsCornerJoin) {
  // R1's one real bug, pinned at the pixel it lived in. getSegment returns
  // an OPEN run whose ends merely coincide, so a whole-contour claim used to
  // put two butt caps at the seam vertex instead of a miter join — a notch,
  // two pixels wide, at ONE corner of every rectangle, invisible until a
  // wide additive brush walks over it. The corpus has no spans:: sites, so
  // the byte-compares could not reach this branch: only a test can.
  //
  // THE SEAM IS THE BOTTOM-LEFT CORNER. `addRRect` starts at index 3 and
  // runs up the left edge, so fraction 0 sits at (left, bottom) — and the
  // corner that loses its join is the one AT the seam, not the top-left.
  // Sampling the wrong corner makes this test vacuous: every other corner
  // is mid-run and joins correctly whether or not the contour was closed,
  // so the assertions pass with the close() reverted. Verified by doing
  // exactly that before restoring it.
  auto corner = [](int form) {
    Host host(200, 200);
    Element e = revealBox();
    PathFormat wide = util::stroke(12, red());
    wide.join = SkPaint::kMiter_Join;
    switch (form) {
    case 0: e.stroke(std::move(wide)); break;              // untrimmed truth
    case 1: e.stroke(spans::every(1), std::move(wide)); break;
    case 2: e.stroke(spans::range(0.0f, 1.0f), std::move(wide)); break;
    case 3: e.stroke(spans::wrap(0.0f, 1.0f), std::move(wide)); break;
    default: e.stroke(spans::rest(), std::move(wide)); break;
    }
    host.composer.render(stack().child(std::move(e)));
    host.frame();
    std::vector<SkColor> out;
    // The miter's own square at the SEAM corner (20, 120), reaching out to
    // the 6 px the 12 px stroke throws beyond it: a butt-capped pair leaves
    // that outer square uncovered, so these are exactly the pixels that
    // differ.
    for (int y = 120; y <= 125; ++y)
      for (int x = 15; x <= 20; ++x)
        out.push_back(host.pixel(x, y));
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
            .stroke(spans::wrap(0.9f, 1.15f), util::stroke(4, red()), "ants")
            // touches only the SECOND run
            .stroke(spans::range(0.05f, 0.3f), util::stroke(2, green()),
                    "keyline")));
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
            .stroke(spans::wrap(0.9f, 1.05f), util::stroke(4, red()), "ants")
            .stroke(spans::range(0.3f, 0.6f), util::stroke(2, green()),
                    "keyline")));
    host.frame();
  }
  EXPECT_EQ(::testing::internal::GetCapturedStderr(), "");
}

TEST(ComposeR2Wrap, RestIsTheComplementOfBothOfWrapsRuns) {
  // rest() reads RESOLVED runs, so a seam-crossing claim leaves rest() a
  // single interval in the middle — not two, and not the naive [end, begin].
  Host host(200, 200);
  host.composer.render(stack().child(
      revealBox()
          .stroke(spans::wrap(0.9f, 1.15f), util::stroke(6, red()))
          .stroke(spans::rest(), util::stroke(6, green()))));
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
  // The fourth pinned obligation. spanVolatile reads the PASS BRUSH's
  // isAnimated(), and the only arm of that never exercised was a live
  // Material: a stroke whose colour comes from a uTime shader must repaint
  // every frame with no re-describe, exactly like a bound endpoint does.
  auto paintedPerFrame = [](bool live) {
    Host host(200, 200);
    PathFormat mark = util::stroke(8, red());
    mark.strokeMaterial = Material::sksl(heavyEffect(live));
    host.composer.render(
        stack().child(revealBox().stroke(spans::upTo(0.6f), std::move(mark))));
    host.frame();
    host.frame(); // no re-describe: only declared volatility can paint now
    return host.composer.stats().nodesPainted;
  };
  EXPECT_GT(paintedPerFrame(true), 0u)
      << "a live stroke material on a span pass must declare isAnimated()";
  EXPECT_EQ(paintedPerFrame(false), 0u)
      << "…and a static one must still cache";
}

