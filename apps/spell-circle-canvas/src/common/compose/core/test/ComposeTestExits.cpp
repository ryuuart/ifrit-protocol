// Exits: a keyed node stating exit() that a description leaves out keeps
// its place, plays its entrances backwards, answers no hit while it does,
// and goes on the frame the last of them lands; one named again turns
// round; a run leaving together leaves in the reverse of the order it came
// in; and a text entrance runs its track back to where its glyphs began.

#include <sigilcompose/typography/Typography.h>
#include <sigilmotion/ease/Ease.h>

#include "support/CoreTestSupport.h"
#include "support/Rows.h"

namespace {

/** A red strip that fades in over 200ms and leaves as it came. */
Element fading(std::string_view key) {
  return box()
      .key(key)
      .width(80)
      .height(40)
      .fill(red())
      .opacity(motion::animate({.from = 0.0f,
                                .to = 1.0f,
                                .duration = 200ms,
                                .ease = motion::ease::linear}))
      .exit();
}

/** A green strip with no entrance, under whatever leaves above it. */
Element staying() {
  return box().key("stay").width(80).height(40).fill(green());
}

}  // namespace

TEST(ComposeExits, AChildNamedAgainWhileLeavingTurnsRound) {
  Host host;
  const auto both = [] { return box().children({fading("leave"), staying()}); };
  host.composer.render(both());
  host.frame(0.3);
  host.composer.render(box().children({staying()}));
  host.frame(0.1);  // half way out
  host.composer.render(both());
  host.frame();
  const SkColor turned = host.pixel(40, 20);
  EXPECT_GT(SkColorGetR(turned), 90u);
  EXPECT_LT(SkColorGetR(turned), 165u);
  // On the way back in it answers hits again, and the time its exit would
  // have landed passes with it still there and still coming.
  EXPECT_EQ(host.composer.hitTest({40, 20}), "leave");
  host.frame(0.1);
  EXPECT_TRUE(host.composer.bounds("leave").has_value());
  EXPECT_GT(SkColorGetR(host.pixel(40, 20)), SkColorGetR(turned));
  host.frame(0.3);
  EXPECT_EQ(host.pixel(40, 20), SK_ColorRED);
  EXPECT_EQ(host.pixel(40, 60), SK_ColorGREEN);
}

TEST(ComposeExits, AChildWithNoExitOrNothingToPlayDropsAtOnce) {
  Host host;
  const auto plain =
      box().key("plain").width(80).height(40).fill(red()).opacity(
          motion::animate({.from = 0.0f, .to = 1.0f, .duration = 200ms}));
  const auto stillOne =
      box().key("still").width(80).height(40).fill(red()).exit();
  host.composer.render(box().children({plain, stillOne, staying()}));
  host.frame(0.3);
  host.composer.render(box().children({staying()}));
  host.frame();
  // One states no exit, the other states it with nothing that entered.
  EXPECT_FALSE(host.composer.bounds("plain").has_value());
  EXPECT_FALSE(host.composer.bounds("still").has_value());
  EXPECT_FLOAT_EQ(host.composer.bounds("stay")->top(), 0.0f);
}

