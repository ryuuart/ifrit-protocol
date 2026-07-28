#include "ComposeTestSupport.h"

namespace {

/** A 100×100 box at (20,20) whose boundary is the ring `boundaryRing`
 *  samples, dressed with one red stroke. The masking family's fixture. */
Element maskBox() {
  return box().rect(SkRect::MakeXYWH(20, 20, 100, 100));
}

/** How much red ink is anywhere in a 200×200 host. */
int redInk(Host &host, int x0 = 0, int y0 = 0, int x1 = 200, int y1 = 200) {
  int n = 0;
  for (int y = y0; y < y1; ++y)
    for (int x = x0; x < x1; ++x)
      if (SkColorGetR(host.pixel(x, y)) > 140 &&
          SkColorGetG(host.pixel(x, y)) < 90)
        ++n;
  return n;
}

} // namespace

// ---- S1 · the helper's three strokes, gated from OUTSIDE the helper -------

TEST(ComposeR4Mask, S1AHelpersMarksAreGatedFromOutsideIt) {
  // astral_tome's `linkPass()` returns an element carrying THREE strokes.
  // The caller wants all three to draw on together and can reach none of
  // them: this is the sample that fails any shape where the gate lives at
  // the call site or inside the mark value, because the person who wants
  // the gate is not the person who wrote the mark.
  const auto helper = [] {
    return maskBox()
        .stroke(util::stroke(10, blue()))
        .stroke(util::stroke(6, green()))
        .stroke(util::stroke(2, red()));
  };
  Host all(200, 200), gated(200, 200);
  all.composer.render(stack().child(helper()));
  all.frame();
  gated.composer.render(
      stack().child(helper().mask(parts::marks(), by::spans(spans::upTo(0.4f)))));
  gated.frame();
  // Every one of the three is cut by the one call the CALLER wrote.
  EXPECT_EQ(gated.pixel(40, 20), all.pixel(40, 20)) << "inside the window";
  EXPECT_EQ(gated.pixel(120, 100), SK_ColorBLACK) << "past the window";
  EXPECT_LT(inkedCount(boundaryRing(gated)), inkedCount(boundaryRing(all)));
}

// ---- S2 · the wet nib rides the head of the gate --------------------------

TEST(ComposeR4Mask, S2ADecorationReceivesTheAlreadyGatedRun) {
  // thunder_fulu: a brush stroke writes itself at the scribe's pace with a
  // wet pool at the NIB. The pool is a PathFormat with its own
  // trimStart 0.93 — a fraction of what is written, not of the whole line,
  // so it must ride the head of the node gate and needs no second node.
  const auto line = [](SkSize s) {
    SkPathBuilder b;
    b.moveTo(0, s.height() * 0.5f);
    b.lineTo(s.width(), s.height() * 0.5f);
    return b.detach();
  };
  PathFormat wet = util::stroke(8, green());
  wet.trimStart = 0.90f;
  wet.trimEnd = 1.0f;
  Host host(200, 200);
  host.composer.render(stack().child(
      box().absolute().inset(0).shape(line).fill(Fill::none())
          .stroke(util::stroke(4, red()))
          .foreground(wet)
          .mask(by::spans(spans::upTo(0.5f)))));
  host.frame();
  // The body reaches x≈100; nothing past it.
  EXPECT_GT(redInk(host, 20, 95, 90, 105), 40);
  EXPECT_EQ(redInk(host, 120, 95, 190, 105), 0);
  // …and the nib sits at the HEAD OF THE REVEALED PART (x≈95), not at the
  // head of the whole line (x≈190), which is the entire point.
  int nib = 0, farEnd = 0;
  for (int x = 88; x < 100; ++x)
    nib += SkColorGetG(host.pixel(x, 100)) > 170;
  for (int x = 170; x < 195; ++x)
    farEnd += SkColorGetG(host.pixel(x, 100)) > 170;
  EXPECT_GT(nib, 3);
  EXPECT_EQ(farEnd, 0);
}

// ---- S3 · the retarget: one mask in both branches -------------------------

// AUDIT-FLAG 2026-07-27 — NAME OVERCLAIM (medium): phase-0 parks at 0.0001, so retarget vs fresh-mount are numerically indistinguishable; park ABOVE target (e.g. 0.8 -> 0.5) to discriminate.
TEST(ComposeR4Mask, S3TheGateRetargetsAcrossAnIfElseInsteadOfMounting) {
  // ScenesBeethoven: phase 0 is unswept, phase 1 sweeps each arc over its
  // measured span. `animate(to(span))` is RAMP-ON-CHANGE — it starts from
  // the property's CURRENT value — so the gate must occupy the same
  // animation slot in both branches or the motion mounts from zero and the
  // ring blinks. maskAnims is positional, which is what makes this work.
  const auto tree = [](int phase) {
    Element e = maskBox().stroke(util::stroke(6, red()));
    if (phase == 0)
      e.mask(by::spans(spans::upTo(0.0001f)));
    else
      e.mask(by::spans(
          spans::upTo(animate(to(0.5f), {400ms, &choreograph::easeNone}))));
    return stack().child(std::move(e));
  };
  Host host(200, 200);
  host.composer.render(tree(0));
  host.frame();
  const size_t unswept = inkedCount(boundaryRing(host));
  EXPECT_LE(unswept, 2u) << "phase 0 is the 0.0001 nub and nothing else";
  host.composer.render(tree(1));
  host.frame(0.2); // halfway: a RAMP from 0, not a jump to 0.5
  const size_t half = inkedCount(boundaryRing(host));
  host.frame(0.25); // settled
  const size_t settled = inkedCount(boundaryRing(host));
  EXPECT_GT(half, unswept + 5u) << "the ramp started";
  EXPECT_GT(settled, half) << "…and it retargeted rather than mounting";
}

