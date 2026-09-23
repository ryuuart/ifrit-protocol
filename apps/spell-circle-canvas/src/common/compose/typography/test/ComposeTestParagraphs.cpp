// The block controls a leaf states, through its own verbs: what a
// ParagraphStyle opens before and between blocks, the tables that close
// the gaps between full-width characters, and the room left over down a
// box and down a story frame.

#include <string>
#include <vector>

#include "support/ParagraphTestSupport.h"

namespace {

/** Two blocks, the second after a hard break. */
std::u8string twoBlocks() {
  return u8"First block runs on for several words so that it wraps here.\n"
         u8"Second block does the same and wraps as well over here.";
}

/** Distinct baselines of a keyed text node's placed lines, ascending. */
std::vector<float> baselinesOf(Host& host, const char* key) {
  std::vector<float> found;
  const std::vector<TextUnit> lines = host.composer.units(
      key, sigil::weave::selectors::each(sigil::weave::Unit::Line),
      sigil::weave::Unit::Line);
  for (const TextUnit& line : lines) found.push_back(line.axis);
  return found;
}

}  // namespace

TEST(ComposeParagraphs, ReservedBeforeBelongsToTheMeasuredLeaf) {
  const auto render = [](Host& host, float before) {
    host.composer.render(box().column().padding(10).children(
        {text(u8"First line\nSecond line\nThird line", whiteStyle(18))
             .key("passage")
             .width(200)
             .textLineMargin({.before = before}),
         box().key("next").width(200).height(10).fill(red())}));
    host.frame();
  };
  Host plain(260, 300), reserved(260, 300);
  render(plain, 0);
  render(reserved, 14);
  const auto bare = plain.composer.bounds("passage");
  const auto withRoom = reserved.composer.bounds("passage");
  const auto next = reserved.composer.bounds("next");
  ASSERT_TRUE(bare && withRoom && next);
  const auto lines = reserved.composer.units(
      "passage", sigil::weave::selectors::each(sigil::weave::Unit::Line),
      sigil::weave::Unit::Line);
  ASSERT_EQ(lines.size(), 3u);
  EXPECT_NEAR(withRoom->height() - bare->height(), 14 * lines.size(), 1.0f);
  EXPECT_GE(withRoom->bottom() + 0.01f, lines.back().rect.bottom());
  EXPECT_GE(next->top() + 0.01f, lines.back().rect.bottom());
}

TEST(ComposeParagraphs, ReservedBeforeParticipatesInBaselineAlignment) {
  Host host(300, 160);
  host.composer.render(box()
                           .row()
                           .alignItems(Align::Baseline)
                           .gap(20)
                           .children({text(u8"A", whiteStyle(20)).key("plain"),
                                      text(u8"A", whiteStyle(20))
                                          .key("reserved")
                                          .textLineMargin({.before = 14})}));
  host.frame();
  const auto plain = baselinesOf(host, "plain");
  const auto reserved = baselinesOf(host, "reserved");
  ASSERT_EQ(plain.size(), 1u);
  ASSERT_EQ(reserved.size(), 1u);
  EXPECT_NEAR(plain.front(), reserved.front(), 0.01f);
}

TEST(ComposeParagraphs, ABlockStyleOpensThePitchTheLeafSetsIt) {
  Host plain(360, 300);
  plain.composer.render(
      box().children({text(passage(), whiteStyle(14)).key("t").width(200.0f)}));
  plain.frame();
  const std::vector<float> tight = baselinesOf(plain, "t");

  Host led(360, 300);
  led.composer.render(box().children(
      {text(passage(), whiteStyle(14))
           .key("t")
           .width(200.0f)
           .paragraphStyles(
               {{.leading = sigil::weave::Leading::multiple(2.0f)}})}));
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
  // At 13 px in the instrument face the first block's first line is "First
  // block runs on for several words so" at 288.6 px, so centred in 300 it
  // starts 5.7 px in.
  host.composer.render(box().children({text(twoBlocks(), whiteStyle(13))
                                           .key("t")
                                           .width(300.0f)
                                           .paragraphStyles({heading})}));
  host.frame();
  const std::vector<TextUnit> lines = host.composer.units(
      "t", sigil::weave::selectors::each(sigil::weave::Unit::Line),
      sigil::weave::Unit::Line);
  ASSERT_GE(lines.size(), 3u);
  // The first block's lines are centred and the last block's are not.
  EXPECT_GT(lines.front().rect.left(), 1.0f);
  EXPECT_LT(lines.back().rect.left(), 1.0f);
}

TEST(ComposeLineTables, TsumeClosesTheGapsBetweenFullWidthCharacters) {
  // The room between two full-width characters is a table, and tsume is
  // the fraction closed at every gap the table gives no class of its own.
  // A passage set with it is narrower than the same passage without.
  const auto widthWith = [](float tsume) {
    Host host(400, 300);
    Element leaf = text(u8"あいうえお、かきくけこ。さしすせそ", whiteStyle(20))
                       .key("t")
                       .width(360.0f);
    if (tsume != 0)
      leaf.paragraph(
          {.mojikumi = sigil::weave::MojikumiTable{}, .tsume = tsume});
    host.composer.render(box().children({std::move(leaf)}));
    host.frame();
    const std::vector<TextUnit> line = host.composer.units(
        "t", sigil::weave::selectors::line(0), sigil::weave::Unit::Line);
    return line.empty() ? 0.0f : line.front().rect.width();
  };
  const float plain = widthWith(0.0f);
  ASSERT_GT(plain, 0.0f);
  EXPECT_LT(widthWith(0.5f), plain);
}

TEST(ComposeFrameOptions, DistributeSpendsTheRoomLeftOverDownTheBox) {
  // A leaf of a STATED height taller than its lines has room left over,
  // and `textVerticalAlign` says what becomes of it. kStart leaves it past the
  // last line; kCenter puts half of it above; kEnd puts all of it above;
  // kJustify spreads it BETWEEN the lines as extra leading, which moves
  // the last line to the foot and leaves the first where it stood.
  const auto baselines = [](sigil::weave::FrameOptions::Distribute rule) {
    Host host(360, 400);
    host.composer.render(box().children({text(passage(), whiteStyle(14))
                                             .key("t")
                                             .width(200.0f)
                                             .height(300.0f)
                                             .textVerticalAlign(rule)}));
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
  EXPECT_NEAR(centred.front() - start.front(),
              (ended.front() - start.front()) * 0.5f, 2.0f);
  EXPECT_GT(ended.front() - start.front(), 20.0f);
  EXPECT_NEAR(centred.back() - start.back(), centred.front() - start.front(),
              0.5f);
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
    sigil::weave::Story article(
        sigil::weave::rich(whiteStyle(14)).add(passage()));
    host.composer.render(box().children(
        {frame(article).key("t").width(200.0f).height(300.0f).textVerticalAlign(
            rule)}));
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
