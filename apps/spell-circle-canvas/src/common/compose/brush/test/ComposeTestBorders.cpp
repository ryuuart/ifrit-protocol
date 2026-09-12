// The border rules a silhouette wears -- brackets, gapped rules, weighted
// corners and stacked insets -- with the node words beside them: the echo
// stamps, the entrance staggers, the release on removal and the mask window
// an open contour wraps.

#include <utility>

#include "support/BrushTestSupport.h"

namespace {
Fill white() { return Fill::color({1, 1, 1, 1}); }

/** A 100x100 blue panel with `dec` as its foreground, in a 200x200 host.
 *  The panel sits at (0,0), so pixel(50, 1) is the middle of its top edge
 *  and pixel(10, 1) is 10 px along from its top-left corner. */
Element panel(Decoration dec) {
  return box().child(
      box().width(100).height(100).fill(blue()).foreground(std::move(dec)));
}

/** The same panel with a span-qualified stroke pass, which is how the
 *  corner vocabulary is spelled. */
Element spanPanel(Spans where, Decoration dec) {
  return box().child(box().width(100).height(100).fill(blue()).stroke(
      std::move(where), std::move(dec)));
}

Element shapedSpanPanel(std::function<SkPath(SkSize)> outline, Spans where,
                        Decoration dec) {
  return box().child(box()
                         .width(100)
                         .height(100)
                         .shape(std::move(outline))
                         .fill(blue())
                         .stroke(std::move(where), std::move(dec)));
}

}  // namespace

TEST(ComposeBorders, BracketsPaintOnlyNearTheCorners) {
  // Four L-shaped marks and nothing else — one node, one pass. The
  // alternative an author reaches for is four absolutely-placed Elements,
  // which costs four nodes and stops tracking the silhouette the moment it
  // is not a rectangle.
  Host host;
  host.composer.render(spanPanel(spans::corners(20), brush::solid(6, white())));
  host.frame();
  EXPECT_EQ(host.pixel(10, 1), SK_ColorWHITE);   // along the top, near a corner
  EXPECT_EQ(host.pixel(1, 10), SK_ColorWHITE);   // and down the left of it
  EXPECT_EQ(host.pixel(90, 98), SK_ColorWHITE);  // bottom-right bracket
  EXPECT_EQ(host.pixel(50, 1), SK_ColorBLUE);    // mid-run: bare
  EXPECT_EQ(host.pixel(98, 50), SK_ColorBLUE);
}

TEST(ComposeBorders, GappedRuleIsTheExactComplement) {
  Host host;
  host.composer.render(spanPanel(spans::edges(20), brush::solid(6, white())));
  host.frame();
  EXPECT_EQ(host.pixel(50, 1), SK_ColorWHITE);  // mid-run: drawn
  EXPECT_EQ(host.pixel(98, 50), SK_ColorWHITE);
  EXPECT_EQ(host.pixel(10, 1), SK_ColorBLUE);  // corner: left open
  EXPECT_EQ(host.pixel(1, 10), SK_ColorBLUE);
}

TEST(ComposeBorders, BracketsFollowAChamferedSilhouette) {
  // The property four hand-placed corner Elements can never have: the
  // brackets land on whatever corners the SHAPE has. A 30 px chamfer turns
  // four corners into eight, and the chamfer face itself gets marked.
  Host host;
  host.composer.render(shapedSpanPanel(geometry::shapes::chamfered(30),
                                       spans::corners(10),
                                       brush::solid(6, white())));
  host.frame();
  // (74, 4) sits ON the top-right chamfer face, ~5 px from its upper end.
  EXPECT_EQ(host.pixel(74, 4), SK_ColorWHITE);
  // The chamfered top edge runs (30,0)->(70,0); its midpoint is 20 px from
  // either end corner, which is outside a 10 px arm: bare.
  EXPECT_EQ(host.pixel(50, 1), SK_ColorBLUE);
  // A plain rectangle has no corner there at all, so the same call leaves
  // that pixel untouched — which is what makes this a test about the SHAPE.
  Host plain;
  plain.composer.render(
      spanPanel(spans::corners(10), brush::solid(6, white())));
  plain.frame();
  EXPECT_EQ(plain.pixel(74, 4), SK_ColorBLUE);
}

