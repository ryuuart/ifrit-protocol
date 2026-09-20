// Readings that reserve: a reading opens the pitch above its base before
// the base is broken, stands over the whole compound it was selected
// from, and is shared by the pieces of a base that breaks across a line
// rather than renumbered onto them.

#include <sigilcompose/kit/Typeset.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "support/ParagraphTestSupport.h"

namespace {

/** Three sentences at a measure that breaks the second across two lines. */
std::u8string threeSentences() {
  return u8"Aa bb. Cc dd ee ff gg hh ii jj kk. Ll mm.";
}

/** The box the green ink stands in inside `region` — empty when none. */
SkIRect greenBoxIn(Host& host, SkIRect region) {
  SkIRect box = SkIRect::MakeEmpty();
  for (int y = region.top(); y < region.bottom(); ++y)
    for (int x = region.left(); x < region.right(); ++x)
      if (host.pixel(x, y) == SK_ColorGREEN)
        box.join(SkIRect::MakeXYWH(x, y, 1, 1));
  return box;
}

/** The band a reading stands in above `unit`: one line pitch deep and
 *  exactly as wide as the unit, so what a neighbouring unit carries is
 *  outside it. */
SkIRect bandAbove(const TextUnit& unit) {
  const int top = (int)unit.rect.top() - (int)std::ceil(unit.pitch);
  return SkIRect::MakeLTRB((int)unit.rect.left(), std::max(top, 0),
                           (int)std::ceil(unit.rect.right()),
                           (int)unit.rect.top());
}

/** ONE GROUP READING OVER `alpha beta`, read back as the pieces that
 *  selection was placed as and the green ink standing over each. The
 *  measure is the caller's so one passage answers both the unbroken base
 *  and the broken one. */
struct GroupReading {
  std::vector<SkRect> bases;
  std::vector<SkIRect> ink;
};

GroupReading groupReading(sigil::weave::Unit unit, std::u8string reading,
                          float measure) {
  Host host(400, 280);
  host.composer.render(box().children(
      {text(u8"alpha beta gamma", whiteStyle(16))
           .key("t")
           .absolute()
           .left(20.0f)
           .top(70.0f)
           .width(measure)
           .annotate(kit::ruby(sigil::weave::selectors::text(u8"alpha beta"),
                               unit, {std::move(reading)},
                               colouredType(9, SK_ColorGREEN), 2.0f))}));
  host.frame();
  GroupReading out;
  for (const TextUnit& piece :
       host.composer.units("t", sigil::weave::selectors::text(u8"alpha beta"),
                           sigil::weave::Unit::Selection)) {
    out.bases.push_back(piece.rect);
    out.ink.push_back(greenBoxIn(host, bandAbove(piece)));
  }
  return out;
}

}  // namespace

TEST(ComposeAnnotate, AReservingReadingOpensThePitchBeforeTheBaseIsBroken) {
  const auto pitchOf = [](Host& host) {
    const std::vector<TextUnit> lines = host.composer.units(
        "t", sigil::weave::selectors::each(sigil::weave::Unit::Line),
        sigil::weave::Unit::Line);
    return lines.empty() ? 0.0f : lines.front().pitch;
  };
  Host bare(400, 400);
  bare.composer.render(
      box().children({text(passage(), whiteStyle(16)).key("t").width(220.0f)}));
  bare.frame();

  Host read(400, 400);
  read.composer.render(box().children(
      {text(passage(), whiteStyle(16))
           .key("t")
           .width(220.0f)
           .annotate(kit::ruby(sigil::weave::selectors::text(u8"three"),
                               sigil::weave::Unit::Word, {u8"iii"},
                               {.size = 8.0f}, 1.0f))}));
  read.frame();

  EXPECT_GT(pitchOf(read), pitchOf(bare) + 4.0f);
}

TEST(ComposeAnnotate, AReadingThatReservesNothingLeavesThePitchAlone) {
  const auto pitchOf = [](Host& host) {
    const std::vector<TextUnit> lines = host.composer.units(
        "t", sigil::weave::selectors::each(sigil::weave::Unit::Line),
        sigil::weave::Unit::Line);
    return lines.empty() ? 0.0f : lines.front().pitch;
  };
  Host bare(400, 400);
  bare.composer.render(
      box().children({text(passage(), whiteStyle(16)).key("t").width(220.0f)}));
  bare.frame();

  Host marked(400, 400);
  marked.composer.render(box().children(
      {text(passage(), whiteStyle(16))
           .key("t")
           .width(220.0f)
           .annotate(kit::kenten(sigil::weave::selectors::text(u8"three"),
                                 {.size = 6.0f}, u8".", 1.0f))}));
  marked.frame();

  EXPECT_NEAR(pitchOf(marked), pitchOf(bare), 0.01f);
}

