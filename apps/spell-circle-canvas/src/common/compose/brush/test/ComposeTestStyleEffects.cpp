// The looks a style mark once drew, as a material's effects and layers:
// each pinned to the pixel the drawn mark put there, over an 80 px grey
// box standing 20 px in from the corner of a 120 px host.

#include <sigilcompose/brush/PixelStyles.h>
#include <sigilmaterial/pattern/Patterns.h>

#include "support/BrushTestSupport.h"

namespace {

const material::Color kGrey = {0.5f, 0.5f, 0.5f, 1};

/** The pixel at (@p x, @p y) of a host holding @p node as that box. */
SkColor pixelOf(Element node, int x, int y) {
  Host host(120, 120);
  host.composer.render(box().children(
      {std::move(node.absolute().left(20).top(20).width(80).height(80))}));
  host.frame();
  return host.pixel(x, y);
}

}  // namespace

TEST(ComposeStyleEffects, AnInnerShadowHugsTheEdgeItIsCastFrom) {
  // Cast downward, so the band lies along the TOP inner edge.
  EXPECT_EQ(pixelOf(box().fill(material::from(kGrey).effects(
                        material::Filter::shadow({0, 0, 0, 0.8f},
                                                 {.blur = 6,
                                                  .offset = {0, 4},
                                                  .inside = true}))),
                    60, 22),
            0xFF2A2A2Au);
}

TEST(ComposeStyleEffects, AnOuterGlowIsASpreadShadowWithNoOffset) {
  EXPECT_EQ(pixelOf(box().fill(material::from(kGrey).effects(
                        material::Filter::shadow({1, 1, 1, 0.8f},
                                                 {.blur = 8, .spread = 2}))),
                    60, 15),
            0xFF393939u);
}

TEST(ComposeStyleEffects, ABevelLightsOneEdgeAndShadesTheOther) {
  const Element bevelled =
      box().fill(material::from(kGrey).effects(material::Filter::bevel()));
  EXPECT_EQ(pixelOf(bevelled, 22, 60), 0xFFA3A3A3u);  // lit, upper left
  EXPECT_EQ(pixelOf(bevelled, 97, 60), 0xFF636363u);  // shaded
}

TEST(ComposeStyleEffects, ScanlinesAreALayerOfRows) {
  const Element rows = box().fill(material::from(kGrey).layer(
      material::pattern::scanlines({.color = {0, 0, 0, 0.5f}})));
  EXPECT_EQ(pixelOf(rows, 60, 20), 0xFF404040u);  // in a row
  EXPECT_EQ(pixelOf(rows, 60, 22), 0xFF808080u);  // between rows
}

TEST(ComposeStyleEffects, AStippleIsALayerThroughItsBits) {
  const Element stippled = box().fill(material::from(kGrey).layer(
      material::pattern::stipple({.color = {1, 0, 0, 1}})));
  EXPECT_EQ(pixelOf(stippled, 20, 20), 0xFFFF0000u);  // a set cell
  EXPECT_EQ(pixelOf(stippled, 21, 20), 0xFF808080u);  // a clear one
}

TEST(ComposeStyleEffects, ABevelPairInkedByAColourIsTheColoursMark) {
  EXPECT_EQ(pixelOf(box().fill(Fill::color(kGrey)).foreground(styles::bevelPair(
                        material::Color{1, 1, 1, 0.6f},
                        material::Color{0, 0, 0, 0.5f}, 2)),
                    20, 60),
            0xFFCCCCCCu);
}

TEST(ComposeStyleEffects, AColourInkAndTheMaterialItConvertsToPaintTheSame) {
  const material::Color red = {1, 0, 0, 1};
  const auto framed = [](Fill ink) {
    return box().fill(Fill::color(kGrey)).foreground(
        decorations::border(3, std::move(ink), 4));
  };
  Host byColour(120, 120), byMaterial(120, 120);
  byColour.composer.render(box().children(
      {framed(Fill::color(red)).absolute().left(20).top(20).width(80).height(80)}));
  byMaterial.composer.render(box().children(
      {framed(material::Material(red)).absolute().left(20).top(20).width(80).height(80)}));
  byColour.frame();
  byMaterial.frame();
  EXPECT_TRUE(identicalPixels(byColour, byMaterial, 120, 120));
  EXPECT_EQ(byColour.pixel(24, 60), SK_ColorRED);  // on the inset rule
}

TEST(ComposeStyleEffects, AMarkInkedWithAGradientTakesItAcrossTheBox) {
  // Brackets at all four corners, inked left red to right blue: the left
  // arms are red and the right ones blue.
  const Element bracketed =
      box().fill(Fill::color({0, 0, 0, 1})).foreground(styles::brackets(
          material::linearGradient({0, 0}, {1, 0},
                                   {material::Color{1, 0, 0, 1},
                                    material::Color{0, 0, 1, 1}}),
          12, 2));
  const SkColor left = pixelOf(bracketed, 20, 25);
  const SkColor right = pixelOf(bracketed, 99, 25);
  EXPECT_GT(SkColorGetR(left), 200u);
  EXPECT_LT(SkColorGetB(left), 40u);
  EXPECT_GT(SkColorGetB(right), 200u);
  EXPECT_LT(SkColorGetR(right), 40u);
}

TEST(ComposeStyleEffects, AShadowTakesAMaterialInk) {
  EXPECT_EQ(pixelOf(box().fill(Fill::color(kGrey)).background(sigil::compose::shadow(
                        material::Color{0, 0, 1, 1}, {10, 10}, 0)),
                    105, 105),
            SK_ColorBLUE);
}
