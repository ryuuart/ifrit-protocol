// kit/Frame.h — the pieces a drawing is pinned into: the absolute rect
// `at` places ink in, the centred box, the ring and the dot at their own
// radius, and the rules — one line, a pair of them and a ladder.
//
// The claim is about WHERE INK LANDS, so these render: restating
// `left/top/width/height` in the assertion would check the spelling
// against itself.

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilmaterial/paint/Bases.h>

#include <utility>

#include "support/Rows.h"
#include "support/ShapeTestSupport.h"

namespace kit = sigil::compose::kit;

TEST(KitLine, ALineRunsItsLengthAtItsThicknessInTheInkInForce) {
  sigil::compose::test::checkRows({
      {"StretchesAcrossItsFlowAndIsDrawnInTheInkInForce",
       [&] {
         Host host(160, 120);
         host.composer.render(
             box()
                 .width(160)
                 .height(120)
                 .column()
                 .padding(Dimension(20))
                 .ink({1, 0, 0, 1})
                 .children(
                     {kit::line({}).key("rule"),
                      kit::line({.thickness = 3, .inset = 10}).key("tick")}));
         host.frame();
         // A hairline is the default thickness, and it stretches across the
         // flow it stands in — the padded width, here.
         const SkRect rule = require(host.composer.bounds("rule"));
         EXPECT_EQ(rule, SkRect::MakeXYWH(20, 20, 120, 1));
         EXPECT_EQ(host.pixel(80, 20), SK_ColorRED);
         // A 3 px tick is the same call at another thickness, held off at both
         // ends by its inset.
         const SkRect tick = require(host.composer.bounds("tick"));
         EXPECT_FLOAT_EQ(tick.height(), 3);
         EXPECT_FLOAT_EQ(tick.left(), 30);
         EXPECT_FLOAT_EQ(tick.right(), 130);
       }},
      {"ARuleIsASurfaceAndAPairedOneRulesInTheInkItCannotCollapse",
       [&] {
         // An UNPAIRED rule is a node's own fill, so it takes the whole surface
         // set: a gradient rules it as it grounds a well.
         Host gradient(120, 40);
         gradient.composer.render(box().width(120).height(40).column().children(
             {kit::line({.length = Dimension(100),
                         .thickness = 8,
                         .fill = material::linearGradient(
                             {0, 0}, {100, 0},
                             {{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}},
                             {.units = material::GradientUnits::Pixels})})}));
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
                 .children({kit::line(
                     {.length = Dimension(100),
                      .thickness = 4,
                      .fill = material::linearGradient(
                          {0, 0}, {1, 0},
                          {{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}}),
                      .pair = {{.thickness = 2, .gap = 6}}})}));
         paired.frame();
         EXPECT_EQ(paired.pixel(50, 1), SK_ColorGREEN);
         EXPECT_EQ(paired.pixel(50, 11), SK_ColorGREEN);
       }},
  });
}
