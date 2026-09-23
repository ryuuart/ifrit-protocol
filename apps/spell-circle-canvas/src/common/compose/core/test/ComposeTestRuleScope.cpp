// What a rule states: the element's own verbs, written in a rule, folded
// between the parent's answers and the element's own declarations — and
// folded again when a rule moves while the element's own description does
// not. The box first, then the text properties, which a rule states and an
// element cannot.

#include <sigilcompose/core/Property.h>
#include <sigilcompose/core/StyleSheet.h>

#include <concepts>
#include <string_view>

#include "support/CoreTestSupport.h"

namespace {

using sigil::compose::rule;
using sigil::compose::StyleSheet;

/** Where the keyed node laid out, in the canvas. */
SkRect rectOf(Host& host, std::string_view key) {
  return require(host.composer.bounds(key));
}

/** A root that applies @p sheet and holds one card of @p classes, which
 *  holds one 10x10 box — so the card's size is its padding and its
 *  content, and its content's place is its padding. */
Element page(const StyleSheet& sheet, std::string_view classes) {
  return box()
      .applyStyleSheet(sheet)
      .alignItems(Align::Start)
      .children({box().key("card").styleClass(classes).children(
          {box().key("inner").width(10).height(10)})});
}

}  // namespace

// -------------------------------------------------------------------------
// What a rule can say. Each question is a concept over the value, so the
// answers are checked by every build of this file.

template <class Value>
concept SaysPadding = requires(Value value) { value.padding(4); };
template <class Value>
concept SaysMaxTextLines = requires(Value value) { value.maxTextLines(2); };
template <class Value>
concept SaysShape = requires(Value value) { value.shape(Shape{}); };
template <class Value>
concept SaysCover = requires(Value value) { value.cover(); };
template <class Value>
concept SaysKey = requires(Value value) { value.key("k"); };
template <class Value>
concept SaysContentFlowAround =
    requires(Value value) { value.contentFlowAround("k"); };

static_assert(SaysPadding<Rule>);
static_assert(SaysMaxTextLines<Rule>);
static_assert(!SaysShape<Rule>);
static_assert(!SaysCover<Rule>);
static_assert(!SaysKey<Rule>);
static_assert(!SaysContentFlowAround<Rule>);
static_assert(
    std::same_as<decltype(std::declval<Rule&>().padding(4).maxTextLines(2)),
                 Rule&>);

// -------------------------------------------------------------------------
// The box.

TEST(ComposeRuleScope, ARuleStatesTheBoxWithTheElementsOwnVerbs) {
  Host host(400, 200);
  host.composer.render(
      page(StyleSheet{rule(".card").width(120).height(40).padding(8)}, "card"));
  host.frame();
  EXPECT_FLOAT_EQ(rectOf(host, "card").width(), 120.0f);
  EXPECT_FLOAT_EQ(rectOf(host, "card").height(), 40.0f);
  EXPECT_FLOAT_EQ(rectOf(host, "inner").left(), 8.0f);
  EXPECT_FLOAT_EQ(rectOf(host, "inner").top(), 8.0f);
}

TEST(ComposeRuleScope, TheElementsOwnVerbStandsOverTheRule) {
  // Property by property, as CSS's inline style stands over a sheet: the
  // width the element states wins, and the height it leaves unsaid is
  // still the rule's.
  Host host(400, 200);
  host.composer.render(
      box()
          .applyStyleSheet(StyleSheet{rule(".card").width(120).height(40)})
          .alignItems(Align::Start)
          .children({box().key("card").styleClass("card").width(50)}));
  host.frame();
  EXPECT_FLOAT_EQ(rectOf(host, "card").width(), 50.0f);
  EXPECT_FLOAT_EQ(rectOf(host, "card").height(), 40.0f);
}

TEST(ComposeRuleScope, TheHeavierRuleWinsWhateverOrderItStandsIn) {
  Host host(400, 200);
  host.composer.render(
      page(StyleSheet{rule(".card.wide").width(200), rule(".card").width(120)},
           "card wide"));
  host.frame();
  EXPECT_FLOAT_EQ(rectOf(host, "card").width(), 200.0f);
}

