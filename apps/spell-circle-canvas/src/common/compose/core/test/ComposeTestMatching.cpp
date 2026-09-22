// Which rules of an applied sheet speak about which nodes: the four
// combinators, the structural pseudo-classes, the selector-list
// pseudo-classes, where a matched rule sits against the name-keyed
// sheet and against the node's own verbs, how far an applied sheet
// reaches, and what makes the pass resolve a node again.
//
// Every case reads the answer off a swatch — a 20x20 box filled with
// the ink in force where it landed — so the colour a pixel comes back
// says which rule won there.

#include <sigilcompose/core/StyleSheet.h>
#include <sigilweave/layout/StyleSheet.h>

#include <string>
#include <vector>

#include "support/CoreTestSupport.h"

namespace {

using sigil::compose::rule;
using sigil::compose::StyleSheet;

const SkColor4f kWhiteInk{1, 1, 1, 1};
const SkColor4f kRedInk{1, 0, 0, 1};
const SkColor4f kBlueInk{0, 0, 1, 1};
const SkColor4f kGreenInk{0, 1, 0, 1};

const SkColor kWhite = SkColorSetARGB(255, 255, 255, 255);
const SkColor kRed = SkColorSetARGB(255, 255, 0, 0);
const SkColor kBlue = SkColorSetARGB(255, 0, 0, 255);
const SkColor kGreen = SkColorSetARGB(255, 0, 255, 0);

/** A 20x20 box painted in the ink in force where it lands. */
Element swatch() {
  return box().width(20).height(20).fill(Fill::currentInk());
}

/** The ink the @p at'th swatch down a column resolved to. */
SkColor inkOf(Host& host, int at) { return host.pixel(10, 20 * at + 10); }

/** A column of swatches under a root that applies @p sheet and inks
 *  everything white, so an unmatched swatch reads white. */
Element column(const StyleSheet& sheet, std::vector<Element> swatches) {
  return box()
      .key("root")
      .ink(kWhiteInk)
      .applyStyleSheet(sheet)
      .children({std::move(swatches)});
}

}  // namespace

TEST(ComposeMatching, EachCombinatorReachesExactlyTheElementsItNames) {
  Host host;
  // A container holding one direct .title and one a level further down,
  // so a child combinator and a descendant combinator differ.
  const auto nested = [](const StyleSheet& sheet) {
    return box().key("root").ink(kWhiteInk).applyStyleSheet(sheet).children(
        {box().styleClass("card").children(
            {swatch().styleClass("title"),
             box().children({swatch().styleClass("title")})})});
  };
  host.composer.render(nested(StyleSheet{rule(".card > .title").ink(kRedInk)}));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kRed);
  EXPECT_EQ(inkOf(host, 1), kWhite);
  host.composer.render(nested(StyleSheet{rule(".card .title").ink(kRedInk)}));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kRed);
  EXPECT_EQ(inkOf(host, 1), kRed);

  // The two sibling combinators, over a flat row: the one immediately
  // after, and every later one.
  Host siblings;
  const auto three = [](const StyleSheet& sheet) {
    return column(sheet, {swatch().styleClass("head"), swatch(), swatch()});
  };
  siblings.composer.render(three(StyleSheet{rule(".head + *").ink(kRedInk)}));
  siblings.frame();
  EXPECT_EQ(inkOf(siblings, 0), kWhite);
  EXPECT_EQ(inkOf(siblings, 1), kRed);
  EXPECT_EQ(inkOf(siblings, 2), kWhite);
  siblings.composer.render(three(StyleSheet{rule(".head ~ *").ink(kRedInk)}));
  siblings.frame();
  EXPECT_EQ(inkOf(siblings, 0), kWhite);
  EXPECT_EQ(inkOf(siblings, 1), kRed);
  EXPECT_EQ(inkOf(siblings, 2), kRed);
}

TEST(ComposeMatching, ALooseCombinatorBacktracksUntilTheWholeChainMatches) {
  // The nearest .b above the subject carries no .a above IT, so a
  // matcher that stopped at the first candidate would miss the pair
  // that does match two levels up.
  Host host;
  host.composer.render(
      box()
          .key("root")
          .ink(kWhiteInk)
          .applyStyleSheet(StyleSheet{rule(".a .b .title").ink(kRedInk)})
          .children({box().styleClass("a").children(
              {box().styleClass("b").children(
                  {box().styleClass("b").children(
                      {swatch().styleClass("title")})})})}));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kRed);
  // And a chain whose ancestor is missing still matches nothing.
  host.composer.render(
      box()
          .key("root")
          .ink(kWhiteInk)
          .applyStyleSheet(StyleSheet{rule(".a .b .title").ink(kRedInk)})
          .children({box().styleClass("b").children(
              {swatch().styleClass("title")})}));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kWhite);
}

