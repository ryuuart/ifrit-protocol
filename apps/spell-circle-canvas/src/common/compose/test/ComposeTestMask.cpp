#include "ComposeTestSupport.h"

#include "../ComposeInternal.h" // propsEqual — the comparator, taken directly

// ---------------------------------------------------------------------------
// PHASE R4 — THE MASKING FAMILY
//
// `trim()` and `wipe()` are gone; one verb replaced both, and it is a
// relation between two named factors: `parts::` says WHICH of a node's
// paint outputs a mask reaches, `by::` says HOW that paint arrives.
//
// The eight tests S1–S8 below are the design's own sample set — eight real
// corpus sites, chosen because each one broke a different candidate shape.
// They are here as tests because a family designed against eight pictures
// should be able to draw all eight, and because the shape that could not
// draw one of them was rejected for exactly that.

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

// RE-FIXTURED 2026-07-29 (ROADMAP §35.2), and the ORIGINAL NAME IS BACK.
// §34's audit renamed this rather than repairing it: phase 0 parked at
// 0.0001, so a retarget from 0.0001 and a fresh mount from 0 both ramp UP
// to 0.5 and no assertion here could tell them apart. Phase 0 now parks
// ABOVE the target — 0.8 → 0.5 — which makes the two hypotheses point in
// OPPOSITE DIRECTIONS: a retarget ramps DOWN through 0.65, a mount from
// zero ramps UP through 0.25. The midpoint sample separates them, and
// `half > settled` is the assertion that could not previously exist.
TEST(ComposeR4Mask, S3TheGateRetargetsAcrossAnIfElseInsteadOfMounting) {
  // ScenesBeethoven: phase 0 is unswept, phase 1 sweeps each arc over its
  // measured span. `animate(to(span))` is RAMP-ON-CHANGE — it starts from
  // the property's CURRENT value — so the gate must occupy the same
  // animation slot in both branches or the motion mounts from zero and the
  // ring blinks. maskAnims is positional, which is what makes this work.
  const auto tree = [](int phase) {
    Element e = maskBox().stroke(util::stroke(6, red()));
    if (phase == 0)
      e.mask(by::spans(spans::upTo(0.8f)));
    else
      e.mask(by::spans(
          spans::upTo(animate(to(0.5f), {400ms, &choreograph::easeNone}))));
    return stack().child(std::move(e));
  };
  Host host(200, 200);
  host.composer.render(tree(0));
  host.frame();
  const size_t parked = inkedCount(boundaryRing(host));
  host.composer.render(tree(1));
  host.frame(0.2); // halfway
  const size_t half = inkedCount(boundaryRing(host));
  host.frame(0.25); // settled at 0.5
  const size_t settled = inkedCount(boundaryRing(host));
  EXPECT_GT(parked, settled + 5u) << "0.8 shows more arc than 0.5";
  EXPECT_LT(half, parked) << "the gate left its parked value";
  // THE DISCRIMINATING ASSERTION. A retarget descends 0.8 → 0.5 and is
  // ABOVE 0.5 at the midpoint; a mount from zero ascends 0 → 0.5 and is
  // BELOW it. Only one of the two can satisfy this line.
  EXPECT_GT(half, settled + 2u)
      << "the gate mounted from zero instead of retargeting from 0.8";
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

// ---- S7c · the OTHER coverage channel, and both complements ---------------

namespace {
/** Five white plates side by side, each masked by its own coverage gate.
 *  White over the black clear colour, so the byte a plate shows IS its
 *  coverage times 255 and nothing else is in the arithmetic. */
Element coveragePlates(const std::vector<Gate> &gates) {
  Element root = stack();
  int i = 0;
  for (const Gate &g : gates) {
    root.child(box()
                   .absolute()
                   .left(10.0f + 38.0f * (float)i)
                   .top(20)
                   .width(30)
                   .height(160)
                   .fill(Fill::color({1, 1, 1, 1}))
                   .mask(g));
    ++i;
  }
  return root;
}
int plateByte(Host &host, int i) {
  return (int)SkColorGetR(host.pixel(25 + 38 * i, 100));
}
} // namespace

TEST(ComposeR4Mask, S7cTheLumaGateIsRec601OnEncodedPremultipliedValues) {
  // THE PIXEL PIN FOR THE LUMA LAW, and it is deliberately not pinned with
  // greys: a grey pins NOTHING about the coefficients (every weighting of
  // equal channels is the same number), and the primaries alone pin nothing
  // about the transfer function (0 and 1 are the sRGB curve's fixed points,
  // the trap `RendersClearColorWhenEmpty` fell into one library over).
  //
  // Five plates, five different wrong answers ruled out:
  //
  //   matte           Rec.601   Rec.709   any weights   unpremultiplied
  //                   encoded   encoded   on LINEAR     luma
  //   pure red         76        54        76            76
  //   pure green      150       182       150           150
  //   pure blue        29        18        29            29
  //   50% grey        128       128        55           128
  //   50% white       128       128        55           255
  //
  // Column 1 is the law (by::luma). Column 2 is the Poynton mistake —
  // LUMINANCE coefficients, which are defined on linear light, applied to
  // encoded values. Column 3 is linearising first, which compose has no
  // stage for: its surfaces carry NO colour space, so a shader's channels
  // are the display-encoded numbers the author wrote. Column 4 ignores the
  // matte's alpha; premultiplied is what makes a transparent matte read as
  // black, the way After Effects' luma matte does.
  Host host(200, 200);
  host.composer.render(coveragePlates({
      by::luma(Material::solid({1, 0, 0, 1})),
      by::luma(Material::solid({0, 1, 0, 1})),
      by::luma(Material::solid({0, 0, 1, 1})),
      by::luma(Material::solid({0.5f, 0.5f, 0.5f, 1})),
      by::luma(Material::solid({1, 1, 1, 0.5f})),
  }));
  host.frame();
  EXPECT_NEAR(plateByte(host, 0), 76, 2) << "red is 0.299, not 0.2126 (Rec.709 "
                                            "luminance) and not 1/3";
  EXPECT_NEAR(plateByte(host, 1), 150, 2) << "green is 0.587, not 0.7152";
  EXPECT_NEAR(plateByte(host, 2), 29, 2) << "blue is 0.114, not 0.0722";
  EXPECT_NEAR(plateByte(host, 3), 128, 2)
      << "50% grey weighs 0.5 — the luma is taken on the ENCODED value. A "
         "linearised reading gives 0.214 and this plate reads 55";
  EXPECT_NEAR(plateByte(host, 4), 128, 2)
      << "half-transparent white IS 50% grey: the luma is premultiplied, so "
         "a transparent matte reads as black and hides";
}

TEST(ComposeR4Mask, S7cTheLumaLawIsTheSameThroughAShader) {
  // The colour path and the shader path are two implementations of one law
  // (a dot product in C++, an SkSL pass over the coverage layer), which is
  // exactly the shape of asymmetry that ships wrong. One ramp, green to
  // blue, checked at both ends and in the middle:
  //   left  (0,1,0) -> 0.587          -> 150
  //   mid   (0,.5,.5) -> .2935+.057   ->  89
  //   right (0,0,1) -> 0.114          ->  29
  Host host(200, 200);
  host.composer.render(stack().child(
      box().absolute().left(20).top(20).width(160).height(160)
          .fill(Fill::color({1, 1, 1, 1}))
          .mask(by::luma(Material::linear({0, 0}, {160, 0},
                                          {{0.0f, {0, 1, 0, 1}},
                                           {1.0f, {0, 0, 1, 1}}})))));
  host.frame();
  EXPECT_NEAR((int)SkColorGetR(host.pixel(22, 100)), 150, 4) << "green end";
  EXPECT_NEAR((int)SkColorGetR(host.pixel(100, 100)), 89, 4) << "the mix";
  EXPECT_NEAR((int)SkColorGetR(host.pixel(177, 100)), 29, 4) << "blue end";
}

TEST(ComposeR4Mask, S7dEachCoverageGateHasItsComplementAsItsOwnTerm) {
  // `by::outside(r)` made the region complement a TERM rather than a mode
  // flag; the coverage sources get the same treatment and the same law —
  // a gate is a SHOW set, and the complement shows exactly the rest.
  // Mechanically it is kDstOut instead of kDstIn, which is dst·(1-a): the
  // pair of plates must sum to 255 at every sample, not merely differ.
  Host host(200, 200);
  const Material ramp = Material::solid({0.5f, 0.5f, 0.5f, 0.25f});
  host.composer.render(coveragePlates({
      by::alpha(ramp),    // 0.25            ->  64
      by::alphaOut(ramp), // 1 - 0.25        -> 191
      by::luma(ramp),     // 0.25 * 0.5      ->  32
      by::lumaOut(ramp),  // 1 - 0.125       -> 223
  }));
  host.frame();
  EXPECT_NEAR(plateByte(host, 0), 64, 2);
  EXPECT_NEAR(plateByte(host, 1), 191, 2) << "alphaOut is not alpha";
  EXPECT_EQ(plateByte(host, 0) + plateByte(host, 1), 255)
      << "the alpha complement must show exactly the rest";
  EXPECT_NEAR(plateByte(host, 2), 32, 2);
  EXPECT_NEAR(plateByte(host, 3), 223, 2) << "lumaOut is not luma";
  EXPECT_EQ(plateByte(host, 2) + plateByte(host, 3), 255)
      << "…and so must the luma complement";
}

TEST(ComposeR4Mask, ACoverageGatesChannelAndSenseReachTheComparator) {
  // THE COMPARATOR PIN, taken against propsEqual() DIRECTLY — the lesson of
  // the equality audit is that a render-two-trees harness can pass while the
  // comparator is broken, because keyed siblings never prune into one
  // another.
  //
  // `channel` is a new field, so the structured-binding pin in
  // ComposeInternal.h forced a ruling on it. `outside` is NOT new — it was
  // already compared in the Shape arm — so nothing in the compiler would
  // have noticed the Coverage arm ignoring it, and a matte would have
  // compared equal to its own inverse: pruned, and stuck showing the wrong
  // half for as long as the node lives.
  const Material m = Material::solid({0.5f, 0.5f, 0.5f, 1});
  const auto node = [&](Gate g) {
    Element el = box().mask(std::move(g));
    return el.node();
  };
  const auto same = [&](Gate a, Gate b) {
    return detail::propsEqual(*node(std::move(a)), *node(std::move(b)));
  };
  EXPECT_TRUE(same(by::alpha(m), by::alpha(m))) << "a static matte prunes";
  EXPECT_TRUE(same(by::lumaOut(m), by::lumaOut(m)));
  EXPECT_FALSE(same(by::alpha(m), by::alphaOut(m)))
      << "a matte compares equal to its own INVERSE — `outside` is missing "
         "from the Coverage arm of Gate::operator==";
  EXPECT_FALSE(same(by::luma(m), by::lumaOut(m))) << "…and for luma";
  EXPECT_FALSE(same(by::alpha(m), by::luma(m)))
      << "the two channels compare equal — `channel` is missing from the "
         "Coverage arm of Gate::operator==";
  EXPECT_FALSE(same(by::alphaOut(m), by::lumaOut(m)));
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

// (`StackedSpanGatesIntersectRatherThanUnion` was deleted by the 2026-07-28
//  audit ruling: it ran the same fixture and the same mask pair as the test
//  below, which pins the intersection at PIXELS and refutes union with a
//  disjoint pair — strictly more, on the same claim.)

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

// RENAMED 2026-07-28 (audit): the fixture carries ONE mark, so there is no
// unnamed sibling to leave alone; what this uniquely pins is the
// silent-no-op law for a label that matches nothing.
TEST(ComposeR4Mask, AnUnmatchedMaskNameIsASilentNoOp) {
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

// ---- §38: the Fill lane — §20's release, one lane over --------------------
//
// The measured defect (ROADMAP §38): ONE bound `fill(&output)` on one leaf
// held `subtreeVolatile` on the whole chain forever, so `contentStable` was
// false, promotion answered `Promotion::Volatile`, and 512 cells replayed
// 512 pictures at ~11 µs each for a colour that never moved — 5.02 ms/frame,
// 19.6×. The extension: `ContentScalars` gained a Fill lane, `boundFill`
// moved from the unconditional ownContent terms into `scalarContent`, and
// the §20 settle/release/scan machinery covers it with no new concept.
//
// The pins below prove the release against PROMOTION (`contentStable` /
// the refusals mask), not merely against the recording — the 19.6× was
// promotion denial — and prove the staleness contract: the frame the
// Output moves, volatility re-declares BEFORE anything stale replays.

namespace {

/** §38's measured fixture, at test size: root → frame → row of stroked,
 *  shaped cells + ONE accent whose fill is bound. The accent's ancestors
 *  are what the binding used to poison. `Cache::Texture` on the root makes
 *  the promotion observable deterministically (the asked-for bake obeys
 *  the same volatility gate the automatic one does, without the
 *  cost-threshold timing an assertion could flap on). */
Element settledFillPanel(const choreograph::Output<Fill> *tint) {
  auto row = box().key("row").row().wrapLines().gap(2);
  for (int id = 0; id < 12; ++id)
    row.child(box()
                  .width(26)
                  .height(26)
                  .shape(shapes::star(5 + id % 3, 0.45f, 0.08f))
                  .fill(blue())
                  .stroke(util::stroke(1.5f, green())));
  row.child(box().key("accent").width(26).height(26).fill(
      Animatable<Fill>(tint)));
  return box().key("root").cache(Cache::Texture).column().padding(6).child(
      box().key("frame").column().padding(4).child(std::move(row)));
}

/** The profile row for the node keyed `key`, from the last draw (labels
 *  are "<key> (<kind> WxH)"). */
const Composer::NodeCost *rowOf(Host &host, const char *key) {
  const std::string prefix = std::string(key) + " (";
  for (const Composer::NodeCost &row : host.composer.profile())
    if (row.label.rfind(prefix, 0) == 0)
      return &row;
  return nullptr;
}

} // namespace

TEST(ComposeSettledFill, ASettledBoundFillReleasesVolatilityAndPromotes) {
  // Pin (a): the release must show against Promotion::Volatile's
  // `contentStable`, because promotion is a SEPARATE consumer of
  // `subtreeVolatile` from the memo — keeping the recording while still
  // refusing the bake would keep all of the 5 ms.
  choreograph::Output<Fill> tint{red()};
  Host host(300, 300);
  host.composer.render(settledFillPanel(&tint));
  host.composer.setProfiling(true);
  host.frame();
  const auto accentRect = host.composer.bounds("accent");
  ASSERT_TRUE(accentRect);
  const int ax = (int)accentRect->centerX(), ay = (int)accentRect->centerY();
  EXPECT_EQ(host.pixel(ax, ay), SK_ColorRED);
  // BEFORE the settle: the bound fill denies contentStable at the root —
  // the asked-for texture is refused and the whole chain paints live.
  {
    const Composer::NodeCost *root = rowOf(host, "root");
    ASSERT_TRUE(root);
    EXPECT_TRUE(root->refused(Composer::Promotion::Volatile))
        << "a fresh bound fill must declare volatility";
    EXPECT_EQ(host.composer.stats().texturesLive, 0u)
        << "the root's asked-for bake must be refused while volatile";
  }
  for (int i = 0; i < 12; ++i) // kScalarSettleFrames = 8, plus the release
    host.frame(0.016);         // walk and the settling frame's re-record
  // AFTER: released — the node promotes like a plain one.
  {
    const Composer::NodeCost *root = rowOf(host, "root");
    ASSERT_TRUE(root);
    EXPECT_FALSE(root->refused(Composer::Promotion::Volatile))
        << "a settled bound fill still denies contentStable — no release";
    EXPECT_GE(host.composer.stats().texturesLive, 1u)
        << "the released root never took its bake — promotion still denied";
  }
  // …and the hold costs NOTHING (the §20 acceptance test's bar).
  unsigned settledRecords = 0, settledPaints = 0;
  for (int i = 0; i < 4; ++i) {
    host.frame(0.016);
    settledRecords += host.composer.stats().picturesRecorded;
    settledPaints += host.composer.stats().nodesPainted;
  }
  EXPECT_EQ(settledRecords, 0u) << "a settled bound fill re-recorded";
  EXPECT_EQ(settledPaints, 0u) << "a settled bound fill painted live";
  EXPECT_EQ(host.pixel(ax, ay), SK_ColorRED);

  // Pin (b), THE STALENESS CONTROL — the moved-again pin, §20's model: the
  // frame the Output moves, the scan must re-declare and stale every
  // recording AND the root's bake BEFORE anything paints. One frame, no
  // stale pixel, ever.
  tint = green();
  host.frame(0.016);
  EXPECT_EQ(host.pixel(ax, ay), SK_ColorGREEN)
      << "the moved fill's frame showed a stale colour";
  {
    const Composer::NodeCost *root = rowOf(host, "root");
    ASSERT_TRUE(root);
    EXPECT_TRUE(root->refused(Composer::Promotion::Volatile))
        << "the moved fill must re-declare volatility the same frame";
  }
  // …and the cycle closes: it settles AGAIN and re-releases.
  for (int i = 0; i < 12; ++i)
    host.frame(0.016);
  const Composer::NodeCost *root = rowOf(host, "root");
  ASSERT_TRUE(root);
  EXPECT_FALSE(root->refused(Composer::Promotion::Volatile))
      << "a re-settled bound fill did not re-release";
  EXPECT_EQ(host.pixel(ax, ay), SK_ColorGREEN);
}

TEST(ComposeSettledFill, AMovingBoundFillNeverReleases) {
  // Pin (c): the release must NOT fire for a fill that IS moving — a
  // binding driven every frame keeps full volatility, keeps the refusal,
  // and keeps painting the true colour.
  choreograph::Output<Fill> tint{red()};
  Host host(300, 300);
  host.composer.render(settledFillPanel(&tint));
  host.composer.setProfiling(true);
  host.frame();
  const auto accentRect = host.composer.bounds("accent");
  ASSERT_TRUE(accentRect);
  const int ax = (int)accentRect->centerX(), ay = (int)accentRect->centerY();
  for (int i = 0; i < 20; ++i) {
    const float t = (float)(i % 10) / 10.0f;
    tint = Fill::color({1.0f - t, 0.0f, t, 1.0f});
    host.frame(0.016);
    const Composer::NodeCost *root = rowOf(host, "root");
    ASSERT_TRUE(root);
    EXPECT_TRUE(root->refused(Composer::Promotion::Volatile))
        << "a driven bound fill released its volatility at frame " << i;
    EXPECT_EQ(host.composer.stats().texturesLive, 0u)
        << "a bake was held across a driven bound fill at frame " << i;
  }
  // The last written colour is on screen, not a settled ancestor's bake.
  tint = green();
  host.frame(0.016);
  EXPECT_EQ(host.pixel(ax, ay), SK_ColorGREEN);
}

// ---- §17/§20's memo carve-outs: the three lanes they forgot ---------------
//
// `computeVolatile` enumerated its content-volatility terms four times —
// once for `ownContent`, once for the group memo, once inside each of the
// two memo carve-outs. The two carve-outs never mentioned a bound fill or
// a live effect, so a node carrying one of those AND an animated gate took
// a memo it had no right to and replayed a recording that had baked the
// old colour. Each test below was RED before the terms were derived by
// subtraction; each control is the removal of exactly one term.

TEST(ComposeCache, ABoundFillMovingUnderAHeldGateRepaints) {
  // A node carrying BOTH a bound fill() and an animated mask gate. §17's
  // scalarMemo holds the recording while the GATE's floats hold still —
  // and the recording baked the fill colour. If the bound fill is not part
  // of the memo's compare, moving it while the gate holds replays the old
  // colour.
  choreograph::Output<float> reveal{1.0f};
  choreograph::Output<Fill> tint{Fill::color({1, 0, 0, 1})}; // red
  Host host(200, 200);
  host.composer.render(box().child(
      maskBox().fill(&tint).mask(by::spans(spans::upTo(&reveal)))));
  host.frame();
  for (int i = 0; i < 4; ++i)
    host.frame(0.016); // let the memo bake and hold
  EXPECT_GT(redInk(host, 25, 25, 115, 115), 4000) << "red to begin with";
  // THE CACHING NUMBERS. When this pin was written (§38's bugfix night)
  // the node took NO memo — 0 records, 8 live paints (node + parent, 4
  // frames each) — because a bound Fill was not a number the §17 compare
  // could see. §38's extension gave ContentScalars a Fill lane, so the
  // node takes the memo now: the recording (which baked the red) REPLAYS
  // while the fill provably holds, and only the parent still paints live.
  // The pixel half below is unchanged and is the pin that matters — the
  // memo's compare must see the fill move, or the old colour replays.
  unsigned records = 0, live = 0;
  for (int i = 0; i < 4; ++i) {
    host.frame(0.016);
    records += host.composer.stats().picturesRecorded;
    live += host.composer.stats().nodesPainted;
  }
  EXPECT_EQ(records, 0u) << "the memoized recording holds while both hold";
  EXPECT_EQ(live, 4u) << "the parent paints live; the node replays its memo";
  tint = Fill::color({0, 0, 1, 1}); // …now turn it blue, gate unmoved
  host.frame(0.016);
  EXPECT_LT(redInk(host, 25, 25, 115, 115), 100)
      << "the bound fill moved and the node replayed a stale recording";
}

TEST(ComposeCache, ALiveEffectMovingUnderAHeldGateRepaints) {
  // Same hole, a different lane: a LIVE effect (§11 bound uniform) is
  // captured by the recording, and the §17 scalar memo's refusal list
  // does not mention it.
  static sk_sp<SkRuntimeEffect> fx = [] {
    auto [e, err] = SkRuntimeEffect::MakeForShader(
        SkString("uniform shader content; uniform float amt;"
                 "half4 main(float2 p) { half4 c = content.eval(p);"
                 "  return half4(c.r * half(amt), c.g, c.b, c.a); }"));
    if (!e)
      ADD_FAILURE() << err.c_str();
    return e;
  }();
  choreograph::Output<float> reveal{1.0f};
  choreograph::Output<float> amt{1.0f};
  Host host(200, 200);
  host.composer.render(box().child(
      maskBox()
          .fill(Fill::color({1, 0, 0, 1}))
          .effect(Effect::shader(fx, {{"amt", 1.0f}}).uniform("amt", &amt))
          .mask(by::spans(spans::upTo(&reveal)))));
  host.frame();
  for (int i = 0; i < 4; ++i)
    host.frame(0.016);
  EXPECT_GT(redInk(host, 25, 25, 115, 115), 4000) << "red to begin with";
  amt = 0.0f; // the effect must now kill the red channel
  host.frame(0.016);
  EXPECT_LT(redInk(host, 25, 25, 115, 115), 100)
      << "the live effect moved and the node replayed a stale recording";
}

TEST(ComposeCache, ALiveEffectMovingOverAHeldMaterialRepaints) {
  // The OTHER memo, same hole: liveMatOnly holds the recording while the
  // MATERIAL's resolved shader pointer is stable, and its refusal list
  // does not mention a live effect either.
  static sk_sp<SkRuntimeEffect> fx = [] {
    auto [e, err] = SkRuntimeEffect::MakeForShader(
        SkString("uniform shader content; uniform float amt;"
                 "half4 main(float2 p) { half4 c = content.eval(p);"
                 "  return half4(c.r * half(amt), c.g, c.b, c.a); }"));
    if (!e)
      ADD_FAILURE() << err.c_str();
    return e;
  }();
  static sk_sp<SkRuntimeEffect> matfx = [] {
    auto [e, err] = SkRuntimeEffect::MakeForShader(
        SkString("uniform float lift;"
                 "half4 main(float2 p) { return half4(1.0, half(lift), 0.0,"
                 "                                    1.0); }"));
    if (!e)
      ADD_FAILURE() << err.c_str();
    return e;
  }();
  choreograph::Output<float> lift{0.0f}; // the material's own bound uniform
  choreograph::Output<float> amt{1.0f};  // the effect's
  Host host(200, 200);
  host.composer.render(box().child(
      maskBox()
          .fill(Material::sksl(matfx).uniform("lift", &lift))
          .effect(Effect::shader(fx, {{"amt", 1.0f}}).uniform("amt", &amt))));
  host.frame();
  for (int i = 0; i < 4; ++i)
    host.frame(0.016);
  EXPECT_GT(redInk(host, 25, 25, 115, 115), 4000) << "red to begin with";
  amt = 0.0f; // the material holds; only the EFFECT moves
  host.frame(0.016);
  EXPECT_LT(redInk(host, 25, 25, 115, 115), 100)
      << "the live effect moved and the live-material memo replayed stale";
}

