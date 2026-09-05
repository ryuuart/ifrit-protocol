/** @file
 * Mixed text as a value: what each `add` form records, how a name resolves
 * through a style set whichever order the two are written in, what a slot
 * reserves, and when two rich texts are equal — the question a caller asks
 * before it shapes anything.
 */

#include <gtest/gtest.h>

#include <string>

#include "sigilweave/paragraph/RichText.h"
#include "sigilweave/paragraph/Unit.h"

using namespace sigil::weave;

namespace {

TextStyle colored(SkColor color) {
  TextStyle style;
  style.shaping.fontSize = 20;
  style.paint.foreground.setColor(color);
  return style;
}

SkColor colorOf(const RichText& value, size_t run) {
  return value.runs()[run].style.paint.foreground.getColor();
}

}  // namespace

TEST(RichText, RunsConcatenateInTheOrderTheyWereAdded) {
  const TextStyle base = colored(SK_ColorWHITE);
  const TextStyle accent = colored(SK_ColorRED);
  const RichText value =
      rich(base).add(u8"Signal ").add(u8"woven", accent).add(u8" through");
  ASSERT_EQ(value.runs().size(), 3u);
  EXPECT_TRUE(value.runs()[0].utf8 == std::u8string(u8"Signal "));
  EXPECT_EQ(colorOf(value, 0), SK_ColorWHITE) << "an unstyled run is the base";
  EXPECT_EQ(colorOf(value, 1), SK_ColorRED);
  EXPECT_TRUE(value.runs()[2].utf8 == std::u8string(u8" through"))
      << "nothing is inserted between runs — the spaces are the author's";
  EXPECT_FALSE(value.empty());
  EXPECT_TRUE(RichText().empty());
}

TEST(RichText, NamesResolveThroughAStyleSetInEitherOrder) {
  const TextStyle base = colored(SK_ColorWHITE);
  StyleSet reds;
  reds.set("accent", colored(SK_ColorRED));

  const RichText after = rich(base).add(u8"x", "accent").styles(reds);
  const RichText before = rich(base).styles(reds).add(u8"x", "accent");
  EXPECT_EQ(colorOf(after, 0), SK_ColorRED);
  EXPECT_EQ(colorOf(before, 0), SK_ColorRED);
  EXPECT_TRUE(after == before) << "the order the two are written in is not "
                                  "a difference in the finished value";
  EXPECT_EQ(after.runs()[0].styleName, "accent")
      << "the name it was written with stays on the run";
}

TEST(RichText, AnUnregisteredNameResolvesToTheBase) {
  const TextStyle base = colored(SK_ColorWHITE);
  StyleSet reds;
  reds.set("accent", colored(SK_ColorRED));
  const RichText unknown = rich(base).add(u8"x", "nope").styles(reds);
  EXPECT_EQ(colorOf(unknown, 0), SK_ColorWHITE)
      << "a misspelled name is content set in the base, not content missing";
  const RichText unbound = rich(base).add(u8"x", "accent");
  EXPECT_EQ(colorOf(unbound, 0), SK_ColorWHITE) << "no set is no resolution";
}

TEST(RichText, AStyleSetIsInPlayOnlyOnceGiven) {
  // What a host offering an ambient registry asks before it supplies one.
  const RichText none = rich(colored(SK_ColorWHITE)).add(u8"x", "accent");
  EXPECT_FALSE(none.hasStyles());
  StyleSet reds;
  reds.set("accent", colored(SK_ColorRED));
  RichText given = none;
  EXPECT_TRUE(given.styles(reds).hasStyles());
  EXPECT_EQ(colorOf(given, 0), SK_ColorRED)
      << "a set arriving late re-resolves the names already added";
}

TEST(RichText, ASlotIsOneCodePointOfContent) {
  const RichText value =
      rich(colored(SK_ColorWHITE)).add(u8"press ").slot("key", {28, 18}, 4);
  ASSERT_EQ(value.runs().size(), 2u);
  const RichText::Run& slot = value.runs()[1];
  EXPECT_EQ(slot.slotName, "key");
  EXPECT_EQ(slot.slotSize.width(), 28);
  EXPECT_EQ(slot.slotBaselineDrop, 4);
  EXPECT_TRUE(slot.utf8 == std::u8string(u8"￼"))
      << "one object-replacement character, so it counts as a cluster";
}

TEST(RichText, EqualityIsTheBaseTheRunsAndTheirResolvedStyles) {
  const TextStyle base = colored(SK_ColorWHITE);
  EXPECT_TRUE(rich(base).add(u8"a") == rich(base).add(u8"a"));
  EXPECT_FALSE(rich(base).add(u8"a") == rich(base).add(u8"b"));
  EXPECT_FALSE(rich(base).add(u8"a") ==
               rich(colored(SK_ColorRED)).add(u8"a"));

  // The SET is not compared: two values that resolved to the same styles
  // describe the same passage however they got there.
  StyleSet reds;
  reds.set("accent", colored(SK_ColorRED));
  StyleSet more = reds;
  more.set("unused", colored(SK_ColorBLUE));
  EXPECT_TRUE(rich(base).add(u8"x", "accent").styles(reds) ==
              rich(base).add(u8"x", "accent").styles(more));
}

TEST(Unit, TheConstantsNameTheEnumerators) {
  EXPECT_EQ(unit::Glyph, Unit::Glyph);
  EXPECT_EQ(unit::Cluster, Unit::Cluster);
  EXPECT_EQ(unit::Word, Unit::Word);
  EXPECT_EQ(unit::Line, Unit::Line);
  EXPECT_EQ(unit::Sentence, Unit::Sentence);
}
