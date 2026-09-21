// The units everything beside a text is placed from: what a selector
// addresses, how a selection that spans words or lines is partitioned,
// what an unknown key answers, and where a sibling annotation and an
// anchored object stand against the unit they were given.

#include <sigilcompose/kit/Annotations.h>

#include <vector>

#include "support/ParagraphTestSupport.h"

TEST(ComposeUnits, EveryUnitASelectorAddressesIsReportedOnce) {
  Host host(400, 300);
  host.composer.render(box().children(
      {text(u8"alpha beta gamma", whiteStyle(16)).key("t").width(360.0f)}));
  host.frame();
  const std::vector<TextUnit> words = host.composer.units(
      "t", sigil::weave::selectors::each(sigil::weave::Unit::Word),
      sigil::weave::Unit::Word);
  ASSERT_EQ(words.size(), 3u);
  // In draw order, left to right, each with its own rect and none of them
  // the union of the others — which is the whole difference from mark().
  EXPECT_LT(words[0].rect.right(), words[1].rect.left() + 1.0f);
  EXPECT_LT(words[1].rect.right(), words[2].rect.left() + 1.0f);
  for (const TextUnit& word : words) {
    EXPECT_GT(word.rect.width(), 0.0f);
    EXPECT_GT(word.ascent, 0.0f);
    EXPECT_GT(word.pitch, 0.0f);
    EXPECT_LT(word.range.start, word.range.end);
  }
  EXPECT_EQ(words[0].index, 0u);
  EXPECT_EQ(words[2].index, 2u);
}

TEST(ComposeUnits, AUnitReportsOnEveryLineItLandedOn) {
  // A word cannot break, so a LINE unit is the one that shows the rule: a
  // selector over a wrapped passage reports one entry per line, never one
  // rect spanning the break.
  Host host(300, 300);
  host.composer.render(
      box().children({text(passage(), whiteStyle(14)).key("t").width(160.0f)}));
  host.frame();
  const std::vector<TextUnit> lines = host.composer.units(
      "t", sigil::weave::selectors::each(sigil::weave::Unit::Line),
      sigil::weave::Unit::Line);
  ASSERT_GE(lines.size(), 2u);
  for (size_t i = 1; i < lines.size(); ++i) {
    EXPECT_GT(lines[i].axis, lines[i - 1].axis);
    EXPECT_EQ(lines[i].lineIndex, lines[i - 1].lineIndex + 1);
  }
}

TEST(ComposeUnits, ASelectionIsOneUnitHoweverManyWordsItCovers) {
  // THE SELECTION IS ITS OWN UNIT. The same address answers two units at
  // word granularity and one at selection granularity, because a
  // selection is the extent the caller named and the breaker's opinion
  // about where it may divide is not part of it.
  Host host(400, 300);
  host.composer.render(box().children(
      {text(u8"alpha beta gamma", whiteStyle(16)).key("t").width(360.0f)}));
  host.frame();
  const std::vector<TextUnit> words =
      host.composer.units("t", sigil::weave::selectors::text(u8"alpha beta"),
                          sigil::weave::Unit::Word);
  ASSERT_EQ(words.size(), 2u);
  const std::vector<TextUnit> whole =
      host.composer.units("t", sigil::weave::selectors::text(u8"alpha beta"),
                          sigil::weave::Unit::Selection);
  ASSERT_EQ(whole.size(), 1u);
  EXPECT_EQ(whole.front().range.start, words.front().range.start);
  EXPECT_EQ(whole.front().range.end, words.back().range.end);
  EXPECT_NEAR(whole.front().rect.left(), words.front().rect.left(), 0.5f);
  EXPECT_NEAR(whole.front().rect.right(), words.back().rect.right(), 0.5f);
  // It covers the word it does NOT address no more than the word units do.
  const std::vector<TextUnit> last =
      host.composer.units("t", sigil::weave::selectors::text(u8"gamma"),
                          sigil::weave::Unit::Selection);
  ASSERT_EQ(last.size(), 1u);
  EXPECT_GE(last.front().rect.left(), whole.front().rect.right());
}

TEST(ComposeUnits, EachOccurrenceOfASelectionIsItsOwnUnit) {
  // One unit per stretch the selector addressed: the same phrase twice
  // with other words between them is two units, in draw order, which is
  // what pairs a list of readings off with the bases it names.
  Host host(500, 300);
  host.composer.render(
      box().children({text(u8"alpha beta gamma alpha beta", whiteStyle(16))
                          .key("t")
                          .width(460.0f)}));
  host.frame();
  const std::vector<TextUnit> found =
      host.composer.units("t", sigil::weave::selectors::text(u8"alpha beta"),
                          sigil::weave::Unit::Selection);
  ASSERT_EQ(found.size(), 2u);
  EXPECT_LT(found[0].rect.right(), found[1].rect.left());
  EXPECT_LT(found[0].range.end, found[1].range.start);
  EXPECT_EQ(found[0].index, 0u);
  EXPECT_EQ(found[1].index, 1u);
}

