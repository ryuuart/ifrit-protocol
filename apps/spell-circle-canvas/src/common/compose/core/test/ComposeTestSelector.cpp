// Which elements a rule speaks about: the CSS text front door read
// against the typed builders it parses into, the weight CSS gives each
// form, the set algebra over selectors, and what a text this library
// does not read does instead.

#include <sigilcompose/core/Selector.h>

#include <string_view>
#include <vector>

#include "support/CoreTestSupport.h"

namespace {

// The two front doors must agree exactly, so every parse case is
// written as the typed selector it is supposed to produce.
void expectReads(std::string_view cssText, const ElementSelector& typed) {
  const ElementSelector parsed = sigil::compose::selector(cssText);
  EXPECT_FALSE(parsed.matchesNothing()) << cssText;
  EXPECT_TRUE(parsed == typed) << cssText;
}

}  // namespace

TEST(ComposeSelector, AClassARoleAndTheUniversalReadTheSameFromBothDoors) {
  expectReads(".card", select::styleClass("card"));
  expectReads("heading", select::role("heading"));
  expectReads("*", select::any());
  // A compound says every one of its parts about ONE element, and `*`
  // says nothing beside them, so it drops out of the value.
  expectReads(".card.wide", select::styleClass("card") & select::styleClass("wide"));
  expectReads("heading.wide", select::role("heading") & select::styleClass("wide"));
  expectReads("*.card", select::styleClass("card"));
  // Whitespace around the parts is not part of what they say.
  expectReads("  .card  ", select::styleClass("card"));
}

TEST(ComposeSelector, EveryCombinatorReadsTheSameFromBothDoors) {
  const ElementSelector card = select::styleClass("card");
  const ElementSelector title = select::styleClass("title");
  expectReads(".card > .title", card.child(title));
  expectReads(".card .title", card.descendant(title));
  expectReads(".card + .title", card.next(title));
  expectReads(".card ~ .title", card.sibling(title));
  expectReads("heading + paragraph",
              select::role("heading").next(select::role("paragraph")));
  // A chain of three, and the subject is the last compound.
  expectReads(".card > .body .title", card.child(select::styleClass("body"))
                                          .descendant(title));
  // The combinator is the same with or without the spaces around it.
  expectReads(".card>.title", card.child(title));
}

TEST(ComposeSelector, TheStructuralPseudoClassesReadTheSameFromBothDoors) {
  const ElementSelector row = select::styleClass("row");
  expectReads(".row:first-child", row.firstChild());
  expectReads(".row:last-child", row.lastChild());
  expectReads(".row:only-child", row.onlyChild());
  expectReads(".row:empty", row.empty());
  expectReads(".row:root", row.root());
  expectReads(".row:nth-child(odd)", row.nthChild(2, 1));
  expectReads(".row:nth-last-child(even)", row.nthLastChild(2, 0));
  // Type means ROLE, so the of-type family reads exactly like the
  // child family with the count taken over one role's siblings.
  expectReads(".row:first-of-type", row.firstOfType());
  expectReads(".row:last-of-type", row.lastOfType());
  expectReads(".row:only-of-type", row.onlyOfType());
  expectReads(".row:nth-of-type(2n)", row.nthOfType(2, 0));
  expectReads(".row:nth-last-of-type(3)", row.nthLastOfType(0, 3));
  // A pseudo-class standing alone is `*` carrying it.
  expectReads(":root", select::any().root());
  expectReads(":first-child", select::any().firstChild());
  // CSS's filtered count: the position is taken over the siblings that
  // match the filter rather than over all of them.
  expectReads(".row:nth-child(2n+1 of .heavy)",
              row.nthChild(2, 1, select::styleClass("heavy")));
  expectReads(":nth-last-child(1 of heading)",
              select::any().nthLastChild(0, 1, select::role("heading")));
}

TEST(ComposeSelector, AnNthChildCountReadsEveryFormCssWrites) {
  const auto counted = [](std::string_view inner) {
    return sigil::compose::selector(std::string(".row:nth-child(") +
                                    std::string(inner) + ")");
  };
  EXPECT_TRUE(counted("odd") == select::styleClass("row").nthChild(2, 1));
  EXPECT_TRUE(counted("even") == select::styleClass("row").nthChild(2, 0));
  EXPECT_TRUE(counted("3") == select::styleClass("row").nthChild(0, 3));
  EXPECT_TRUE(counted("-3") == select::styleClass("row").nthChild(0, -3));
  EXPECT_TRUE(counted("n") == select::styleClass("row").nthChild(1, 0));
  EXPECT_TRUE(counted("2n") == select::styleClass("row").nthChild(2, 0));
  EXPECT_TRUE(counted("2n+1") == select::styleClass("row").nthChild(2, 1));
  EXPECT_TRUE(counted("2n - 1") == select::styleClass("row").nthChild(2, -1));
  EXPECT_TRUE(counted("-n+3") == select::styleClass("row").nthChild(-1, 3));
  EXPECT_TRUE(counted("+2n+1") == select::styleClass("row").nthChild(2, 1));
}

