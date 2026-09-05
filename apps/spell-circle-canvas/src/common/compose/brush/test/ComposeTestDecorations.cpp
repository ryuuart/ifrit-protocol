// Decorations on an ordinary node: borders and dashes along an outline,
// shadows under a fill, strokes over one, the mask a decoration is gated
// by, and rail(), the component that IS a line.

#include <functional>

#include "support/BrushTestSupport.h"

TEST(ComposeDecorations, DashedBorderPaintsAlongOutline) {
  Host host;
  PathFormat dashed;
  dashed.width = 6;
  dashed.strokeFill = Fill::color({1, 1, 0, 1});
  dashed.dashIntervals = {10, 10};
  host.composer.render(box().child(
      box().width(120).height(120).inset(20).absolute().foreground(dashed)));
  host.frame();
  // Somewhere along the top edge a dash lands; somewhere it doesn't.
  int lit = 0;
  for (int x = 25; x < 135; ++x)
    if (host.pixel(x, 20) == SK_ColorYELLOW) ++lit;
  EXPECT_GT(lit, 10);
  EXPECT_LT(lit, 110);  // gaps exist → it really dashed
}

TEST(ComposeDecorations, ContourWalkVisitsSamplesPositioned) {
  Host host;
  static int visits;
  visits = 0;
  ContourWalk walk;
  walk.spacing = 25.0f;
  walk.draw = [](SkCanvas& c, const PathSample& s, const PaintContext&) {
    ++visits;
    EXPECT_GE(s.fraction, 0.0f);
    EXPECT_LE(s.fraction, 1.0f);
    SkPaint p;
    p.setColor(SK_ColorGREEN);
    c.drawRect(SkRect::MakeXYWH(-2, -2, 4, 4), p);  // at the sample origin
  };
  host.composer.render(box().child(box()
                                       .width(100)
                                       .height(100)
                                       .inset(50, 50, 50, 50)
                                       .absolute()
                                       .foreground(walk)));
  host.frame();
  EXPECT_EQ(visits, 16);  // 400px perimeter / 25px spacing
  EXPECT_EQ(host.pixel(100, 50), SK_ColorGREEN);  // top edge stamped
  host.frame();
  EXPECT_EQ(visits, 16);  // static walk → recorded once, replayed
}

TEST(ComposeDecorations, AnimatedWalkDeclaresVolatility) {
  Host host;
  static int visits;
  visits = 0;
  ContourWalk walk;
  walk.spacing = 50.0f;
  walk.animatedWalk = true;
  walk.draw = [](SkCanvas&, const PathSample&, const PaintContext&) {
    ++visits;
  };
  host.composer.render(
      box().child(box().width(100).height(100).foreground(walk)));
  host.frame();
  host.frame();
  EXPECT_EQ(visits, 16);  // 8 samples × 2 frames: repainted per frame
}

TEST(ComposeDecorations, ContourWalkStampAtSequencesPerSampleArt) {
  // `stamp` replays ONE piece of art at every sample. stampAt(sample, index)
  // is the sequence form — ruler ticks carrying numbers, chained ornament —
  // returning per-index art, with nullopt falling back to the shared
  // `stamp`.
  //
  // Two costs come with it, both inherent. ContourWalk carries a raw
  // callable, so it has no operator== and its node never prunes. And the art
  // is re-baked per call: each returned Element is a fresh node, so the
  // instance-side stamp cache has no stable key to hold it by.
  Host host;
  static int asked;
  asked = 0;
  ContourWalk walk;
  walk.spacing = 40.0f;
  walk.stamp = box().width(10).height(10).fill(green());
  walk.stampAt = [](const PathSample& s, size_t i) -> std::optional<Element> {
    ++asked;
    EXPECT_FLOAT_EQ(s.distance, 40.0f * (float)i);  // the sequence contract
    if (i % 2 == 1)
      return std::nullopt;  // odd samples: the shared stamp replays
    return box().width(10).height(10).fill(red());
  };
  host.composer.render(box().child(box()
                                       .absolute()
                                       .inset(20, 80, 20, 80)
                                       .shape([](SkSize s) {
                                         SkPathBuilder b;
                                         b.moveTo(0, s.height() / 2);
                                         b.lineTo(s.width(), s.height() / 2);
                                         return b.detach();
                                       })
                                       .foreground(walk)));
  host.frame();
  // 160px rail, spacing 40 → samples at x = 20, 60, 100, 140 (y = 100).
  EXPECT_EQ(host.pixel(20, 100), SK_ColorRED);     // index 0: its own art
  EXPECT_EQ(host.pixel(60, 100), SK_ColorGREEN);   // index 1: fallback
  EXPECT_EQ(host.pixel(100, 100), SK_ColorRED);    // index 2
  EXPECT_EQ(host.pixel(140, 100), SK_ColorGREEN);  // index 3
  EXPECT_EQ(asked, 4);
  host.frame();  // a static walk records once and replays — no re-bakes
  EXPECT_EQ(asked, 4);
}

