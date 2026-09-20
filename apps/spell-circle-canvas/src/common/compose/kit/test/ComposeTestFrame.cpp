// kit/Frame.h — the pieces a drawing is pinned into: the absolute rect
// `at` places ink in, the centred box, the ring and the dot at their own
// radius, and the rules — one line, a pair of them and a ladder.
//
// The claim is about WHERE INK LANDS, so these render: restating
// `left/top/width/height` in the assertion would check the spelling
// against itself.

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/kit/Frame.h>

#include <utility>

#include "support/ShapeTestSupport.h"

namespace kit = sigil::compose::kit;

TEST(KitAt, PinsInkAtTheAbsoluteRectAndNowhereElse) {
  Host host;
  host.composer.render(box().width(200).height(200).children(
      {kit::at(20, 30, 40, 50).fill(red())}));
  host.frame();
  EXPECT_EQ(host.pixel(21, 31), SK_ColorRED);
  EXPECT_EQ(host.pixel(59, 79), SK_ColorRED);
  EXPECT_EQ(host.pixel(19, 31), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(21, 29), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(61, 81), SK_ColorBLACK);
}

TEST(KitAt, TheElementOverloadPlacesANodeItDidNotBuild) {
  Host host;
  // The node carries its own paint and knows nothing about the plate; the
  // plate says where it goes. That split is the overload's whole reason.
  Element painted = box().fill(green());
  host.composer.render(box().width(200).height(200).children(
      {kit::at(std::move(painted), 100, 10, 30, 20)}));
  host.frame();
  EXPECT_EQ(host.pixel(101, 11), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(129, 29), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(99, 11), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(131, 31), SK_ColorBLACK);
}

TEST(KitLine, StretchesAcrossItsFlowAndIsDrawnInTheInkInForce) {
  Host host(160, 120);
  host.composer.render(
      box()
          .width(160)
          .height(120)
          .column()
          .padding(Dimension(20))
          .ink({1, 0, 0, 1})
          .children({kit::line({}).key("rule"),
                     kit::line({.thickness = 3, .inset = 10}).key("tick")}));
  host.frame();
  // A hairline is the default thickness, and it stretches across the flow
  // it stands in — the padded width, here.
  const SkRect rule = require(host.composer.bounds("rule"));
  EXPECT_EQ(rule, SkRect::MakeXYWH(20, 20, 120, 1));
  EXPECT_EQ(host.pixel(80, 20), SK_ColorRED);
  // A 3 px tick is the same call at another thickness, held off at both
  // ends by its inset.
  const SkRect tick = require(host.composer.bounds("tick"));
  EXPECT_FLOAT_EQ(tick.height(), 3);
  EXPECT_FLOAT_EQ(tick.left(), 30);
  EXPECT_FLOAT_EQ(tick.right(), 130);
}

TEST(KitLine, RunsDownWhereItIsAskedToAndTakesTheLengthItIsGiven) {
  Host host(160, 120);
  host.composer.render(box().width(160).height(120).row().children(
      {kit::line({.column = true}).key("down"),
       kit::line({.length = Dimension(40), .thickness = 2}).key("across"),
       kit::line({.length = Dimension(30), .column = true}).key("short")}));
  host.frame();
  const SkRect down = require(host.composer.bounds("down"));
  EXPECT_FLOAT_EQ(down.width(), 1);
  EXPECT_FLOAT_EQ(down.height(), 120);
  const SkRect across = require(host.composer.bounds("across"));
  EXPECT_FLOAT_EQ(across.width(), 40);
  EXPECT_FLOAT_EQ(across.height(), 2);
  const SkRect shortRun = require(host.composer.bounds("short"));
  EXPECT_FLOAT_EQ(shortRun.width(), 1);
  EXPECT_FLOAT_EQ(shortRun.height(), 30);
}

TEST(KitFrame, CentredPutsWhatItHoldsInTheMiddleBothWays) {
  Host host(100, 60);
  host.composer.render(box().width(100).height(60).children(
      {kit::centred(box().key("mark").width(20).height(10))
           .absolute()
           .inset(0)}));
  host.frame();
  EXPECT_EQ(require(host.composer.bounds("mark")),
            SkRect::MakeXYWH(40, 25, 20, 10));
}

TEST(KitFrame, ARingIsStrokedAndADotIsFilledAtTheirOwnRadius) {
  Host host(120, 120);
  host.composer.render(box().width(120).height(120).children(
      {kit::ring({60, 60}, 40, stroke(4, green())).key("ring"),
       kit::dot({60, 60}, 10, red()).key("dot")}));
  host.frame();
  // Each stands in the box its own radius names, about the point it was
  // given — which is what `disc` decides and these two draw.
  EXPECT_EQ(require(host.composer.bounds("ring")),
            SkRect::MakeXYWH(20, 20, 80, 80));
  EXPECT_EQ(require(host.composer.bounds("dot")),
            SkRect::MakeXYWH(50, 50, 20, 20));
  // The ring is a line and not a disc: its rim is inked and its middle is
  // whatever stands there, which here is the dot.
  EXPECT_EQ(host.pixel(60, 21), SK_ColorGREEN);
  EXPECT_NE(host.pixel(60, 40), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(60, 60), SK_ColorRED);
}

