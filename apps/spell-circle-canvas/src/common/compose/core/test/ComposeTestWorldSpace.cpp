// A material authored in canvas coordinates stays anchored there: through a
// node's rotation, across two separately laid-out siblings, under a layout
// offset, when the node or an ancestor moves, and per frame under a bound
// transform. The flag rides the prune signature and the node's world matrix
// joins the resolve digest.

#include "support/CoreTestSupport.h"

namespace {

SkPoint brightestPixel(Host& host) {
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(200, 200));
  host.surface->readPixels(bm.pixmap(), 0, 0);
  int bestX = 0, bestY = 0, best = -1;
  for (int y = 0; y < 200; ++y)
    for (int x = 0; x < 200; ++x) {
      const SkColor c = bm.getColor(x, y);
      const int lum =
          (int)SkColorGetR(c) + (int)SkColorGetG(c) + (int)SkColorGetB(c);
      if (lum > best) {
        best = lum;
        bestX = x;
        bestY = y;
      }
    }
  return {(float)bestX, (float)bestY};
}

/** One light over the whole canvas — a radial highlight authored at CANVAS
 *  (70,70). Flagged, it is authored once; unflagged, the caller has to
 *  hand-convert it into the node's local px, which is what the flag
 *  exists to spare them. */
material::skia::Paint canvasLight(bool flagged,
                                  SkPoint nodeOriginForHandConversion) {
  const SkPoint c = flagged ? SkPoint{70, 70}
                            : SkPoint{70 - nodeOriginForHandConversion.x(),
                                      70 - nodeOriginForHandConversion.y()};
  material::skia::Paint m = material::skia::Paint::radial(
      c, 70, {{0.0f, {1, 1, 1, 1}}, {1.0f, {0.1f, 0.05f, 0, 1}}});
  if (flagged) m.worldSpace();
  return m;
}

/** The chaucer shape in miniature: a panel at canvas (40,40,120,120)
 *  inside a group that rotates about its own centre — the rete. */
Element rotatedInstrument(float rotationDeg, bool flagged) {
  auto group = box()
                   .rect(SkRect::MakeXYWH(20, 20, 160, 160))
                   .key("group")
                   .rotate(rotationDeg)
                   .transformOrigin(0.5f, 0.5f);
  // The corner child makes the panel RECORD — a childless leaf paints
  // live and re-resolves on every reach, which would hide a stale-W
  // recording from every pin built on this fixture.
  group.child(
      box()
          .absolute()
          .left(20)
          .top(20)
          .width(120)
          .height(120)
          .key("panel")
          .fill(canvasLight(flagged, {40, 40}))
          .child(box().absolute().left(0).top(0).width(4).height(4).fill(
              Fill::color({0, 0.3f, 0, 1}))));
  return box().child(std::move(group));
}

}  // namespace

// A rotated node samples the world field THROUGH its rotation, so the
// highlight sits still in canvas space while the object turns. Without the
// flag the highlight rotates with the object, which is the control below.
//
// This also exercises the reconcile-time staleWorldSpaceBelow walk: the
// rotation is a DESCRIBED transform on an ancestor, and the panel below it
// prunes, so its recording has to be staled explicitly — nothing about the
// panel's own description changed.
TEST(ComposeWorldSpace, ARotatedNodeSamplesTheWorldFieldThroughItsRotation) {
  Host host;
  host.composer.render(rotatedInstrument(0, true));
  host.frame();
  const SkPoint at0 = brightestPixel(host);
  EXPECT_LT(SkPoint::Distance(at0, {70, 70}), 3.0f)
      << "flagged light not at its authored canvas position";
  // SAME host, re-described rotation — the ancestor patches, the panel
  // prunes, and the panel's recording must not replay the old anchoring.
  host.composer.render(rotatedInstrument(40, true));
  host.frame();
  const SkPoint at40 = brightestPixel(host);
  EXPECT_LT(SkPoint::Distance(at0, at40), 3.0f)
      << "the highlight turned with the object — world anchoring failed";
  // Control: the same instrument WITHOUT the flag, whose light turns with
  // the object. Without this arm the assertion above would also pass on a
  // fixture where the rotation never reached the light at all.
  Host c0, c40;
  c0.composer.render(rotatedInstrument(0, false));
  c40.composer.render(rotatedInstrument(40, false));
  c0.frame();
  c40.frame();
  EXPECT_GT(SkPoint::Distance(brightestPixel(c0), brightestPixel(c40)), 10.0f)
      << "the unflagged control no longer reproduces the defect";
}