// ---- S4 · a gate applied conditionally to an ALREADY-BUILT element --------

TEST(ComposeR4Mask, S4TheGateIsAPropertyOfABuiltElement) {
  // twoadvanced_v4: the orbital ring draws on with the panel, but the
  // still-frame capture shows it whole. §31's capture audit says this
  // pattern recurs — so the conditional must survive as a conditional, on
  // an element that already exists. A gate that lived in the mark value or
  // in the slot call would force the `if` above construction, which is a
  // re-authoring rather than a rename.
  const auto build = [](bool still) {
    Element ring = maskBox().stroke(util::stroke(6, red()));
    if (!still)
      ring.mask(by::spans(spans::upTo(0.25f)));
    return stack().child(std::move(ring));
  };
  Host live(200, 200), shot(200, 200);
  live.composer.render(build(false));
  live.frame();
  shot.composer.render(build(true));
  shot.frame();
  EXPECT_LT(inkedCount(boundaryRing(live)), inkedCount(boundaryRing(shot)));
  EXPECT_GT(inkedCount(boundaryRing(shot)), 100u) << "the still is whole";
}

// ---- S5 · a span-qualified CLAIM under a whole-node gate ------------------

TEST(ComposeR4Mask, S5AClaimUnderAGateIsTheIntersection) {
  // The corners composition: reticle brackets that LIGHT UP AS A SWEEP
  // REACHES THEM. The pass claims the corners; the mask gates the marks to
  // [0, t]; the pass paints `corners ∩ upTo(t)`. One line, no re-authoring,
  // and no second node.
  const auto draw = [](float t) {
    Host host(200, 200);
    host.composer.render(stack().child(
        maskBox()
            .stroke(spans::corners(18), util::stroke(6, red()), "brk")
            .mask(parts::marks(), by::spans(spans::upTo(t)))));
    host.frame();
    return inkedCount(boundaryRing(host));
  };
  const size_t none = draw(0.0f), quarter = draw(0.3f), all = draw(1.0f);
  EXPECT_EQ(none, 0u) << "the sweep has not reached any bracket";
  EXPECT_GT(quarter, 0u) << "…it reached some…";
  EXPECT_LT(quarter, all) << "…and not all of them";
  EXPECT_GT(all, 0u);

  // AND THE CLAIM LEDGER READS THE UNMASKED BOUNDARY. Two passes that
  // overlap are a description-level mistake; an overlap must not blink in
  // and out between 0.3 and 0.7 of a transition because a gate was
  // shrinking one of them. So a gated pair that overlaps says so at EVERY
  // gate value, including 0.
  testing::internal::CaptureStderr();
  {
    Host host(200, 200);
    host.composer.render(stack().child(
        maskBox()
            .stroke(spans::range(0.0f, 0.5f), util::stroke(4, red()), "a")
            .stroke(spans::range(0.3f, 0.8f), util::stroke(4, green()), "b")
            .mask(parts::marks(), by::spans(spans::upTo(0.0f)))));
    host.frame();
  }
  EXPECT_NE(testing::internal::GetCapturedStderr().find("both claim"),
            std::string::npos)
      << "the no-overlap law must read the UNMASKED claims";
}

// ---- S6 · the directional wipe, over a lattice of children ---------------

TEST(ComposeR4Mask, S6TheEdgeGateReachesTheChildren) {
  // chevreul_circle's twelve grounds arrive and withdraw as a downward
  // wipe. The CHILDREN are the point — an arc-length window has nothing to
  // say about them, and this is the sample that says the family needs more
  // than one gate kind.
  const auto lattice = [](float t) {
    Element g = box().absolute().left(20).top(20).width(160).height(160)
                    .mask(by::edge(90.0f, t));
    for (int i = 0; i < 4; ++i)
      g.child(box().absolute().left(0).top((float)i * 40).width(160)
                  .height(36).fill(red()));
    return stack().child(std::move(g));
  };
  Host half(200, 200), whole(200, 200);
  half.composer.render(lattice(0.5f));
  half.frame();
  whole.composer.render(lattice(1.0f));
  whole.frame();
  EXPECT_GT(redInk(half, 0, 20, 200, 95), 1000) << "the top half arrived";
  EXPECT_EQ(redInk(half, 0, 105, 200, 190), 0) << "the bottom half has not";
  EXPECT_GT(redInk(whole, 0, 105, 200, 190), 1000) << "…and at 1 it has";
}

// ---- S7 · the seal: a region gate, and its complement --------------------