TEST(ComposeMatching, TheStructuralPseudoClassesCountPositionAndRole) {
  Host host;
  // Two of one role, one of another, and one with no role at all.
  const auto row = [](const StyleSheet& sheet) {
    return column(sheet, {swatch().role("item"), swatch(),
                          swatch().role("item"), swatch().role("other")});
  };
  host.composer.render(row(StyleSheet{rule(":first-child").ink(kRedInk),
                                      rule(":last-child").ink(kBlueInk)}));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kRed);
  EXPECT_EQ(inkOf(host, 1), kWhite);
  EXPECT_EQ(inkOf(host, 3), kBlue);

  host.composer.render(row(StyleSheet{rule(":nth-child(odd)").ink(kRedInk)}));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kRed);
  EXPECT_EQ(inkOf(host, 1), kWhite);
  EXPECT_EQ(inkOf(host, 2), kRed);
  EXPECT_EQ(inkOf(host, 3), kWhite);

  // Type means ROLE: the count runs over the siblings of one role and
  // skips everything between them.
  host.composer.render(row(StyleSheet{rule("item:first-of-type").ink(kRedInk),
                                      rule("item:last-of-type").ink(kBlueInk)}));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kRed);
  EXPECT_EQ(inkOf(host, 1), kWhite);
  EXPECT_EQ(inkOf(host, 2), kBlue);
  EXPECT_EQ(inkOf(host, 3), kWhite);

  // AN ELEMENT WITH NO ROLE HAS NO TYPE, so an of-type pseudo-class
  // never matches it however alone it stands: the roleless swatch is
  // the only one of nothing, and stays white.
  host.composer.render(row(StyleSheet{rule(":only-of-type").ink(kRedInk)}));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kWhite);
  EXPECT_EQ(inkOf(host, 1), kWhite);
  EXPECT_EQ(inkOf(host, 2), kWhite);
  EXPECT_EQ(inkOf(host, 3), kRed);

  host.composer.render(
      row(StyleSheet{rule("item:nth-last-of-type(1)").ink(kRedInk)}));
  host.frame();
  EXPECT_EQ(inkOf(host, 2), kRed);
  EXPECT_EQ(inkOf(host, 0), kWhite);
}

TEST(ComposeMatching, AFilteredCountRunsOverTheSiblingsThatMatchTheFilter) {
  Host host;
  // Positions 1, 2 and 4 carry .heavy, so the SECOND heavy one is the
  // swatch at position 2 and the second CHILD is the same swatch only
  // by accident; the third heavy one is at position 4.
  const auto row = [](const StyleSheet& sheet) {
    return column(sheet,
                  {swatch().styleClass("heavy"), swatch().styleClass("heavy"),
                   swatch(), swatch().styleClass("heavy")});
  };
  host.composer.render(
      row(StyleSheet{rule(":nth-child(3 of .heavy)").ink(kRedInk)}));
  host.frame();
  EXPECT_EQ(inkOf(host, 3), kRed);
  EXPECT_EQ(inkOf(host, 0), kWhite);
  EXPECT_EQ(inkOf(host, 1), kWhite);
  EXPECT_EQ(inkOf(host, 2), kWhite);
  // The unfiltered count over the same row answers differently, which
  // is the whole point of the filter.
  host.composer.render(row(StyleSheet{rule(":nth-child(3)").ink(kRedInk)}));
  host.frame();
  EXPECT_EQ(inkOf(host, 2), kRed);
  EXPECT_EQ(inkOf(host, 3), kWhite);
  // A swatch that does not match the filter is never counted, even at
  // the position the count names.
  host.composer.render(
      row(StyleSheet{rule(":nth-last-child(1 of .heavy)").ink(kBlueInk)}));
  host.frame();
  EXPECT_EQ(inkOf(host, 3), kBlue);
  EXPECT_EQ(inkOf(host, 2), kWhite);
}

TEST(ComposeMatching, IsAndWhereAndNotSelectSetsOfElements) {
  Host host;
  const auto row = [](const StyleSheet& sheet) {
    return column(sheet, {swatch().styleClass("a"), swatch().styleClass("b"),
                          swatch()});
  };
  host.composer.render(row(StyleSheet{rule(":is(.a, .b)").ink(kRedInk)}));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kRed);
  EXPECT_EQ(inkOf(host, 1), kRed);
  EXPECT_EQ(inkOf(host, 2), kWhite);

  host.composer.render(row(StyleSheet{rule(":not(.a)").ink(kBlueInk)}));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kWhite);
  EXPECT_EQ(inkOf(host, 1), kBlue);
  EXPECT_EQ(inkOf(host, 2), kBlue);

  // :where() weighs nothing, so a single class beats it however late
  // the :where() rule stands in the sheet.
  host.composer.render(row(StyleSheet{rule(".b").ink(kGreenInk),
                                      rule(":where(.a, .b)").ink(kRedInk)}));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kRed);
  EXPECT_EQ(inkOf(host, 1), kGreen);
}