// `Annotation::reserve` IS THE FIELD, not the helper. `kit::ruby` and
// `kit::kenten` differ in several ways at once; this is ONE annotation with
// one bit flipped, so the pitch that opens is the flag and nothing else.
// The reading is placed either way — reserving is about the room the base
// is broken with, never about whether the reading is drawn.
TEST(ComposeAnnotate, ReserveIsWhatOpensTheBaseLineBox) {
  const auto pitchOf = [](Host& host) {
    const std::vector<TextUnit> lines = host.composer.units(
        "t", sigil::weave::selectors::each(sigil::weave::Unit::Line),
        sigil::weave::Unit::Line);
    return lines.empty() ? 0.0f : lines.front().pitch;
  };
  const auto reading = [](bool reserve) {
    return Annotation{.where = sigil::weave::selectors::text(u8"three"),
                      .unit = sigil::weave::Unit::Word,
                      .readings = {u8"iii"},
                      .style = {.size = 8.0f},
                      .side = Annotation::Side::Before,
                      .gap = 1.0f,
                      .reserve = reserve};
  };
  const auto pitchWith = [&](Host& host, bool reserve) {
    host.composer.render(box().children({text(passage(), whiteStyle(16))
                                             .key("t")
                                             .width(220.0f)
                                             .annotate(reading(reserve))}));
    host.frame();
    return pitchOf(host);
  };
  Host bare(400, 400);
  bare.composer.render(
      box().children({text(passage(), whiteStyle(16)).key("t").width(220.0f)}));
  bare.frame();

  Host over(400, 400), reserved(400, 400);
  EXPECT_NEAR(pitchWith(over, false), pitchOf(bare), 0.01f);
  EXPECT_GT(pitchWith(reserved, true), pitchOf(bare) + 4.0f);
  // …and the two annotations are not the same value, which is what keeps a
  // node carrying one from pruning against a node carrying the other.
  EXPECT_NE(reading(true), reading(false));
}

TEST(ComposeAnnotate, OneReadingStandsOverTheWholeSelectedCompound) {
  // GROUP RUBY: one reading over the compound the selector named. The
  // reading is narrower than the base and centred on it, because it reads
  // the whole of it once.
  const GroupReading grouped =
      groupReading(sigil::weave::Unit::Selection, u8"ab", 360.0f);
  ASSERT_EQ(grouped.bases.size(), 1u);
  ASSERT_FALSE(grouped.ink.front().isEmpty()) << "no reading was drawn";
  const SkRect base = grouped.bases.front();
  EXPECT_LT((float)grouped.ink.front().width(), base.width() * 0.5f);
  EXPECT_NEAR(
      (float)(grouped.ink.front().left() + grouped.ink.front().right()) / 2.0f,
      (base.left() + base.right()) / 2.0f, 3.0f);

  // Addressed by WORD, the same declaration is a reading over each word,
  // whose ink together spans most of the base — which is why a compound
  // the breaker is free to divide needs a unit of its own.
  const GroupReading perWord =
      groupReading(sigil::weave::Unit::Word, u8"ab", 360.0f);
  ASSERT_FALSE(perWord.ink.empty());
  ASSERT_FALSE(perWord.ink.front().isEmpty());
  EXPECT_GT((float)perWord.ink.front().width(), base.width() * 0.5f);
}

TEST(ComposeAnnotate, ASelectionBrokenAcrossLinesPartitionsItsOneReading) {
  const GroupReading whole =
      groupReading(sigil::weave::Unit::Selection, u8"mnmnmn", 360.0f);
  ASSERT_EQ(whole.bases.size(), 1u);
  ASSERT_FALSE(whole.ink.front().isEmpty());
  const int reading = whole.ink.front().width();

  // A measure that holds the first word and not the second breaks the
  // selection, which is then two pieces on two lines and still ONE base.
  const GroupReading broken =
      groupReading(sigil::weave::Unit::Selection, u8"mnmnmn", 56.0f);
  ASSERT_EQ(broken.bases.size(), 2u) << "the selection did not break";
  EXPECT_LT(broken.bases[0].bottom(), broken.bases[1].bottom());
  ASSERT_FALSE(broken.ink[0].isEmpty()) << "the first piece carries nothing";
  ASSERT_FALSE(broken.ink[1].isEmpty()) << "the second piece carries nothing";
  // The pieces SHARE the one reading. Repeating it on each would make both
  // of them as wide as the whole, and the two together twice as wide.
  EXPECT_LT(broken.ink[0].width(), reading);
  EXPECT_LT(broken.ink[1].width(), reading);
  const int shared = broken.ink[0].width() + broken.ink[1].width();
  EXPECT_GT(shared * 2, reading);
  EXPECT_LT(shared * 2, reading * 3);
}

