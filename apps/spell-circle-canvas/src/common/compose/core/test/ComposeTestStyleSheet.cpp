// What a rule states about the elements its selector speaks about, and
// the sheet that holds rules in order: the value semantics two sheets
// compare by, joining with +, and a literal that includes sheets.

#include <sigilcompose/core/StyleSheet.h>

#include <vector>

#include "support/CoreTestSupport.h"

using sigil::compose::Rule;
using sigil::compose::StyleSheet;

namespace {

const SkColor4f kRed{1.0f, 0.0f, 0.0f, 1.0f};
const SkColor4f kBlue{0.0f, 0.0f, 1.0f, 1.0f};

}  // namespace

TEST(ComposeStyleSheet, ARuleIsASelectorAndTheSamePartialsAVerbWrites) {
  const Rule stated =
      sigil::compose::rule(".card")
          .font({.size = 18})
          .paragraph({.leading = sigil::weave::Leading::multiple(1.5f)})
          .ink(kRed)
          .var("accent", kBlue)
          .var("gutter", sigil::compose::Dimension(12.0f));
  EXPECT_TRUE(stated.selector() == sigil::compose::selector(".card"));
  EXPECT_TRUE(stated.type().size.has_value());
  EXPECT_TRUE(stated.paragraph().leading.has_value());
  EXPECT_TRUE(stated.type().color == kRed);
  EXPECT_FALSE(stated.inkVar().has_value());
  EXPECT_EQ(stated.vars().entries().size(), 2u);
  // The same merge a verb uses: a second call wins field by field and
  // leaves the fields it does not name where the first call left them.
  Rule again = sigil::compose::rule(".card").font({.size = 18});
  again.font({.weight = 700});
  EXPECT_TRUE(again.type().size.has_value());
  EXPECT_TRUE(again.type().weight.has_value());
  // The two front doors onto a rule build the same value.
  EXPECT_TRUE(sigil::compose::rule(".card > .title") ==
              sigil::compose::rule(sigil::compose::selector(".card > .title")));
  // A rule states what it states about no element at all where its
  // text did not read, rather than about every element.
  EXPECT_TRUE(sigil::compose::rule("[data-x]").selector().matchesNothing());
}

TEST(ComposeStyleSheet, AnInkIsEitherAColourOrAPropertyAndTheLaterOneStands) {
  const Rule fromProperty =
      sigil::compose::rule(".card").ink(kRed).ink(sigil::compose::var("brand"));
  EXPECT_TRUE(fromProperty.inkVar() == sigil::compose::var("brand"));
  EXPECT_FALSE(fromProperty.type().color.has_value());
  const Rule fromColour =
      sigil::compose::rule(".card").ink(sigil::compose::var("brand")).ink(kRed);
  EXPECT_FALSE(fromColour.inkVar().has_value());
  EXPECT_TRUE(fromColour.type().color == kRed);
  // A colour written through font() is the ink too, so it displaces a
  // property the ink was read from just as ink() does.
  const Rule throughFont = sigil::compose::rule(".card")
                               .ink(sigil::compose::var("brand"))
                               .font({.color = kBlue});
  EXPECT_FALSE(throughFont.inkVar().has_value());
  EXPECT_TRUE(throughFont.type().color == kBlue);
}

TEST(ComposeStyleSheet, TwoSheetsBuiltSeparatelyFromEqualRulesAreEqual) {
  const StyleSheet one{sigil::compose::rule(".card").font({.size = 18}),
                       sigil::compose::rule("heading").ink(kRed)};
  const StyleSheet same{sigil::compose::rule(".card").font({.size = 18}),
                        sigil::compose::rule("heading").ink(kRed)};
  const StyleSheet shared = one;
  EXPECT_TRUE(one == same);
  EXPECT_TRUE(one == shared);
  EXPECT_EQ(one.size(), 2u);
  // Order is part of what a sheet is, because it is the last tiebreak.
  const StyleSheet reversed{sigil::compose::rule("heading").ink(kRed),
                            sigil::compose::rule(".card").font({.size = 18})};
  EXPECT_FALSE(one == reversed);
  // A rule that states something else is a different sheet.
  const StyleSheet other{sigil::compose::rule(".card").font({.size = 19}),
                         sigil::compose::rule("heading").ink(kRed)};
  EXPECT_FALSE(one == other);
  // An empty sheet is empty however it was reached.
  EXPECT_TRUE(StyleSheet{} == StyleSheet{});
  EXPECT_TRUE(StyleSheet{}.empty());
  EXPECT_FALSE(one == StyleSheet{});
}