TEST(KitLine, ALadderPutsEveryRuleOnItsOwnLineOfTheRhythm) {
  // The pitch is the RHYTHM, not the gap: a rule SITS ON each line of it,
  // so a caller states no `pitch - thickness` of its own.
  Host host(160, 120);
  host.composer.render(box().width(160).height(120).children({kit::ladder(
      {.count = 3, .pitch = 20, .fill = Fill::color({1, 0, 0, 1})})}));
  host.frame();
  for (int i = 1; i <= 3; ++i) {
    EXPECT_EQ(host.pixel(80, i * 20 - 1), SK_ColorRED) << "rule " << i;
    EXPECT_EQ(host.pixel(80, i * 20 + 4), SK_ColorBLACK) << "under " << i;
  }
  // Three rules and no fourth: the ladder is as deep as its count says.
  EXPECT_EQ(host.pixel(80, 79), SK_ColorBLACK);
  // The column rhythm is the same ladder turned: rules DOWN, ranged
  // across at the pitch.
  host.composer.render(box().width(160).height(120).children(
      {kit::ladder({.count = 2,
                    .pitch = 30,
                    .column = true,
                    .fill = Fill::color({0, 1, 0, 1})})}));
  host.frame();
  EXPECT_EQ(host.pixel(29, 60), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(59, 60), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(89, 60), SK_ColorBLACK);
}

TEST(KitLine, APairIsOneNodeAsDeepAsBothItsRails) {
  Host host(120, 60);
  host.composer.render(box().width(120).height(60).column().children(
      {kit::line({.length = Dimension(100),
                  .thickness = 4,
                  .fill = green(),
                  .pair = {{.thickness = 2, .gap = 6, .fill = red()}}})
           .key("rule")}));
  host.frame();
  // The node is the heavy rail, the gap and the companion together, and
  // its route runs along the first: the heavy rail is at the top.
  const SkRect rule = require(host.composer.bounds("rule"));
  EXPECT_EQ(rule, SkRect::MakeXYWH(0, 0, 100, 12));
  EXPECT_EQ(host.pixel(50, 1), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(50, 11), SK_ColorRED);
  EXPECT_EQ(host.pixel(50, 6), SK_ColorBLACK);
}

TEST(KitLine, ARuleIsASurfaceAndAPairedOneRulesInTheInkItCannotCollapse) {
  // An UNPAIRED rule is a node's own fill, so it takes the whole surface
  // set: a gradient rules it as it grounds a well.
  Host gradient(120, 40);
  gradient.composer.render(box().width(120).height(40).column().children(
      {kit::line({.length = Dimension(100),
                  .thickness = 8,
                  .fill = material::skia::Paint::linear(
                      {0, 0}, {100, 0},
                      {{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}})})}));
  gradient.frame();
  EXPECT_GT(SkColorGetR(gradient.pixel(2, 4)), 200u);
  EXPECT_GT(SkColorGetB(gradient.pixel(97, 4)), 200u);

  // A PAIRED rule is two strokes, and a stroke stores one comparable
  // fill: a unit-square ramp has no colour to give one measured without
  // a frame. The pair rules in the ink in force rather than in the
  // opaque black an empty fill leaves behind.
  Host paired(120, 60);
  paired.composer.render(
      box()
          .width(120)
          .height(60)
          .column()
          .ink({0, 1, 0, 1})
          .children(
              {kit::line({.length = Dimension(100),
                          .thickness = 4,
                          .fill = material::skia::Paint::linearUnit(
                              {0, 0}, {1, 0},
                              {{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}}),
                          .pair = {{.thickness = 2, .gap = 6}}})}));
  paired.frame();
  EXPECT_EQ(paired.pixel(50, 1), SK_ColorGREEN);
  EXPECT_EQ(paired.pixel(50, 11), SK_ColorGREEN);
}

TEST(KitLine, TakesAStatedFillOverTheInk) {
  Host host(60, 40);
  host.composer.render(
      box()
          .width(60)
          .height(40)
          .column()
          .ink({1, 0, 0, 1})
          .children(
              {kit::line({.thickness = 4, .fill = green()}).key("rule")}));
  host.frame();
  EXPECT_EQ(host.pixel(30, 1), SK_ColorGREEN);
}
