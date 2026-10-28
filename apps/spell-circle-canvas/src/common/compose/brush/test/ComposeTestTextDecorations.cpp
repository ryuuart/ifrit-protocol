// Decorations whose subject is text: a run dressed by a hatch or a
// pattern, the textFx track a dressed glyph carries, and the grid a repeat
// is panned across, and what an edge gate does to a box that measured
// to nothing.

#include <sigilcompose/brush/Hatches.h>
#include <sigilcompose/kit/Strokes.h>
#include <sigildraw/Pen.h>
#include <sigilmaterial/paint/Bases.h>

#include "support/BrushTestSupport.h"
#include "support/Rows.h"

TEST(ComposeDecorations, TheBrushVocabularyPaintsOnGeometryBuiltInACustomLeaf) {
  // Live geometry in custom() forfeits pruning and nothing else:
  // PathFormat, lines::Line and Brush read ONLY PaintContext::outline,
  // Decoration::paint is public, and PaintContext is a plain aggregate.
  // So a simulated rope, a live EQ curve or a plotted signal can wear all
  // of it — including PathFormat's own trim window.
  PathFormat dashedHead = stroke(5, Fill::color({0, 1, 0, 1}));
  dashedHead.trimStart = 0.75f;  // the last quarter only
  dashedHead.trimEnd = 1.0f;

  Host host(200, 200);
  host.composer.render(box().children(
      {custom([dashedHead](sigil::draw::Pen& pen, const PaintContext& ctx) {
         // Geometry computed HERE, per paint, and decorated.
         SkPathBuilder b;
         b.moveTo(10, 100).lineTo(190, 100);
         decorations::paintOn(pen, ctx, geometry::path::fromSk(b.detach()),
                              dashedHead);
       })
           .absolute()
           .inset(0)
           .cache(Cache::None)}));
  host.frame();

  auto lit = [&](int x0, int x1) {
    int n = 0;
    for (int x = x0; x < x1; ++x) n += SkColorGetG(host.pixel(x, 100)) > 180;
    return n;
  };
  // The decoration's own trim window applied to hand-built geometry:
  // the last quarter is drawn, the first three are not.
  EXPECT_GT(lit(150, 190), 30);
  EXPECT_EQ(lit(15, 130), 0);
}

TEST(ComposeDecorations, WashFloodsTheOutlineWithAMaterialAndPrunes) {
  // The decoration primitives were PathFormat (strokes), Slice,
  // ContourWalk and a raw PaintProgram — none of which fills a shape with
  // a Material. So "a grain pass over this whole panel, ABOVE its
  // children" had to be a raw PaintProgram, an incomparable std::function
  // that never prunes. overlay() is a different slot: it paints UNDER the
  // children.
  auto build = [](float amount) {
    return box().children({box()
                               .absolute()
                               .inset(0)
                               .fill(Fill::color({0, 0, 0, 1}))
                               .children({box().absolute().inset(40).fill(
                                   Fill::color({0, 0, 1, 1}))})
                               .foreground(decorations::wash(
                                   material::Color{1, 0, 0, 1},
                                   material::BlendMode::PlusLighter, amount))});
  };
  Host full(120, 120), half(120, 120), none(120, 120);
  full.composer.render(build(1.0f));
  full.frame();
  half.composer.render(build(0.5f));
  half.frame();
  none.composer.render(build(0.0f));
  none.frame();

  // It reaches OVER the child, which is what foreground() means and what
  // overlay() deliberately does not do.
  EXPECT_GT(SkColorGetR(full.pixel(60, 60)), 200);
  EXPECT_GT(SkColorGetB(full.pixel(60, 60)),
            200);  // the additive blend kept the blue
  // amount is a real dial, not a flag.
  EXPECT_NEAR(SkColorGetR(half.pixel(60, 60)), 128, 12);
  EXPECT_EQ(SkColorGetR(none.pixel(60, 60)), 0);

  // And it is a comparable VALUE, so a static wash prunes.
  EXPECT_TRUE(decorations::wash(material::Color{1, 0, 0, 1},
                                material::BlendMode::PlusLighter, 0.5f) ==
              decorations::wash(material::Color{1, 0, 0, 1},
                                material::BlendMode::PlusLighter, 0.5f));
  EXPECT_FALSE(decorations::wash(material::Color{1, 0, 0, 1}) ==
               decorations::wash(material::Color{0, 1, 0, 1}));
}

