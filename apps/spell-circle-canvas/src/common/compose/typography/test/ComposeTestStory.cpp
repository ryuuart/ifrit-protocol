// A story through a chain of frames: each frame fills from where the one
// before it stopped, the marker ends the chain, the lines are numbered
// from the story rather than from the frame, and one master progress
// carries the beats across the whole chain.

#include <sigilcompose/kit/Typeset.h>

#include <array>
#include <string>
#include <vector>

#include "support/ParagraphTestSupport.h"

namespace {

/** A chain of two frames over one story, drawn once. */
void twoFrames(Host& host, float measure = 160.0f) {
  sigil::weave::Story article(
      sigil::weave::rich(whiteStyle(13)).add(longPassage()));
  host.composer.render(box().row().children(
      {frame(article).key("a").textThreadTo("b").width(measure).height(70.0f),
       frame(article).key("b").width(measure).height(400.0f)}));
  host.frame();
}

}  // namespace

TEST(ComposeStory, EachFrameFillsFromWhereTheOneBeforeItStopped) {
  sigil::weave::Story article(sigil::weave::rich(whiteStyle(13))
                                  .add(passage())
                                  .add(u8" ")
                                  .add(passage()));
  Host host(500, 300);
  host.composer.render(box().row().children(
      {frame(article).key("a").textThreadTo("b").width(160.0f).height(60.0f),
       frame(article).key("b").width(160.0f).height(200.0f)}));
  host.frame();
  const std::vector<TextUnit> first = host.composer.units(
      "a", sigil::weave::selectors::each(sigil::weave::Unit::Word),
      sigil::weave::Unit::Word);
  const std::vector<TextUnit> second = host.composer.units(
      "b", sigil::weave::selectors::each(sigil::weave::Unit::Word),
      sigil::weave::Unit::Word);
  // The first frame ran out of room, which is the normal case for every
  // frame of a chain but the last.
  const sigil::weave::ParagraphLayout* head =
      host.composer.paragraphLayout("a");
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
    sigil::weave::Story article(sigil::weave::rich(whiteStyle(13))
                                    .add(passage())
                                    .add(u8" ")
                                    .add(passage()));
    Host host(500, 300);
    host.composer.render(box().row().children(
        {frame(article).key("a").textThreadTo("b").width(measure).height(60.0f),
         frame(article).key("b").width(160.0f).height(200.0f)}));
    host.frame();
    const std::vector<TextUnit> second = host.composer.units(
        "b", sigil::weave::selectors::each(sigil::weave::Unit::Word),
        sigil::weave::Unit::Word);
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
    sigil::weave::Story article(sigil::weave::rich(whiteStyle(13))
                                    .add(passage())
                                    .add(u8" ")
                                    .add(passage()));
    host.composer.render(box().children({kit::textColumns(
        article, 3, 12.0f, 240.0f, 32.0f, "col", std::move(marker))}));
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

TEST(ComposeStory, ABalancedRunHoldsTheStoryDownToTheLineItWasGiven) {
  // `textThreadBalance(throughLine)` is what stops a run of columns at a
  // spanning element: the run is shortened to the shallowest depth that
  // still holds the story DOWN TO THAT LINE, and everything after it is
  // left to the frames below. The number is the STORY's line, which is the
  // one address a second run can be given — a count of its own lines would
  // say nothing about where the first run ended.
  std::u8string words;
  for (int i = 0; i < 120; ++i) words += u8"aa ";
  words.pop_back();
  const sigil::weave::Story article{words, whiteStyle(12)};
  Host host(600, 500);
  const uint32_t through = 5;
  host.composer.render(box().row().children(
      {frame(article)
           .key("a")
           .textThreadTo("b")
           .width(120.0f)
           .height(200.0f)
           .textThreadBalance(through),
       frame(article).key("b").textThreadTo("c").width(120.0f).height(200.0f),
       frame(article)
           .key("c")
           .width(120.0f)
           .height(200.0f)
           .textThreadBalance()}));
  host.frame();
  host.frame();  // the first draw has no fill to balance against

  const float depthA = require(host.composer.bounds("a")).height();
  const float depthB = require(host.composer.bounds("b")).height();
  EXPECT_FLOAT_EQ(depthA, depthB) << "a run resolves to ONE depth";
  EXPECT_LT(depthA, 200) << "…shallower than the depth it declared";
  EXPECT_GT(depthA, 0);

  const auto holds = [&](const char* key, uint32_t line) {
    return !host.composer
                .units(key, sigil::weave::selectors::line(line),
                       sigil::weave::Unit::Line)
                .empty();
  };
  // The line the run was asked to reach is the last one it holds, and the
  // line after it opens the frame below the run.
  EXPECT_TRUE(holds("a", through) || holds("b", through));
  EXPECT_FALSE(holds("c", through));
  EXPECT_TRUE(holds("c", through + 1));
}

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
  EXPECT_FALSE(host.composer
                   .units("a", sigil::weave::selectors::line(0),
                          sigil::weave::Unit::Line)
                   .empty());
  EXPECT_TRUE(host.composer
                  .units("b", sigil::weave::selectors::line(0),
                         sigil::weave::Unit::Line)
                  .empty());
  // …and the line just past it is the second frame's first line, addressed
  // by its number in the STORY rather than by its number in the frame.
  EXPECT_TRUE(host.composer
                  .units("a",
                         sigil::weave::selectors::line((uint32_t)headLines),
                         sigil::weave::Unit::Line)
                  .empty());
  EXPECT_FALSE(host.composer
                   .units("b",
                          sigil::weave::selectors::line((uint32_t)headLines),
                          sigil::weave::Unit::Line)
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
  EXPECT_FALSE(
      host.composer
          .units("b", selectors::inFrame("b"), sigil::weave::Unit::Line)
          .empty());
  EXPECT_TRUE(host.composer
                  .units("a", selectors::inFrame("b"), sigil::weave::Unit::Line)
                  .empty());
  // Composed, it cuts a story-wide address to one frame: the story's line 0
  // is in frame a, so asking for it inside frame b addresses nothing.
  EXPECT_TRUE(
      host.composer
          .units("b",
                 selectors::inFrame("b") & sigil::weave::selectors::line(0),
                 sigil::weave::Unit::Line)
          .empty());
  EXPECT_FALSE(
      host.composer
          .units("a",
                 selectors::inFrame("a") & sigil::weave::selectors::line(0),
                 sigil::weave::Unit::Line)
          .empty());
}

TEST(ComposeStory, BeatsSpanTheChainOnOneMasterProgress) {
  Host host(600, 500);
  sigil::weave::Story article(
      sigil::weave::rich(whiteStyle(13)).add(longPassage()));
  const auto reveal = [] {
    Track track;
    track.effect = fx::rise(20.0f);
    track.unit = sigil::weave::Unit::Word;
    track.beatsOver = beats::Text;
    track.stagger = {.eachMs = 20.0f, .durationMs = 100.0f};
    track.progress = 0.5f;
    return track;
  };
  host.composer.render(box().row().children(
      {frame(article).key("a").textThreadTo("b").width(160.0f).height(70.0f).fx(
           reveal()),
       frame(article).key("b").width(160.0f).height(400.0f).fx(reveal())}));
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
