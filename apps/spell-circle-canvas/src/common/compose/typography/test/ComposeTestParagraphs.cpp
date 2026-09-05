// The block controls, what stands beside a text, and what a decoration
// dresses: ParagraphStyle per block through the leaf's own verbs, the
// per-unit read-back everything beside a passage is placed from, readings
// that reserve room in the base's strut, a story filling a chain of
// frames, and a text leaf whose decorations dress its glyphs.

#include <sigilcompose/kit/Annotations.h>
#include <sigilcompose/kit/Typeset.h>

#include <array>
#include <string>
#include <vector>

#include "support/TextTestSupport.h"

namespace {

/** Any green pixel in a region — the probe a boundary assertion reads. */
bool anyGreenIn(Host& host, SkIRect region) {
  for (int y = region.top(); y < region.bottom(); ++y)
    for (int x = region.left(); x < region.right(); ++x)
      if (host.pixel(x, y) == SK_ColorGREEN) return true;
  return false;
}

/** A text style of a stated size in a stated colour — the probe a span
 *  restyle is read back with. */
sigil::weave::TextStyle colouredStyle(float size, SkColor colour) {
  sigil::weave::TextStyle style = whiteStyle(size);
  style.paint.foreground.setColor(colour);
  return style;
}

/** A passage long enough to wrap at the measures below. */
std::u8string passage() {
  return toU8(
      "One two three four five six seven eight nine ten eleven twelve.");
}

/** Two blocks, the second after a hard break. */
std::u8string twoBlocks() {
  return toU8(
      "First block runs on for several words so that it wraps here.\n"
      "Second block does the same and wraps as well over here.");
}

/** Distinct baselines of a keyed text node's placed lines, ascending. */
std::vector<float> baselinesOf(Host& host, const char* key) {
  std::vector<float> found;
  const std::vector<TextUnit> lines =
      host.composer.units(key, sel::each(unit::Line), unit::Line);
  for (const TextUnit& line : lines) found.push_back(line.axis);
  return found;
}

}  // namespace

// ── The block controls, through the leaf ─────────────────────────────────

TEST(ComposeParagraphs, ABlockStyleOpensThePitchTheLeafSetsIt) {
  Host plain(360, 300);
  plain.composer.render(
      box().child(text(passage(), whiteStyle(14)).key("t").width(Dim(200.0f))));
  plain.frame();
  const std::vector<float> tight = baselinesOf(plain, "t");

  Host led(360, 300);
  led.composer.render(box().child(text(passage(), whiteStyle(14))
                                      .key("t")
                                      .width(Dim(200.0f))
                                      .paragraph({.leading = sigil::weave::
                                                      Leading::multiple(2.0f)})));
  led.frame();
  const std::vector<float> loose = baselinesOf(led, "t");

  ASSERT_GE(tight.size(), 2u);
  ASSERT_EQ(tight.size(), loose.size());
  EXPECT_GT(loose[1] - loose[0], (tight[1] - tight[0]) * 1.6f);
}

TEST(ComposeParagraphs, OneEntryStylesTheFirstBlockAndLeavesTheRestPlain) {
  // A list shorter than the text's blocks is not an error and not a
  // repetition: every block past the end is set by the leaf's own
  // settings, which is what a heading over a body wants.
  Host host(400, 400);
  sigil::weave::ParagraphStyle heading;
  heading.alignment = sigil::weave::TextAlignment::kCenter;
  host.composer.render(box().child(text(twoBlocks(), whiteStyle(13))
                                       .key("t")
                                       .width(Dim(320.0f))
                                       .paragraphs({heading})));
  host.frame();
  const std::vector<TextUnit> lines =
      host.composer.units("t", sel::each(unit::Line), unit::Line);
  ASSERT_GE(lines.size(), 3u);
  // The first block's lines are centred and the last block's are not.
  EXPECT_GT(lines.front().rect.left(), 1.0f);
  EXPECT_LT(lines.back().rect.left(), 1.0f);
}

TEST(ComposeParagraphs, TheGapBetweenBlocksIsTheLargerOfTheTwo) {
  const auto renderWith = [](Host& host, float after, float before) {
    sigil::weave::ParagraphStyle first;
    first.spaceAfter = after;
    sigil::weave::ParagraphStyle second;
    second.spaceBefore = before;
    host.composer.render(box().child(text(twoBlocks(), whiteStyle(13))
                                         .key("t")
                                         .width(Dim(320.0f))
                                         .paragraphs({first, second})));
    host.frame();
  };
  Host bare(400, 500), spaced(400, 500);
  renderWith(bare, 0.0f, 0.0f);
  renderWith(spaced, 30.0f, 12.0f);
  const std::vector<float> tight = baselinesOf(bare, "t");
  const std::vector<float> apart = baselinesOf(spaced, "t");
  ASSERT_EQ(tight.size(), apart.size());
  ASSERT_GE(tight.size(), 3u);
  // Everything after the boundary has moved by 30, never by 42 or 12.
  const float moved = apart.back() - tight.back();
  EXPECT_NEAR(moved, 30.0f, 0.5f);
}

// ── Justification, read off the ink the lines actually reached ───────────

namespace {

/** A passage long enough, and awkward enough, that a narrow measure
 *  leaves the word gaps unable to fill every line on their own — which is
 *  what gives the letter pass anything to do. */
std::u8string tightPassage() {
  return toU8(
      "Justification spends interword gaps before letterspacing, and "
      "reaches for horizontal glyph-scaling last of all.");
}

/** Where each line's LAST GLYPH ends, ascending by line, for every line
 *  but the closing one — the closing line is not justified. The units'
 *  rects are glyph boxes, so this is where the ink stops and not where
 *  the pen did. */
std::vector<float> justifiedLineEnds(Host& host, const char* key) {
  std::vector<float> ends;
  const std::vector<TextUnit> lines =
      host.composer.units(key, sel::each(unit::Line), unit::Line);
  for (size_t i = 0; i + 1 < lines.size(); ++i)
    ends.push_back(lines[i].rect.right());
  return ends;
}

/** The passage set justified in a 130 px measure under @p just, and where
 *  each of its justified lines ended. */
std::vector<float> endsUnder(Host& host,
                             const sigil::weave::JustificationOptions& just) {
  host.composer.render(box().child(
      text(tightPassage(), whiteStyle(12))
          .key("t")
          .width(Dim(130.0f))
          .textAlign(sigil::weave::TextAlignment::kJustify)
          .lineBreak(sigil::weave::LineBreakStrategy::kKnuthPlass)
          .justification(just)));
  host.frame();
  return justifiedLineEnds(host, "t");
}

}  // namespace