TEST(ComposeDecorations, SliceStretchesCenterKeepsCorners) {
  // Synthesize a 30x30 nine-patch: 10px red border ring, green center.
  SkBitmap src;
  src.allocN32Pixels(30, 30);
  src.eraseColor(SK_ColorRED);
  src.erase(SK_ColorGREEN, SkIRect::MakeXYWH(10, 10, 10, 10));
  auto asset = std::make_shared<sigil::image::ImageAsset>(
      sigil::image::ImageAsset::wrap(src.asImage()));

  Host host;
  Slice nine;
  nine.asset = asset;
  nine.xDivs = {10, 20};
  nine.yDivs = {10, 20};
  host.composer.render(
      box().child(box().width(120).height(120).background(nine)));
  host.frame();
  EXPECT_EQ(host.pixel(60, 60), SK_ColorGREEN);  // stretched center
  EXPECT_EQ(host.pixel(4, 4), SK_ColorRED);      // corner intact
  EXPECT_EQ(host.pixel(115, 115), SK_ColorRED);
  EXPECT_EQ(host.pixel(60, 4), SK_ColorRED);  // edge strip
}

TEST(ComposeDecorations, ShadowSitsUnderTheFillAndAStrokeSitsOverIt) {
  Host host;
  host.composer.render(box().child(
      box()
          .width(80)
          .height(80)
          .inset(40, 40, 40, 40)
          .absolute()
          .corners({10})
          .background(sigil::compose::shadow({0, 0, 1, 1}, {12, 12}, 0))
          .fill(red())
          .foreground(sigil::compose::stroke(4, green()))));
  host.frame();
  EXPECT_EQ(host.pixel(80, 80), SK_ColorRED);     // fill over shadow
  EXPECT_EQ(host.pixel(128, 128), SK_ColorBLUE);  // shadow offset corner
  EXPECT_EQ(host.pixel(80, 40), SK_ColorGREEN);   // stroked top edge
}

TEST(ComposeReconcile, StructuralPruneCoversDecorations) {
  // Value decorations (Shadow, PathFormat stroke/dash) let a static decorated
  // node prune without memo — the P0 chrome fix. Before it, any decoration
  // forced a re-patch + re-record on every render().
  Host host;
  auto tree = [] {
    PathFormat dash;
    dash.width = 1;
    dash.strokeFill = blue();
    dash.dashIntervals = {4, 3};
    return box()
        .row()
        .gap(8)
        .padding(12)
        .child(
            box()
                .width(40)
                .height(40)
                .corners({6})
                .fill(red())
                .background(sigil::compose::shadow({0, 0, 0, 0.5f}, {2, 2}, 4))
                .foreground(sigil::compose::stroke(2, green())))
        .child(box().width(60).height(20).foreground(dash));
  };
  host.composer.render(tree());
  host.frame();

  host.composer.render(tree());  // identical, brand-new Elements + decorations
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  EXPECT_FALSE(host.composer.dirty());  // hosts may skip the redraw entirely
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
}

TEST(ComposeShapes, InsetRunsADecorationAgainstAShrunkOutline) {
  // "The same bevel again, six pixels in" is the whole vocabulary of
  // nested chrome. A stroke run through inset(12, ...) must land INSIDE
  // the box, not on its edge.
  Host host(120, 120);
  host.composer.render(box().child(
      box().width(120).height(120).absolute().left(0).top(0).foreground(
          inset(12.0f, stroke(4.0f, Fill::color({1, 0, 0, 1}))))));
  host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(60, 12)), 150u);  // the inset rule
  EXPECT_LT(SkColorGetR(host.pixel(60, 1)), 60u);    // the edge is bare
  EXPECT_LT(SkColorGetR(host.pixel(60, 60)), 60u);   // and so is the middle
}

