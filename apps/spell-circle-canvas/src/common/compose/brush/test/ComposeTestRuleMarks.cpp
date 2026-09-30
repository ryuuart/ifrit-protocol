// What a rule states about the marks of the elements it matches: a
// class carries its element's keyline as CSS's border does, the colour free
// to be a custom property or the ink in force, and the element's own marks
// stand over the class's in each slot.

#include <sigilcompose/core/StyleSheet.h>

#include "support/BrushTestSupport.h"

namespace {

constexpr int kSide = 200;

/** A root setting `--line` to red and applying @p sheet, over one absolute
 *  100 px square @p square. */
Element framed(Element square, StyleSheet sheet = {}) {
  return box()
      .var("line", material::Color{1, 0, 0, 1})
      .applyStyleSheet(std::move(sheet))
      .children({std::move(square).width(100).height(100).inset(50).absolute()});
}

}  // namespace

TEST(ComposeRuleMarks, AClassStrokePaintsWhatTheElementsOwnStrokePaints) {
  Host byClass(kSide, kSide), byElement(kSide, kSide);
  byClass.composer.render(framed(
      box().styleClass("frame"),
      StyleSheet{rule(".frame").stroke(stroke(4, Fill::var("line")))}));
  byElement.composer.render(framed(box().stroke(stroke(4, Fill::var("line")))));
  byClass.frame();
  byElement.frame();
  EXPECT_GT(redInk(byClass), 100) << "the class's keyline painted nothing";
  EXPECT_TRUE(identicalPixels(byClass, byElement, kSide, kSide));
}

TEST(ComposeRuleMarks, TheElementsOwnStrokePaintsOverTheClasssAsASecondCallWould) {
  const auto blue = material::Color{0, 0, 1, 1};
  Host byClass(kSide, kSide), byElement(kSide, kSide);
  byClass.composer.render(framed(
      box().styleClass("frame").stroke(stroke(2, Fill::color(blue))),
      StyleSheet{rule(".frame").stroke(stroke(8, Fill::var("line")))}));
  byElement.composer.render(framed(box()
                                       .stroke(stroke(8, Fill::var("line")))
                                       .stroke(stroke(2, Fill::color(blue)))));
  byClass.frame();
  byElement.frame();
  EXPECT_TRUE(identicalPixels(byClass, byElement, kSide, kSide));
  // The outline's own line is the element's blue; three pixels off it,
  // inside the class's wider keyline and outside the element's, is red.
  EXPECT_EQ(byClass.pixel(100, 50), SK_ColorBLUE);
  EXPECT_EQ(byClass.pixel(100, 47), SK_ColorRED);
}

TEST(ComposeRuleMarks, TwoMatchedRulesStrokeWeakerFirst) {
  const auto blue = material::Color{0, 0, 1, 1};
  Host byClasses(kSide, kSide), byElement(kSide, kSide);
  // `.frame.hot` weighs more than `.frame`, so its ring is the upper one
  // whatever order the sheet states them in.
  byClasses.composer.render(framed(
      box().styleClass("frame hot"),
      StyleSheet{rule(".frame.hot").stroke(stroke(2, Fill::color(blue))),
                 rule(".frame").stroke(stroke(8, Fill::var("line")))}));
  byElement.composer.render(framed(box()
                                       .stroke(stroke(8, Fill::var("line")))
                                       .stroke(stroke(2, Fill::color(blue)))));
  byClasses.frame();
  byElement.frame();
  EXPECT_TRUE(identicalPixels(byClasses, byElement, kSide, kSide));
}

TEST(ComposeRuleMarks, AClassThatStopsMatchingTakesItsKeylineWithIt) {
  Host host(kSide, kSide);
  const StyleSheet sheet{rule(".frame").stroke(stroke(4, Fill::var("line")))};
  host.composer.render(framed(box().styleClass("frame"), sheet));
  host.frame();
  EXPECT_GT(redInk(host), 100);
  host.composer.render(framed(box(), sheet));
  host.frame();
  EXPECT_EQ(redInk(host), 0);
  // …and a sheet that states the keyline in another colour repaints it.
  host.composer.render(framed(
      box().styleClass("frame"),
      StyleSheet{rule(".frame").stroke(
          stroke(4, Fill::color(material::Color{0, 0, 1, 1})))}));
  host.frame();
  EXPECT_EQ(redInk(host), 0);
  EXPECT_EQ(host.pixel(100, 50), SK_ColorBLUE);
}

TEST(ComposeRuleMarks, ARuleBackgroundStandsUnderTheFill) {
  Host byClass(kSide, kSide), byElement(kSide, kSide);
  const Decoration offsetShadow =
      shadow(material::Color{1, 0, 0, 1}, {12, 12}, 0);
  byClass.composer.render(
      framed(box().styleClass("card").fill(material::Color{0, 0, 1, 1}),
             StyleSheet{rule(".card").background(offsetShadow)}));
  byElement.composer.render(framed(box()
                                       .background(offsetShadow)
                                       .fill(material::Color{0, 0, 1, 1})));
  byClass.frame();
  byElement.frame();
  EXPECT_GT(redInk(byClass), 100);
  EXPECT_EQ(byClass.pixel(100, 100), SK_ColorBLUE);
  EXPECT_TRUE(identicalPixels(byClass, byElement, kSide, kSide));
}