TEST(ComposeBorders, ARoundedCornerIsNotACorner) {
  // The surprise worth pinning: corner spans are found at hard tangent
  // breaks, and a rounded rect has none. So a bracket set on one paints
  // NOTHING at all, and a gapped rule runs the whole way round without a
  // single gap. Neither reports an error.
  Host host;
  host.composer.render(box().child(
      box().width(100).height(100).corners({30}).fill(blue()).stroke(
          spans::corners(20), brush::solid(6, white()))));
  host.frame();
  EXPECT_EQ(host.pixel(50, 1), SK_ColorBLUE);
  EXPECT_EQ(host.pixel(1, 50), SK_ColorBLUE);
  EXPECT_EQ(host.pixel(9, 9), SK_ColorBLUE);  // on the corner arc itself

  Host gapped;
  gapped.composer.render(box().child(
      box().width(100).height(100).corners({30}).fill(blue()).stroke(
          spans::edges(20), brush::solid(6, white()))));
  gapped.frame();
  EXPECT_EQ(gapped.pixel(50, 1), SK_ColorWHITE);
  EXPECT_EQ(gapped.pixel(9, 9), SK_ColorWHITE);
}

TEST(ComposeBorders, InsetMovesTheRuleInsideTheSilhouette) {
  Host host;
  host.composer.render(panel(decorations::border(4, white(), 10)));
  host.frame();
  EXPECT_EQ(host.pixel(50, 10), SK_ColorWHITE);  // the rule, 10 px in
  EXPECT_EQ(host.pixel(50, 1), SK_ColorBLUE);    // the edge itself is bare
  EXPECT_EQ(host.pixel(10, 50), SK_ColorWHITE);
}

TEST(ComposeBorders, WeightedCornersThickenWhereTheRuleTurns) {
  // A border whose weight varies along its run: 2 px along the edges,
  // 8 px within 20 px of each corner.
  Host host;
  host.composer.render(panel(decorations::weightedCorners(2, 8, white(), 20)));
  host.frame();
  EXPECT_EQ(host.pixel(10, 3), SK_ColorWHITE);  // inside the heavy corner run
  EXPECT_EQ(host.pixel(50, 3), SK_ColorBLUE);   // the light run is only 2 px
  EXPECT_EQ(host.pixel(50, 0), SK_ColorWHITE);  // …but it IS drawn
}

TEST(ComposeBorders, DoubleBorderStacksTwoIndependentInsets) {
  Host host;
  host.composer.render(
      box().child(box().width(100).height(100).fill(blue()).style(
          decorations::doubleBorder(decorations::border(3, white()),
                                    decorations::border(3, white(), 12)))));
  host.frame();
  EXPECT_EQ(host.pixel(50, 0), SK_ColorWHITE);   // outer rule
  EXPECT_EQ(host.pixel(50, 12), SK_ColorWHITE);  // inner rule
  EXPECT_EQ(host.pixel(50, 6), SK_ColorBLUE);    // clear paper between them
}

// ---------------------------------------------------------------------------
// Echo misprints, stagger origin, quantized time.
TEST(ComposePaint, EchoStampsShapeUnderTheFill) {
  Host host;
  host.composer.render(box().child(box()
                                       .absolute()
                                       .inset(50, 50, 90, 90)
                                       .fill(red())
                                       .echo({10, 10}, {0, 1, 0, 1})));
  host.frame();
  EXPECT_EQ(host.pixel(80, 80), SK_ColorRED);      // real fill on top
  EXPECT_EQ(host.pixel(115, 115), SK_ColorGREEN);  // echo peeking past it
  EXPECT_EQ(host.pixel(45, 45), SK_ColorBLACK);    // nothing before either
  host.frame();  // survives the cached replay (cull grew by the offset)
  EXPECT_EQ(host.pixel(115, 115), SK_ColorGREEN);
}

TEST(ComposePaint, EchoesAppendSoRegistrationDoublingIsTwoCalls) {
  // echo() APPENDS — the node holds a vector of stamps, not one — so
  // registration doubling (a stamp each side of a glyph run rather than one
  // behind it) is two calls, not a missing feature. This pins that, and the
  // ordering: stamps paint in declaration order beneath the real pass.
  Host host;
  host.composer.render(box().child(box()
                                       .absolute()
                                       .inset(60, 60, 100, 100)
                                       .fill(red())
                                       .echo({-14, -14}, {0, 0, 1, 1})
                                       .echo({14, 14}, {0, 1, 0, 1})
                                       .echo({20, 20}, {1, 1, 0, 1})));
  host.frame();
  EXPECT_EQ(host.pixel(90, 90), SK_ColorRED);     // the real pass, on top
  EXPECT_EQ(host.pixel(50, 50), SK_ColorBLUE);    // one stamp up-left…
  EXPECT_EQ(host.pixel(76, 110), SK_ColorGREEN);  // …one down-right
  EXPECT_EQ(host.pixel(118, 118), SK_ColorYELLOW);
  // Declaration order is bottom-first, so the LAST echo wins where they
  // overlap — three stamps, not one, and they are ordered.
  EXPECT_EQ(host.pixel(110, 110), SK_ColorYELLOW);
}