TEST(ComposeRuleScope, ARuleThatMovedReachesAnElementWhoseDescriptionDidNot) {
  // THE CASE THE PATCH CANNOT SEE. Between the two frames only the sheet
  // the ROOT applies changes; the card's own description is the same, so
  // the reconciler prunes it and never patches it. The rule's padding
  // reaches it through the cascade pass alone.
  Host host(400, 200);
  host.composer.render(page(StyleSheet{rule(".card").padding(10)}, "card"));
  host.frame();
  ASSERT_FLOAT_EQ(rectOf(host, "card").width(), 30.0f);
  host.composer.render(page(StyleSheet{rule(".card").padding(30)}, "card"));
  EXPECT_EQ(host.composer.stats().patchedNodes, 1u)
      << "only the root's description moved";
  host.frame();
  EXPECT_FLOAT_EQ(rectOf(host, "card").width(), 70.0f);
  EXPECT_FLOAT_EQ(rectOf(host, "inner").left(), 30.0f);
}

TEST(ComposeRuleScope, AClassToggleMovesTheBoxItNames) {
  const StyleSheet sheet{rule(".narrow").width(50), rule(".wide").width(150)};
  Host host(400, 200);
  host.composer.render(page(sheet, "narrow"));
  host.frame();
  EXPECT_FLOAT_EQ(rectOf(host, "card").width(), 50.0f);
  host.composer.render(page(sheet, "wide"));
  host.frame();
  EXPECT_FLOAT_EQ(rectOf(host, "card").width(), 150.0f);
  host.composer.render(page(sheet, "plain"));
  host.frame();
  EXPECT_FLOAT_EQ(rectOf(host, "card").width(), 10.0f)
      << "no rule states a width, so the card is as wide as its content";
}

TEST(ComposeRuleScope, AClassToggleEasesUnderARulesTransition) {
  // The rule states the transition and both ends; the element states
  // neither, and still eases, because the lane watches the value the
  // cascade computed rather than what the element declared.
  const StyleSheet sheet{rule(".card").transition({.duration = 200ms}),
                         rule(".dim").opacity(0.2f),
                         rule(".lit").opacity(1.0f)};
  Host host;
  const auto scene = [&](std::string_view classes) {
    return box().applyStyleSheet(sheet).children(
        {box().styleClass(classes).width(60).height(60).fill(red())});
  };
  host.composer.render(scene("card dim"));
  host.frame();
  const SkColor dim = host.pixel(30, 30);
  host.composer.render(scene("card lit"));
  host.frame(0.1);
  const SkColor mid = host.pixel(30, 30);
  EXPECT_NE(mid, dim) << "the toggle eased from where it stood";
  EXPECT_NE(mid, SK_ColorRED) << "the toggle eased rather than snapping";
  host.frame(0.3);
  EXPECT_EQ(host.pixel(30, 30), SK_ColorRED);
}

TEST(ComposeRuleScope, ARuleKeywordStandsWhereTheElementSaysNothing) {
  Host host(400, 200);
  host.composer.render(
      box()
          .width(200)
          .applyStyleSheet(StyleSheet{rule(".child").inherit(Property::Width)})
          .children(
              {box().key("child").styleClass("child").flexShrink(0).height(
                  10)}));
  host.frame();
  EXPECT_FLOAT_EQ(rectOf(host, "child").width(), 200.0f);
}

TEST(ComposeRuleScope, ARuleKeywordResetsAnInheritedLane) {
  // `initial` on the ink stops the colour arriving from above, as it does
  // written on the element: the swatch paints in the initial ink.
  Host host;
  host.composer.render(
      box()
          .ink(SkColor4f{1, 0, 0, 1})
          .applyStyleSheet(StyleSheet{rule(".reset").initial(Property::Ink)})
          .children({box().styleClass("reset").width(20).height(20).fill(
              Fill::currentInk())}));
  host.frame();
  EXPECT_EQ(host.pixel(10, 10), SK_ColorBLACK);
}