TEST(ComposeDecorations,
     APathFormatMarchesCapsJoinsTrimsAndPaintsAsItsFieldsSay) {
  sigil::compose::test::checkRows({
      {"EachStrokeCarriesItsOwnTrimWindow",
       [&] {
         // Recorded twice as "one trim window per node", which is wrong and
         // worth pinning: PathFormat has had trimStart/trimEnd/trimOffset/
         // trimPhase all along, and the windows COMPOSE — a decoration receives
         // the node's already-trimmed outline, so a second window is a fraction
         // of the revealed part. That is exactly the pen-tip-behind-the-head
         // case that got rebuilt as a duplicate node re-measuring the same
         // 2000-segment path.
         auto lit = [](Host& host, int x0, int y0, int x1, int y1) {
           int n = 0;
           for (int y = y0; y < y1; ++y)
             for (int x = x0; x < x1; ++x)
               n += host.pixel(x, y) != SK_ColorBLACK;
           return n;
         };
         auto line = skiaShape([](SkSize) {
           SkPathBuilder b;
           b.moveTo(10, 100).lineTo(190, 100);
           return b.detach();
         });

         // Node gated to the first 60% — one geometry, two windows on it:
         // a wide dim body over all of it, and a bright sliver at its head.
         PathFormat head = stroke(6, Fill::color({0, 1, 0, 1}));
         head.trimStart = 0.90f;
         head.trimEnd = 1.0f;
         Host host(200, 200);
         host.composer.render(box().children(
             {box()
                  .absolute()
                  .inset(0)
                  .shape(line)
                  .fill(Fill::none())
                  .mask(by::spans(spans::upTo(0.6f)))
                  .foreground(stroke(3, Fill::color({1, 0, 0, 1})))
                  .foreground(head)}));
         host.frame();

         // The body reaches ~x118 (10 + 0.6*180); nothing past it.
         EXPECT_GT(lit(host, 20, 95, 110, 105), 100);
         EXPECT_EQ(lit(host, 130, 95, 190, 105), 0);

         // The head sliver is the LAST tenth of the revealed part, so it is
         // green near x118 and red back at x40 — two independent windows.
         int greenHead = 0, redBody = 0;
         for (int x = 100; x < 120; ++x)
           greenHead += SkColorGetG(host.pixel(x, 100)) > 180 &&
                        SkColorGetR(host.pixel(x, 100)) < 80;
         for (int x = 20; x < 60; ++x)
           redBody += SkColorGetR(host.pixel(x, 100)) > 180 &&
                      SkColorGetG(host.pixel(x, 100)) < 80;
         EXPECT_GT(greenHead, 5);
         EXPECT_GT(redBody, 20);
       }},
      {"ABoundDashPhaseMarchesTheDashes",
       [&] {
         // trimPhase took a bound live value and declared isRunning();
         // dashPhase was a plain float, so marching ants — the commonest
         // animated-line idiom in map and diagram UI — could only be had by
         // re-describing every frame, which defeats the pruning the library is
         // built on. Without a bound phase the only spelling left is a
         // DecorationScheme of one's own.
         sigil::motion::Animatable<float> march =
             sigil::motion::animatable(0.0f);
         PathFormat dashed = stroke(4, Fill::color({1, 1, 1, 1}));
         dashed.dashIntervals = {10.0f, 10.0f};
         dashed.dashPhaseBinding = march;

         Host host(200, 200);
         host.composer.render(box().children({box()
                                                  .absolute()
                                                  .left(0)
                                                  .top(90)
                                                  .width(200)
                                                  .height(20)
                                                  .fill(Fill::none())
                                                  .foreground(dashed)}));
         host.frame();
         auto row = [&] {
           std::vector<bool> lit;
           lit.reserve(200);
           for (int x = 0; x < 200; ++x)
             lit.push_back(host.pixel(x, 90) != SK_ColorBLACK);
           return lit;
         };
         const std::vector<bool> before = row();

         // Advance the phase by half a dash — no render(), no re-describe.
         march = 10.0f;
         host.frame();
         const std::vector<bool> after = row();

         int moved = 0;
         for (size_t i = 0; i < before.size(); ++i)
           moved += before[i] != after[i];
         EXPECT_GT(moved, 20);  // the dashes actually travelled
       }},
      {"CapAndJoin",
       [&] {
         // ~30 open contours of line art all ended square and mitred because
         // this paint was built and never asked.
         auto elbow = skiaShape([](SkSize) {
           SkPathBuilder b;
           b.moveTo(40, 40).lineTo(160, 40).lineTo(160, 160);
           return b.detach();
         });
         auto corner = [&](geometry::path::Join join) {
           PathFormat f = stroke(24, Fill::color({1, 1, 1, 1}));
           f.join = join;
           Host host(200, 200);
           host.composer.render(box().children({box()
                                                    .absolute()
                                                    .inset(0)
                                                    .shape(elbow)
                                                    .fill(Fill::none())
                                                    .foreground(f)}));
           host.frame();
           // The outer corner of the elbow: a miter reaches it, a round does
           // not.
           return host.pixel(171, 29) != SK_ColorBLACK;
         };
         EXPECT_TRUE(corner(geometry::path::Join::Miter));
         EXPECT_FALSE(corner(geometry::path::Join::Round));
       }},
  });
}