TEST(ComposeR4Mask, S7TheShapeGateAndItsComplementAreBothTerms) {
  // A portrait masked to a wax-seal silhouette. Nothing in the tree could
  // express this: a study reached for `clipOut()` and `shapes::subtract` BY
  // NAME, found neither, and dropped below the Compose seam to a raw
  // SkPathOp. Both halves are terms here, and two masks INTERSECT — so a
  // set difference is one node and two lines.
  const SkRect seal = SkRect::MakeXYWH(20, 20, 60, 60);
  Host inside(200, 200), outside(200, 200), diff(200, 200);
  const auto plate = [] {
    return box().absolute().left(20).top(20).width(100).height(100)
        .fill(red());
  };
  inside.composer.render(
      stack().child(plate().mask(by::shape(Region::rect(seal)))));
  inside.frame();
  outside.composer.render(
      stack().child(plate().mask(by::outside(Region::rect(seal)))));
  outside.frame();
  // The gate is stated in the node's LOCAL space, so the seal covers
  // (40,40)-(100,100) on the canvas.
  EXPECT_GT(SkColorGetR(inside.pixel(70, 70)), 180) << "kept inside";
  EXPECT_EQ(inside.pixel(110, 110), SK_ColorBLACK) << "…and only inside";
  EXPECT_EQ(outside.pixel(70, 70), SK_ColorBLACK) << "the complement";
  EXPECT_GT(SkColorGetR(outside.pixel(110, 110)), 180);
  // THE SET DIFFERENCE: inside one region AND outside another, which is
  // the picture the raw SkPathOp was written for.
  diff.composer.render(stack().child(
      plate()
          .mask(by::shape(Region::rect(SkRect::MakeXYWH(0, 0, 80, 80))))
          .mask(by::outside(Region::rect(SkRect::MakeXYWH(0, 0, 40, 40))))));
  diff.frame();
  EXPECT_EQ(diff.pixel(35, 35), SK_ColorBLACK) << "cut out of the middle";
  EXPECT_GT(SkColorGetR(diff.pixel(70, 70)), 180) << "inside the outer";
  EXPECT_EQ(diff.pixel(110, 110), SK_ColorBLACK) << "outside the outer";
}

TEST(ComposeR4Mask, S7bTheAlphaGateTakesItsCoverageFromAMaterial) {
  // …and soft-edged, which is the other half of the seal. `Console.h`
  // prescribes `Material` + `kDstIn` as an IDIOM in a shipped header
  // because it is not a feature; two more studies hand-roll kSrcIn. It is
  // a feature now.
  Host host(200, 200);
  host.composer.render(stack().child(
      box().absolute().left(20).top(20).width(160).height(160).fill(red())
          .mask(by::alpha(Material::linear({0, 0}, {160, 0},
                                           {{0.0f, {1, 1, 1, 1}},
                                            {1.0f, {1, 1, 1, 0}}})))));
  host.frame();
  // Opaque at the left of the ramp, gone at the right, monotone between.
  EXPECT_GT(SkColorGetR(host.pixel(25, 100)), 200);
  EXPECT_LT(SkColorGetR(host.pixel(175, 100)), 40);
  EXPECT_GT(SkColorGetR(host.pixel(60, 100)), SkColorGetR(host.pixel(140, 100)));
}

// ---- S8 · per-mark granularity -------------------------------------------

TEST(ComposeR4Mask, S8OneMarkIsGatedAndItsSiblingIsNot) {
  // Hazard stripes wipe on while the bevel keyline STAYS. This is the
  // sample that a whole-node-only family cannot draw: its workaround is a
  // child node re-declaring the parent's shape, which is verbatim the
  // thing `overlay()` was added to abolish ("costs a node and loses the
  // outline").
  const auto panel = [](float t) {
    return stack().child(
        box().absolute().left(20).top(20).width(160).height(160)
            .overlay(util::stroke(20, red()), "hazard")
            .foreground(util::stroke(4, green()), "keyline")
            .mask(parts::named("hazard"), by::edge(0.0f, t)));
  };
  Host closed(200, 200), open(200, 200);
  closed.composer.render(panel(0.0f));
  closed.frame();
  open.composer.render(panel(1.0f));
  open.frame();
  // Withheld — down to the leading sliver of the half-plane, which is
  // exactly wipe()'s own geometry: the region at fraction 0 is one unit
  // wide at the far edge, and the OUTER half of a 20 px stroke on the
  // boundary reaches into it. That is the behaviour being preserved, not
  // an approximation of it.
  EXPECT_LT(redInk(closed), 30) << "the gated mark is withheld";
  EXPECT_GT(redInk(open), 200) << "…and fully shown at 1";
  // The SIBLING is untouched in both — that is the whole sample.
  const auto greenInk = [](Host &h) {
    int n = 0;
    for (int y = 0; y < 200; ++y)
      for (int x = 0; x < 200; ++x)
        if (SkColorGetG(h.pixel(x, y)) > 170 && SkColorGetR(h.pixel(x, y)) < 90)
          ++n;
    return n;
  };
  EXPECT_GT(greenInk(closed), 200);
  EXPECT_EQ(greenInk(closed), greenInk(open));
}