// ---------------------------------------------------------------------------
// A material's effects belong to its ink and its fill, wherever either is
// stated.

namespace {

/** A white ink whose material casts a soft red shadow and blurs the layer
 *  — one coverage step and one pass that reads pixels. */
material::Material halation(material::Color shadowColour = {1, 0, 0, 1}) {
  return material::from(material::Color{1, 1, 1, 1})
      .effects(material::Filter::shadow(shadowColour,
                                        {.offset = {6, 6}, .blur = 4})
                   .then(material::Filter::blur(1.5f)));
}

/** One word, keyed "word", under a root setting the type and applying
 *  @p sheet. */
Element page(Text word, StyleSheet sheet = {}) {
  return box()
      .padding(30)
      .font({.face = sigil::test::instrument::sans(), .size = 48})
      .ink(material::Color{1, 1, 1, 1})
      .applyStyleSheet(std::move(sheet))
      .children({std::move(word).key("word")});
}

/** How much of @p host's picture is blue — a pixel whose blue clearly
 *  leads its red and green. */
int blueInk(Host& host) {
  int count = 0;
  for (int y = 0; y < kSide; ++y)
    for (int x = 0; x < kSide; ++x) {
      const SkColor c = host.pixel(x, y);
      if (SkColorGetB(c) > 140 && SkColorGetR(c) < 90 && SkColorGetG(c) < 90)
        ++count;
    }
  return count;
}

}  // namespace

TEST(ComposeRuleMarks, ARulesInkMaterialDressesTheLeafAsItsOwnInkWould) {
  Host byRule(kSide, kSide), byLeaf(kSide, kSide);
  byRule.composer.render(page(text(u8"Ag").styleClass("near"),
                              StyleSheet{rule(".near").ink(halation())}));
  byLeaf.composer.render(page(text(u8"Ag").ink(halation())));
  byRule.frame();
  byLeaf.frame();
  EXPECT_GT(redInk(byRule), 20) << "the ink's shadow never reached the leaf";
  EXPECT_TRUE(identicalPixels(byRule, byLeaf, kSide, kSide));
  EXPECT_EQ(require(byRule.composer.bounds("word")),
            require(byLeaf.composer.bounds("word")));
}

TEST(ComposeRuleMarks, ReplacingTheRuleReplacesTheEffectsItsInkCarried) {
  Host host(kSide, kSide);
  host.composer.render(page(text(u8"Ag").styleClass("near"),
                            StyleSheet{rule(".near").ink(halation())}));
  host.frame();
  ASSERT_GT(redInk(host), 20);
  // Another material: its shadow in place of the first, not beside it.
  host.composer.render(page(
      text(u8"Ag").styleClass("near"),
      StyleSheet{rule(".near").ink(halation(material::Color{0, 0, 1, 1}))}));
  host.frame();
  EXPECT_EQ(redInk(host), 0);
  EXPECT_GT(blueInk(host), 20);
  // A plain ink: no effects at all.
  host.composer.render(
      page(text(u8"Ag").styleClass("near"),
           StyleSheet{rule(".near").ink(material::Color{1, 1, 1, 1})}));
  host.frame();
  EXPECT_EQ(blueInk(host), 0);
  EXPECT_EQ(redInk(host), 0);
}

TEST(ComposeRuleMarks, TheLeafsOwnInkStandsWithItsOwnEffectsOverTheRules) {
  Host host(kSide, kSide);
  host.composer.render(page(text(u8"Ag").styleClass("near").ink(
                                material::Color{1, 1, 1, 1}),
                            StyleSheet{rule(".near").ink(halation())}));
  host.frame();
  EXPECT_EQ(redInk(host), 0);
  // …and a stronger rule's plain ink stands over a weaker rule's material.
  host.composer.render(page(
      text(u8"Ag").styleClass("near far"),
      StyleSheet{rule(".near").ink(halation()),
                 rule(".near.far").ink(material::Color{1, 1, 1, 1})}));
  host.frame();
  EXPECT_EQ(redInk(host), 0);
}

TEST(ComposeRuleMarks, ARulesFillMaterialDressesTheBoxAsItsOwnFillWould) {
  Host byRule(kSide, kSide), byElement(kSide, kSide);
  const material::Material card =
      material::from(material::Color{0, 0, 1, 1})
          .effects(material::Filter::shadow(material::Color{1, 0, 0, 1},
                                            {.offset = {10, 10}, .blur = 2}));
  byRule.composer.render(framed(box().styleClass("card"),
                                StyleSheet{rule(".card").fill(card)}));
  byElement.composer.render(framed(box().fill(card)));
  byRule.frame();
  byElement.frame();
  EXPECT_GT(redInk(byRule), 100);
  EXPECT_TRUE(identicalPixels(byRule, byElement, kSide, kSide));
}