// A keyed node stating exit that a description leaves out keeps its place,
// plays its entrances backwards, a run of them last first, and goes the
// frame the last lands.
TEST(ComposeExits, ALeavingNodePlaysItsEntrancesBackwardsAndGoesWhenTheyLand) {
  sigil::compose::test::checkRows({
      {"AKeyedChildLeavesAsItEnteredAndGoesWhenItLands",
       [] {
         Host host;
         host.composer.render(box().children({fading("leave"), staying()}));
         host.frame(0.3);  // the entrance has landed
         EXPECT_EQ(host.pixel(40, 20), SK_ColorRED);
         EXPECT_EQ(host.pixel(40, 60), SK_ColorGREEN);
         EXPECT_EQ(host.composer.hitTest({40, 20}), "leave");

         host.composer.render(box().children({staying()}));
         host.frame();
         // Still there, at the opacity its entrance ended on, and in its place:
         // the sibling below has not moved up.
         EXPECT_EQ(host.pixel(40, 20), SK_ColorRED);
         EXPECT_EQ(host.pixel(40, 60), SK_ColorGREEN);
         ASSERT_TRUE(host.composer.bounds("stay").has_value());
         EXPECT_FLOAT_EQ(host.composer.bounds("stay")->top(), 40.0f);
         // Leaving, it lets the pointer through.
         EXPECT_NE(host.composer.hitTest({40, 20}), "leave");

         host.frame(0.1);  // half way back along the linear ramp
         const SkColor half = host.pixel(40, 20);
         EXPECT_GT(SkColorGetR(half), 90u);
         EXPECT_LT(SkColorGetR(half), 165u);
         EXPECT_TRUE(host.composer.bounds("leave").has_value());

         host.frame(
             0.12);  // landed: gone on this frame, and the sibling moves up
         EXPECT_FALSE(host.composer.bounds("leave").has_value());
         EXPECT_EQ(host.pixel(40, 20), SK_ColorGREEN);
         EXPECT_FLOAT_EQ(host.composer.bounds("stay")->top(), 0.0f);
         EXPECT_FALSE(host.composer.isRunning());
       }},
      {"ARunLeavingTogetherLeavesInReverse",
       [] {
         // Three rows that came in a step apart go out the same step apart,
         // last first: each one's exit waits for as long as the run went on
         // after its entrance ended.
         Host host;
         const auto row = [](std::string_view key) {
           return box()
               .key(key)
               .width(80)
               .height(20)
               .fill(red())
               .opacity(motion::animate({.from = 0.0f,
                                         .to = 1.0f,
                                         .duration = 100ms,
                                         .delay = motion::stagger(100ms),
                                         .ease = motion::ease::linear}))
               .exit();
         };
         host.composer.render(box().children({row("a"), row("b"), row("c")}));
         host.frame(0.5);
         for (const int y : {10, 30, 50})
           EXPECT_EQ(host.pixel(40, y), SK_ColorRED);

         host.composer.render(box());
         host.frame(
             0.15);  // the last row out first: gone, the first still whole
         EXPECT_FALSE(host.composer.bounds("c").has_value());
         EXPECT_TRUE(host.composer.bounds("b").has_value());
         EXPECT_EQ(host.pixel(40, 10), SK_ColorRED);
         host.frame(0.1);  // then the middle one, while the first is on its way
         EXPECT_FALSE(host.composer.bounds("b").has_value());
         EXPECT_TRUE(host.composer.bounds("a").has_value());
         host.frame(0.1);  // and the first one in last
         EXPECT_FALSE(host.composer.bounds("a").has_value());
         EXPECT_EQ(host.pixel(40, 10), SK_ColorBLACK);
       }},
      {"ALeavingTextEntranceRunsItsTrackBackwards",
       [] {
         Host host;
         const auto title = [] {
           return text(u8"EXIT", whiteStyle(24))
               .key("title")
               .exit()
               .textFx(textFx::entrance(textFx::rise(20),
                                        {.progress = motion::animate(
                                             {.from = 0.0f,
                                              .to = 1.0f,
                                              .duration = 300ms,
                                              .ease = motion::ease::linear})}));
         };
         host.composer.render(box().padding(20).children({title()}));
         host.frame(0.5);
         ASSERT_TRUE(anyWhiteIn(host, {0, 0, 200, 80}));
         const std::vector<Beat> rested = host.composer.beatsOf("title", 0);
         ASSERT_EQ(rested.size(), 4u);
         for (const Beat& beat : rested)
           EXPECT_FLOAT_EQ(beat.localProgress, 1.0f);

         host.composer.render(box().padding(20));
         host.frame(0.15);
         // On its way back: the last letter in is the furthest gone, and every
         // letter only goes further back as the run goes on.
         const std::vector<Beat> early = host.composer.beatsOf("title", 0);
         ASSERT_EQ(early.size(), 4u);
         EXPECT_LT(early.back().localProgress, early.front().localProgress);
         EXPECT_LT(early.back().localProgress, 1.0f);
         host.frame(0.075);
         const std::vector<Beat> later = host.composer.beatsOf("title", 0);
         ASSERT_EQ(later.size(), 4u);
         for (size_t index = 0; index < later.size(); ++index)
           EXPECT_LE(later[index].localProgress, early[index].localProgress);

         host.frame(0.1);  // the track is back at its start: the text is gone
         EXPECT_FALSE(host.composer.bounds("title").has_value());
         EXPECT_FALSE(anyWhiteIn(host, {0, 0, 200, 80}));
       }},
      {"ALeavingBareTextEntranceRunsItsTrackBackToItsStart",
       [] {
         // An entrance stating no progress plays on mount over its span, and
         // the exit runs that span backwards: the last letter in goes first,
         // and the text goes as the track arrives back at zero.
         Host host;
         const auto title = [] {
           return text(u8"EXIT", whiteStyle(24))
               .key("title")
               .exit()
               .textFx(textFx::entrance(textFx::rise(20)));
         };
         host.composer.render(box().padding(20).children({title()}));
         host.frame(0.6);  // the 540ms span has passed
         ASSERT_TRUE(anyWhiteIn(host, {0, 0, 200, 80}));
         const std::vector<Beat> rested = host.composer.beatsOf("title", 0);
         ASSERT_EQ(rested.size(), 4u);
         for (const Beat& beat : rested)
           EXPECT_FLOAT_EQ(beat.localProgress, 1.0f);

         host.composer.render(box().padding(20));
         host.frame(0.2);
         const std::vector<Beat> early = host.composer.beatsOf("title", 0);
         ASSERT_EQ(early.size(), 4u);
         EXPECT_LT(early.back().localProgress, early.front().localProgress);
         EXPECT_LT(early.back().localProgress, 1.0f);
         host.frame(0.3);  // 40ms short of the span: nearly back at the start
         const std::vector<Beat> nearly = host.composer.beatsOf("title", 0);
         ASSERT_EQ(nearly.size(), 4u);
         EXPECT_FLOAT_EQ(nearly.back().localProgress, 0.0f);
         for (const Beat& beat : nearly) EXPECT_LT(beat.localProgress, 0.1f);

         host.frame(0.05);  // the track is back at zero: the text is gone
         EXPECT_FALSE(host.composer.bounds("title").has_value());
         EXPECT_FALSE(anyWhiteIn(host, {0, 0, 200, 80}));
       }},
  });
}