TEST(ComposeR4Mask, S8PlusThreeMasksAtThreeRatesIntersectPerFrame) {
  // THE COMPOSITION THE DESIGNER ASKED FOR BY NAME: masks whose selections
  // overlap INTERSECT, and each carries its OWN animation, so three masks
  // may run at three rates on one node. If they shared a slot the second
  // would retarget the first and this would be a race instead of a
  // picture; maskAnims is indexed per mask, which is what makes it one.
  choreograph::Output<float> slow{1.0f}, fast{1.0f};
  Host host(200, 200);
  host.composer.render(stack().child(
      box().absolute().left(20).top(20).width(160).height(160).fill(red())
          .mask(by::edge(0.0f, &fast))    // from the left
          .mask(by::edge(180.0f, &slow))  // …and from the right
          .mask(by::shape(Region::rect(SkRect::MakeXYWH(0, 40, 160, 80))))));
  host.frame();
  // All three open: the band the shape gate leaves is fully lit.
  EXPECT_GT(redInk(host, 25, 65, 175, 155), 8000);
  EXPECT_EQ(redInk(host, 0, 0, 200, 58), 0) << "the shape gate holds";
  EXPECT_EQ(redInk(host, 0, 142, 200, 200), 0);

  // Now drive them at DIFFERENT rates and pin the intersection at pixels:
  // fast shows [left, 0.25], slow shows [0.5 from the right, right] — the
  // two half-planes are disjoint, so their intersection is EMPTY, and no
  // single-gate implementation can produce that answer.
  fast = 0.25f;
  slow = 0.25f;
  host.frame();
  EXPECT_EQ(redInk(host), 0) << "disjoint half-planes intersect to nothing";

  // …and where they DO overlap, only the overlap paints.
  fast = 0.75f; // shows x in [20, 140]
  slow = 0.75f; // shows x in [60, 180]
  host.frame();
  EXPECT_EQ(redInk(host, 20, 60, 55, 150), 0) << "left of the slow edge";
  EXPECT_GT(redInk(host, 70, 65, 130, 150), 2000) << "the overlap paints";
  EXPECT_EQ(redInk(host, 145, 60, 180, 150), 0) << "right of the fast edge";
}

// ---- the intersection law, as arithmetic ---------------------------------

// AUDIT-FLAG 2026-07-27 — REDUNDANT (high): same fixture/mask pair as TheIntersectionIsExactIntervalArithmetic, which pins pixels AND refutes union; delete candidate.
TEST(ComposeR4Mask, StackedSpanGatesIntersectRatherThanUnion) {
  // Union is spelled INSIDE a gate value (Spans::operator|); across masks
  // there is only intersection, because two masks are two conditions and
  // stacking them can only ever show less.
  const auto ink = [](bool second) {
    Host host(200, 200);
    Element e = maskBox().stroke(util::stroke(6, red()));
    e.mask(by::spans(spans::range(0.0f, 0.5f)));
    if (second)
      e.mask(by::spans(spans::range(0.3f, 1.0f)));
    host.composer.render(stack().child(std::move(e)));
    host.frame();
    return inkedCount(boundaryRing(host));
  };
  const size_t one = ink(false), both = ink(true);
  EXPECT_GT(one, 0u);
  EXPECT_GT(both, 0u) << "[0.3,0.5] is not empty";
  EXPECT_LT(both, one) << "the second mask can only narrow the first";
}

TEST(ComposeR4Mask, TheIntersectionIsExactIntervalArithmetic) {
  // Pinned at pixels rather than at the helper, because the arithmetic is
  // only worth anything if it reaches the boundary. maskBox()'s perimeter
  // is 400 px and fraction 0 is the BOTTOM-LEFT corner running UP the left
  // edge, so [0.25, 0.5] is exactly the top edge, left to right.
  const auto topEdgeInk = [](Host &host, int x0, int x1) {
    int n = 0;
    for (int x = x0; x < x1; ++x)
      if (host.pixel(x, 20) != SK_ColorBLACK)
        ++n;
    return n;
  };
  Host host(200, 200);
  host.composer.render(stack().child(
      maskBox().stroke(util::stroke(6, red()))
          .mask(by::spans(spans::range(0.0f, 0.5f)))    // left + top
          .mask(by::spans(spans::range(0.3f, 1.0f)))));  // top's last 80%
  host.frame();
  // [0.3, 0.5] is the top edge from x = 40 to x = 120 and nothing else.
  EXPECT_EQ(topEdgeInk(host, 22, 36), 0) << "before the intersection";
  EXPECT_GT(topEdgeInk(host, 46, 114), 60) << "inside it";
  EXPECT_EQ(host.pixel(20, 70), SK_ColorBLACK)
      << "the left edge is in the FIRST mask and not the second";
  EXPECT_EQ(host.pixel(120, 70), SK_ColorBLACK)
      << "the right edge is in the SECOND and not the first";

  // Two masks that share nothing show nothing — intersection, never union.
  Host disjoint(200, 200);
  disjoint.composer.render(stack().child(
      maskBox().stroke(util::stroke(6, red()))
          .mask(by::spans(spans::range(0.0f, 0.2f)))
          .mask(by::spans(spans::range(0.6f, 0.8f)))));
  disjoint.frame();
  EXPECT_EQ(inkedCount(boundaryRing(disjoint)), 0u);
}

// ---- the sugar law, pinned -----------------------------------------------