TEST(ComposeMatching, TheHeavierRuleWinsHoweverEarlyItStands) {
  Host host;
  // The heavier rule stands FIRST and still wins; two rules of equal
  // weight are settled by order, and the later one wins.
  host.composer.render(
      box()
          .key("root")
          .ink(kWhiteInk)
          .applyStyleSheet(StyleSheet{rule(".outer .swatch").ink(kRedInk),
                                      rule(".swatch").ink(kBlueInk)})
          .children({box().styleClass("outer").children(
              {swatch().styleClass("swatch")})}));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kRed);
  host.composer.render(
      box()
          .key("root")
          .ink(kWhiteInk)
          .applyStyleSheet(StyleSheet{rule(".swatch").ink(kRedInk),
                                      rule(".swatch").ink(kBlueInk)})
          .children({box().styleClass("outer").children(
              {swatch().styleClass("swatch")})}));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kBlue);
}

TEST(ComposeMatching, ASheetSeesOnlyTheSubtreeItWasAppliedAt) {
  Host host;
  // The sheet is applied on the left branch alone, and its rule names
  // an ancestor standing ABOVE the applying node. That `.page` is
  // outside everything the sheet sees, so the rule reaches nothing.
  const StyleSheet sheet{rule(".page .swatch").ink(kRedInk)};
  host.composer.render(
      box()
          .key("root")
          .ink(kWhiteInk)
          .styleClass("page")
          .children({box().applyStyleSheet(sheet).children(
                         {swatch().styleClass("swatch")}),
                     box().children({swatch().styleClass("swatch")})}));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kWhite);  // the `.page` above is out of reach
  EXPECT_EQ(inkOf(host, 1), kWhite);  // the other branch never sees it
  // Applied AT the `.page` instead, the same sheet does see it: the
  // applying node answers the outer compound and both swatches are
  // inside the subtree.
  host.composer.render(
      box()
          .key("root")
          .ink(kWhiteInk)
          .styleClass("page")
          .applyStyleSheet(sheet)
          .children({box().children({swatch().styleClass("swatch")}),
                     box().children({swatch().styleClass("swatch")})}));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kRed);
  EXPECT_EQ(inkOf(host, 1), kRed);
  // A nearer application wins a tie of equal weight over a farther one.
  host.composer.render(
      box()
          .key("root")
          .ink(kWhiteInk)
          .applyStyleSheet(StyleSheet{rule(".swatch").ink(kBlueInk)})
          .children({box()
                         .applyStyleSheet(StyleSheet{rule(".swatch").ink(kRedInk)})
                         .children({swatch().styleClass("swatch")}),
                     box().children({swatch().styleClass("swatch")})}));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kRed);
  EXPECT_EQ(inkOf(host, 1), kBlue);
}

TEST(ComposeMatching, TheApplyingNodeIsInsideTheSheetItApplies) {
  Host host;
  // The node that applies a sheet is the root of what that sheet sees,
  // not a fence outside it: a rule naming it takes it as its subject.
  host.composer.render(
      box().key("root").ink(kWhiteInk).children(
          {swatch().styleClass("card").applyStyleSheet(
              StyleSheet{rule(".card").ink(kRedInk)})}));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kRed);
  // And it answers the ancestor compound of a chain whose subject
  // stands under it.
  host.composer.render(
      box().key("root").ink(kWhiteInk).children(
          {box()
               .styleClass("card")
               .applyStyleSheet(StyleSheet{rule(".card .swatch").ink(kBlueInk)})
               .children({swatch().styleClass("swatch")})}));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kBlue);
  // Being that root, it is the only child of nothing for its own
  // sheet, however many siblings it has in the tree.
  host.composer.render(
      box().key("root").ink(kWhiteInk).children(
          {swatch(), swatch().applyStyleSheet(
                         StyleSheet{rule(":only-child").ink(kGreenInk)})}));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kWhite);
  EXPECT_EQ(inkOf(host, 1), kGreen);
}

