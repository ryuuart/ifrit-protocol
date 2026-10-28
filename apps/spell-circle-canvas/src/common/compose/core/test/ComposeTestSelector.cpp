// Which elements a rule speaks about: the CSS text front door read
// against the typed builders it parses into, the weight CSS gives each
// form, the set algebra over selectors, and what a text this library
// does not read does instead.

#include <sigilcompose/core/Selector.h>
#include <sigilcore/report/Report.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "support/CoreTestSupport.h"
#include "support/Rows.h"
#include "support/Selectors.h"

using sigil::compose::test::checkRows;
using sigil::compose::test::forEachRow;

// The two front doors build one value: the CSS text and the typed builders
// say the same selector for every class, role, compound, combinator and
// pseudo-class the text reads.
TEST(ComposeSelector, TheTextAndTheBuildersReadAsOneValue) {
  struct ReadingRow {
    const char* name;
    std::string_view cssText;
    ElementSelector typed;
  };
  const ElementSelector card = select::styleClass("card");
  const ElementSelector title = select::styleClass("title");
  const ElementSelector row = select::styleClass("row");
  const ReadingRow readings[] = {
      {"AClass", ".card", card},
      {"ARole", "heading", select::role("heading")},
      {"TheUniversal", "*", select::any()},
      // A compound says every one of its parts about ONE element, and `*`
      // says nothing beside them, so it drops out of the value.
      {"ACompoundOfTwoClasses", ".card.wide",
       card & select::styleClass("wide")},
      {"ACompoundOfARoleAndAClass", "heading.wide",
       select::role("heading") & select::styleClass("wide")},
      {"TheUniversalBesideAClassDropsOut", "*.card", card},
      {"SpaceAroundTheSelectorIsNotPartOfIt", "  .card  ", card},
      {"AChild", ".card > .title", card.child(title)},
      {"ADescendant", ".card .title", card.descendant(title)},
      {"TheNextSibling", ".card + .title", card.next(title)},
      {"AnyLaterSibling", ".card ~ .title", card.sibling(title)},
      {"TheNextSiblingOfARole", "heading + paragraph",
       select::role("heading").next(select::role("paragraph"))},
      // A chain of three, and the subject is the last compound.
      {"AChainOfThree", ".card > .body .title",
       card.child(select::styleClass("body")).descendant(title)},
      {"ACombinatorWithoutSpaces", ".card>.title", card.child(title)},
      {"FirstChild", ".row:first-child", row.firstChild()},
      {"LastChild", ".row:last-child", row.lastChild()},
      {"OnlyChild", ".row:only-child", row.onlyChild()},
      {"Empty", ".row:empty", row.empty()},
      {"Root", ".row:root", row.root()},
      {"NthChild", ".row:nth-child(odd)", row.nthChild(2, 1)},
      {"NthLastChild", ".row:nth-last-child(even)", row.nthLastChild(2, 0)},
      // Type means ROLE, so the of-type family reads exactly like the child
      // family with the count taken over one role's siblings.
      {"FirstOfType", ".row:first-of-type", row.firstOfType()},
      {"LastOfType", ".row:last-of-type", row.lastOfType()},
      {"OnlyOfType", ".row:only-of-type", row.onlyOfType()},
      {"NthOfType", ".row:nth-of-type(2n)", row.nthOfType(2, 0)},
      {"NthLastOfType", ".row:nth-last-of-type(3)", row.nthLastOfType(0, 3)},
      // A pseudo-class standing alone is `*` carrying it.
      {"ALoneRoot", ":root", select::any().root()},
      {"ALoneFirstChild", ":first-child", select::any().firstChild()},
      // CSS's filtered count: the position is taken over the siblings that
      // match the filter rather than over all of them.
      {"AFilteredCount", ".row:nth-child(2n+1 of .heavy)",
       row.nthChild(2, 1, select::styleClass("heavy"))},
      {"AFilteredCountFromTheEnd", ":nth-last-child(1 of heading)",
       select::any().nthLastChild(0, 1, select::role("heading"))},
  };
  forEachRow(readings, [](const ReadingRow& reading) {
    expectReads(reading.cssText, reading.typed);
  });
}