TEST(ComposeR4Mask, TheStrokeSpansSugarLawIsPixelExact) {
  // STATED AS LAW in Compose.h:
  //     .stroke(where, what, name)
  //        == .stroke(what, name).mask(parts::named(name), by::spans(where))
  // Two doors, one machine — checked on a multi-run claim, where the two
  // paths through the library are least likely to agree by accident.
  const auto draw = [](bool sugar) {
    Host host(200, 200);
    Element e = maskBox();
    if (sugar)
      e.stroke(spans::corners(18), util::stroke(6, red()), "brk");
    else
      e.stroke(util::stroke(6, red()), "brk")
          .mask(parts::named("brk"), by::spans(spans::corners(18)));
    host.composer.render(stack().child(std::move(e)));
    host.frame();
    return boundaryRing(host);
  };
  const std::vector<SkColor> passDoor = draw(true);
  EXPECT_EQ(draw(false), passDoor);
  EXPECT_GT(inkedCount(passDoor), 5u);
  EXPECT_LT(inkedCount(passDoor), passDoor.size());
}

// AUDIT-FLAG 2026-07-27 — NAME OVERCLAIM (low): fixture has no unnamed sibling; what it actually (uniquely) pins is the silent-no-op law for an unmatched label — rename, keep.
TEST(ComposeR4Mask, ANamedMaskLeavesTheUnnamedMarksAlone) {
  // parts::named() addresses ONE mark by its LOCAL label. A label that
  // matches nothing selects nothing, silently — the same law as
  // spans::rest("unknown") and spans::fit("unknown").
  const auto draw = [](const char *label) {
    Host host(200, 200);
    host.composer.render(stack().child(
        maskBox()
            .stroke(util::stroke(6, red()), "outer")
            .mask(parts::named(label), by::spans(spans::upTo(0.0f)))));
    host.frame();
    return inkedCount(boundaryRing(host));
  };
  EXPECT_EQ(draw("outer"), 0u) << "the named mark is gated";
  EXPECT_GT(draw("typo"), 100u) << "…and a name that matches nothing is a "
                                   "silent no-op, not a hidden node";
}

// ---- the memo repair (§3.6) ----------------------------------------------

TEST(ComposeR4Mask, AGatedNodeKeepsTheScalarMemoAndPrunes) {
  // THE REPAIR. ContentScalars used to be a FIXED five-float struct, which
  // is why a per-pass span reveal is excluded from the §17 memo by a
  // written decision — and therefore why R2's 58 trim→spans ports each
  // moved their node from the scalar memo to per-frame content volatility
  // with a byte-identical plate ledger and nothing to catch it. A mask's
  // gate scalars are a bounded per-node list, so they ride ContentScalars
  // as a vector and an element-level gate keeps the memo.
  //
  // The probe is the same one §17 shipped with: a keyframe path with a
  // HELD segment. A held gate must repaint NOTHING.
  const auto ring = [] {
    return box().cache(Cache::None).child(
        box().width(120).height(120).key("ring")
            .shape(shapes::circle())
            .stroke(util::stroke(6.0f, Fill::color({1, 1, 1, 1})))
            .mask(by::spans(spans::upTo(animate(
                through({{std::chrono::milliseconds(0), 0.0f},
                         {std::chrono::milliseconds(200), 0.6f},
                         {std::chrono::milliseconds(600), 0.6f},
                         {std::chrono::milliseconds(800), 1.0f}}),
                &choreograph::easeNone)))));
  };
  Host host;
  host.composer.render(ring());
  host.frame();
  // Warm PAST the §20 release: after kScalarSettleFrames stable paints
  // the volatility flag releases and the tree re-records ONCE (the
  // "settling frame" cost — the price of ancestors gaining the cache).
  // The hold's steady state after that is the zero this test pins.
  for (int i = 0; i < 26; ++i)
    host.frame(1.0 / 60.0); // t ≈ 0.43 s — deep in the hold, post-release
  unsigned duringHold = 0;
  for (int i = 0; i < 8; ++i) {
    host.frame(1.0 / 60.0); // t ≈ 0.43 → 0.57 s, still held
    duringHold += host.composer.stats().picturesRecorded;
  }
  EXPECT_EQ(duringHold, 0u) << "a held gate must repaint nothing";
  // …and it is a memo, not a freeze: the ramp resumes and re-records.
  unsigned afterHold = 0;
  for (int i = 0; i < 12; ++i) {
    host.frame(1.0 / 60.0);
    afterHold += host.composer.stats().picturesRecorded;
  }
  EXPECT_GT(afterHold, 0u) << "…and re-record the moment the number ticks";
}

TEST(ComposeR4Mask, AStaticGateStillPrunesAndAMovingOneRepaints) {
  // The other half of the same claim: a mask is read live, so it
  // participates in reconciler equality. A re-describe with the SAME mask
  // must prune; a re-describe with a different one must not.
  const auto tree = [](float t) {
    return stack().child(maskBox().key("m").stroke(util::stroke(6, red()))
                             .mask(by::spans(spans::upTo(t))));
  };
  Host host(200, 200);
  host.composer.render(tree(0.4f));
  host.frame();
  host.composer.render(tree(0.4f));
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "an identical mask prunes";
  host.composer.render(tree(0.7f));
  EXPECT_GT(host.composer.stats().patchedNodes, 0u)
      << "…and a different one does not, or a pruned node would reveal to "
         "its first frame and stay there";
}

// ---- Region is a VALUE (the §3 wall) -------------------------------------