namespace {

/** A 20×20 keyed station box; center lands at (left+10, top+10). */
Element station(const char* key, float left, float top) {
  return box()
      .key(key)
      .width(20)
      .height(20)
      .inset(left, top, 180 - left, 160 - top)
      .absolute()
      .fill(blue());
}

PathFormat railLine() {
  PathFormat line;
  line.width = 4;
  line.strokeFill = green();
  return line;
}

}  // namespace

TEST(ComposeRail, ThreadsThroughAnchors) {
  // Three stations, one rail through their centers: the routed polyline is
  // the element; the PathFormat foreground dresses it.
  Host host;
  host.composer.render(stack()
                           .child(station("s1", 10, 40))
                           .child(station("s2", 90, 40))
                           .child(station("s3", 170, 40))
                           .child(rail({{"s1"}, {"s2"}, {"s3"}})
                                      .absolute()
                                      .inset(0)
                                      .foreground(railLine())));
  host.frame();
  EXPECT_EQ(host.pixel(60, 50), SK_ColorGREEN);   // between s1 and s2
  EXPECT_EQ(host.pixel(140, 50), SK_ColorGREEN);  // between s2 and s3
  EXPECT_EQ(host.pixel(60, 80), SK_ColorBLACK);   // off the rail
}

TEST(ComposeRail, DrawsOnWithTrim) {
  // Composition, not new machinery: a span gate on a rail = the self-drawing
  // subway line. A bound reveal advances with no render() calls.
  choreograph::Output<float> reveal{0.05f};
  Host host;
  host.composer.render(stack()
                           .child(station("a", 10, 40))
                           .child(station("b", 170, 40))
                           .child(rail({{"a"}, {"b"}})
                                      .absolute()
                                      .inset(0)
                                      .mask(by::spans(spans::upTo(&reveal)))
                                      .foreground(railLine())));
  host.frame();
  EXPECT_EQ(host.pixel(100, 50), SK_ColorBLACK);  // reveal stops at ~x=28
  reveal = 1.0f;                                  // no render()
  host.frame();
  EXPECT_EQ(host.pixel(100, 50), SK_ColorGREEN);  // the whole line
}

TEST(ComposeRail, OctilinearRoutesDiagonalThenStraight) {
  // The metro-map router: a 45° leg for the shorter delta, then straight —
  // never the direct slanted line.
  Host host;
  host.composer.render(
      stack()
          .child(station("a", 10, 40))    // center (20, 50)
          .child(station("b", 130, 100))  // center (140, 110)
          .child(rail({{"a"}, {"b"}}, routers::octilinear(0.0f))
                     .absolute()
                     .inset(0)
                     .foreground(railLine())));
  host.frame();
  EXPECT_EQ(host.pixel(50, 80), SK_ColorGREEN);    // on the 45° leg
  EXPECT_EQ(host.pixel(110, 110), SK_ColorGREEN);  // on the straight leg
  EXPECT_EQ(host.pixel(80, 80), SK_ColorBLACK);    // NOT the direct line
}

namespace {

/** One thing that decides a rail's route, and what happens when it
 *  changes: a scene built either way, a point the first route inks, a
 *  point the second one inks, and a point the second one must leave
 *  bare. Anchors are keys and normalized points rather than absolute
 *  coordinates, so all three of these reach the derive guard by different
 *  routes and each has to reach it. */
struct RailDecision {
  const char* what;
  std::function<Element(bool second)> scene;
  SkIPoint first;
  SkIPoint second;
  SkIPoint vacated;
};

class RailRoute : public testing::TestWithParam<RailDecision> {};

}  // namespace