// -------------------------------------------------------------------------
// An edge gate on a box that measured to nothing.

TEST(ComposeFx, AnEdgeGateRevealsTheBoxBeforeItsEdgeAtAnyAngle) {
  sigil::compose::test::checkRows({
      {"ItBindsWithoutADescribe",
       [&] {
         // An edge gate binds like a transform: a bound fraction repaints with
         // no render() call at all. Note the limit of the claim — this says
         // nothing about the gate being paint-only, since nothing here checks
         // that bounds() held still.
         Host host(200, 200);
         sigil::motion::Animatable<float> reveal =
             sigil::motion::animatable(0.0f);
         host.composer.render(
             box().children({box()
                                 .absolute()
                                 .inset(20)
                                 .fill(Fill::color({1, 0, 0, 1}))
                                 .mask(by::edge(0.0f, reveal))}));
         host.frame();
         EXPECT_EQ(host.pixel(100, 100), SK_ColorBLACK);

         reveal = 1.0f;  // no render(), no re-describe
         host.frame();
         EXPECT_GT(SkColorGetR(host.pixel(100, 100)), 180);
       }},
      {"AFullRevealOnABoxThatMeasuredNothingHidesNothing",
       [&] {
         // A container of absolutely-positioned children measures zero, and a
         // half-plane built from an empty box is empty — so a FULL reveal hid
         // the whole subtree. A reveal at 1 must never hide anything.
         auto tree = [](bool withWipe) {
           Element outer =
               box().absolute().left(0).top(0);  // no dims: measures 0
           outer.children(
               {box().absolute().left(40).top(40).width(80).height(80).fill(
                   Fill::color({1, 0, 0, 1}))});
           if (withWipe) outer.mask(by::edge(90.0f, 1.0f));
           return box().children({std::move(outer)});
         };
         auto ink = [](Host& host) {
           int n = 0;
           for (int y = 0; y < 200; ++y)
             for (int x = 0; x < 200; ++x)
               n += SkColorGetR(host.pixel(x, y)) > 180;
           return n;
         };

         Host plain(200, 200), wiped(200, 200);
         plain.composer.render(tree(false));
         plain.frame();
         wiped.composer.render(tree(true));
         wiped.frame();
         EXPECT_GT(ink(plain), 5000);
         EXPECT_EQ(ink(wiped), ink(plain));  // a full reveal changes nothing
       }},
  });
}
