/** @file
 * Mixed text as a value: what each `add` form records, what a run written
 * with a partial keeps of the base, how a name resolves through a style
 * sheet whichever order the two are written in, what a slot reserves, and
 * when two rich texts are equal — the question a caller asks before it
 * shapes anything.
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

TEST(RichText, NamesResolveThroughAStyleSheetInEitherOrder) {
  const TextStyle base = colored(SK_ColorWHITE);
  StyleSheet reds;
  reds.set("accent", Type{.color = SkColor4f{1, 0, 0, 1}});

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
  StyleSheet reds;
  reds.set("accent", Type{.color = SkColor4f{1, 0, 0, 1}});
  const RichText unknown = rich(base).add(u8"x", "nope").styles(reds);
  EXPECT_EQ(colorOf(unknown, 0), SK_ColorWHITE)
      << "a misspelled name is content set in the base, not content missing";
  const RichText unbound = rich(base).add(u8"x", "accent");
  EXPECT_EQ(colorOf(unbound, 0), SK_ColorWHITE) << "no sheet is no resolution";
  EXPECT_FALSE(unknown.runs()[0].over.has_value())
      << "a name that resolved to nothing carries no partial either";
}

TEST(RichText, AStyleSheetIsInPlayOnlyOnceGiven) {
  // What a host offering an ambient registry asks before it supplies one.
  const RichText none = rich(colored(SK_ColorWHITE)).add(u8"x", "accent");
  EXPECT_FALSE(none.hasStyles());
  StyleSheet reds;
  reds.set("accent", Type{.color = SkColor4f{1, 0, 0, 1}});
  RichText given = none;
  EXPECT_TRUE(given.styles(reds).hasStyles());
  EXPECT_EQ(colorOf(given, 0), SK_ColorRED)
      << "a sheet arriving late re-resolves the names already added";
  ASSERT_TRUE(given.runs()[0].over.has_value())
      << "and leaves the class's partial on the run it resolved";
  EXPECT_EQ(given.runs()[0].over->color, (SkColor4f{1, 0, 0, 1}));
  EXPECT_FALSE(given.runs()[0].total) << "a class is inherited, not whole";
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
  EXPECT_FALSE(rich(base).add(u8"a") == rich(colored(SK_ColorRED)).add(u8"a"));

  // The SHEET is not compared: two values that resolved to the same styles
  // describe the same passage however they got there.
  StyleSheet reds;
  reds.set("accent", Type{.color = SkColor4f{1, 0, 0, 1}});
  StyleSheet more = reds;
  more.set("unused", Type{.color = SkColor4f{0, 0, 1, 1}});
  EXPECT_TRUE(rich(base).add(u8"x", "accent").styles(reds) ==
              rich(base).add(u8"x", "accent").styles(more));
}

TEST(RichText, ARunWrittenWithAPartialKeepsTheBasesFaceAndSize) {
  TextStyle base;
  base.shaping.fontSize = 18.0f;
  base.shaping.letterSpacing = 1.5f;
  base.paint.foreground.setColor(SK_ColorWHITE);

  const RichText value =
      rich(base).add(u8"plain ").add(u8"loud", Type{.weight = 800.0f});
  const RichText::Run& loud = value.runs()[1];
  EXPECT_FLOAT_EQ(loud.style.shaping.fontSize, 18.0f)
      << "a size the partial did not name is the base's";
  EXPECT_FLOAT_EQ(loud.style.shaping.letterSpacing, 1.5f);
  EXPECT_EQ(loud.style.paint.foreground.getColor(), SK_ColorWHITE);
  ASSERT_EQ(loud.style.shaping.variations.size(), 1u);
  EXPECT_EQ(loud.style.shaping.variations[0], FontVariation("wght", 800));

  // The partial stays ON the run, which is what lets a host that supplies a
  // base the passage never had resolve the run against that base instead.
  ASSERT_TRUE(loud.over.has_value());
  EXPECT_EQ(loud.over->weight, 800.0f);
  EXPECT_FALSE(loud.total) << "a partial inherits, so it is not a whole style";
  EXPECT_FALSE(value.runs()[0].over.has_value())
      << "and a run written with nothing carries none";

  // A run written with a WHOLE style says so, which is how a consumer tells
  // it apart from the run that simply took the base.
  EXPECT_TRUE(rich(base).add(u8"own", colored(SK_ColorRED)).runs()[0].total);
}

TEST(RichText, HasBaseIsFalseForARichTextThatNamedNoBase) {
  const RichText unstated = rich().add(u8"inherit me");
  EXPECT_FALSE(unstated.hasBase())
      << "a passage that states no default waits to be given one";
  EXPECT_TRUE(rich(TextStyle{}).hasBase())
      << "an explicitly default base IS a named base";
  EXPECT_FALSE(unstated == rich(TextStyle{}).add(u8"inherit me"))
      << "waiting for a base is not the same value as having settled on one";
}

TEST(Unit, TheConstantsNameTheEnumerators) {
  EXPECT_EQ(Unit::Glyph, Unit::Glyph);
  EXPECT_EQ(Unit::Cluster, Unit::Cluster);
  EXPECT_EQ(Unit::Word, Unit::Word);
  EXPECT_EQ(Unit::Line, Unit::Line);
  EXPECT_EQ(Unit::Sentence, Unit::Sentence);
}