TEST(ComposeAnnotate, AListOfOneStillReadsEveryBaseAlike) {
  // KENTEN'S SHAPE, which the share must not have cost: one mark, several
  // bases, a mark over each. A list of one is read over every base it
  // addresses — what it is never read twice over is ONE base.
  Host host(400, 220);
  host.composer.render(box().children(
      {text(u8"alpha beta", whiteStyle(24))
           .key("t")
           .absolute()
           .left(20.0f)
           .top(80.0f)
           .width(360.0f)
           .annotate(kit::ruby(sigil::weave::selectors::text(u8"alpha"),
                               sigil::weave::Unit::Cluster, {u8"o"},
                               colouredType(9, SK_ColorGREEN), 2.0f))}));
  host.frame();
  const std::vector<TextUnit> clusters =
      host.composer.units("t", sigil::weave::selectors::text(u8"alpha"),
                          sigil::weave::Unit::Cluster);
  ASSERT_EQ(clusters.size(), 5u);
  for (size_t index = 0; index < clusters.size(); ++index)
    EXPECT_FALSE(greenBoxIn(host, bandAbove(clusters[index])).isEmpty())
        << "cluster " << index << " of the base carries no mark";
  // …and nothing was read over the word the selector never named.
  const std::vector<TextUnit> after = host.composer.units(
      "t", sigil::weave::selectors::text(u8"beta"), sigil::weave::Unit::Word);
  ASSERT_EQ(after.size(), 1u);
  EXPECT_TRUE(greenBoxIn(host, bandAbove(after.front())).isEmpty());
}

TEST(ComposeAnnotate, ABrokenBaseSharesOneReadingAndShiftsNothingAfterIt) {
  // A base that breaks across a line is TWO placed units and ONE base: the
  // pieces share the reading in the proportion of their advances. The
  // readings therefore pair off with the BASES, not with the units the
  // layout produced — pair them with the units and the tail of a broken
  // base takes the next base's reading and every reading after it names
  // the wrong base, which shows up as the last base left bare.
  Host host(400, 400);
  const sigil::weave::Type reading = colouredType(9, SK_ColorGREEN);
  host.composer.render(box().children(
      {text(threeSentences(), whiteStyle(16))
           .key("t")
           .absolute()
           .left(20.0f)
           .top(40.0f)
           .width(150.0f)
           .annotate(kit::ruby(
               sigil::weave::selectors::each(sigil::weave::Unit::Sentence),
               sigil::weave::Unit::Sentence,
               {u8"one", u8"two two two", u8"three"}, reading, 2.0f))}));
  host.frame();

  const std::vector<TextUnit> units = host.composer.units(
      "t", sigil::weave::selectors::each(sigil::weave::Unit::Sentence),
      sigil::weave::Unit::Sentence);
  // The measure is chosen so a sentence breaks: three sentences report
  // more than three units, which is a base standing on two lines.
  ASSERT_GT(units.size(), 3u) << "no sentence broke across a line";

  // A reading stands in the band ABOVE its base, which the reservation
  // opened before the base was broken. EVERY piece of every base carries
  // one: the broken base's pieces share its reading, and the base after it
  // keeps its own — numbering the readings by the placed unit instead
  // leaves the last base bare.
  const auto readingOver = [&](const TextUnit& unit) {
    const int top = (int)unit.rect.top() - (int)std::ceil(unit.pitch);
    return anyGreenIn(
        host,
        SkIRect::MakeLTRB((int)unit.rect.left() - 4, std::max(top, 0),
                          (int)unit.rect.right() + 4, (int)unit.rect.top()));
  };
  for (size_t i = 0; i < units.size(); ++i)
    EXPECT_TRUE(readingOver(units[i]))
        << "unit " << i << " of " << units.size() << " carries no reading";
}
