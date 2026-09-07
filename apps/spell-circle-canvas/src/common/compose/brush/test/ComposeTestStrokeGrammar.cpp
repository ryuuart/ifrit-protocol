// The span claim itself: how corners and edges partition a boundary, what
// the ladder of claim terms resolves to, what the overlap law says when two
// passes want the same run, and how a background half and a whole-contour
// claim read.

#include "support/BrushTestSupport.h"

namespace {

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

TEST(ComposeSpanBackground, TrimmedBackgroundFollowerHasASpanSpelling) {
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

TEST(ComposeSpanBackground, ThePassPaintsUNDERTheChildren) {
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

TEST(ComposeSpanBackground, OneBoundaryIsOneClaimLedgerAcrossBothHalves) {
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

TEST(ComposeSpanBackground, RestReadsAcrossTheHalvesToo) {
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

TEST(ComposeSpanCorner, AWholeContourClaimKeepsItsCornerJoin) {
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
