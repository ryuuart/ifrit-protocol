// The relational pseudo-class `:has()`: its relative selectors read the
// same from both doors, it weighs as its heaviest argument, a `:has()`
// inside another is refused, each of the four relations reaches exactly
// the elements it names, and an element whose subtree changes is resolved
// again — a texture it holds baked again then and only then.
//
// A matching case reads its answer off a swatch — a 20x20 box filled with
// the ink in force where it landed — so the colour a pixel comes back
// says whether the rule spoke about that box.

#include <sigilcompose/core/Selector.h>
#include <sigilcompose/core/StyleSheet.h>

#include <string>
#include <string_view>
#include <vector>

#include "support/CoreTestSupport.h"

namespace {

using sigil::compose::rule;
using sigil::compose::StyleSheet;

const SkColor4f kWhiteInk{1, 1, 1, 1};
const SkColor4f kRedInk{1, 0, 0, 1};

const SkColor kWhite = SkColorSetARGB(255, 255, 255, 255);
const SkColor kRed = SkColorSetARGB(255, 255, 0, 0);

void expectReads(std::string_view cssText, const ElementSelector& typed) {
  const ElementSelector parsed = sigil::compose::selector(cssText);
  EXPECT_FALSE(parsed.matchesNothing()) << cssText;
  EXPECT_TRUE(parsed == typed) << cssText;
}

/** A 20x20 box painted in the ink in force where it lands. */
Element swatch() {
  return box().width(20).height(20).fill(Fill::currentInk());
}

/** An element that paints nothing and carries @p classes. */
Element marker(std::string_view classes) {
  return box().styleClass(classes);
}

/** The ink the @p at'th swatch down a column resolved to. */
SkColor inkOf(Host& host, int at) { return host.pixel(10, 20 * at + 10); }

/** A column of @p children under a root that applies @p sheet and inks
 *  everything white, so an unmatched swatch reads white. */
Element column(const StyleSheet& sheet, std::vector<Element> children) {
  return box()
      .key("root")
      .ink(kWhiteInk)
      .applyStyleSheet(sheet)
      .children({std::move(children)});
}

}  // namespace

TEST(ComposeHas, EveryRelationReadsTheSameFromBothDoors) {
  const ElementSelector card = select::styleClass("card");
  const ElementSelector a = select::styleClass("a");
  const ElementSelector b = select::styleClass("b");
  expectReads(".card:has(.a)", card & select::has(a));
  expectReads(".card:has(> .a)", card & select::has(select::child(a)));
  expectReads(":has(+ .a, ~ .b)",
              select::has(select::next(a) | select::sibling(b)));
  // The relation opens the chain; what follows it reads as any chain.
  expectReads(".card:has(> .a .b)",
              card & select::has(select::child(a).descendant(b)));
  // Chained, a :has() asks for both; a list inside one asks for either.
  expectReads(".card:has(.a):has(.b)",
              card & select::has(a) & select::has(b));
  expectReads(".card:has(.a, .b)", card & select::has(a | b));
  expectReads(".card:not(:has(.a))", card & !select::has(a));
  // A relation is part of the value: > .a is not .a.
  EXPECT_FALSE(select::has(select::child(a)) == select::has(a));
}

TEST(ComposeHas, AHasInsideAHasIsRefusedAsCssRefusesIt) {
  const ElementSelector a = select::styleClass("a");
  EXPECT_TRUE(sigil::compose::selector(":has(:has(.a))").matchesNothing());
  EXPECT_TRUE(
      sigil::compose::selector(".x:has(:is(.y:has(.a)))").matchesNothing());
  EXPECT_TRUE(select::has(select::has(a)).matchesNothing());
  EXPECT_TRUE(select::has(select::is(select::has(a))).matchesNothing());
  // An empty :has() and a relation with nothing after it are misprints.
  EXPECT_TRUE(sigil::compose::selector(":has()").matchesNothing());
  EXPECT_TRUE(sigil::compose::selector(":has(>)").matchesNothing());
  // Two siblings of one element, each asking about its own subtree, are
  // not nested.
  EXPECT_FALSE(
      sigil::compose::selector(".x:has(.a) .y:has(.b)").matchesNothing());
}