TEST_P(RailRoute, ARailReRoutesWhenWhatDecidesItsRouteChanges) {
  const RailDecision& decision = GetParam();
  Host host;
  host.composer.render(decision.scene(false));
  host.frame();
  EXPECT_EQ(host.pixel(decision.first.x(), decision.first.y()), SK_ColorGREEN)
      << "the first route did not draw, so nothing below tests a re-route";
  host.composer.render(decision.scene(true));
  host.frame();
  EXPECT_EQ(host.pixel(decision.second.x(), decision.second.y()), SK_ColorGREEN)
      << "the second route did not draw";
  EXPECT_EQ(host.pixel(decision.vacated.x(), decision.vacated.y()),
            SK_ColorBLACK)
      << "the first route is still on the canvas";
}

INSTANTIATE_TEST_SUITE_P(
    ComposeRail, RailRoute,
    testing::Values(
        RailDecision{"AnAnchorMoves",
                     [](bool second) {
                       return stack()
                           .child(station("a", 10, 40))
                           .child(station("b", 90, second ? 140 : 40))
                           .child(rail({{"a"}, {"b"}})
                                      .absolute()
                                      .inset(0)
                                      .foreground(railLine()));
                     },
                     {60, 50},
                     {60, 100},
                     {60, 50}},
        RailDecision{"TheRouterIsSwapped",
                     [](bool second) {
                       return stack()
                           .child(station("a", 10, 40))
                           .child(station("b", 130, 100))
                           .child(rail({{"a"}, {"b"}},
                                       second ? routers::octilinear(0.0f)
                                              : RailRouter{})
                                      .absolute()
                                      .inset(0)
                                      .foreground(railLine()));
                     },
                     {80, 80},
                     {50, 80},
                     {80, 80}},
        RailDecision{"AnAnchorsNormMoves",
                     [](bool second) {
                       const float ny = second ? 0.0f : 0.5f;
                       return stack()
                           .child(station("a", 10, 40))
                           .child(station("b", 170, 40))
                           .child(rail({{"a", {0.5f, ny}}, {"b", {0.5f, ny}}})
                                      .absolute()
                                      .inset(0)
                                      .foreground(railLine()));
                     },
                     {100, 50},
                     {100, 40},
                     {100, 52}}),
    [](const testing::TestParamInfo<RailDecision>& info) {
      return info.param.what;
    });

TEST(ComposeRail, ClearsWhenAnchorUnmounts) {
  // An unmounted station takes its rail with it. A route whose anchor is
  // gone resolves to nothing and must draw nothing, not keep its last path.
  Host host;
  auto scene = [](bool withB) {
    auto s = stack().child(station("a", 10, 40));
    if (withB) s.child(station("b", 170, 40));
    s.child(rail({{"a"}, {"b"}}).absolute().inset(0).foreground(railLine()));
    return s;
  };
  host.composer.render(scene(true));
  host.frame();
  EXPECT_EQ(host.pixel(100, 50), SK_ColorGREEN);
  host.composer.render(scene(false));  // station b unmounts
  host.frame();
  EXPECT_EQ(host.pixel(100, 50), SK_ColorBLACK);  // rail vanished, not stale
}

TEST(ComposeRail, HitsNearPathOnlyNotItsLayoutBox) {
  // A rail's layout box is inset(0) — the whole canvas — so hit testing it
  // by box would swallow every hit in the frame. It must hit near the routed
  // PATH instead.
  Host host;
  host.composer.render(stack()
                           .child(station("s1", 10, 40))
                           .child(rail({{"s1"}, {"s2"}})
                                      .key("line")
                                      .absolute()
                                      .inset(0)
                                      .foreground(railLine()))
                           .child(station("s2", 170, 40)));
  host.frame();
  auto onPath = host.composer.hitTest({100, 50});
  ASSERT_TRUE(onPath.has_value());
  EXPECT_EQ(*onPath, "line");
  auto onStation = host.composer.hitTest({180, 50});
  ASSERT_TRUE(onStation.has_value());
  EXPECT_EQ(*onStation, "s2");  // stations still win over the rail overlay
  EXPECT_FALSE(host.composer.hitTest({30, 150}).has_value());  // empty canvas
}

// ---- Trim Path (draw-on reveals) -------------------------------------------