TEST(ComposeMatching, ASiblingOfTheApplyingNodeSatisfiesNeitherSiblingCombinator) {
  Host host;
  // The `.head` swatch stands BESIDE the node that applied the sheet,
  // so it is outside what that sheet sees and no combinator reaches it.
  const auto beside = [](const StyleSheet& sheet) {
    return box().key("root").ink(kWhiteInk).children(
        {swatch().styleClass("head"), swatch().applyStyleSheet(sheet)});
  };
  host.composer.render(beside(StyleSheet{rule(".head + *").ink(kRedInk)}));
  host.frame();
  EXPECT_EQ(inkOf(host, 1), kWhite);
  host.composer.render(beside(StyleSheet{rule(".head ~ *").ink(kRedInk)}));
  host.frame();
  EXPECT_EQ(inkOf(host, 1), kWhite);
  // Applied one node up, the same rule does reach it: both swatches
  // are inside then, and one stands after the other.
  host.composer.render(column(StyleSheet{rule(".head + *").ink(kRedInk)},
                              {swatch().styleClass("head"), swatch()}));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kWhite);
  EXPECT_EQ(inkOf(host, 1), kRed);
}

TEST(ComposeMatching, AMatchedRuleStandsOverAClassAndUnderTheNodesOwnVerbs) {
  Host host;
  const sigil::weave::StyleSheet named{sigil::weave::Rule(
      "note",
      sigil::weave::Type{.color = material::skia::toSkColor(kGreenInk)})};
  const StyleSheet applied{rule(".note").ink(kRedInk)};
  // A class resolves through the name-keyed sheet; the selector rule
  // stands over it, and the node's own ink over them both.
  host.composer.render(box()
                           .key("root")
                           .ink(kWhiteInk)
                           .styleSheet(named)
                           .applyStyleSheet(applied)
                           .children({swatch().styleClass("note"),
                                      swatch().styleClass("note").ink(kBlueInk)}));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kRed);
  EXPECT_EQ(inkOf(host, 1), kBlue);
  // Without the selector rule the class alone still answers, which is
  // the behaviour that stood before rules could match.
  host.composer.render(
      box().key("root").ink(kWhiteInk).styleSheet(named).children(
          {swatch().styleClass("note")}));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kGreen);
}

TEST(ComposeMatching, ARuleSetsACustomPropertyEveryMatchedSubtreeReads) {
  Host host;
  host.composer.render(
      box()
          .key("root")
          .ink(kWhiteInk)
          .applyStyleSheet(StyleSheet{rule(".themed").var("accent", kRedInk),
                                      rule(".themed .swatch")
                                          .ink(sigil::compose::var("accent"))})
          .children({box().styleClass("themed").children(
                         {swatch().styleClass("swatch")}),
                     box().children({swatch().styleClass("swatch")})}));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kRed);
  EXPECT_EQ(inkOf(host, 1), kWhite);
}

TEST(ComposeMatching, TogglingAClassResolvesTheNodeAgain) {
  Host host;
  const StyleSheet sheet{rule(".on").ink(kRedInk)};
  const auto page = [&](bool on) {
    Element mark = swatch();
    if (on) mark.styleClass("on");
    return column(sheet, {std::move(mark)});
  };
  host.composer.render(page(false));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kWhite);
  host.composer.render(page(true));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kRed);
  host.composer.render(page(false));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kWhite);
}

TEST(ComposeMatching, AChangedChildListResolvesThatParentsChildrenAgain) {
  Host host;
  const StyleSheet sheet{rule(":last-child").ink(kRedInk)};
  const auto page = [&](int count) {
    std::vector<Element> swatches;
    for (int at = 0; at < count; ++at) swatches.push_back(swatch());
    return column(sheet, std::move(swatches));
  };
  host.composer.render(page(2));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kWhite);
  EXPECT_EQ(inkOf(host, 1), kRed);
  // The swatch that was last is no longer last, and says so.
  host.composer.render(page(3));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kWhite);
  EXPECT_EQ(inkOf(host, 1), kWhite);
  EXPECT_EQ(inkOf(host, 2), kRed);
  host.composer.render(page(2));
  host.frame();
  EXPECT_EQ(inkOf(host, 1), kRed);
}

TEST(ComposeMatching, ATreeThatAppliesNoSheetResolvesExactlyAsItDid) {
  // The pass with no sheet in force must answer what it always did: a
  // class through the name-keyed sheet, a role under it, the node's own
  // ink over both.
  Host host;
  const sigil::weave::StyleSheet named{sigil::weave::Rule(
      "note",
      sigil::weave::Type{.color = material::skia::toSkColor(kGreenInk)})};
  host.composer.render(
      box().key("root").ink(kWhiteInk).styleSheet(named).children(
          {swatch(), swatch().styleClass("note"),
           swatch().styleClass("note").ink(kBlueInk)}));
  host.frame();
  EXPECT_EQ(inkOf(host, 0), kWhite);
  EXPECT_EQ(inkOf(host, 1), kGreen);
  EXPECT_EQ(inkOf(host, 2), kBlue);
}
