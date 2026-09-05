/** @file
 * Selection as a value: what each `sel::` form records, what the
 * combinators build, how `take`/`drop` slice a granularity, and when two
 * selectors are equal — which is the whole contract, because resolving one
 * against glyphs is the caller's.
 */

#include <gtest/gtest.h>

#include <string>

#include "sigilweave/query/Selector.h"

using namespace sigil::weave;

TEST(Selector, DefaultAddressesEverything) {
  const Selector all;
  EXPECT_EQ(all.state(), nullptr) << "the everything selector carries no state";
  EXPECT_TRUE(all == Selector());
  EXPECT_FALSE(all == sel::word(0));
}

TEST(Selector, AbsoluteFormsRecordTheirBounds) {
  const Selector oneWord = sel::word(3);
  const Selector::State* word = oneWord.state();
  ASSERT_NE(word, nullptr);
  EXPECT_EQ(word->kind, Selector::Kind::Word);
  EXPECT_EQ(word->lo, 3u);
  EXPECT_EQ(word->hi, 4u) << "one word is the half-open range [i, i+1)";

  const Selector someWords = sel::words(2, 5);
  const Selector::State* words = someWords.state();
  ASSERT_NE(words, nullptr);
  EXPECT_EQ(words->kind, Selector::Kind::Words);
  EXPECT_EQ(words->lo, 2u);
  EXPECT_EQ(words->hi, 5u);

  // A single line and a run of lines are ONE kind: line(i) is lines(i, i+1),
  // so a resolver has one case to answer rather than two that must agree.
  const Selector oneLine = sel::line(1);
  EXPECT_EQ(oneLine.state()->kind, Selector::Kind::Line);
  EXPECT_TRUE(sel::line(1) == sel::lines(1, 2));

  const Selector chars = sel::range({4, 9});
  const Selector::State* range = chars.state();
  ASSERT_NE(range, nullptr);
  EXPECT_EQ(range->kind, Selector::Kind::Range);
  EXPECT_EQ(range->lo, 4u);
  EXPECT_EQ(range->hi, 9u);
}

TEST(Selector, PatternFormsCarryTheirNeedle) {
  const Selector literal = sel::text(u8"beta");
  const Selector::State* text = literal.state();
  ASSERT_NE(text, nullptr);
  EXPECT_EQ(text->kind, Selector::Kind::Text);
  EXPECT_TRUE(text->pattern == std::u8string(u8"beta"));

  const Selector pattern = sel::regex(u8"[0-9]+");
  const Selector::State* regex = pattern.state();
  ASSERT_NE(regex, nullptr);
  EXPECT_EQ(regex->kind, Selector::Kind::Regex);
  EXPECT_TRUE(regex->pattern == std::u8string(u8"[0-9]+"));
}

TEST(Selector, EachSlicesOneGranularity) {
  const Selector everyWord = sel::each(unit::Word);
  const Selector::State* plain = everyWord.state();
  ASSERT_NE(plain, nullptr);
  EXPECT_EQ(plain->kind, Selector::Kind::Each);
  EXPECT_EQ(plain->each, Unit::Word);
  EXPECT_EQ(plain->take, -1) << "unsliced keeps every glyph of every unit";
  EXPECT_EQ(plain->drop, 0);

  // take and drop partition a unit exactly: they are two edges of one cut,
  // so neither loses the other's setting.
  const Selector cut = sel::each(unit::Line).drop(2).take(3);
  const Selector::State* sliced = cut.state();
  ASSERT_NE(sliced, nullptr);
  EXPECT_EQ(sliced->each, Unit::Line);
  EXPECT_EQ(sliced->drop, 2);
  EXPECT_EQ(sliced->take, 3);
}

TEST(Selector, SlicingTheEverythingSelectorStartsFromADefaultState) {
  // .take() on a default-constructed selector must not dereference the null
  // state it carries.
  const Selector one = Selector().take(1);
  const Selector::State* s = one.state();
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(s->kind, Selector::Kind::All);
  EXPECT_EQ(s->take, 1);
}

TEST(Selector, CombinatorsNestTheirOperands) {
  const Selector both = sel::word(0) | sel::word(2);
  ASSERT_NE(both.state(), nullptr);
  EXPECT_EQ(both.state()->kind, Selector::Kind::Union);
  ASSERT_EQ(both.state()->operands.size(), 2u);
  EXPECT_TRUE(both.state()->operands[0] == sel::word(0));

  const Selector shared = sel::line(0) & sel::text(u8"x");
  EXPECT_EQ(shared.state()->kind, Selector::Kind::Intersect);
  EXPECT_EQ(shared.state()->operands.size(), 2u);

  const Selector rest = !sel::word(0);
  EXPECT_EQ(rest.state()->kind, Selector::Kind::Complement);
  ASSERT_EQ(rest.state()->operands.size(), 1u);
}

TEST(Selector, EqualityIsByState) {
  EXPECT_TRUE(sel::word(1) == sel::word(1));
  EXPECT_FALSE(sel::word(1) == sel::word(2));
  EXPECT_FALSE(sel::word(1) == sel::words(1, 2))
      << "a different kind is a different selector even at the same bounds";
  EXPECT_TRUE((sel::word(0) | sel::word(1)) == (sel::word(0) | sel::word(1)));
  EXPECT_FALSE((sel::word(0) | sel::word(1)) == (sel::word(1) | sel::word(0)))
      << "operands are compared in order";
  EXPECT_FALSE(Selector() == sel::each(unit::Glyph))
      << "everything and every-glyph are the same set and different values";
}

TEST(Selector, CallerDefinedFormsRideTheSameState) {
  // Named and Scope have no builder here: a caller spells its own form
  // through Selector::of and the combinators treat it like any other.
  const Selector named = Selector::of(
      {.kind = Selector::Kind::Named, .pattern = std::u8string(u8"accent")});
  const Selector scope = Selector::of(
      {.kind = Selector::Kind::Scope, .pattern = std::u8string(u8"b")});
  EXPECT_TRUE(named == Selector::of({.kind = Selector::Kind::Named,
                                     .pattern = std::u8string(u8"accent")}));
  EXPECT_FALSE(named == scope) << "one needle slot, two kinds";
  const Selector both = scope & sel::line(0);
  EXPECT_EQ(both.state()->kind, Selector::Kind::Intersect);
}