TEST(ComposeRuleScope, ARuleFillsAndRoundsTheElementsItMatches) {
  Host host;
  host.composer.render(
      box()
          .applyStyleSheet(
              StyleSheet{rule(".tile").fill(red()).borderRadius({30})})
          .children({box().styleClass("tile").width(60).height(60)}));
  host.frame();
  EXPECT_EQ(host.pixel(30, 30), SK_ColorRED);
  EXPECT_NE(host.pixel(1, 1), SK_ColorRED) << "the corner was rounded away";
}

TEST(ComposeRuleScope, ARuleHoldsStaticValuesAndLeavesALiveOneOut) {
  // A binding is a verb on the element; written in a rule it is left out,
  // so the element keeps the opacity it had without it.
  choreograph::Output<float> gain{0.0f};
  Host host;
  host.composer.render(
      box()
          .applyStyleSheet(StyleSheet{rule(".faded").opacity(&gain)})
          .children(
              {box().styleClass("faded").width(60).height(60).fill(red())}));
  host.frame();
  EXPECT_EQ(host.pixel(30, 30), SK_ColorRED);
}

// -------------------------------------------------------------------------
// The text properties.

namespace {

const char8_t* const kPassage = u8"one two three four five six seven eight";

/** The height the keyed passage laid out to under @p sheet, clamped to
 *  @p ownLines where the leaf states a clamp of its own. */
float passageHeight(const StyleSheet& sheet, int ownLines = 0) {
  Host host(400, 400);
  Text leaf = text(kPassage, styleAt(18));
  leaf.key("passage").styleClass("clamp").width(90);
  if (ownLines > 0) leaf.maxTextLines(ownLines);
  host.composer.render(
      box().applyStyleSheet(sheet).alignItems(Align::Start).children({leaf}));
  host.frame();
  return rectOf(host, "passage").height();
}

}  // namespace

TEST(ComposeRuleScope, ARuleStatesATextPropertyTheLeafReads) {
  const float one = passageHeight(StyleSheet{}, 1);
  const float two = passageHeight(StyleSheet{}, 2);
  ASSERT_LT(one, two);
  EXPECT_FLOAT_EQ(passageHeight(StyleSheet{rule(".clamp").maxTextLines(1)}),
                  one);
  EXPECT_FLOAT_EQ(passageHeight(StyleSheet{rule(".clamp").maxTextLines(1)}, 2),
                  two)
      << "the leaf's own clamp stands over the rule's";
}

TEST(ComposeRuleScope, ATextRuleThatMovedReachesALeafWhoseDescriptionDidNot) {
  Host host(400, 400);
  const auto scene = [](int lines) {
    Text leaf = text(kPassage, styleAt(18));
    leaf.key("passage").styleClass("clamp").width(90);
    return box()
        .applyStyleSheet(StyleSheet{rule(".clamp").maxTextLines(lines)})
        .alignItems(Align::Start)
        .children({leaf});
  };
  host.composer.render(scene(1));
  host.frame();
  const float one = rectOf(host, "passage").height();
  host.composer.render(scene(2));
  EXPECT_EQ(host.composer.stats().patchedNodes, 1u)
      << "only the root's description moved";
  host.frame();
  EXPECT_FLOAT_EQ(rectOf(host, "passage").height(), passageHeight({}, 2));
  EXPECT_LT(one, rectOf(host, "passage").height());
}

TEST(ComposeRuleScope, AGlyphOutlineFromARuleDrawsAsTheVerbsDoes) {
  const auto scene = [](bool fromRule) {
    Text leaf = text(u8"Outline", styleAt(40));
    leaf.styleClass("engraved");
    if (!fromRule) leaf.textStroke(3, red());
    return box()
        .applyStyleSheet(
            fromRule ? StyleSheet{rule(".engraved").textStroke(3, red())}
                     : StyleSheet{})
        .children({leaf});
  };
  Host ruled, stated, plain;
  ruled.composer.render(scene(true));
  ruled.frame();
  stated.composer.render(scene(false));
  stated.frame();
  plain.composer.render(box().children({text(u8"Outline", styleAt(40))}));
  plain.frame();
  EXPECT_TRUE(identicalPixels(ruled, stated, 200, 200));
  EXPECT_FALSE(identicalPixels(ruled, plain, 200, 200));
}