TEST(ComposeText, EchoStampsTextUnderThePass) {
  Host host(300, 120);
  host.composer.render(box().padding(20).child(
      text(u8"ECHO", whiteStyle(48)).echo({6, -8}, {1, 0, 0, 1})));
  host.frame();
  int redCount = 0, whiteCount = 0;
  for (int y = 0; y < 120; y += 2)
    for (int x = 0; x < 300; x += 2) {
      redCount += host.pixel(x, y) == SK_ColorRED;
      whiteCount += host.pixel(x, y) == SK_ColorWHITE;
    }
  EXPECT_GT(whiteCount, 50);  // the real pass
  EXPECT_GT(redCount, 20);    // the misprint peeking out at (6,−8)
}

TEST(ComposeMotion, StaggerFromEndRunsBottomUp) {
  Host host;
  auto card = [] {
    return box().width(60).height(30).fill(red()).opacity(
        animate(motion::from(0.0f).to(1.0f), {200ms, &choreograph::easeNone}));
  };
  host.composer.render(box()
                           .column()
                           .gap(10)
                           .staggerChildren(400ms, motion::Spread::From::End)
                           .child(card())
                           .child(card()));
  host.frame(0.3);  // LAST child leads; first still holds its `from`
  EXPECT_EQ(host.pixel(30, 15), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(30, 55), SK_ColorRED);
  host.frame(0.5);
  EXPECT_EQ(host.pixel(30, 15), SK_ColorRED);
}

TEST(ComposeMotion, KeyframesPlayTheMountPath) {
  // A cursor overshoot: +40 → −20 → 0. Mid-path the box sits LEFT of rest,
  // which is the point — a single from→to ramp can never cross its own
  // resting value, so this shape only exists if waypoints really play.
  Host host;
  host.composer.render(box().child(
      box()
          .absolute()
          .inset(100, 80, 60, 80)
          .fill(red())
          .translateX(animate(
              sigil::motion::through({{std::chrono::milliseconds(0), 40.0f},
                                      {std::chrono::milliseconds(200), -20.0f},
                                      {std::chrono::milliseconds(400), 0.0f}}),
              &choreograph::easeNone))));
  host.frame();
  EXPECT_EQ(host.pixel(145, 100), SK_ColorRED);  // starts at +40
  EXPECT_EQ(host.pixel(105, 100), SK_ColorBLACK);
  host.frame(0.2);  // waypoint 2: x = −20
  EXPECT_EQ(host.pixel(85, 100), SK_ColorRED);
  EXPECT_EQ(host.pixel(145, 100), SK_ColorBLACK);
  host.frame(0.3);  // settled at 0
  EXPECT_EQ(host.pixel(105, 100), SK_ColorRED);
  EXPECT_EQ(host.pixel(85, 100), SK_ColorBLACK);
  // Identical re-describe prunes (waypoints compare structurally).
  host.composer.render(box().child(
      box()
          .absolute()
          .inset(100, 80, 60, 80)
          .fill(red())
          .translateX(animate(
              sigil::motion::through({{std::chrono::milliseconds(0), 40.0f},
                                      {std::chrono::milliseconds(200), -20.0f},
                                      {std::chrono::milliseconds(400), 0.0f}}),
              &choreograph::easeNone))));
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
}

// ---------------------------------------------------------------------------
// Reconcile and motion cases where the failure is a value that LINGERS from
// the previous describe rather than one that is computed wrongly.
TEST(ComposeReconcile, RemovedDimsAndInsetsRelease) {
  // Patch reuses the yoga node: dims/aspect/insets REMOVED from the
  // description must actually unset, not linger from the last describe.
  Host host;
  host.composer.render(
      box().row().child(box().width(120).height(40).fill(red()).key("b")));
  host.frame();
  ASSERT_EQ(require(host.composer.bounds("b")).width(), 120);
  host.composer.render(box().row().child(
      box().height(40).fill(red()).key("b").child(box().width(30))));
  host.frame();
  EXPECT_EQ(require(host.composer.bounds("b")).width(),
            30);  // released to content

  Host pins;
  pins.composer.render(
      box().child(box().inset(10, 10, 10, 10).fill(blue()).key("p")));
  pins.frame();
  ASSERT_EQ(require(pins.composer.bounds("p")).width(), 180);
  pins.composer.render(box().child(box()
                                       .left(Dimension(20.0f))
                                       .top(Dimension(20.0f))
                                       .width(50)
                                       .height(20)
                                       .fill(blue())
                                       .key("p")));
  pins.frame();
  EXPECT_EQ(require(pins.composer.bounds("p")),
            SkRect::MakeXYWH(20, 20, 50, 20));
}

