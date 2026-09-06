// The window a span claim runs over: the trim endpoints and their
// arithmetic, the summed live offset, the wrap that carries a run across
// the seam, and the pass door that claims exactly what the node gate does.

#include "support/BrushTestSupport.h"

namespace {

SkPath unitBox() {
  SkPathBuilder b;
  b.addRect(SkRect::MakeWH(100, 100));
  return b.detach();
}

}  // namespace

// ---- the wrapping span -----------------------------------------------------
//
// `spans::wrap` is a window that may cross the contour seam. These are
// PARITY tests rather than "does it draw something": each compares the
// pass door (`stroke(spans, …)`) against the node door
// (`mask(by::spans(…))`), which are two spellings of one geometry, and each
// also bounds the ink away from "nothing" and "everything" so that two
// agreeing blank frames cannot pass.
TEST(ComposeSpanWrap, MarchingAntsMatchTrimAtEveryPhaseIncludingMidSeam) {
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

TEST(ComposeSpanWrap, AnimatedEndpointsMarchAcrossTheSeamAndMatchTrim) {
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

TEST(ComposeSpanWrap, DegenerateWindowsMatchTrimToo) {
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

TEST(ComposeSpanWrap, WrapIsItsOwnTermAndRangeStillClamps) {
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

TEST(ComposeSpanWrap, WrapWindowsParticipateInReconcilerEquality) {
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
namespace {

/** One static window, and how much of the boundary ring it may ink.
 *
 *  Every row is drawn twice -- through the pass door
 *  (`stroke(spans, …)`) and through the node gate (`mask(by::spans(…))`)
 *  -- because those are two spellings of one geometry. The ink bounds are
 *  what stops two identically blank frames from satisfying the
 *  comparison. */
struct StaticWindow {
  const char* what;
  Spans window;
  size_t leastInked;
  float mostInkedFraction;
};

class SpanDoor : public testing::TestWithParam<StaticWindow> {};

}  // namespace

TEST_P(SpanDoor, ThePassDoorAndTheNodeGateClaimTheSameRun) {
  const StaticWindow& row = GetParam();
  auto draw = [&row](bool throughTheGate) {
    Host host(200, 200);
    Element e = revealBox();
    if (throughTheGate)
      e.mask(by::spans(row.window)).stroke(stroke(6, red()));
    else
      e.stroke(row.window, stroke(6, red()));
    host.composer.render(stack().child(std::move(e)));
    host.frame();
    return boundaryRing(host);
  };
  const std::vector<SkColor> spanned = draw(false);
  EXPECT_EQ(spanned, draw(true));
  const size_t inked = inkedCount(spanned);
  EXPECT_GT(inked, row.leastInked) << "the window painted nothing at all";
  EXPECT_LT((float)inked, (float)spanned.size() * row.mostInkedFraction)
      << "the window painted more of the boundary than it claimed";
}

INSTANTIATE_TEST_SUITE_P(
    ComposeSpans, SpanDoor,
    testing::Values(
        // A window that crosses the contour seam: some ink, far from all.
        StaticWindow{"AWindowAcrossTheSeam", spans::wrap(0.9f, 1.15f), 5u,
                     0.5f},
        // A NON-ZERO start. upTo() only ever covers start == 0, so this is
        // where a start offset would go wrong unnoticed.
        StaticWindow{"AWindowWithBothEndsNamed", spans::range(0.15f, 0.55f), 5u,
                     1.0f},
        // Fractions outside [0,1] pin rather than wrap, and both doors
        // clamp the same way.
        StaticWindow{"AWindowReachingBelowZero", spans::range(-0.4f, 0.6f), 5u,
                     1.0f}),
    [](const testing::TestParamInfo<StaticWindow>& info) {
      return info.param.what;
    });

TEST(ComposeSpanTrim, AWindowReachingOutsideZeroToOnePinsRatherThanWraps) {
  // Parity between the two doors cannot by itself tell a pin from a wrap:
  // both doors would wrap together. So this names pixels instead. Fraction
  // 0 is the bottom-left corner running UP the left edge, so the clamped
  // [0, 0.6] is the left edge, the top edge and the top 40% of the right --
  // and the BOTTOM edge ([0.75, 1]) is the piece a wrapped reading would
  // add. An inked-fraction bound cannot say this: boundaryRing samples
  // points outside the stroke too, so "not all of the ring" is true of
  // every window.
  Host probe(200, 200);
  probe.composer.render(stack().child(
      revealBox().stroke(spans::range(-0.4f, 0.6f), stroke(6, red()))));
  probe.frame();
  EXPECT_NE(probe.pixel(70, 20), SK_ColorBLACK) << "the top edge is inside";
  EXPECT_EQ(probe.pixel(70, 120), SK_ColorBLACK)
      << "…and the bottom edge is not: [-0.4, 0.6] PINNED, it did not wrap";
}

TEST(ComposeSpanTrim, BoundEndpointsScrubTheSameWindow) {
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

TEST(ComposeSpanTrim, TheOffsetArgumentIsEndpointArithmetic) {
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

TEST(ComposeSpanTrim, AnimatedEndpointsRampTheSameWindow) {
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

TEST(ComposeSpanTrim, OnePassPerClaimIsTheNPassRule) {
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
// The harder parity rows: a third live term, the seam, and wrap under the
// overlap law.
TEST(ComposeSpanOffset, TwoLiveSourcesSummedIntoOneEndpointMatchTrim) {
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

TEST(ComposeSpanOffset, TheSummedEndpointWrapsLikeTrimDoes) {
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

TEST(ComposeSpanOffset, TheOffsetIsAComparableEndpointLikeTheOthers) {
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

TEST(ComposeSpanWrap, WrapIsUnderTheOverlapLawLikeEveryOtherTerm) {
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

TEST(ComposeSpanWrap, RestIsTheComplementOfBothOfWrapsRuns) {
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