TEST(ComposeSelector, SpecificityCountsExactlyWhatCssCounts) {
  struct Case {
    std::string_view cssText;
    int classes;
    int roles;
  };
  // CSS's own numbers, with the id column struck out: classes and
  // pseudo-classes first, then the roles that stand for types.
  const std::vector<Case> cases = {
      {"*", 0, 0},
      {"heading", 0, 1},
      {".card", 1, 0},
      {".card .title", 2, 0},
      {".card > heading", 1, 1},
      {"heading + paragraph", 0, 2},
      {".row:nth-child(2n+1)", 2, 0},
      {"paragraph:first-of-type", 1, 1},
      {"*:empty", 1, 0},
      {":root", 1, 0},
      // A combinator and the universal weigh nothing at all.
      {"heading > * ~ .title", 1, 1},
      // The filtered count adds its heaviest argument to its own class.
      {".x:nth-child(2 of .heavy.heavier)", 4, 0},
  };
  for (const Case& one : cases) {
    const Specificity weight = sigil::compose::selector(one.cssText).specificity();
    EXPECT_EQ(weight.classes, one.classes) << one.cssText;
    EXPECT_EQ(weight.roles, one.roles) << one.cssText;
  }
  // One class outweighs any number of roles, which is the whole point
  // of comparing the pair left to right.
  EXPECT_TRUE(sigil::compose::selector(".card").specificity() >
              sigil::compose::selector("a b c d").specificity());
}

TEST(ComposeSelector, IsAndNotWeighTheirHeaviestArgumentAndWhereWeighsNothing) {
  EXPECT_EQ(sigil::compose::selector(":is(.a, heading)").specificity().classes, 1);
  EXPECT_EQ(sigil::compose::selector(":is(.a, heading)").specificity().roles, 0);
  EXPECT_EQ(sigil::compose::selector(":is(heading, paragraph)").specificity().roles, 1);
  EXPECT_EQ(sigil::compose::selector(":not(.a, heading)").specificity().classes, 1);
  // :where() is the one form that states a rule and weighs nothing, so
  // anything at all overrides what it sets.
  EXPECT_EQ(sigil::compose::selector(":where(.a, .b.c)").specificity().classes, 0);
  EXPECT_EQ(sigil::compose::selector(":where(.a, .b.c)").specificity().roles, 0);
  // The argument's own weight is counted whole, not one per name.
  EXPECT_EQ(sigil::compose::selector(":is(.a.b.c, .d)").specificity().classes, 3);
  // A list's own weight reports its heaviest alternative.
  EXPECT_EQ(sigil::compose::selector(".a.b, heading").specificity().classes, 2);
}

TEST(ComposeSelector, TheSetAlgebraBuildsListsCompoundsAndNegations) {
  const ElementSelector a = select::styleClass("a");
  const ElementSelector b = select::styleClass("b");
  const ElementSelector c = select::styleClass("c");
  EXPECT_TRUE((a | b) == sigil::compose::selector(".a, .b"));
  EXPECT_TRUE((a & b) == sigil::compose::selector(".a.b"));
  EXPECT_TRUE(!a == sigil::compose::selector(":not(.a)"));
  EXPECT_TRUE(select::is(a | b) == sigil::compose::selector(":is(.a, .b)"));
  EXPECT_TRUE(select::where(a | b) == sigil::compose::selector(":where(.a, .b)"));
  EXPECT_TRUE(select::notAnyOf(a | b) == sigil::compose::selector(":not(.a, .b)"));
  // A comma does not nest: a list of lists is one flat list, so the
  // two ways of writing three alternatives are one value.
  EXPECT_TRUE(((a | b) | c) == (a | (b | c)));
  EXPECT_TRUE(((a | b) | c) == sigil::compose::selector(".a, .b, .c"));
  // A list compounded onto one element is exactly CSS's :is().
  EXPECT_TRUE(((a | b) & c) == sigil::compose::selector(":is(.a, .b).c"));
  // A compound folds into the SUBJECT of a chain, not its ancestor.
  EXPECT_TRUE((a.child(b) & c) == sigil::compose::selector(".a > .b.c"));
  // A negation of a chain is not a compound, so it says nothing.
  EXPECT_TRUE((a & b.child(c)).matchesNothing());
}

TEST(ComposeSelector, ASelectorTextThisLibraryDoesNotReadMatchesNothing) {
  // Each of these warns once and yields a selector that never matches,
  // so a misprint loses one rule instead of the sheet around it.
  const std::vector<std::string_view> unreadable = {
      "",       "..",           ".a >",           ":bogus",
      "::before", ":is()",      ":",              ":nth-child(x)",
      ":nth-child(2n+)", ":nth-of-type(2 of .a)", ".a #identifier",
      "[data-x]", ".a)",        ":is(.a"};
  for (const std::string_view bad : unreadable)
    EXPECT_TRUE(sigil::compose::selector(bad).matchesNothing()) << bad;
  // A default-built selector is the same nothing, and is what the
  // algebra leaves standing rather than a half-built value.
  EXPECT_TRUE(ElementSelector().matchesNothing());
  EXPECT_EQ(ElementSelector().specificity().classes, 0);
}

TEST(ComposeSelector, SelectorsAreValuesThatCompareByWhatTheySay) {
  const ElementSelector card = sigil::compose::selector(".card > .title");
  const ElementSelector same = select::styleClass("card").child(select::styleClass("title"));
  const ElementSelector copy = card;
  EXPECT_TRUE(card == same);
  EXPECT_TRUE(card == copy);
  EXPECT_TRUE(ElementSelector() == ElementSelector());
  // The combinator, the order of the chain and the kind of a name are
  // each part of what a selector says.
  EXPECT_FALSE(card == sigil::compose::selector(".card .title"));
  EXPECT_FALSE(card == sigil::compose::selector(".title > .card"));
  EXPECT_FALSE(select::role("card") == select::styleClass("card"));
  EXPECT_FALSE(sigil::compose::selector(".row:nth-child(2n)") ==
               sigil::compose::selector(".row:nth-child(2n+1)"));
  EXPECT_FALSE(ElementSelector() == card);
}