// A count is written as a number, as `an+b` with either sign and with or
// without the space around the sign, or as `odd` or `even`.
TEST(ComposeSelector, AnNthChildCountReadsEveryFormCssWrites) {
  struct CountRow {
    const char* name;
    int step;
    int offset;
  };
  const CountRow counts[] = {
      {"odd", 2, 1},   {"even", 2, 0},  {"3", 0, 3},    {"-3", 0, -3},
      {"n", 1, 0},     {"2n", 2, 0},    {"2n+1", 2, 1}, {"2n - 1", 2, -1},
      {"-n+3", -1, 3}, {"+2n+1", 2, 1},
  };
  forEachRow(counts, [](const CountRow& count) {
    EXPECT_TRUE(sigil::compose::selector(std::string(".row:nth-child(") +
                                         count.name + ")") ==
                select::styleClass("row").nthChild(count.step, count.offset));
  });
}

// CSS's own numbers, with the id column struck out: classes and
// pseudo-classes first, then the roles that stand for types. A combinator
// and the universal weigh nothing; `:is()`, `:not()` and `:has()` weigh as
// their heaviest argument, a relation weighing nothing; `:where()` weighs
// nothing; the filtered count adds its heaviest argument to its own class;
// a list reports its heaviest alternative.
TEST(ComposeSelector, ASelectorWeighsWhatCssCounts) {
  struct WeightRow {
    const char* name;
    std::string_view cssText;
    int classes;
    int roles;
  };
  const WeightRow weights[] = {
      {"TheUniversal", "*", 0, 0},
      {"ARole", "heading", 0, 1},
      {"AClass", ".card", 1, 0},
      {"TwoClassesInAChain", ".card .title", 2, 0},
      {"AClassAndARole", ".card > heading", 1, 1},
      {"TwoRoles", "heading + paragraph", 0, 2},
      {"AStructuralPseudoClass", ".row:nth-child(2n+1)", 2, 0},
      {"AnOfTypePseudoClass", "paragraph:first-of-type", 1, 1},
      {"EmptyOnTheUniversal", "*:empty", 1, 0},
      {"Root", ":root", 1, 0},
      {"CombinatorsAndTheUniversalWeighNothing", "heading > * ~ .title", 1, 1},
      {"AFilteredCountAddsItsArgument", ".x:nth-child(2 of .heavy.heavier)", 4,
       0},
      {"IsWeighsItsHeaviestArgument", ":is(.a, heading)", 1, 0},
      {"IsOverRolesWeighsOneRole", ":is(heading, paragraph)", 0, 1},
      {"NotWeighsItsHeaviestArgument", ":not(.a, heading)", 1, 0},
      // :where() is the one form that states a rule and weighs nothing, so
      // anything at all overrides what it sets.
      {"WhereWeighsNothing", ":where(.a, .b.c)", 0, 0},
      // The argument's own weight is counted whole, not one per name.
      {"AnArgumentWeighsWhole", ":is(.a.b.c, .d)", 3, 0},
      {"AListReportsItsHeaviestAlternative", ".a.b, heading", 2, 0},
      {"HasWeighsItsHeaviestArgument", ":has(.a, heading)", 1, 0},
      {"HasOverARoleWeighsOneRole", ":has(heading)", 0, 1},
      // The relation weighs nothing; every compound of the chain after it
      // counts, as it does in any chain.
      {"HasCountsTheChainAfterItsRelation", ".x:has(> .a .b)", 3, 0},
  };
  forEachRow(weights, [](const WeightRow& weight) {
    const Specificity read =
        sigil::compose::selector(weight.cssText).specificity();
    EXPECT_EQ(read.classes, weight.classes) << weight.cssText;
    EXPECT_EQ(read.roles, weight.roles) << weight.cssText;
  });
  // One class outweighs any number of roles, which is the whole point of
  // comparing the pair left to right.
  EXPECT_TRUE(sigil::compose::selector(".card").specificity() >
              sigil::compose::selector("a b c d").specificity());
}