TEST(ComposeJustification, EveryPassRunningOutOfRoomLeavesTheGapsToFillTheLine) {
  // THE BOUND ON THE GAPS RESTS ON ONE CLAIM: what they may not take, a
  // later pass takes. A pass standing at its own limit does not take it,
  // and holding the gaps at their limit anyway leaves a hole at the right
  // margin that nothing in the line is allowed to close — which is the
  // one thing the bound exists to prevent. Both of these settings run
  // their later pass out of room: the letter pass may add half again what
  // it was asked for, and the glyph pass may only scale back up to one.
  sigil::weave::JustificationOptions letters;
  letters.letterSpacing = 0.05f;
  letters.letterSpacingMaximum = 0.1f;
  sigil::weave::JustificationOptions glyphs;
  glyphs.glyphScale = 0.92f;
  glyphs.glyphScaleMinimum = 0.92f;

  for (const auto& [what, spec] :
       {std::pair{"the letter pass", letters}, std::pair{"the glyph pass",
                                                         glyphs}}) {
    Host host(400, 400);
    const std::vector<float> ends = endsUnder(host, spec);
    ASSERT_GE(ends.size(), 3u) << what;
    for (size_t index = 0; index < ends.size(); ++index)
      EXPECT_NEAR(ends[index], 130.0f, 1.0f) << what << ", line " << index;
  }
}

// ── The units everything beside a text is placed from ─────────────────────

TEST(ComposeUnits, EveryUnitASelectorAddressesIsReportedOnce) {
  Host host(400, 300);
  host.composer.render(box().child(
      text(toU8("alpha beta gamma"), whiteStyle(16)).key("t").width(
          Dim(360.0f))));
  host.frame();
  const std::vector<TextUnit> words =
      host.composer.units("t", sel::each(unit::Word), unit::Word);
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
  host.composer.render(box().child(
      text(passage(), whiteStyle(14)).key("t").width(Dim(160.0f))));
  host.frame();
  const std::vector<TextUnit> lines =
      host.composer.units("t", sel::each(unit::Line), unit::Line);
  ASSERT_GE(lines.size(), 2u);
  for (size_t i = 1; i < lines.size(); ++i) {
    EXPECT_GT(lines[i].axis, lines[i - 1].axis);
    EXPECT_EQ(lines[i].lineIndex, lines[i - 1].lineIndex + 1);
  }
}

TEST(ComposeUnits, AnUnknownKeyAndAnEmptySelectionAnswerEmpty) {
  Host host(300, 200);
  host.composer.render(box().child(
      text(toU8("alpha beta"), whiteStyle(16)).key("t").width(Dim(280.0f))));
  host.frame();
  EXPECT_TRUE(host.composer.units("nope", sel::each(unit::Word), unit::Word)
                  .empty());
  EXPECT_TRUE(
      host.composer.units("t", sel::text(toU8("omega")), unit::Word).empty());
}