TEST(ComposeMask, PartialOutlineStrokesOnlyRevealedStretch) {
  // mask(by::spans(upTo(0.2))) on a square + stroked outline: only the
  // first 20% of the perimeter is dressed; right/bottom stay bare. The
  // fill and every outline decoration trace the CUT path.
  Host host;
  host.composer.render(
      box().child(box()
                      .width(100)
                      .height(100)
                      .inset(0, 0, 100, 100)
                      .absolute()
                      .mask(by::spans(spans::upTo(0.2f)))
                      .foreground(sigil::compose::stroke(4, green()))));
  host.frame();
  // Perimeter order for this outline: left → top → right → bottom, so the
  // first 20% is about the left edge. That order is a property of how the
  // path is built, which is why the assertions name specific edges.
  EXPECT_EQ(host.pixel(1, 50), SK_ColorGREEN);   // left edge revealed
  EXPECT_EQ(host.pixel(50, 1), SK_ColorBLACK);   // top edge bare
  EXPECT_EQ(host.pixel(50, 99), SK_ColorBLACK);  // bottom edge bare
}

TEST(ComposeMask, TransitionDrawsOn) {
  // The draw-on border: a span gate's end transitioned 0 → 1 reveals the
  // perimeter over time (retarget-safe like every transitioned prop).
  Host host;
  auto tree = [](motion::Animatable<float> end) {
    return box().child(box()
                           .key("b")
                           .width(100)
                           .height(100)
                           .inset(0, 0, 100, 100)
                           .absolute()
                           .mask(by::spans(spans::upTo(std::move(end))))
                           .foreground(sigil::compose::stroke(4, green())));
  };
  host.composer.render(tree(0.001f));
  host.frame();
  EXPECT_EQ(host.pixel(50, 99), SK_ColorBLACK);
  host.composer.render(
      tree(animate(sigil::motion::to(1.0f), {400ms, &choreograph::easeNone})));
  host.frame(0.2);  // ~50%: left + top revealed, bottom still bare
  EXPECT_EQ(host.pixel(50, 1), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(50, 99), SK_ColorBLACK);
  host.frame(0.25);  // settle → the full perimeter
  EXPECT_EQ(host.pixel(50, 99), SK_ColorGREEN);
}

TEST(ComposeMask, BoundGateRevealsWithoutRender) {
  // A bound gate end is content volatility: mutate the Output, no
  // render(), and the reveal advances — the self-drawing wire primitive.
  choreograph::Output<float> end{0.2f};
  Host host;
  host.composer.render(
      box().child(box()
                      .width(100)
                      .height(100)
                      .inset(0, 0, 100, 100)
                      .absolute()
                      .mask(by::spans(spans::upTo(&end)))
                      .foreground(sigil::compose::stroke(4, green()))));
  host.frame();
  // (99,30) sits at ~57.5% of the perimeter (right edge, top→bottom).
  EXPECT_EQ(host.pixel(99, 30), SK_ColorBLACK);  // bare at end=0.2
  end = 0.6f;                                    // no render()
  host.frame();
  EXPECT_EQ(host.pixel(99, 30), SK_ColorGREEN);       // reveal reached it
  EXPECT_GT(host.composer.stats().nodesPainted, 0u);  // paints live
}

TEST(ComposeBrushes, FilamentGlowsAroundItsCore) {
  // A filament mark: white-hot core with an additive glow envelope falling
  // off around it, built as a value brush on a rail rather than as a stack
  // of hand-placed nodes.
  Host host;
  host.composer.render(stack()
                           .child(station("a", 10, 90))
                           .child(station("b", 170, 90))
                           .child(rail({{"a"}, {"b"}})
                                      .absolute()
                                      .inset(0)
                                      .stroke(brush::presets::filament())));
  host.frame();
  const SkColor core = host.pixel(100, 100);  // on the line (y=100)
  EXPECT_GT(SkColorGetR(core), 180u);         // near-white core
  EXPECT_GT(SkColorGetB(core), 220u);
  const SkColor glow = host.pixel(100, 106);  // 6px off the line
  EXPECT_GT(SkColorGetB(glow), 25u);          // inside the glow envelope
  EXPECT_LT(SkColorGetB(glow), SkColorGetB(core));
  const SkColor far = host.pixel(100, 140);  // well outside
  EXPECT_LT(SkColorGetB(far), 12u);
}