// A field continuous ACROSS separately-laid-out nodes: two flex siblings
// share one worldSpace ramp and the edge between them is pixel-continuous.
// Unflagged, each node restarts the ramp in its own space and the edge
// jumps, which is the control.
TEST(ComposeWorldSpace, TwoSiblingsShareOneContinuousField) {
  const auto scene = [](bool flagged) {
    material::skia::Paint ramp = material::skia::Paint::linear(
        {0, 0}, {200, 0}, {{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}});
    if (flagged) ramp.worldSpace();
    return box()
        .row()
        .child(box().width(100).height(200).fill(ramp))
        .child(box().width(100).height(200).fill(ramp));
  };
  Host flagged, control;
  flagged.composer.render(scene(true));
  control.composer.render(scene(false));
  flagged.frame();
  control.frame();
  const auto redAt = [](Host& h, int x) {
    return (int)SkColorGetR(h.pixel(x, 100));
  };
  // Flagged: the edge is continuous (a 4-px step across it moves the ramp
  // by ~2% of its span at most)…
  EXPECT_LT(std::abs(redAt(flagged, 98) - redAt(flagged, 102)), 20)
      << "the flagged siblings do not share one field";
  // …and it IS the canvas-wide ramp: red end left, blue end right.
  EXPECT_GT(redAt(flagged, 4), 220);
  EXPECT_LT(redAt(flagged, 196), 40);
  // Control: each node restarts, so the second sibling snaps back to red.
  EXPECT_GT(std::abs(redAt(control, 98) - redAt(control, 102)), 80)
      << "the unflagged control is continuous — the pin lost its contrast";
}

// Alignment through the layout offset: a node at (40,40) samples the field
// where it SITS. The control is a resolve with an identity root matrix —
// the documented degradation when a material is resolved outside a composer
// — which anchors node-locally and shifts the falloff by exactly the
// offset.
TEST(ComposeWorldSpace, TheLayoutOffsetAlignsTheFieldAndIdentityDegrades) {
  Host host;
  host.composer.render(
      box().child(box().absolute().left(40).top(40).width(120).height(120).fill(
          canvasLight(true, {0, 0}))));
  host.frame();
  EXPECT_LT(SkPoint::Distance(brightestPixel(host), {70, 70}), 3.0f)
      << "the composer resolve did not anchor through the layout offset";
  // Identity-toRoot resolve: same material, standalone PaintContext (no
  // composer, toRoot = I). The falloff lands node-LOCALLY — painted at the
  // node's offset it sits at canvas (110,110), a (40,40) shift.
  PaintContext bare;
  bare.size = {120, 120};
  const Fill f = resolveFill(canvasLight(true, {0, 0}), bare);
  ASSERT_EQ(f.kind, Fill::Kind::Shader);
  Host raw;
  SkPaint p;
  p.setShader(f.shaderValue);
  raw.surface->getCanvas()->clear(SK_ColorBLACK);
  raw.surface->getCanvas()->save();
  raw.surface->getCanvas()->translate(40, 40);
  raw.surface->getCanvas()->drawRect(SkRect::MakeWH(120, 120), p);
  raw.surface->getCanvas()->restore();
  EXPECT_LT(SkPoint::Distance(brightestPixel(raw), {110, 110}), 3.0f)
      << "identity toRoot must degrade to node-local, deterministically";
}