TEST(ComposeHas, AHasWeighsAsItsHeaviestArgument) {
  EXPECT_EQ(sigil::compose::selector(":has(.a, heading)").specificity(),
            (Specificity{1, 0}));
  EXPECT_EQ(sigil::compose::selector(":has(heading)").specificity(),
            (Specificity{0, 1}));
  // The relation weighs nothing; every compound of the chain after it
  // counts, as it does in any chain.
  EXPECT_EQ(sigil::compose::selector(".x:has(> .a .b)").specificity(),
            (Specificity{3, 0}));
}

TEST(ComposeHas, EachRelationReachesExactlyTheElementsItNames) {
  Host host;
  // A descendant anywhere under the element, and a child directly.
  const auto nested = [](const StyleSheet& sheet) {
    return column(sheet,
                  {swatch().styleClass("card").children({marker("hot")}),
                   swatch().styleClass("card").children(
                       {box().children({marker("hot")})}),
                   swatch().styleClass("card").children({marker("cold")})});
  };
  host.composer.render(nested(StyleSheet{rule(".card:has(.hot)").ink(kRedInk)}));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kRed);
  EXPECT_EQ(inkOf(host, 1), kRed);
  EXPECT_EQ(inkOf(host, 2), kWhite);
  host.composer.render(
      nested(StyleSheet{rule(".card:has(> .hot)").ink(kRedInk)}));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kRed);
  EXPECT_EQ(inkOf(host, 1), kWhite) << "a grandchild is not a child";
  EXPECT_EQ(inkOf(host, 2), kWhite);

  // The sibling immediately after, and any later one.
  Host siblings;
  const auto row = [](const StyleSheet& sheet) {
    return column(sheet, {swatch(), swatch(), swatch().styleClass("hot")});
  };
  siblings.composer.render(row(StyleSheet{rule(":has(+ .hot)").ink(kRedInk)}));
  siblings.frame();
  EXPECT_EQ(inkOf(siblings, 0), kWhite);
  EXPECT_EQ(inkOf(siblings, 1), kRed);
  EXPECT_EQ(inkOf(siblings, 2), kWhite);
  siblings.composer.render(row(StyleSheet{rule(":has(~ .hot)").ink(kRedInk)}));
  siblings.frame();
  EXPECT_EQ(inkOf(siblings, 0), kRed);
  EXPECT_EQ(inkOf(siblings, 1), kRed);
  EXPECT_EQ(inkOf(siblings, 2), kWhite);
}

TEST(ComposeHas, AListAsksForEitherAChainForBothAndANegationForNeither) {
  Host host;
  const auto cards = [](const StyleSheet& sheet) {
    return column(sheet,
                  {swatch().styleClass("card").children({marker("a")}),
                   swatch().styleClass("card").children({marker("b")}),
                   swatch().styleClass("card").children(
                       {marker("a"), marker("b")}),
                   swatch().styleClass("card")});
  };
  host.composer.render(
      cards(StyleSheet{rule(".card:has(.a, .b)").ink(kRedInk)}));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kRed);
  EXPECT_EQ(inkOf(host, 1), kRed);
  EXPECT_EQ(inkOf(host, 2), kRed);
  EXPECT_EQ(inkOf(host, 3), kWhite);
  host.composer.render(
      cards(StyleSheet{rule(".card:has(.a):has(.b)").ink(kRedInk)}));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kWhite);
  EXPECT_EQ(inkOf(host, 1), kWhite);
  EXPECT_EQ(inkOf(host, 2), kRed);
  host.composer.render(
      cards(StyleSheet{rule(".card:not(:has(.a))").ink(kRedInk)}));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kWhite);
  EXPECT_EQ(inkOf(host, 1), kRed);
  EXPECT_EQ(inkOf(host, 2), kWhite);
  EXPECT_EQ(inkOf(host, 3), kRed);
  // Two names on ONE element are what a compound asks for; one of each
  // on two elements is not that.
  host.composer.render(
      cards(StyleSheet{rule(".card:has(.a.b)").ink(kRedInk)}));
  host.frame();
  EXPECT_EQ(inkOf(host, 2), kWhite);
}

TEST(ComposeHas, AHasQualifiesAnAncestorOfTheSubjectToo) {
  Host host;
  host.composer.render(column(
      StyleSheet{rule(".card:has(.hot) .title").ink(kRedInk)},
      {box().styleClass("card").children(
           {swatch().styleClass("title"), marker("hot")}),
       box().styleClass("card").children({swatch().styleClass("title")})}));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kRed);
  EXPECT_EQ(inkOf(host, 1), kWhite);
}