TEST(ComposeStyleSheet, JoiningTwoSheetsKeepsTheirRulesInOrder) {
  const StyleSheet house{sigil::compose::rule(".card").font({.size = 18})};
  const StyleSheet dark{sigil::compose::rule(".card").ink(kRed)};
  const StyleSheet local{sigil::compose::rule("heading").ink(kBlue)};
  const StyleSheet joined = house + dark + local;
  EXPECT_EQ(joined.size(), 3u);
  EXPECT_TRUE(joined.rules()[0] == house.rules()[0]);
  EXPECT_TRUE(joined.rules()[1] == dark.rules()[0]);
  EXPECT_TRUE(joined.rules()[2] == local.rules()[0]);
  // Joining is ordered, so the other order is another sheet.
  EXPECT_FALSE(joined == (local + dark + house));
  // An empty sheet on either side leaves the other one standing.
  EXPECT_TRUE((house + StyleSheet{}) == house);
  EXPECT_TRUE((StyleSheet{} + house) == house);
}

TEST(ComposeStyleSheet, ASheetLiteralMayIncludeSheetsWhereARuleWouldStand) {
  const StyleSheet house{sigil::compose::rule(".card").font({.size = 18})};
  const StyleSheet dark{sigil::compose::rule(".card").ink(kRed),
                        sigil::compose::rule("heading").ink(kRed)};
  const StyleSheet page{house, dark, sigil::compose::rule(".badge").ink(kBlue)};
  // The included sheet's rules stand where it stood, in its own order.
  EXPECT_EQ(page.size(), 4u);
  EXPECT_TRUE(page.rules()[0] == house.rules()[0]);
  EXPECT_TRUE(page.rules()[1] == dark.rules()[0]);
  EXPECT_TRUE(page.rules()[2] == dark.rules()[1]);
  EXPECT_TRUE(page.rules()[3] == sigil::compose::rule(".badge").ink(kBlue));
  // Including sheets says exactly what joining them says.
  EXPECT_TRUE(page == (house + dark +
                       StyleSheet{sigil::compose::rule(".badge").ink(kBlue)}));
}

TEST(ComposeStyleSheet, TheSheetsANodeAppliesArePartOfWhatThatNodeIs) {
  // Nothing matches yet, so what is under test is only this: a node
  // that applies the same sheets is the same node and prunes, and one
  // that applies different sheets is a change reconcile must see.
  Host host;
  const StyleSheet house{sigil::compose::rule(".card").font({.size = 18})};
  const StyleSheet dark{sigil::compose::rule(".card").ink(kRed)};
  const auto page = [](const StyleSheet& applied) {
    return box().key("p").applyStyleSheet(applied).children(
        {box().key("c").width(20).height(20)});
  };
  host.composer.render(page(house));
  host.frame();
  // The same value again, and a separately built sheet that says the
  // same thing, are both the same description.
  host.composer.render(page(house));
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  host.composer.render(
      page(StyleSheet{sigil::compose::rule(".card").font({.size = 18})}));
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  // A sheet that states something else is a change.
  host.composer.render(page(dark));
  EXPECT_EQ(host.composer.stats().patchedNodes, 1u);
}

TEST(ComposeStyleSheet, ApplyingASheetAgainAddsItAfterTheOneBefore) {
  Host host;
  const StyleSheet house{sigil::compose::rule(".card").font({.size = 18})};
  const StyleSheet dark{sigil::compose::rule(".card").ink(kRed)};
  const auto page = [&](bool both) {
    Element node = box().key("p").applyStyleSheet(house);
    if (both) node.applyStyleSheet(dark);
    return node.children({box().key("c").width(20).height(20)});
  };
  host.composer.render(page(false));
  host.frame();
  host.composer.render(page(false));
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  // A second application is another sheet on the node, not a replaced
  // one, so the node is not what it was.
  host.composer.render(page(true));
  EXPECT_EQ(host.composer.stats().patchedNodes, 1u);
  host.composer.render(page(true));
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
}

TEST(ComposeStyleSheet, ARuleComparesTheTransitionItStates) {
  using sigil::compose::rule;
  EXPECT_TRUE(rule(".a").transition({.duration = 200ms}) ==
              rule(".a").transition({.duration = 200ms}));
  EXPECT_FALSE(rule(".a").transition({.duration = 200ms}) == rule(".a"));
  EXPECT_FALSE(rule(".a").transition({.duration = 200ms}) ==
               rule(".a").transition({.duration = 300ms}));
  EXPECT_FALSE(rule(".a").transition()) << "unstated until a call states it";
}
