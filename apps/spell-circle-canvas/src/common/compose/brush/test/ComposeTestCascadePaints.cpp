// A paint the cascade carries: a custom property holding a material, read
// by a fill, an ink and a stroke as a colour property is; and the marks
// that take their paint as a `Fill` — a shadow and a layered brush's
// passes — following the ink in force and a custom property.

#include <sigilcompose/brush/Layered.h>

#include "support/BrushTestSupport.h"

namespace {

constexpr int kSide = 200;
const material::Color kRed{1, 0, 0, 1};
const material::Color kBlue{0, 0, 1, 1};

/** Red on the left of a box, blue on the right. */
material::Material leftToRight() {
  return material::linearGradient({0, 0}, {1, 0}, {kRed, kBlue});
}

/** A root holding @p root's statements over one absolute 100 px square
 *  @p square at (50, 50). */
Element square(Element root, Element square) {
  return std::move(root).children(
      {std::move(square).width(100).height(100).inset(50).absolute()});
}

bool reddish(SkColor c) { return SkColorGetR(c) > 200 && SkColorGetB(c) < 60; }
bool bluish(SkColor c) { return SkColorGetB(c) > 200 && SkColorGetR(c) < 60; }

}  // namespace

// ---------------------------------------------------------------------------
// A custom property holding a paint

TEST(ComposeCascadePaints, APropertyHoldingAMaterialFillsAsTheMaterialDoes) {
  Host byProperty(kSide, kSide), byValue(kSide, kSide);
  byProperty.composer.render(square(
      box().applyStyleSheet({rule(":root").var("stone", leftToRight())}),
      box().fill(Fill::var("stone"))));
  byValue.composer.render(square(box(), box().fill(leftToRight())));
  byProperty.frame();
  byValue.frame();
  EXPECT_TRUE(reddish(byProperty.pixel(52, 100)));
  EXPECT_TRUE(bluish(byProperty.pixel(147, 100)));
  EXPECT_TRUE(identicalPixels(byProperty, byValue, kSide, kSide));
}

TEST(ComposeCascadePaints, APropertyHoldingAMaterialInksAndStrokes) {
  // The ink: a paint the leaf's box is laid under, as ink(material) lays it.
  Host byProperty(kSide, kSide), byValue(kSide, kSide);
  byProperty.composer.render(square(
      box().var("stone", leftToRight()),
      box().ink(var("stone")).stroke(stroke(6))));
  byValue.composer.render(
      square(box(), box().ink(leftToRight()).stroke(stroke(6))));
  byProperty.frame();
  byValue.frame();
  EXPECT_TRUE(reddish(byProperty.pixel(50, 100)));
  EXPECT_TRUE(bluish(byProperty.pixel(150, 100)));
  EXPECT_TRUE(identicalPixels(byProperty, byValue, kSide, kSide));
  // A stroke's own paint read from the property.
  Host byStroke(kSide, kSide);
  byStroke.composer.render(square(box().var("stone", leftToRight()),
                                  box().stroke(stroke(6, Fill::var("stone")))));
  byStroke.frame();
  EXPECT_TRUE(identicalPixels(byStroke, byValue, kSide, kSide));
}

TEST(ComposeCascadePaints, AFlatMaterialInAPropertyIsItsColour) {
  const StyleSheet sheet{
      rule(":root").var("flat", material::from(material::Color{0, 1, 0, 1}))};
  Host host(kSide, kSide);
  // A colour property eases under a transition and is read as the ink's
  // colour, so a flat material is held as one.
  host.composer.render(
      square(box().applyStyleSheet(sheet), box().ink(var("flat")).fill(Fill::currentInk())));
  host.frame();
  EXPECT_EQ(host.pixel(100, 100), SK_ColorGREEN);
}

TEST(ComposeCascadePaints, APaintReadAsALengthLeavesTheTargetStanding) {
  Host host(kSide, kSide);
  host.composer.render(box().var("stone", leftToRight()).children(
      {box().key("sized").width(var("stone")).height(40),
       box().var("colour", kRed).children(
           {box().key("coloured").width(var("colour")).height(40)})}));
  host.frame();
  // As a colour property read as a length does.
  EXPECT_EQ(require(host.composer.bounds("sized")).width(),
            require(host.composer.bounds("coloured")).width());
}

// ---------------------------------------------------------------------------
// A shadow and a layered pass take their paint as a Fill

TEST(ComposeCascadePaints, AShadowFollowsTheInkInForceAndAProperty) {
  Host byInk(kSide, kSide), byProperty(kSide, kSide), byValue(kSide, kSide);
  byInk.composer.render(
      square(box().var("accent", kRed).ink(var("accent")),
             box()
                 .background(shadow(Fill::currentInk(), {12, 12}, 0))
                 .fill(Fill::color(kBlue))));
  byProperty.composer.render(
      square(box().var("accent", kRed),
             box()
                 .background(shadow(Fill::var("accent"), {12, 12}, 0))
                 .fill(Fill::color(kBlue))));
  byValue.composer.render(square(
      box(), box().background(shadow(kRed, {12, 12}, 0)).fill(Fill::color(kBlue))));
  byInk.frame();
  byProperty.frame();
  byValue.frame();
  EXPECT_EQ(byInk.pixel(155, 155), SK_ColorRED);
  EXPECT_TRUE(identicalPixels(byInk, byValue, kSide, kSide));
  EXPECT_TRUE(identicalPixels(byProperty, byValue, kSide, kSide));
  // A shadow that names no colour is in the ink, as CSS's box-shadow is.
  Host unnamed(kSide, kSide);
  Shadow plain;
  plain.offset = {12, 12};
  unnamed.composer.render(square(box().ink(kRed),
                                 box().background(plain).fill(Fill::color(kBlue))));
  unnamed.frame();
  EXPECT_TRUE(identicalPixels(unnamed, byValue, kSide, kSide));
}

TEST(ComposeCascadePaints, ALayeredPassTakesAGradientAndAColourFill) {
  LayeredBrush ramp;
  ramp.layers.push_back({.width = 8, .ink = leftToRight()});
  Host host(kSide, kSide);
  host.composer.render(square(box(), box().stroke(ramp)));
  host.frame();
  EXPECT_TRUE(reddish(host.pixel(51, 100))) << "the left edge";
  EXPECT_TRUE(bluish(host.pixel(149, 100))) << "the right edge";

  LayeredBrush flat;
  flat.layers.push_back({.width = 8, .ink = Fill::color({0, 1, 0, 1})});
  Host colour(kSide, kSide);
  colour.composer.render(square(box(), box().stroke(flat)));
  colour.frame();
  EXPECT_EQ(colour.pixel(51, 100), SK_ColorGREEN);
  EXPECT_EQ(colour.pixel(100, 51), SK_ColorGREEN);

  // Unstated, a pass is in the ink in force.
  LayeredBrush unstated;
  unstated.layers.push_back({.width = 8});
  Host inked(kSide, kSide);
  inked.composer.render(
      square(box().ink({0, 1, 0, 1}), box().stroke(unstated)));
  inked.frame();
  EXPECT_TRUE(identicalPixels(inked, colour, kSide, kSide));
}