TEST(ComposeMotion, UnrelatedPatchDoesNotRestartAnEntrance) {
  // Mid-entrance, changing an UNRELATED prop must leave the running motion
  // alone. If a patch rebuilds the ramp, it also re-serves the transition's
  // delay from wherever the value happens to be, so the entrance stalls at
  // the fraction it had reached and then starts over — visible only as
  // slightly-wrong timing, never as an error.
  Host host;
  auto tree = [](Fill f) {
    return box().child(box()
                           .width(80)
                           .height(80)
                           .fill(std::move(f))
                           .opacity(animate(motion::from(0.0f).to(1.0f),
                                            {std::chrono::milliseconds(400),
                                             &choreograph::easeNone,
                                             std::chrono::milliseconds(300)})));
  };
  host.composer.render(tree(red()));
  host.frame(0.35);  // 50ms into the ramp (after the 300ms hold)
  host.composer.render(tree(blue()));  // fill changes; opacity prop identical
  host.frame(0.2);  // t = 0.55 → ramp fraction (0.55-0.3)/0.4 = 0.625
  const SkColor c = host.pixel(40, 40);
  // Continuing motion: strong blue. A restarted+re-held ramp would still
  // sit at the 0.125 it had when the patch landed.
  EXPECT_GT(SkColorGetB(c), 120u);
}

// ---------------------------------------------------------------------------
// Cases where the wrong answer looks plausible: a stale motion that lands on
// a believable value, and a mask that invents geometry.
TEST(ComposeMotion, ToggleBackDuringDelayHoldLands) {
  // Retargeting a slot to its CURRENT value must DISCONNECT a motion headed
  // elsewhere, not merely leave it unstarted. If it does not, the delay hold
  // expires later and the value fades to the target nobody asked for any
  // more — arbitrarily long after the describe that cancelled it.
  Host host;
  auto tree = [](float op) {
    return box().child(
        box()
            .width(80)
            .height(80)
            .fill(red())
            .transition({std::chrono::milliseconds(200), &choreograph::easeNone,
                         std::chrono::milliseconds(300)})
            .opacity(op));
  };
  host.composer.render(tree(1.0f));
  host.frame();
  host.composer.render(tree(0.0f));  // starts Hold(1, .3s) + RampTo(0)
  host.frame(0.1);                   // still holding at 1
  host.composer.render(tree(1.0f));  // toggle back DURING the hold
  host.frame(0.7);                   // any stale motion would have finished
  EXPECT_EQ(host.pixel(40, 40), SK_ColorRED);  // description wins: opaque
}

namespace {

/** The two doors a wrapped window can be spelled through: the node gate
 *  and PathFormat's own trim. Both must obey the open-contour rule, and
 *  neither can be trusted to because the other one does. */
enum class WrapDoor { NodeGate, PathFormatTrim };

class OpenContourWrap : public testing::TestWithParam<WrapDoor> {};

}  // namespace

TEST_P(OpenContourWrap, AWrappedWindowOnAnOpenContourStaysTwoPieces) {
  // A wrapped window on an OPEN contour is genuinely two pieces: its head
  // and its tail are at opposite ends of the path and are not adjacent.
  // Joining them into one run draws a chord across the middle that exists
  // in no path the author supplied.
  Host host;
  Element run = box().absolute().inset(20, 80, 20, 80).shape([](SkSize s) {
    SkPathBuilder b;  // an open horizontal line
    b.moveTo(0, s.height() / 2);
    b.lineTo(s.width(), s.height() / 2);
    return b.detach();
  });
  if (GetParam() == WrapDoor::NodeGate) {
    run.mask(by::spans(spans::wrap(0.9f, 1.2f))).stroke(stroke(6, green()));
  } else {
    PathFormat format;
    format.width = 6;
    format.strokeFill = green();
    format.trimStart = 0.9f;
    format.trimEnd = 1.2f;
    run.stroke(format);
  }
  host.composer.render(box().child(std::move(run)));
  host.frame();
  EXPECT_EQ(host.pixel(170, 100), SK_ColorGREEN);  // tail piece [0.9, 1]
  EXPECT_EQ(host.pixel(40, 100), SK_ColorGREEN);   // head piece [0, 0.2]
  EXPECT_EQ(host.pixel(100, 100), SK_ColorBLACK);  // NO chord between them
}