TEST(ComposeR4Mask, RegionIsComparableFromDayOne) {
  // The shape gate's obvious signature takes an OutlineFn — an
  // incomparable std::function, which never prunes. That is not a
  // hypothetical: it is the highest measured-impact item on the roadmap
  // (43.4 of 43.5 ms on one un-prunable callable). Region is a closed,
  // comparable value instead, and that is the whole reason the shape
  // member could ship with the family rather than after it.
  EXPECT_TRUE(Region::own() == Region::own());
  EXPECT_TRUE(Region::rect(SkRect::MakeWH(4, 4)) ==
              Region::rect(SkRect::MakeWH(4, 4)));
  EXPECT_FALSE(Region::rect(SkRect::MakeWH(4, 4)) ==
               Region::rect(SkRect::MakeWH(4, 5)));
  EXPECT_FALSE(Region::rect(SkRect::MakeWH(4, 4)) ==
               Region::oval(SkRect::MakeWH(4, 4)));
  EXPECT_FALSE(Region::own() == Region::rect(SkRect::MakeWH(4, 4)));
  SkPathBuilder a, b;
  a.addRect(SkRect::MakeWH(3, 3));
  b.addRect(SkRect::MakeWH(3, 3));
  EXPECT_TRUE(Region::path(a.detach()) == Region::path(b.detach()));
  // …and the gates built from them compare, which is what the reconciler
  // actually asks.
  EXPECT_TRUE(by::shape(Region::own()) == by::shape(Region::own()));
  EXPECT_FALSE(by::shape(Region::own()) == by::outside(Region::own()));
  EXPECT_FALSE(by::edge(0.0f, 0.5f) == by::edge(90.0f, 0.5f));
  EXPECT_FALSE(by::edge(0.0f, 0.5f) == by::edge(0.0f, 0.6f));
  EXPECT_FALSE(by::spans(spans::upTo(0.4f)) == by::edge(0.0f, 0.4f));
  EXPECT_TRUE(by::spans(spans::upTo(0.4f)) == by::spans(spans::upTo(0.4f)));
  // Parts too — a selection is a value you can look at, which was the
  // designer's question ("I still don't get the shape [of selection]").
  EXPECT_TRUE(parts::marks() == parts::marks());
  EXPECT_FALSE(parts::marks() == parts::surface());
  EXPECT_FALSE(parts::named("a") == parts::named("b"));
  EXPECT_TRUE((parts::surface() | parts::content() | parts::children() |
               parts::marks()) == parts::all());
}

// ---- the fold: what trim() and wipe() were -------------------------------

TEST(ComposeR4Mask, TheSpansGateReachesSurfaceAndMarksAndNotTheChildren) {
  // The fold table, as behaviour. A boundary is a 1-D coordinate: the
  // paint that TRACES it is the surface and the marks, and the content and
  // children do not. So `mask(by::spans(...))` — the taught one-argument
  // form, whose selection is parts::all() — is exactly the reveal the
  // deleted trim() drew, and gating children with an arc-length window is
  // not a picture and does nothing.
  Host host(200, 200);
  Element e = box().absolute().left(20).top(20).width(100).height(100)
                  .fill(red())
                  .mask(by::spans(spans::upTo(0.0f)));
  e.child(box().absolute().left(10).top(10).width(40).height(40).fill(green()));
  host.composer.render(stack().child(std::move(e)));
  host.frame();
  EXPECT_EQ(redInk(host), 0) << "the SURFACE is gated by a spans gate";
  EXPECT_EQ(host.pixel(50, 50), SK_ColorGREEN)
      << "…and the CHILD is not: an arc-length window has nothing to say "
         "about a child, so it says nothing";

  // parts::surface() alone leaves the marks whole, and vice versa.
  Host onlyMarks(200, 200), onlySurface(200, 200);
  const auto both = [] {
    return maskBox().fill(red()).stroke(util::stroke(6, green()));
  };
  onlyMarks.composer.render(stack().child(
      both().mask(parts::marks(), by::spans(spans::upTo(0.0f)))));
  onlyMarks.frame();
  onlySurface.composer.render(stack().child(
      both().mask(parts::surface(), by::spans(spans::upTo(0.0f)))));
  onlySurface.frame();
  EXPECT_GT(SkColorGetR(onlyMarks.pixel(70, 70)), 180) << "surface kept";
  EXPECT_EQ(onlySurface.pixel(70, 70), SK_ColorBLACK) << "surface gated";
  EXPECT_GT(SkColorGetG(onlySurface.pixel(20, 70)), 150) << "marks kept";
}

TEST(ComposeR4Mask, TheEdgeGateIsWipesHalfPlaneToTheBit) {
  // wipe(angle, t) is `mask(by::edge(angle, t))` and nothing else changed:
  // same half-plane, same empty-box guard, same reach over decorations and
  // children. This is the parity witness the corpus port rests on.
  Host host(200, 200);
  host.composer.render(stack().child(
      box().absolute().left(20).top(20).width(160).height(160).fill(red())
          .stroke(util::stroke(6, green()))
          .mask(by::edge(0.0f, 0.5f))));
  host.frame();
  // A reveal, not a squash: the edge lands at the box's MIDPOINT.
  int edge = 0;
  for (int x = 20; x < 180; ++x)
    if (host.pixel(x, 100) != SK_ColorBLACK)
      edge = x;
  EXPECT_NEAR(edge, 100, 3);
  // …and it covers the node's decorations too, because a reveal reveals.
  EXPECT_EQ(host.pixel(178, 100), SK_ColorBLACK) << "the right stroke is gone";
  EXPECT_NE(host.pixel(21, 100), SK_ColorBLACK) << "the left stroke is not";
}

