// The block controls, what stands beside a text, and what a decoration
// dresses: ParagraphStyle per block through the leaf's own verbs, the
// per-unit read-back everything beside a passage is placed from, readings
// that reserve room in the base's strut, a story filling a chain of
// frames, and a text leaf whose decorations dress its glyphs.

#include <sigilcompose/kit/Annotations.h>
#include <sigilcompose/kit/Typeset.h>

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