INSTANTIATE_TEST_SUITE_P(ComposeMask, OpenContourWrap,
                         testing::Values(WrapDoor::NodeGate,
                                         WrapDoor::PathFormatTrim),
                         [](const testing::TestParamInfo<WrapDoor>& info) {
                           return info.param == WrapDoor::NodeGate
                                      ? "ThroughTheNodeGate"
                                      : "ThroughPathFormatsTrim";
                         });

TEST(ComposeMask, ClosedContourWrapSeamIsOnePiece) {
  // The twin of the test above: on a CLOSED contour the two halves of a
  // wrapped window ARE adjacent, so spanPath must stitch them into ONE run.
  //
  // Read the RENDER, not a path the test stitched for itself. A test that
  // rebuilds the window with SkContourMeasure and asserts on the result
  // proves something about its own arithmetic and nothing about what the
  // renderer drew.
  //
  // The seam is parked on a corner, which is what makes the difference
  // visible in pixels at all: one run puts a MITER JOIN there and the outer
  // corner square is covered; two runs put two butt caps there and that
  // square is empty — the visible notch.
  Host host;
  host.composer.render(box().child(
      box()
          .absolute()
          .inset(20, 20, 20, 20)
          .shape([](SkSize s) {  // closed rect, seam at its top-left corner
            SkPathBuilder b;
            b.moveTo(0, 0);
            b.lineTo(s.width(), 0);
            b.lineTo(s.width(), s.height());
            b.lineTo(0, s.height());
            b.close();
            return b.detach();
          })
          // perimeter 640: [0, 0.2] runs 128 px right along the top edge,
          // [0.9, 1] runs the last 64 px UP the left edge into the seam.
          .mask(by::spans(spans::wrap(0.9f, 1.2f)))
          .stroke(stroke(6, green()))));
  host.frame();
  EXPECT_EQ(host.pixel(80, 20), SK_ColorGREEN);    // head piece, top edge
  EXPECT_EQ(host.pixel(20, 50), SK_ColorGREEN);    // tail piece, left edge
  EXPECT_EQ(host.pixel(180, 100), SK_ColorBLACK);  // the mask still masks
  EXPECT_EQ(host.pixel(18, 18), SK_ColorGREEN);  // THE SEAM: joined, not capped
}

TEST(ComposePaint, BackdropLeavesDecorationsUnclipped) {
  // A backdrop filter reads and re-draws the region behind the node, which
  // needs a clip — but that clip must not reach the node's own decorations.
  // An outer-aligned stroke lies OUTSIDE the node's shape, so clipping it
  // away is silent and total.
  Host host;
  host.composer.render(
      box().child(box()
                      .absolute()
                      .inset(60, 60, 60, 60)
                      .backdrop(material::skia::Effect::filter(
                          SkImageFilters::Blur(2, 2, nullptr)))
                      .stroke(stroke(10, green(), PathFormat::Align::Outer))));
  host.frame();
  EXPECT_EQ(host.pixel(52, 100), SK_ColorGREEN);  // outer stroke intact
}

TEST(ComposeMotion, AppendedItemEntersWithoutInheritedDelay) {
  // A stagger delay is an ordinal times a step, so an item appended to a
  // list whose cascade already finished would sit invisible for its full
  // ordinal delay before entering. Only newly mounted children take a
  // stagger, and their delay is counted from the mount, not from the list.
  Host host;
  auto card = [](std::string_view key) {
    return box().width(60).height(20).fill(red()).key(key).opacity(
        animate(motion::from(0.0f).to(1.0f),
                {std::chrono::milliseconds(100), &choreograph::easeNone}));
  };
  host.composer.render(box()
                           .column()
                           .gap(10)
                           .staggerChildren(std::chrono::milliseconds(400))
                           .child(card("a"))
                           .child(card("b")));
  host.frame(1.2);  // initial cascade done
  host.composer.render(box()
                           .column()
                           .gap(10)
                           .staggerChildren(std::chrono::milliseconds(400))
                           .child(card("a"))
                           .child(card("b"))
                           .child(card("c")));  // appended: only new mount
  host.frame(0.15);  // > its 100ms entrance, << 2·400ms ordinal delay
  EXPECT_EQ(host.pixel(30, 70), SK_ColorRED);  // "c" already in
}