// LAYOUT moves the node while the field stays put in canvas space. The
// node's own properties PRUNE here — only a sibling spacer changes — so the
// invalidation can only come from the layout-rect sync noticing the node's
// world matrix moved. No other phase sees this.
TEST(ComposeWorldSpace, ALayoutMoveLeavesTheFieldAnchored) {
  const auto scene = [](float spacer) {
    material::skia::Paint light =
        material::skia::Paint::radial(
            {100, 100}, 70, {{0.0f, {1, 1, 1, 1}}, {1.0f, {0.1f, 0.05f, 0, 1}}})
            .worldSpace();
    // The corner child makes the panel RECORD (a childless leaf paints
    // live and would re-resolve on every reach, hiding the stale-W hole
    // this pin exists to close).
    return box()
        .row()
        .child(box().width(spacer).height(10))
        .child(box().width(120).height(200).key("panel").fill(light).child(
            box().absolute().left(0).top(0).width(4).height(4).fill(green())));
  };
  Host host;
  host.composer.render(scene(20));
  host.frame();
  const SkPoint before = brightestPixel(host);
  EXPECT_LT(SkPoint::Distance(before, {100, 100}), 3.0f);
  host.composer.render(scene(60));  // the panel slides right, pruning
  host.frame();
  const SkPoint after = brightestPixel(host);
  EXPECT_LT(SkPoint::Distance(before, after), 3.0f)
      << "the field moved with the layout — the recording kept its old W";
}

// An ANCESTOR's layout move must re-anchor a descendant whose own
// parent-relative rect never changed. The descendant has no local evidence
// that it moved at all, so "an ancestor above you moved" has to be threaded
// down the walk.
TEST(ComposeWorldSpace, AnAncestorsMoveReanchorsTheDescendant) {
  const auto scene = [](float spacerH) {
    material::skia::Paint light =
        material::skia::Paint::radial(
            {100, 100}, 70, {{0.0f, {1, 1, 1, 1}}, {1.0f, {0.1f, 0.05f, 0, 1}}})
            .worldSpace();
    // column: spacer, then a group whose panel child is absolutely inset —
    // the group MOVES, the panel's rect relative to the group does not.
    return box()
        .child(box().width(10).height(spacerH))
        .child(box().width(200).height(140).key("group").child(
            box().absolute().inset(10).key("panel").fill(light)));
  };
  Host host;
  host.composer.render(scene(20));
  host.frame();
  const SkPoint before = brightestPixel(host);
  EXPECT_LT(SkPoint::Distance(before, {100, 100}), 3.0f);
  host.composer.render(scene(50));  // the whole group slides down
  host.frame();
  EXPECT_LT(SkPoint::Distance(before, brightestPixel(host)), 3.0f)
      << "the ancestor moved and the descendant kept its old anchoring";
}

// A BOUND transform drives the ancestor and the field stays anchored on
// every frame. Three stages are exercised in order: the volatility lift
// while the motion runs, the release once it settles, and the per-draw scan
// on the frame it resumes. Without the lift the parent's recording replays
// the old anchoring under the live rotation, which is a wrong picture rather
// than a stale one.
TEST(ComposeWorldSpace, ABoundTransformKeepsTheFieldAnchoredPerFrame) {
  ch::Output<float> rot{0};
  Host host;
  const auto describe = [&] {
    auto group = box()
                     .rect(SkRect::MakeXYWH(20, 20, 160, 160))
                     .key("group")
                     .rotate(&rot)
                     .transformOrigin(0.5f, 0.5f);
    group.child(box()
                    .absolute()
                    .left(20)
                    .top(20)
                    .width(120)
                    .height(120)
                    .key("panel")
                    .fill(canvasLight(true, {40, 40})));
    host.composer.render(box().child(std::move(group)));
  };
  describe();
  host.frame();
  const SkPoint anchored = brightestPixel(host);
  EXPECT_LT(SkPoint::Distance(anchored, {70, 70}), 3.0f);
  // Drive the rotation externally — no re-describe, no ticker motion.
  for (float angle : {10.0f, 25.0f, 40.0f}) {
    rot = angle;
    host.frame(1.0 / 60.0);
    EXPECT_LT(SkPoint::Distance(anchored, brightestPixel(host)), 3.0f)
        << "at bound rotation " << angle << " the field turned with the object";
  }
  // Hold still long enough for the released-scalar path to take over…
  for (int i = 0; i < 12; ++i) host.frame(1.0 / 60.0);
  EXPECT_LT(SkPoint::Distance(anchored, brightestPixel(host)), 3.0f);
  // …then RESUME: the released scan gains the node's W, so the very next
  // frame re-anchors — nothing stale replays.
  rot = 65.0f;
  host.frame(1.0 / 60.0);
  EXPECT_LT(SkPoint::Distance(anchored, brightestPixel(host)), 3.0f)
      << "the frame the released rotation resumed served stale anchoring";
}