TEST(ComposeR4Mask, TheGateGeometryIsTrimsGeometry) {
  // THE TRIM-PARITY WITNESS THAT SURVIVES THE DELETION. trim() cut the
  // outline with SkTrimPathEffect; the corpus port of the 17 holdouts is
  // byte-identical only if the spans gate cuts the same path. So the
  // expected geometry is built HERE, by SkTrimPathEffect itself, and drawn
  // through a leaf that the masking family never touches.
  const SkRect r = SkRect::MakeXYWH(20, 20, 100, 100);
  // addRRect, not addRect: fraction 0 is the BOTTOM-LEFT corner because
  // that is SkPath::addRRect's start index, and that convention is what
  // every span answer and every trim() answer was always in.
  SkPathBuilder pb;
  pb.addRRect(SkRRect::MakeRect(SkRect::MakeWH(r.width(), r.height())));
  const SkPath boundary = pb.detach();
  for (auto [lo, hi] : {std::pair{0.0f, 0.4f}, std::pair{0.15f, 0.55f},
                        std::pair{0.6f, 1.0f}}) {
    SkPathBuilder dst;
    SkStrokeRec rec(SkStrokeRec::kFill_InitStyle);
    ASSERT_TRUE(SkTrimPathEffect::Make(lo, hi)->filterPath(&dst, boundary,
                                                           &rec));
    const SkPath want = dst.detach();

    Host gated(200, 200), truth(200, 200);
    gated.composer.render(stack().child(
        maskBox().stroke(util::stroke(6, red()))
            .mask(by::spans(spans::range(lo, hi)))));
    gated.frame();
    truth.composer.render(stack().child(
        custom([want](SkCanvas &c, const PaintContext &) {
          SkPaint p;
          p.setAntiAlias(true);
          p.setStyle(SkPaint::kStroke_Style);
          p.setStrokeWidth(6);
          p.setColor4f({1, 0, 0, 1}, nullptr);
          c.drawPath(want, p);
        }).rect(r)));
    truth.frame();
    EXPECT_EQ(boundaryRing(gated), boundaryRing(truth))
        << "window " << lo << ".." << hi;
  }
}

// ---- The positioned leaf set (ROADMAP §2 / Direction 1) --------------------

TEST(ComposePositioned, RectsAreHonoredAndYogaFree) {
  Host host;
  host.composer.render(
      positioned()
          .inset(0, 0, 0, 0)
          .child(box().key("a").left(10).top(20).width(50).height(30).fill(
              green()))
          .child(box().key("b").left(70).top(90).width(40).height(40).fill(
              Fill::color({1, 0, 0, 1}))));
  host.frame();
  const auto a = host.composer.bounds("a");
  const auto b = host.composer.bounds("b");
  ASSERT_TRUE(a && b);
  EXPECT_EQ(*a, SkRect::MakeXYWH(10, 20, 50, 30));
  EXPECT_EQ(*b, SkRect::MakeXYWH(70, 90, 40, 40));
  EXPECT_EQ(host.pixel(30, 30), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(90, 110), SK_ColorRED);
  EXPECT_EQ(host.pixel(5, 5), SK_ColorBLACK);
  // The whole point: root + container carry Yoga nodes, the leaves do not.
  const Composer::Stats &stats = host.composer.stats();
  EXPECT_EQ(stats.instances, 3u);
  EXPECT_EQ(stats.yogaNodes, 1u);
}

TEST(ComposePositioned, NestedRectsComposeYogaFreeAllTheWayDown) {
  Host host;
  host.composer.render(
      positioned().inset(0, 0, 0, 0).child(
          box().key("outer").left(40).top(40).width(100).height(100).child(
              box().key("inner").left(10).top(20).width(30).height(30))));
  host.frame();
  const auto inner = host.composer.bounds("inner");
  ASSERT_TRUE(inner);
  // bounds() is absolute: outer's origin + inner's own rect.
  EXPECT_EQ(*inner, SkRect::MakeXYWH(50, 60, 30, 30));
  EXPECT_EQ(host.composer.stats().yogaNodes, 1u);
  EXPECT_EQ(host.composer.stats().instances, 3u);
}

TEST(ComposePositioned, PctAndOpposingInsetsResolve) {
  Host host; // 200x200 canvas; container fills it
  host.composer.render(
      positioned()
          .inset(0, 0, 0, 0)
          // pct dims resolve against the container's rect
          .child(box().key("half").left(0).top(0).width(pct(50)).height(
              pct(25)))
          // open width + right inset pins the far edge
          .child(box().key("pinned").left(20).top(100).right(30).height(10)));
  host.frame();
  const auto half = host.composer.bounds("half");
  const auto pinned = host.composer.bounds("pinned");
  ASSERT_TRUE(half && pinned);
  EXPECT_EQ(*half, SkRect::MakeXYWH(0, 0, 100, 50));
  EXPECT_EQ(*pinned, SkRect::MakeXYWH(20, 100, 150, 10));
}