TEST(ComposeHas, ASiblingOfTheApplyingNodeStandsOutsideWhatItsSheetSees) {
  // The node that applied the sheet is the root of what the sheet sees,
  // so the sibling after it is no sibling for its rules.
  Host host;
  host.composer.render(box().key("root").ink(kWhiteInk).children(
      {swatch().applyStyleSheet(StyleSheet{rule(":has(+ .hot)").ink(kRedInk)}),
       swatch().styleClass("hot")}));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kWhite);
}

TEST(ComposeHas, ARelativeSelectorOutsideAHasSpeaksAboutNothing) {
  Host host;
  host.composer.render(column(
      StyleSheet{rule(select::child(select::styleClass("title"))).ink(kRedInk)},
      {swatch().styleClass("title")}));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kWhite);
}

TEST(ComposeHas, AnAncestorIsRestyledWhenADescendantsClassToggles) {
  Host host;
  const auto scene = [](std::string_view inner) {
    return column(StyleSheet{rule(".card:has(.hot)").ink(kRedInk)},
                  {swatch().styleClass("card").children(
                      {box().children({marker(inner)})})});
  };
  host.composer.render(scene("cold"));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kWhite);
  host.composer.render(scene("hot"));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kRed);
  host.composer.render(scene("cold"));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kWhite);
}

TEST(ComposeHas, ATextureCachedElementBakesAgainThenAndOnlyThen) {
  // The panel's texture holds the ink its :has() answer gave it. The
  // element the answer depends on stands BESIDE it, so its class toggling
  // re-describes nothing inside the texture: only the answer can move it.
  Host host;
  const auto scene = [](std::string_view beside) {
    return column(StyleSheet{rule(".panel:has(~ .hot)").ink(kRedInk)},
                  {swatch().key("panel").styleClass("panel").cache(
                       Cache::Texture),
                   marker(beside)});
  };
  host.composer.render(scene("cold"));
  host.frame();
  EXPECT_EQ(host.composer.stats().texturesBaked, 1u);
  EXPECT_EQ(inkOf(host, 0), kWhite);
  host.composer.render(scene("warm"));
  host.frame();
  EXPECT_EQ(host.composer.stats().texturesBaked, 0u)
      << "a sibling's class moved, and the panel's answer did not";
  host.composer.render(scene("hot"));
  host.frame();
  EXPECT_EQ(host.composer.stats().texturesBaked, 1u) << "the answer moved";
  EXPECT_EQ(inkOf(host, 0), kRed);
  host.frame();
  EXPECT_EQ(host.composer.stats().texturesBaked, 0u) << "nothing moved";

  // The same through a descendant: the answer flipping bakes again.
  Host nested;
  const auto inside = [](std::string_view inner) {
    return column(StyleSheet{rule(".panel:has(.hot)").ink(kRedInk)},
                  {swatch().key("panel").styleClass("panel")
                       .cache(Cache::Texture)
                       .children({marker(inner)})});
  };
  nested.composer.render(inside("cold"));
  nested.frame();
  nested.composer.render(inside("hot"));
  nested.frame();
  EXPECT_EQ(nested.composer.stats().texturesBaked, 1u);
  EXPECT_EQ(inkOf(nested, 0), kRed);
}

TEST(ComposeHas, ASubtreeAMemoReusedIsStillSummarised) {
  struct Properties {
    int value;
    bool operator==(const Properties&) const = default;
  };
  const auto inner = [](const Properties&) { return marker("hot"); };
  Host host;
  const auto scene = [&](std::string_view other) {
    return column(StyleSheet{rule(".card:has(.hot)").ink(kRedInk)},
                  {swatch().styleClass("card").children(
                       {memo(Properties{1}, inner).key("inner")}),
                   marker(other)});
  };
  host.composer.render(scene("one"));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kRed);
  // Something else moves, so the pass runs again; the memo is reused and
  // the card still finds what is under it.
  host.composer.render(scene("two"));
  host.frame();
  EXPECT_EQ(host.composer.stats().memoHits, 1u);
  EXPECT_EQ(inkOf(host, 0), kRed);
}