TEST(ComposeUnits, OccurrencesThatTouchAreStillTheirOwnUnits) {
  // A MASK CARRIES NO IDENTITY. Two matches with nothing between them
  // leave one unbroken stretch of selected glyphs, and a lane read off the
  // mask alone would call that one unit — which drops the second of the
  // two readings a caller paired with them, and under-reports the units
  // everything beside the text is placed from.
  Host host(400, 300);
  host.composer.render(box().children(
      {text(u8"abab cd", whiteStyle(16)).key("t").width(360.0f)}));
  host.frame();
  const std::vector<TextUnit> matches =
      host.composer.units("t", sigil::weave::selectors::text(u8"ab"),
                          sigil::weave::Unit::Selection);
  ASSERT_EQ(matches.size(), 2u);
  EXPECT_EQ(matches[0].range.end, matches[1].range.start)
      << "the two matches were meant to touch";
  EXPECT_LT(matches[0].rect.right(), matches[1].rect.left() + 0.5f);
  EXPECT_EQ(matches[0].index, 0u);
  EXPECT_EQ(matches[1].index, 1u);
  // The same holds of a form that names several numbered units at once:
  // two words that touch across one space are two extents, not one.
  const std::vector<TextUnit> words = host.composer.units(
      "t", sigil::weave::selectors::words(0, 2), sigil::weave::Unit::Selection);
  ASSERT_EQ(words.size(), 2u);
  EXPECT_LT(words[0].rect.right(), words[1].rect.left() + 0.5f);
}

TEST(ComposeUnits, AnUnknownKeyAndAnEmptySelectionAnswerEmpty) {
  Host host(300, 200);
  host.composer.render(box().children(
      {text(u8"alpha beta", whiteStyle(16)).key("t").width(280.0f)}));
  host.frame();
  EXPECT_TRUE(
      host.composer
          .units("nope",
                 sigil::weave::selectors::each(sigil::weave::Unit::Word),
                 sigil::weave::Unit::Word)
          .empty());
  EXPECT_TRUE(host.composer
                  .units("t", sigil::weave::selectors::text(u8"omega"),
                         sigil::weave::Unit::Word)
                  .empty());
}

TEST(ComposeUnits, ASiblingAnnotationPlacesOneElementPerUnit) {
  Host host(400, 300);
  const auto describe = [&] {
    return box().children(
        {text(u8"alpha beta gamma", whiteStyle(16))
             .key("t")
             .absolute()
             .left(20.0f)
             .top(40.0f)
             .width(360.0f),
         kit::annotate(
             host.composer, "t",
             sigil::weave::selectors::each(sigil::weave::Unit::Word),
             sigil::weave::Unit::Word,
             {.side = kit::Beside::Side::After, .gap = 4.0f},
             [] { return box().width(6.0f).height(6.0f).fill(green()); })
             .absolute()
             .inset({.top = 0, .right = 0, .bottom = 0, .left = 0})});
  };
  host.composer.render(describe());
  host.frame();
  // The read-back is a DESCRIBE-time answer, so the first describe had no
  // layout to read: the second is the one that places anything.
  host.composer.render(describe());
  host.frame();
  const std::vector<TextUnit> words = host.composer.units(
      "t", sigil::weave::selectors::each(sigil::weave::Unit::Word),
      sigil::weave::Unit::Word);
  ASSERT_EQ(words.size(), 3u);
  for (const TextUnit& word : words) {
    const SkIRect under = SkIRect::MakeXYWH(
        (int)word.rect.left(), (int)(word.rect.bottom() + 4.0f), 6, 6);
    bool anyGreen = false;
    for (int y = under.top(); y < under.bottom(); ++y)
      for (int x = under.left(); x < under.right(); ++x)
        if (host.pixel(x, y) == SK_ColorGREEN) anyGreen = true;
    EXPECT_TRUE(anyGreen) << "no marker under the word at " << word.rect.left();
  }
}

TEST(ComposeUnits, AnAnchoredObjectStandsWhereTheOffsetPutsIt) {
  // The custom position: the object is tied to a WORD — it moves when the
  // text reflows — but neither sits in the line nor stands in a reserved
  // band. Here its x is measured from the FRAME's left edge and its y from
  // the word, which is the margin figure every page of print carries, and
  // is why the two references are named per axis.
  Host host(400, 300);
  const auto describe = [&](kit::Anchored anchored) {
    return box().children(
        {text(u8"alpha beta gamma", whiteStyle(16))
             .key("t")
             .absolute()
             .left(80.0f)
             .top(40.0f)
             .width(300.0f),
         kit::annotate(
             host.composer, "t", sigil::weave::selectors::text(u8"gamma"),
             sigil::weave::Unit::Word, anchored,
             [] { return box().width(6.0f).height(6.0f).fill(green()); })
             .absolute()
             .inset({.top = 0, .right = 0, .bottom = 0, .left = 0})});
  };
  const kit::Anchored fromFrame{.horizontal = kit::Anchored::From::Frame,
                                .offset = {-20.0f, 0.0f}};
  host.composer.render(describe(fromFrame));
  host.frame();
  // The read-back is a DESCRIBE-time answer, so the first describe had no
  // layout to read: the second is the one that places anything.
  host.composer.render(describe(fromFrame));
  host.frame();

  const std::vector<TextUnit> words = host.composer.units(
      "t", sigil::weave::selectors::text(u8"gamma"), sigil::weave::Unit::Word);
  ASSERT_EQ(words.size(), 1u);
  const SkRect& word = words.front().rect;
  const auto frame = host.composer.bounds("t");
  ASSERT_TRUE(frame.has_value());
  EXPECT_GT(word.left(), frame->left() + 40.0f)
      << "the third word must be well inside the frame for this to prove "
         "anything";
  // x from the frame, y from the word.
  EXPECT_TRUE(anyGreenIn(host, SkIRect::MakeXYWH((int)(frame->left() - 20.0f),
                                                 (int)word.top(), 6, 6)));
  // …and nothing where the word's own left edge would have put it.
  EXPECT_FALSE(anyGreenIn(host, SkIRect::MakeXYWH((int)(word.left() - 20.0f),
                                                  (int)word.top(), 6, 6)));
}