TEST(ComposePositioned, TextMeasuresAgainstItsSuppliedWidth) {
  Host host;
  sigil::weave::TextStyle style;
  style.shaping.fontSize = 14;
  style.paint.foreground.setColor(SK_ColorWHITE);
  host.composer.render(
      positioned().inset(0, 0, 0, 0).child(
          text(u8"wrap me across a narrow measure", style)
              .key("t")
              .left(10)
              .top(10)
              .width(90)));
  host.frame();
  const auto t = host.composer.bounds("t");
  ASSERT_TRUE(t);
  EXPECT_EQ(t->left(), 10);
  EXPECT_EQ(t->width(), 90);
  // Open height: measured — a wrapped run is taller than one line.
  EXPECT_GT(t->height(), 20.0f);
}

TEST(ComposePositioned, StructuralPruneAndMovesStillWork) {
  Host host;
  auto tree = [](float x) {
    return positioned().inset(0, 0, 0, 0).child(
        box().key("m").left(x).top(10).width(40).height(40).fill(
            green()));
  };
  host.composer.render(tree(10));
  host.frame();
  host.composer.render(tree(10)); // identical: prune, no cache writes
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  host.composer.render(tree(120)); // moved: repaints, and the rect follows
  host.frame();
  EXPECT_EQ(host.pixel(140, 30), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(30, 30), SK_ColorBLACK);
  const auto m = host.composer.bounds("m");
  ASSERT_TRUE(m);
  EXPECT_EQ(m->left(), 120);
}

TEST(ComposePositioned, HitTestAndZOrderSeeChildren) {
  Host host;
  host.composer.render(
      positioned()
          .inset(0, 0, 0, 0)
          .child(box().key("under").left(20).top(20).width(60).height(60)
                     .fill(Fill::color({1, 0, 0, 1}))
                     .hitTestable(true))
          .child(box().key("over").left(50).top(50).width(60).height(60)
                     .zIndex(1)
                     .fill(green())
                     .hitTestable(true)));
  host.frame();
  EXPECT_EQ(host.pixel(60, 60), SK_ColorGREEN); // zIndex 1 paints on top
  const auto hitOver = host.composer.hitTest({60, 60});
  const auto hitUnder = host.composer.hitTest({25, 25});
  ASSERT_TRUE(hitOver && hitUnder);
  EXPECT_EQ(*hitOver, "over");
  EXPECT_EQ(*hitUnder, "under");
}

TEST(ComposePositioned, TogglingPositionedRemountsCleanly) {
  Host host;
  auto child = []() {
    return box().key("c").left(10).top(10).width(30).height(30).fill(
        green());
  };
  host.composer.render(positioned().inset(0, 0, 0, 0).child(child()));
  host.frame();
  EXPECT_EQ(host.composer.stats().yogaNodes, 1u);
  // The same child under a plain box: rejoins the Yoga world.
  host.composer.render(box().inset(0, 0, 0, 0).child(child()));
  host.frame();
  EXPECT_EQ(host.composer.stats().yogaNodes, 2u);
  EXPECT_EQ(host.pixel(20, 20), SK_ColorGREEN); // absolute insets agree
  // And back again.
  host.composer.render(positioned().inset(0, 0, 0, 0).child(child()));
  host.frame();
  EXPECT_EQ(host.composer.stats().yogaNodes, 1u);
  EXPECT_EQ(host.pixel(20, 20), SK_ColorGREEN);
}

TEST(ComposeR4Mask, ASettledBoundGateRecachesWithoutAnyNewApi) {
  // §20's acceptance test, enabled by the measured-stability RELEASE:
  // a bound gate that has held still for kSettleFrames stops declaring
  // volatility, so the ANCESTOR caches across it — the probe that
  // preceded the fix measured recording kept (memo) but 5/5 live paints
  // (the flag never released).
  choreograph::Output<float> reveal{0.0f};
  Host host;
  host.composer.render(box().child(
      maskBox().stroke(util::stroke(6, red()))
          .mask(by::spans(spans::upTo(&reveal)))));
  host.frame();
  reveal = 1.0f;                 // the ramp lands…
  for (int i = 0; i < 12; ++i)   // …and the release warms up (8 frames)
    host.frame(0.016);
  unsigned settledRecords = 0, settledPaints = 0;
  for (int i = 0; i < 5; ++i) {  // the hold must now cost NOTHING
    host.frame(0.016);
    settledRecords += host.composer.stats().picturesRecorded;
    settledPaints += host.composer.stats().nodesPainted;
  }
  EXPECT_EQ(settledRecords, 0u)
      << "a pinned bound gate re-recorded during its hold";
  EXPECT_EQ(settledPaints, 0u)
      << "a pinned bound gate painted live during its hold";
  EXPECT_GT(redInk(host, 0, 0, 200, 200), 400); // fully revealed, drawn

  // THE STALE-REPLAY CONTROL: the frame the binding moves again, the
  // release must re-declare BEFORE anything paints — the ancestor's
  // cached picture holds the settled state and must not replay.
  const int inkBefore = redInk(host, 0, 0, 200, 200);
  reveal = 0.3f;
  host.frame(0.016);
  const int inkAfter = redInk(host, 0, 0, 200, 200);
  EXPECT_LT(inkAfter, inkBefore / 2)
      << "the moved gate did not repaint — a stale picture replayed";
}