// The worldSpace flag is part of the RECIPE: an identical re-describe with
// it set prunes, and flipping it patches. Leave it out of Material equality
// and a node that stops being world-space keeps the old anchoring forever.
TEST(ComposeWorldSpace, TheFlagRidesThePruneSignature) {
  const std::vector<material::skia::Stop> stops{{0.0f, {1, 0, 0, 1}},
                                                {1.0f, {0, 0, 1, 1}}};
  EXPECT_TRUE(
      material::skia::Paint::linear({0, 0}, {200, 0}, stops).worldSpace() ==
      material::skia::Paint::linear({0, 0}, {200, 0}, stops).worldSpace());
  EXPECT_FALSE(
      material::skia::Paint::linear({0, 0}, {200, 0}, stops).worldSpace() ==
      material::skia::Paint::linear({0, 0}, {200, 0}, stops));
  const auto scene = [&](bool flagged) {
    material::skia::Paint m =
        material::skia::Paint::linear({0, 0}, {200, 0}, stops);
    if (flagged) m.worldSpace();
    return box().child(box().width(100).height(100).key("panel").fill(m));
  };
  Host host;
  host.composer.render(scene(true));
  host.frame();
  host.composer.render(scene(true));  // identical → prunes
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "an identical world-space fill failed to prune";
  host.composer.render(scene(false));  // the flip must patch
  EXPECT_GT(host.composer.stats().patchedNodes, 0u)
      << "a flag flip pruned — the node would keep the old anchoring";
}

// The node's world matrix joins the resolve digest. A LIVE world-space
// material whose bound inputs hold still while the NODE moves must still
// rebuild its shader — a digest cannot notice an input it was never fed, so
// omitting the matrix serves the pre-move shader on the frame after a move.
//
// The move here is a setSize() RELAYOUT against a grow() spacer, with no
// re-describe, so the same live recipe and its memo survive it. Re-describing
// the material would mint a fresh memo and hide the hole entirely.
TEST(ComposeWorldSpace, TheResolveDigestSeesTheNodeMove) {
  auto [fx, err] = SkRuntimeEffect::MakeForShader(
      SkString("uniform float uDrive;"
               "half4 main(float2 p) {"
               "  return p.x < 130.0 ? half4(1, 0, 0, 1) + half4(uDrive)*0.0"
               "                     : half4(0, 0, 1, 1);"
               "}"));
  ASSERT_TRUE(fx) << err.c_str();
  ch::Output<float> drive{0};  // bound and HELD — the digest's other input
  material::skia::Paint m = material::skia::Paint::sksl(fx);
  m.uniform("uDrive", &drive);
  m.worldSpace();
  Host host;
  host.composer.render(
      box()
          .row()
          .child(box().grow(1).height(10))
          .child(box().width(120).height(200).key("panel").fill(m)));
  host.frame();
  // Canvas 200 wide: the spacer grows to 80, the panel spans [80, 200] —
  // the red→blue boundary sits at CANVAS x=130 (world coordinates).
  EXPECT_GT(SkColorGetR(host.pixel(124, 100)), 200u);
  EXPECT_GT(SkColorGetB(host.pixel(136, 100)), 200u);
  // Shrink the canvas: the spacer shrinks to 40, the panel slides to
  // [40, 160]. NOTHING was re-described and the binds never moved — W is
  // the only input that changed.
  host.composer.setSize({160, 200});
  host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(124, 100)), 200u)
      << "the boundary rode the node — the digest served a stale shader";
  EXPECT_GT(SkColorGetB(host.pixel(136, 100)), 200u)
      << "the boundary rode the node — the digest served a stale shader";
}