TEST(ComposeSelector, TheSetAlgebraBuildsListsCompoundsAndNegations) {
  const ElementSelector a = select::styleClass("a");
  const ElementSelector b = select::styleClass("b");
  const ElementSelector c = select::styleClass("c");
  EXPECT_TRUE((a | b) == sigil::compose::selector(".a, .b"));
  EXPECT_TRUE((a & b) == sigil::compose::selector(".a.b"));
  EXPECT_TRUE(!a == sigil::compose::selector(":not(.a)"));
  EXPECT_TRUE(select::is(a | b) == sigil::compose::selector(":is(.a, .b)"));
  EXPECT_TRUE(select::where(a | b) ==
              sigil::compose::selector(":where(.a, .b)"));
  EXPECT_TRUE(select::notAnyOf(a | b) ==
              sigil::compose::selector(":not(.a, .b)"));
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

// A text this library does not read warns once, through the one reporting
// door, and yields a selector that matches nothing; a count too large to hold
// and lists nested past the ceiling are refused the same way.
TEST(ComposeSelector, ATextItDoesNotReadWarnsOnceAndMatchesNothing) {
  // Each of these warns once and yields a selector that never matches, so a
  // misprint loses one rule instead of the sheet around it.
  struct UnreadRow {
    const char* name;
    std::string_view cssText;
  };
  const UnreadRow unreadable[] = {
      {"Empty", ""},
      {"TwoDots", ".."},
      {"ACombinatorWithNothingAfterIt", ".a >"},
      {"AnUnknownPseudoClass", ":bogus"},
      {"APseudoElement", "::before"},
      {"AnEmptyIs", ":is()"},
      {"ALoneColon", ":"},
      {"ACountThatIsNoNumber", ":nth-child(x)"},
      {"ACountWithNothingAfterItsSign", ":nth-child(2n+)"},
      {"AFilterOnAnOfTypeCount", ":nth-of-type(2 of .a)"},
      {"AnIdentifier", ".a #identifier"},
      {"AnAttribute", "[data-x]"},
      {"AStrayClosingBracket", ".a)"},
      {"AnUnclosedBracket", ":is(.a"},
  };
  forEachRow(unreadable, [](const UnreadRow& unread) {
    EXPECT_TRUE(sigil::compose::selector(unread.cssText).matchesNothing())
        << unread.cssText;
  });
  checkRows({
      {"ADefaultBuiltSelectorIsTheSameNothing",
       [] {
         // It is what the algebra leaves standing rather than a half-built
         // value.
         EXPECT_TRUE(ElementSelector().matchesNothing());
         EXPECT_EQ(ElementSelector().specificity().classes, 0);
       }},
      {"ACountOrANestingTooLargeToReadIsAParseError",
       [] {
         // The an+b reader refuses a digit run it cannot hold rather than
         // wrapping around into a position nobody wrote.
         EXPECT_TRUE(sigil::compose::selector(".row:nth-child(9999999999)")
                         .matchesNothing());
         EXPECT_TRUE(sigil::compose::selector(".row:nth-child(2n+9999999999)")
                         .matchesNothing());
         EXPECT_TRUE(
             sigil::compose::selector(".row:nth-child(99999999999999999999n+1)")
                 .matchesNothing());
         // A count a child list could actually reach still reads.
         EXPECT_TRUE(sigil::compose::selector(".row:nth-child(1000)") ==
                     select::styleClass("row").nthChild(0, 1000));
         // Bracketed lists nested past the ceiling end as a parse error
         // rather than as the stack running out.
         std::string deep;
         for (int level = 0; level < 64; ++level) deep += ":is(";
         deep += ".card";
         for (int level = 0; level < 64; ++level) deep += ")";
         EXPECT_TRUE(sigil::compose::selector(deep).matchesNothing());
         // A nesting an author might actually write still reads.
         EXPECT_FALSE(
             sigil::compose::selector(":is(:is(:is(.card)))").matchesNothing());
         EXPECT_FALSE(
             sigil::compose::selector(".row:nth-child(2n of :is(.a, .b))")
                 .matchesNothing());
       }},
      {"ATextItCannotReadIsReportedThroughTheReportingDoor",
       [] {
         // A warning goes out through the one reporting door under the
         // library's own name: an installed reporter hears it, and nothing of
         // it reaches the standard error.
         std::vector<std::pair<std::string, std::string>> heard;
         sigil::core::Reporter outer = sigil::core::installReporter(
             [&heard](std::string_view library, std::string_view message) {
               heard.emplace_back(std::string(library), std::string(message));
             });
         testing::internal::CaptureStderr();
         const ElementSelector unread =
             sigil::compose::selector(".card >> ::reported-through-the-door");
         const std::string written = testing::internal::GetCapturedStderr();
         sigil::core::installReporter(std::move(outer));
         EXPECT_TRUE(unread.matchesNothing())
             << "a text it cannot read matches nothing";
         ASSERT_EQ(heard.size(), 1u)
             << "the reporter heard no warning, or several";
         EXPECT_EQ(heard.front().first, "SigilCompose");
         EXPECT_NE(
             heard.front().second.find("is not a selector this library reads"),
             std::string::npos)
             << heard.front().second;
         EXPECT_EQ(heard.front().second.find("[compose]"), std::string::npos)
             << "the door names the library; the message names it again: "
             << heard.front().second;
         EXPECT_TRUE(written.empty())
             << "the standard error heard: " << written;
       }},
  });
}

TEST(ComposeSelector, NothingIsTheZeroOfTheAlgebraAndNeverWidensARule) {
  const ElementSelector nothing = sigil::compose::selector("[data-x]");
  const ElementSelector card = select::styleClass("card");
  EXPECT_TRUE(nothing.matchesNothing());
  // Compounding with nothing is nothing from either side: a misprint
  // must lose its own rule, never hand that rule every element.
  EXPECT_TRUE((nothing & card).matchesNothing());
  EXPECT_TRUE((card & nothing).matchesNothing());
  // Qualifying nothing is nothing, rather than `*` carrying the
  // pseudo-class, and a chain onto nothing is nothing at either end.
  EXPECT_TRUE(nothing.firstChild().matchesNothing());
  EXPECT_TRUE(nothing.nthChild(2, 1).matchesNothing());
  EXPECT_TRUE(nothing.child(card).matchesNothing());
  EXPECT_TRUE(card.descendant(nothing).matchesNothing());
  // A count whose filter matches nothing counts nothing, rather than
  // widening into the unfiltered count over every sibling.
  EXPECT_TRUE(card.nthChild(2, 1, nothing).matchesNothing());
  EXPECT_FALSE(card.nthChild(2, 1, nothing) == card.nthChild(2, 1));
  // The list pseudo-classes carry the same zero through.
  EXPECT_TRUE((!nothing).matchesNothing());
  EXPECT_TRUE(select::is(nothing).matchesNothing());
  EXPECT_TRUE(select::where(nothing).matchesNothing());
  // A selector list is the one place nothing is the identity instead.
  EXPECT_TRUE((nothing | card) == card);
  EXPECT_TRUE((card | nothing) == card);
  // Erasing the `*` of a compound made only of them leaves the `*`,
  // so both doors agree that `*` on `*` is still `*`.
  EXPECT_TRUE((select::any() & select::any()) == select::any());
  EXPECT_TRUE(sigil::compose::selector("**") == select::any());
}

TEST(ComposeSelector, SelectorsAreValuesThatCompareByWhatTheySay) {
  const ElementSelector card = sigil::compose::selector(".card > .title");
  const ElementSelector same =
      select::styleClass("card").child(select::styleClass("title"));
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