TEST(ComposeUnits, ASiblingAnnotationPlacesOneElementPerUnit) {
  Host host(400, 300);
  const auto describe = [&] {
    return box()
        .child(text(toU8("alpha beta gamma"), whiteStyle(16))
                   .key("t")
                   .absolute()
                   .left(Dim(20.0f))
                   .top(Dim(40.0f))
                   .width(Dim(360.0f)))
        .child(kit::annotate(host.composer, "t", sel::each(unit::Word),
                             unit::Word,
                             {.side = kit::Beside::Side::After, .gap = 4.0f},
                             [](const TextUnit&) {
                               return box().width(Dim(6.0f)).height(Dim(6.0f)).fill(
                                   green());
                             })
                   .absolute()
                   .inset(0, 0, 0, 0));
  };
  host.composer.render(describe());
  host.frame();
  // The read-back is a DESCRIBE-time answer, so the first describe had no
  // layout to read: the second is the one that places anything.
  host.composer.render(describe());
  host.frame();
  const std::vector<TextUnit> words =
      host.composer.units("t", sel::each(unit::Word), unit::Word);
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

// ── An object anchored to a text position, at a stated offset ────────────

TEST(ComposeUnits, AnAnchoredObjectStandsWhereTheOffsetPutsIt) {
  // The custom position: the object is tied to a WORD — it moves when the
  // text reflows — but neither sits in the line nor stands in a reserved
  // band. Here its x is measured from the FRAME's left edge and its y from
  // the word, which is the margin figure every page of print carries, and
  // is why the two references are named per axis.
  Host host(400, 300);
  const auto describe = [&](kit::Anchored anchored) {
    return box()
        .child(text(toU8("alpha beta gamma"), whiteStyle(16))
                   .key("t")
                   .absolute()
                   .left(Dim(80.0f))
                   .top(Dim(40.0f))
                   .width(Dim(300.0f)))
        .child(kit::annotate(host.composer, "t", sel::text(toU8("gamma")),
                             unit::Word, anchored,
                             [](const TextUnit&) {
                               return box()
                                   .width(Dim(6.0f))
                                   .height(Dim(6.0f))
                                   .fill(green());
                             })
                   .absolute()
                   .inset(0, 0, 0, 0));
  };
  const kit::Anchored fromFrame{.horizontal = kit::Anchored::From::Frame,
                                .offset = {-20.0f, 0.0f}};
  host.composer.render(describe(fromFrame));
  host.frame();
  // The read-back is a DESCRIBE-time answer, so the first describe had no
  // layout to read: the second is the one that places anything.
  host.composer.render(describe(fromFrame));
  host.frame();

  const std::vector<TextUnit> words =
      host.composer.units("t", sel::text(toU8("gamma")), unit::Word);
  ASSERT_EQ(words.size(), 1u);
  const SkRect& word = words.front().rect;
  const auto frame = host.composer.bounds("t");
  ASSERT_TRUE(frame.has_value());
  EXPECT_GT(word.left(), frame->left() + 40.0f)
      << "the third word must be well inside the frame for this to prove "
         "anything";
  // x from the frame, y from the word.
  EXPECT_TRUE(anyGreenIn(
      host, SkIRect::MakeXYWH((int)(frame->left() - 20.0f), (int)word.top(), 6,
                              6)));
  // …and nothing where the word's own left edge would have put it.
  EXPECT_FALSE(anyGreenIn(
      host,
      SkIRect::MakeXYWH((int)(word.left() - 20.0f), (int)word.top(), 6, 6)));
}

// ── A nested style: the opening of a block, set differently ──────────────

TEST(ComposeTypeset, ANestedStyleCoversTheWordsItCountsAndStops) {
  // The mechanism is a selector and a span restyle, so the run stops where
  // the TEXT says rather than where a pixel count says: three words in,
  // whatever those words are and wherever they break.
  Host host(400, 300);
  const kit::NestedStyle opening{.until = kit::NestedStyle::Until::Words,
                                 .count = 3,
                                 .style = colouredStyle(16, SK_ColorGREEN)};
  host.composer.render(
      box().child(text(toU8("alpha beta gamma delta epsilon"), whiteStyle(16))
                      .key("t")
                      .absolute()
                      .left(Dim(20.0f))
                      .top(Dim(40.0f))
                      .width(Dim(360.0f))
                      .spanStyle(kit::nestedRun(opening), opening.style)));
  host.frame();
  const std::vector<TextUnit> words =
      host.composer.units("t", sel::each(unit::Word), unit::Word);
  ASSERT_EQ(words.size(), 5u);
  for (size_t index = 0; index < words.size(); ++index) {
    const SkRect& word = words[index].rect;
    const SkIRect box = SkIRect::MakeLTRB((int)word.left(), (int)word.top(),
                                          (int)word.right() + 1,
                                          (int)word.bottom() + 1);
    EXPECT_EQ(anyGreenIn(host, box), index < 3u)
        << "word " << index << " is on the wrong side of the nested run";
  }
}

TEST(ComposeTypeset, ANestedRunEndsOnItsDelimiterAndIncludesIt) {
  // The spelling that counts nothing: a lead-in that ends at a mark. The
  // mark is quoted into the pattern rather than pasted into it, so a
  // delimiter that is also a regular-expression operator means itself.
  Host host(400, 300);
  const kit::NestedStyle lead{.until = kit::NestedStyle::Until::Delimiter,
                              .delimiter = toU8("."),
                              .style = colouredStyle(16, SK_ColorGREEN)};
  host.composer.render(
      box().child(text(toU8("alpha beta. gamma delta"), whiteStyle(16))
                      .key("t")
                      .absolute()
                      .left(Dim(20.0f))
                      .top(Dim(40.0f))
                      .width(Dim(360.0f))
                      .spanStyle(kit::nestedRun(lead), lead.style)));
  host.frame();
  const std::vector<TextUnit> words =
      host.composer.units("t", sel::each(unit::Word), unit::Word);
  ASSERT_GE(words.size(), 4u);
  const auto greenAt = [&](size_t index) {
    const SkRect& word = words[index].rect;
    return anyGreenIn(host, SkIRect::MakeLTRB((int)word.left(), (int)word.top(),
                                              (int)word.right() + 1,
                                              (int)word.bottom() + 1));
  };
  EXPECT_TRUE(greenAt(0));
  EXPECT_TRUE(greenAt(1))
      << "the word carrying the delimiter is inside the run";
  EXPECT_FALSE(greenAt(2)) << "the run ends AT the mark, not after it";
  // A delimiter that never occurs covers nothing, rather than everything.
  Host missing(400, 300);
  const kit::NestedStyle absent{.until = kit::NestedStyle::Until::Delimiter,
                                .delimiter = toU8("§"),
                                .style = colouredStyle(16, SK_ColorGREEN)};
  missing.composer.render(
      box().child(text(toU8("alpha beta. gamma delta"), whiteStyle(16))
                      .key("t")
                      .absolute()
                      .left(Dim(20.0f))
                      .top(Dim(40.0f))
                      .width(Dim(360.0f))
                      .spanStyle(kit::nestedRun(absent), absent.style)));
  missing.frame();
  EXPECT_FALSE(anyGreenIn(missing, SkIRect::MakeXYWH(0, 0, 400, 300)));
}

TEST(ComposeTypeset, ADropCapCarriesANestedOpeningIntoItsBody) {
  // The case the two pieces exist for: an initial, and the words after it
  // set in a style of their own. The cap is its own leaf in its own type,
  // so the nested run is stated over the body that flows around it.
  Host host(400, 300);
  const kit::NestedStyle opening{.until = kit::NestedStyle::Until::Words,
                                 .count = 2,
                                 .style = colouredStyle(16, SK_ColorGREEN)};
  auto [initial, body] =
      kit::dropCap(toU8("W"), whiteStyle(48), toU8("hale alpha beta gamma"),
                   whiteStyle(16), "dropcap", 6.0f, opening);
  host.composer.render(box().child(
      box()
          .absolute()
          .left(Dim(20.0f))
          .top(Dim(40.0f))
          .width(Dim(340.0f))
          .height(Dim(200.0f))
          .child(std::move(initial))
          .child(std::move(body).key("body").width(Dim(240.0f)))));
  host.frame();
  const std::vector<TextUnit> words =
      host.composer.units("body", sel::each(unit::Word), unit::Word);
  ASSERT_GE(words.size(), 4u);
  const auto greenAt = [&](size_t index) {
    const SkRect& word = words[index].rect;
    return anyGreenIn(host, SkIRect::MakeLTRB((int)word.left(), (int)word.top(),
                                              (int)word.right() + 1,
                                              (int)word.bottom() + 1));
  };
  EXPECT_TRUE(greenAt(0));
  EXPECT_TRUE(greenAt(1));
  EXPECT_FALSE(greenAt(2)) << "the nested run counted two words, not three";
}

// ── Readings that reserve ─────────────────────────────────────────────────

TEST(ComposeAnnotate, AReservingReadingOpensThePitchBeforeTheBaseIsBroken) {
  const auto pitchOf = [](Host& host) {
    const std::vector<TextUnit> lines =
        host.composer.units("t", sel::each(unit::Line), unit::Line);
    return lines.empty() ? 0.0f : lines.front().pitch;
  };
  Host bare(400, 400);
  bare.composer.render(box().child(
      text(passage(), whiteStyle(16)).key("t").width(Dim(220.0f))));
  bare.frame();

  Host read(400, 400);
  read.composer.render(
      box().child(text(passage(), whiteStyle(16))
                      .key("t")
                      .width(Dim(220.0f))
                      .annotate(kit::ruby(sel::text(toU8("three")), unit::Word,
                                          {toU8("iii")}, whiteStyle(8), 1.0f))));
  read.frame();

  EXPECT_GT(pitchOf(read), pitchOf(bare) + 4.0f);
}

TEST(ComposeAnnotate, AReadingThatReservesNothingLeavesThePitchAlone) {
  const auto pitchOf = [](Host& host) {
    const std::vector<TextUnit> lines =
        host.composer.units("t", sel::each(unit::Line), unit::Line);
    return lines.empty() ? 0.0f : lines.front().pitch;
  };
  Host bare(400, 400);
  bare.composer.render(box().child(
      text(passage(), whiteStyle(16)).key("t").width(Dim(220.0f))));
  bare.frame();

  Host marked(400, 400);
  marked.composer.render(
      box().child(text(passage(), whiteStyle(16))
                      .key("t")
                      .width(Dim(220.0f))
                      .annotate(kit::kenten(sel::text(toU8("three")),
                                            whiteStyle(6), toU8("."), 1.0f))));
  marked.frame();

  EXPECT_NEAR(pitchOf(marked), pitchOf(bare), 0.01f);
}

// ── A story through a chain of frames ────────────────────────────────────

TEST(ComposeStory, EachFrameFillsFromWhereTheOneBeforeItStopped) {
  Story article(rich(whiteStyle(13)).add(passage()).add(toU8(" ")).add(
      passage()));
  Host host(500, 300);
  host.composer.render(
      box()
          .row()
          .child(frame(article).key("a").thread("b").width(Dim(160.0f)).height(
              Dim(60.0f)))
          .child(frame(article).key("b").width(Dim(160.0f)).height(Dim(200.0f))));
  host.frame();
  const std::vector<TextUnit> first =
      host.composer.units("a", sel::each(unit::Word), unit::Word);
  const std::vector<TextUnit> second =
      host.composer.units("b", sel::each(unit::Word), unit::Word);
  // The first frame ran out of room, which is the normal case for every
  // frame of a chain but the last.
  const sigil::weave::ParagraphLayout* head = host.composer.paragraphLayout("a");
  ASSERT_NE(head, nullptr);
  EXPECT_TRUE(head->overflowed());
  ASSERT_FALSE(first.empty());
  ASSERT_FALSE(second.empty());
  // The chain does not repeat itself: the second frame begins past where
  // the first stopped, in the story's own text.
  EXPECT_GE(second.front().range.start, first.back().range.end);
}

TEST(ComposeStory, ANarrowerFirstFrameMovesTheCut) {
  const auto cutAt = [](float measure) {
    Story article(rich(whiteStyle(13)).add(passage()).add(toU8(" ")).add(
        passage()));
    Host host(500, 300);
    host.composer.render(
        box()
            .row()
            .child(frame(article).key("a").thread("b").width(Dim(measure)).height(
                Dim(60.0f)))
            .child(
                frame(article).key("b").width(Dim(160.0f)).height(Dim(200.0f))));
    host.frame();
    const std::vector<TextUnit> second =
        host.composer.units("b", sel::each(unit::Word), unit::Word);
    return second.empty() ? ~0u : second.front().range.start;
  };
  EXPECT_LT(cutAt(120.0f), cutAt(220.0f));
}

TEST(ComposeStory, TheMarkerEndsTheChainAndNoCutInsideIt) {
  // The last column of a chain threads nowhere, so what it cannot hold
  // has nowhere to go: without a marker it simply draws past its box. The
  // marker ends the story there and says it goes on. The columns before
  // it must NOT take one — a mark at every cut reads as three texts
  // rather than one threaded through three frames.
  const auto chainOf = [](Host& host, std::u8string marker) {
    Story article(rich(whiteStyle(13)).add(passage()).add(toU8(" ")).add(
        passage()));
    host.composer.render(box().child(
        kit::columns(article, 3, 12.0f, 240.0f, 32.0f, "col",
                     std::move(marker))));
    host.frame();
    return std::array{host.composer.paragraphLayout("col0"),
                      host.composer.paragraphLayout("col1"),
                      host.composer.paragraphLayout("col2")};
  };
  Host bareHost(500, 300), markedHost(500, 300);
  const auto bare = chainOf(bareHost, {});
  const auto marked = chainOf(markedHost, u8"\u2026");
  for (const auto* column : bare) ASSERT_NE(column, nullptr);
  for (const auto* column : marked) ASSERT_NE(column, nullptr);

  EXPECT_TRUE(marked[2]->overflowed()) << "the chain must actually run out";
  EXPECT_TRUE(marked[2]->ellipsized) << "the marker never landed";
  EXPECT_FALSE(marked[0]->ellipsized) << "a cut inside the chain is silent";
  EXPECT_FALSE(marked[1]->ellipsized) << "a cut inside the chain is silent";
  EXPECT_FALSE(bare[2]->ellipsized) << "no marker asked for, none drawn";
}

// ── What a decoration dresses ────────────────────────────────────────────

TEST(ComposeBoundary, GlyphsHandTheDecorationsTheLettersInsteadOfTheBox) {
  // A decoration that fills the outline it is handed: on the default
  // boundary it covers the node's whole box, and on the glyph boundary it
  // covers only where letters are — so a point inside the box but between
  // two lines is painted by one and not by the other.
  const auto describe = [](Boundary boundary) {
    Element leaf = text(toU8("HH HH"), whiteStyle(40))
                       .key("t")
                       .absolute()
                       .left(Dim(10.0f))
                       .top(Dim(10.0f))
                       .width(Dim(240.0f))
                       .foreground(Decoration(PaintProgram(
                           [](SkCanvas& canvas, const PaintContext& ctx) {
                             SkPaint paint;
                             paint.setColor(SK_ColorGREEN);
                             paint.setAntiAlias(false);
                             canvas.drawPath(ctx.outline, paint);
                           })));
    if (boundary != Boundary::Auto) leaf.boundary(boundary);
    return box().child(std::move(leaf));
  };
  Host boxed(300, 200), lettered(300, 200);
  boxed.composer.render(describe(Boundary::Auto));
  boxed.frame();
  lettered.composer.render(describe(Boundary::Glyphs));
  lettered.frame();

  // The node's top-left corner: inside its box, and above every letter.
  EXPECT_EQ(boxed.pixel(12, 12), SK_ColorGREEN);
  EXPECT_NE(lettered.pixel(12, 12), SK_ColorGREEN);
  // …and the letters themselves are painted under both.
  EXPECT_TRUE(anyGreenIn(lettered, SkIRect::MakeXYWH(10, 10, 240, 60)));
}

// ── Live text: what a moving passage costs, frame by frame ───────────────

namespace {

/** A passage long enough that a frame of it is worth composing — past the
 *  stride the optimizing breaker reads its budget on, so a starved block
 *  can notice it has run out. */
std::u8string longPassage() {
  std::u8string out;
  for (int i = 0; i < 30; ++i) out += passage() + toU8(" ");
  return out;
}

/** One frame of a live passage at `measure`, reporting what it cost. */
TextSettling settlingAt(Host& host, float measure, float budget) {
  host.composer.render(
      box().child(text(longPassage(), whiteStyle(13))
                      .key("t")
                      .width(Dim(measure))
                      .lineBreak(sigil::weave::LineBreakStrategy::kKnuthPlass)
                      .live(true, budget)));
  host.frame();
  return host.composer.settling("t");
}

}  // namespace

TEST(ComposeLiveText, ABoundMeasureRelaysEveryFrameWithoutGrowingTheTree) {
  Host host(600, 500);
  // The width sweeps, so every frame is a layout the frame before it did
  // not answer — which is what a bound measure is.
  const std::vector<float> sweep = {320, 330, 340, 350, 360, 370, 380};
  std::vector<int> lineCounts;
  for (const float measure : sweep) {
    settlingAt(host, measure, 0.0f);
    const sigil::weave::ParagraphLayout* layout =
        host.composer.paragraphLayout("t");
    ASSERT_NE(layout, nullptr);
    lineCounts.push_back(layout->lineCount);
  }
  // It re-laid: a wider measure took fewer lines than the narrowest.
  EXPECT_GT(lineCounts.front(), lineCounts.back());

  // …AND THE TREE DOES NOT GROW WHILE IT DOES. A frame of a moving passage
  // re-decides breaks and re-fills; what it must not do is leave anything
  // behind. Compared between two runs of the same sweep rather than
  // against the first frame, because the first run of a sweep is also the
  // one that warms the caches a settled measure is then answered from.
  for (int pass = 0; pass < 3; ++pass)
    for (const float measure : sweep) settlingAt(host, measure, 0.0f);
  const Composer::Stats warm = host.composer.stats();
  for (int pass = 0; pass < 3; ++pass)
    for (const float measure : sweep) settlingAt(host, measure, 0.0f);
  const Composer::Stats after = host.composer.stats();
  EXPECT_EQ(after.instances, warm.instances);
  EXPECT_EQ(after.picturesLive, warm.picturesLive);
  EXPECT_EQ(after.texturesLive, warm.texturesLive);

  // …and a measure it has already crossed costs no break decision at all:
  // the block is answered from decisions this thread already has.
  const TextSettling seen = settlingAt(host, sweep.front(), 0.0f);
  EXPECT_TRUE(seen.live);
  EXPECT_GT(seen.reused, 0);
  EXPECT_EQ(seen.degraded, 0);
}

TEST(ComposeLiveText, ASettledTextDecidesItsBreaksOnceAndThenComposesNothing) {
  Host host(600, 500);
  const Element leaf =
      box().child(text(longPassage(), whiteStyle(13))
                      .key("t")
                      .width(Dim(340.0f))
                      .lineBreak(sigil::weave::LineBreakStrategy::kKnuthPlass));
  host.composer.render(leaf);
  host.frame();
  // A settled passage never asks the break store — it is not one of a run
  // of layouts, so there is nothing for a later frame to reuse.
  const TextSettling first = host.composer.settling("t");
  EXPECT_FALSE(first.live);
  EXPECT_EQ(first.reused, 0);
  EXPECT_EQ(first.degraded, 0);

  // …and the frames after it compose nothing at all: the layout is valid
  // for the measure it was asked at, so no dynamic program runs and the
  // node's recording stands.
  for (int i = 0; i < 3; ++i) {
    host.composer.render(leaf);
    host.frame();
  }
  const Composer::Stats settled = host.composer.stats();
  EXPECT_EQ(settled.picturesRecorded, 0u);
  EXPECT_EQ(host.composer.settling("t").reused, 0);
  EXPECT_EQ(host.composer.settling("t").degraded, 0);
}

TEST(ComposeLiveText, ADegradedFrameIsProvisionalAndTheSettingComesBack) {
  Host host(600, 500);
  // A budget no composer can meet: the block is filled greedily for that
  // frame and says so.
  const TextSettling starved = settlingAt(host, 340.0f, 1.0f);
  EXPECT_TRUE(starved.live);
  EXPECT_GT(starved.degraded, 0);
  // …and a budget it can meet gets the setting back, at the same measure,
  // because a degrade never held the layout as the answer for it.
  const TextSettling fed = settlingAt(host, 340.0f, 1.0e6f);
  EXPECT_EQ(fed.degraded, 0);
}

// ── A story numbers its own lines ────────────────────────────────────────

namespace {

/** A chain of two frames over one story, drawn once. */
void twoFrames(Host& host, float measure = 160.0f) {
  Story article(rich(whiteStyle(13)).add(longPassage()));
  host.composer.render(
      box()
          .row()
          .child(frame(article)
                     .key("a")
                     .thread("b")
                     .width(Dim(measure))
                     .height(Dim(70.0f)))
          .child(
              frame(article).key("b").width(Dim(measure)).height(Dim(400.0f))));
  host.frame();
}

}  // namespace

TEST(ComposeStory, LinesAreNumberedFromTheStoryAndNotFromTheFrame) {
  Host host(600, 500);
  twoFrames(host);
  const sigil::weave::ParagraphLayout* head =
      host.composer.paragraphLayout("a");
  ASSERT_NE(head, nullptr);
  const int headLines = head->lineCount;
  ASSERT_GT(headLines, 1);

  // A line the FIRST frame holds is addressed on the first frame and
  // nowhere else…
  EXPECT_FALSE(host.composer.units("a", sel::line(0), unit::Line).empty());
  EXPECT_TRUE(host.composer.units("b", sel::line(0), unit::Line).empty());
  // …and the line just past it is the second frame's first line, addressed
  // by its number in the STORY rather than by its number in the frame.
  EXPECT_TRUE(
      host.composer.units("a", sel::line((uint32_t)headLines), unit::Line)
          .empty());
  EXPECT_FALSE(
      host.composer.units("b", sel::line((uint32_t)headLines), unit::Line)
          .empty());
}

TEST(ComposeStory, InFrameIsTheFrameLocalAddressBesideTheStoryWideOnes) {
  Host host(600, 500);
  twoFrames(host);
  const sigil::weave::ParagraphLayout* head =
      host.composer.paragraphLayout("a");
  ASSERT_NE(head, nullptr);

  // On its own it is everything that frame holds, and nothing anywhere
  // else.
  EXPECT_FALSE(host.composer.units("b", sel::inFrame("b"), unit::Line).empty());
  EXPECT_TRUE(host.composer.units("a", sel::inFrame("b"), unit::Line).empty());
  // Composed, it cuts a story-wide address to one frame: the story's line 0
  // is in frame a, so asking for it inside frame b addresses nothing.
  EXPECT_TRUE(
      host.composer.units("b", sel::inFrame("b") & sel::line(0), unit::Line)
          .empty());
  EXPECT_FALSE(
      host.composer.units("a", sel::inFrame("a") & sel::line(0), unit::Line)
          .empty());
}

// ── The line-edge and full-width tables, from a compose leaf ─────────────

TEST(ComposeLineTables, TsumeClosesTheGapsBetweenFullWidthCharacters) {
  // The room between two full-width characters is a table, and tsume is
  // the fraction closed at every gap the table gives no class of its own.
  // A passage set with it is narrower than the same passage without.
  const auto widthWith = [](float tsume) {
    Host host(400, 300);
    Element leaf =
        text(toU8("あいうえお、かきくけこ。さしすせそ"), whiteStyle(20))
            .key("t")
            .width(Dim(360.0f));
    if (tsume != 0) leaf.mojikumi({}, tsume);
    host.composer.render(box().child(std::move(leaf)));
    host.frame();
    const std::vector<TextUnit> line =
        host.composer.units("t", sel::line(0), unit::Line);
    return line.empty() ? 0.0f : line.front().rect.width();
  };
  const float plain = widthWith(0.0f);
  ASSERT_GT(plain, 0.0f);
  EXPECT_LT(widthWith(0.5f), plain);
}

TEST(ComposeStory, BeatsSpanTheChainOnOneMasterProgress) {
  Host host(600, 500);
  Story article(rich(whiteStyle(13)).add(longPassage()));
  const auto reveal = [] {
    Track track;
    track.effect = fx::rise(20.0f);
    track.over = unit::Word;
    track.beatsOver = beats::Text;
    track.stagger = {.durationMs = 100.0f, .eachMs = 20.0f};
    track.progress = 0.5f;
    return track;
  };
  host.composer.render(box()
                           .row()
                           .child(frame(article)
                                      .key("a")
                                      .thread("b")
                                      .width(Dim(160.0f))
                                      .height(Dim(70.0f))
                                      .fx(reveal()))
                           .child(frame(article)
                                      .key("b")
                                      .width(Dim(160.0f))
                                      .height(Dim(400.0f))
                                      .fx(reveal())));
  host.frame();
  const std::vector<Beat> first = host.composer.beatsOf("a", 0);
  const std::vector<Beat> second = host.composer.beatsOf("b", 0);
  ASSERT_FALSE(first.empty());
  ASSERT_FALSE(second.empty());
  // A cascade over a threaded story runs ONE clock across the whole of it:
  // the second frame's first word carries on from where the first frame's
  // last word left off rather than restarting at beat 0.
  EXPECT_EQ(first.front().unitIndex, 0u);
  EXPECT_GT(second.front().unitIndex, first.back().unitIndex);
  EXPECT_GT(second.front().startMs, first.back().startMs);
}

// ── The room left over down a frame ──────────────────────────────────────

TEST(ComposeFrameOptions, DistributeSpendsTheRoomLeftOverDownTheBox) {
  // A leaf of a STATED height taller than its lines has room left over,
  // and `distribute` says what becomes of it. kStart leaves it past the
  // last line; kCenter puts half of it above; kEnd puts all of it above;
  // kJustify spreads it BETWEEN the lines as extra leading, which moves
  // the last line to the foot and leaves the first where it stood.
  const auto baselines = [](sigil::weave::FrameOptions::Distribute rule) {
    Host host(360, 400);
    host.composer.render(box().child(text(passage(), whiteStyle(14))
                                         .key("t")
                                         .width(Dim(200.0f))
                                         .height(Dim(300.0f))
                                         .distribute(rule)));
    host.frame();
    return baselinesOf(host, "t");
  };
  using Distribute = sigil::weave::FrameOptions::Distribute;
  const std::vector<float> start = baselines(Distribute::kStart);
  ASSERT_GE(start.size(), 3u);
  const std::vector<float> centred = baselines(Distribute::kCenter);
  const std::vector<float> ended = baselines(Distribute::kEnd);
  const std::vector<float> spread = baselines(Distribute::kJustify);
  ASSERT_EQ(centred.size(), start.size());
  ASSERT_EQ(ended.size(), start.size());
  ASSERT_EQ(spread.size(), start.size());

  const float used = start.back() - start.front();
  const float leftover = 300.0f - used;
  ASSERT_GT(leftover, 40.0f);
  // Half above, then all above: a pure translation, so every line moves
  // by the same amount and the second is twice the first.
  EXPECT_NEAR(centred.front() - start.front(), (ended.front() - start.front()) * 0.5f,
              2.0f);
  EXPECT_GT(ended.front() - start.front(), 20.0f);
  EXPECT_NEAR(centred.back() - start.back(), centred.front() - start.front(), 0.5f);
  // Spread: the first line does not move and every gap opens.
  EXPECT_NEAR(spread.front(), start.front(), 0.5f);
  EXPECT_GT(spread.back() - start.back(), 20.0f);
  EXPECT_GT(spread[1] - spread[0], (start[1] - start[0]) + 1.0f);
}

TEST(ComposeFrameOptions, DistributeSpendsTheRoomLeftOverDownAStoryFrame) {
  // The same of a frame of a story, which is the form a column of a
  // magazine is written in.
  const auto baselines = [](sigil::weave::FrameOptions::Distribute rule) {
    Host host(360, 400);
    Story article(rich(whiteStyle(14)).add(passage()));
    host.composer.render(box().child(frame(article)
                                         .key("t")
                                         .width(Dim(200.0f))
                                         .height(Dim(300.0f))
                                         .distribute(rule)));
    host.frame();
    return baselinesOf(host, "t");
  };
  using Distribute = sigil::weave::FrameOptions::Distribute;
  const std::vector<float> start = baselines(Distribute::kStart);
  ASSERT_GE(start.size(), 3u);
  const std::vector<float> ended = baselines(Distribute::kEnd);
  const std::vector<float> spread = baselines(Distribute::kJustify);
  ASSERT_EQ(ended.size(), start.size());
  ASSERT_EQ(spread.size(), start.size());
  EXPECT_GT(ended.front() - start.front(), 20.0f);
  EXPECT_NEAR(spread.front(), start.front(), 0.5f);
  EXPECT_GT(spread.back() - start.back(), 20.0f);
}


// ── The three passes a justified line is fitted in ───────────────────────

/** The right edge each line of a justified passage reached, in the measure
 *  `spec` set it in. */
std::vector<float> justifiedEdges(
    sigil::weave::JustificationOptions spec, const char* body, float measure,
    sigil::weave::LineBreakStrategy strategy =
        sigil::weave::LineBreakStrategy::kGreedy) {
  Host host(400, 400);
  host.composer.render(box().child(text(toU8(body), whiteStyle(12))
                                       .key("t")
                                       .width(Dim(measure))
                                       .textAlign(
                                           sigil::weave::TextAlignment::kJustify)
                                       .lineBreak(strategy)
                                       .justification(spec)));
  host.frame();
  std::vector<float> edges;
  for (const TextUnit& line :
       host.composer.units("t", sel::each(unit::Line), unit::Line))
    edges.push_back(line.rect.right());
  return edges;
}

/** A passage that wraps to several justified lines at the measure below. */
const char* kJustified =
    "Justification spends interword gaps before letterspacing, and "
    "reaches for horizontal glyph-scaling last of all.";

TEST(ComposeJustification, EachPassPastTheGapsChangesTheSetting) {
  // A justified line is fitted in three passes: the word gaps first, then
  // letter spacing between the glyphs, then a horizontal scale on the
  // glyphs. Each pass's DESIRED value widens the line before any of them
  // is fitted, and its two limits bound what it may add on top — so a
  // passage set with a pass opened is set differently from the same
  // passage without it, which is the whole of what the passes are for.
  const std::vector<float> stock =
      justifiedEdges({}, kJustified, 130.0f);
  ASSERT_GE(stock.size(), 4u);
  // The stock fit is the gaps alone, and they fill the measure.
  for (size_t index = 0; index + 1 < stock.size(); ++index)
    EXPECT_NEAR(stock[index], 130.0f, 0.5f);

  // WHERE THE WORDS LAND, not where the lines end: EVERY justified line
  // ends at the measure whatever passes are open, because the gaps take
  // back whatever no pass could spend. What a pass changes is the
  // INTERIOR of the line — how much of the fit stands in the gaps and how
  // much between the letters — so that is what is read here.
  const auto wordEdgesUnder = [](sigil::weave::JustificationOptions spec) {
    Host host(400, 400);
    host.composer.render(box().child(
        text(toU8(kJustified), whiteStyle(12))
            .key("t")
            .width(Dim(130.0f))
            .textAlign(sigil::weave::TextAlignment::kJustify)
            .justification(spec)));
    host.frame();
    std::vector<float> lefts;
    for (const TextUnit& word :
         host.composer.units("t", sel::each(unit::Word), unit::Word))
      lefts.push_back(word.rect.left());
    return lefts;
  };
  const std::vector<float> stockWords = wordEdgesUnder({});
  ASSERT_GE(stockWords.size(), 4u);

  const auto differsFromStock = [&](sigil::weave::JustificationOptions spec) {
    const std::vector<float> words = wordEdgesUnder(spec);
    if (words.size() != stockWords.size()) return true;
    for (size_t index = 0; index < words.size(); ++index)
      if (std::abs(words[index] - stockWords[index]) > 0.5f) return true;
    return false;
  };

  sigil::weave::JustificationOptions letters;
  letters.letterSpacing = 0.05f;
  letters.letterSpacingMaximum = 0.1f;
  EXPECT_TRUE(differsFromStock(letters)) << "letterSpacing";

  // A SCALE MEANT TO HOLD SAYS SO WITH ITS LIMITS. Room above the desired
  // scale is room the fit spends: an underfull line scales up through it
  // before the gaps take anything back, so a scale of 0.92 with the stock
  // maximum of 1 is a scale of 1 on every line that needed widening —
  // which is the same setting as stock and not a different one. Pinned
  // both sides, it is what every justified line is set at.
  sigil::weave::JustificationOptions glyphs;
  glyphs.glyphScale = 0.92f;
  glyphs.glyphScaleMinimum = 0.92f;
  glyphs.glyphScaleMaximum = 0.92f;
  EXPECT_TRUE(differsFromStock(glyphs)) << "glyphScale";
}

TEST(ComposeJustification, TheWidthAGapIsAimedAtIsWhatABreakIsWeighedAgainst) {
  // The gap pass's own desired width is not a placement decision: a
  // justified line fills its measure whatever the gaps were aimed at. It
  // is what the optimizing breaker weighs a break against, so a passage
  // aimed at twice the shaped space is BROKEN differently.
  using Strategy = sigil::weave::LineBreakStrategy;
  const std::vector<float> stock =
      justifiedEdges({}, kJustified, 130.0f, Strategy::kKnuthPlass);
  sigil::weave::JustificationOptions wider;
  wider.wordSpacing = 2.0f;
  const std::vector<float> aimed =
      justifiedEdges(wider, kJustified, 130.0f, Strategy::kKnuthPlass);
  bool differs = aimed.size() != stock.size();
  for (size_t index = 0; !differs && index < aimed.size(); ++index)
    differs = std::abs(aimed[index] - stock[index]) > 0.5f;
  EXPECT_TRUE(differs);
}

TEST(ComposeJustification, TheLetterPassTakesWhatTheGapsMayNotStretchTo) {
  // The gaps open only to their own stretch limit where a later pass can
  // spend what they may not: with the letter pass opened, the gaps stay
  // near the width they were aimed at and the letters take the rest, so
  // the line still reaches the measure with tighter word spacing than the
  // gaps alone would have left.
  const auto secondWordOf = [](sigil::weave::JustificationOptions spec) {
    Host host(400, 400);
    host.composer.render(box().child(
        text(toU8(kJustified), whiteStyle(12))
            .key("t")
            .width(Dim(130.0f))
            .textAlign(sigil::weave::TextAlignment::kJustify)
            .justification(spec)));
    host.frame();
    const std::vector<TextUnit> words =
        host.composer.units("t", sel::each(unit::Word), unit::Word);
    return words.size() > 1 ? words[1].rect.left() : 0.0f;
  };
  sigil::weave::JustificationOptions tightGaps;
  tightGaps.spaceStretch = 0.02f;
  tightGaps.letterSpacingMaximum = 0.3f;
  const std::vector<float> closed =
      justifiedEdges(tightGaps, kJustified, 130.0f);
  ASSERT_GE(closed.size(), 2u);
  EXPECT_NEAR(closed.front(), 130.0f, 1.0f) << "the line did not fill";
  EXPECT_LT(secondWordOf(tightGaps), secondWordOf({}) - 1.0f)
      << "the gaps opened as wide as they always did";
}

TEST(ComposeJustification, GapsStayUnboundedWhenNoLaterPassCanSpendWhatTheyDrop) {
  // A bound on the gaps with both later passes shut would open a hole at
  // the right margin that nothing in the line is allowed to close. So a
  // stretch limit alone, and a rule about lone-word lines alone, leave
  // every ordinary line set exactly as the gaps alone set it.
  const std::vector<float> stock = justifiedEdges({}, kJustified, 130.0f);
  ASSERT_GE(stock.size(), 2u);
  sigil::weave::JustificationOptions tightGaps;
  tightGaps.spaceStretch = 0.02f;
  sigil::weave::JustificationOptions lone;
  lone.singleWord = sigil::weave::JustificationOptions::SingleWord::kJustify;
  for (const sigil::weave::JustificationOptions& spec : {tightGaps, lone}) {
    const std::vector<float> edges = justifiedEdges(spec, kJustified, 130.0f);
    ASSERT_EQ(edges.size(), stock.size());
    for (size_t index = 0; index < edges.size(); ++index)
      EXPECT_NEAR(edges[index], stock[index], 0.5f);
  }
}

TEST(ComposeJustification, ALineOfOneWordStretchesOnlyWhenAskedTo) {
  // A line holding ONE word has no gaps to spend at all: kAlign leaves it
  // at the block's alignment, kJustify stretches it across the measure
  // with letter spacing alone.
  sigil::weave::JustificationOptions aligned;
  aligned.justifyLastLine = true;
  sigil::weave::JustificationOptions stretched = aligned;
  stretched.singleWord =
      sigil::weave::JustificationOptions::SingleWord::kJustify;
  // The measure is derived from the word's own shaped width rather than
  // named, so the word is on one line whatever face the machine hands us
  // and the stretch target is a number this case computed rather than one
  // fitted to a face.
  const char* kWord = "Antidisestablishmentarianism";
  float wordWidth = 0;
  for (float advance : measureRun(toU8(kWord), whiteStyle(12), fonts()))
    wordWidth += advance;
  ASSERT_GT(wordWidth, 0.0f);
  const float measure = wordWidth + 40.0f;
  const std::vector<float> left = justifiedEdges(aligned, kWord, measure);
  const std::vector<float> spread = justifiedEdges(stretched, kWord, measure);
  ASSERT_EQ(left.size(), 1u) << "the word did not fit on one line";
  ASSERT_EQ(spread.size(), 1u);
  EXPECT_LT(left.front(), measure - 20.0f);
  EXPECT_NEAR(spread.front(), measure, 1.0f);
}

TEST(ComposeJustification, TheLastLineJustifiesOnlyWhenAskedTo) {
  const std::vector<float> ragged = justifiedEdges({}, kJustified, 130.0f);
  sigil::weave::JustificationOptions all;
  all.justifyLastLine = true;
  const std::vector<float> full = justifiedEdges(all, kJustified, 130.0f);
  ASSERT_EQ(ragged.size(), full.size());
  EXPECT_LT(ragged.back(), 128.0f);
  EXPECT_NEAR(full.back(), 130.0f, 1.0f);
}

// ── What a live passage's last layout cost ──────────────────────────────

/** A passage whose measure swells from 150 px to 230 px and back, drawn at
 *  every whole pixel, then set once more at @p endAt — and what
 *  `Composer::settling` reports about that last frame. */
TextSettling sweptSettling(bool live, float budgetMicroseconds, float endAt) {
  Host host(280, 320);
  const auto step = [&](float measure) {
    Element leaf =
        text(toU8("A measure that animates is one input of a run of layouts "
                  "rather than a question somebody asked once, and the block "
                  "that knows so keeps the break decisions it has already "
                  "made."),
             whiteStyle(11.5f))
            .key("para")
            .width(Dim(measure))
            .lineBreak(sigil::weave::LineBreakStrategy::kKnuthPlass);
    if (live) leaf.live(true, budgetMicroseconds);
    host.composer.render(box().padding(10).child(std::move(leaf)));
    host.frame();
  };
  for (float measure = 150; measure <= 230; measure += 1) step(measure);
  for (float measure = 230; measure >= 150; measure -= 1) step(measure);
  step(endAt);
  return host.composer.settling("para");
}

TEST(ComposeSettling, AMeasureAlreadyCrossedCostsNoBreakDecision) {
  // The break decisions of a live block are kept and reused, keyed on the
  // words and on the measure taken to the whole pixel below it. A swell
  // that has run the whole range and comes back to a measure inside it
  // therefore decides nothing — and the report says so, at either end.
  const TextSettling narrow = sweptSettling(true, 4000.0f, 150.0f);
  EXPECT_TRUE(narrow.live);
  EXPECT_GT(narrow.reused, 0);
  EXPECT_EQ(narrow.degraded, 0);
  const TextSettling wide = sweptSettling(true, 4000.0f, 230.0f);
  EXPECT_TRUE(wide.live);
  EXPECT_GT(wide.reused, 0);
  EXPECT_EQ(wide.degraded, 0);
}

TEST(ComposeSettling, APassageThatNeverSaidItMovesStoresNothing) {
  const TextSettling settled = sweptSettling(false, 0.0f, 230.0f);
  EXPECT_FALSE(settled.live);
  EXPECT_EQ(settled.reused, 0);
  EXPECT_EQ(settled.degraded, 0);
}

TEST(ComposeSettling, ABudgetNothingCanMeetDegradesAndSaysSo) {
  // The floor under a frame the optimizing breaker cannot finish in time:
  // the block is filled greedily for that frame and counted. A budget is
  // read DURING the search, so a block shorter than whatever stride the
  // search reads it at would otherwise never read it at all — which is
  // every ordinary paragraph.
  const TextSettling starved = sweptSettling(true, 1.0f, 230.0f);
  EXPECT_TRUE(starved.live);
  EXPECT_GT(starved.degraded, 0);
}

// ── The kit's hanging list ──────────────────────────────────────────────

TEST(KitBullets, TheMarkerKeepsTheRoomTheIndentOpened) {
  // The marker is placed beside the item's text, at the block's own start,
  // and the item is indented by the hang on EVERY line. A first line
  // pulled back out of the indent would start where the marker already
  // stands and print through it.
  //
  // The marker is left empty here so the only ink on the sheet is the
  // item's own: its leftmost column IS where the first line begins.
  constexpr float kHang = 24.0f;
  const std::array<std::u8string, 1> items = {
      toU8("First line long enough that this item wraps, and a second that "
           "carries on under it.")};
  const std::array<std::u8string, 1> markers = {std::u8string()};
  Host host(300, 160);
  host.composer.render(box().padding(0).child(
      kit::bullets(items, markers, whiteStyle(13), kHang, 240.0f)));
  host.frame();
  int leftmost = 300;
  for (int y = 0; y < 160; ++y)
    for (int x = 0; x < leftmost; ++x)
      if (host.pixel(x, y) != SK_ColorBLACK) {
        leftmost = x;
        break;
      }
  ASSERT_LT(leftmost, 300) << "the list drew nothing";
  EXPECT_GE((float)leftmost, kHang - 1.0f)
      << "the first line was pulled back onto the marker's room";
}
